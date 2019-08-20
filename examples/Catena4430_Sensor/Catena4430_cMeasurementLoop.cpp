/*

Module: Catena4430_cMeasurementLoop.cpp

Function:
    Class for transmitting accumulated measurements.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "Catena4430_cMeasurementLoop.h"

#include <arduino_lmic.h>

using namespace McciCatena4430;

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

void cMeasurementLoop::begin()
    {
    // register for polling.
    if (! this->m_registered)
        {
        this->m_registered = true;

        gCatena.registerObject(this);

        this->m_UplinkTimer.begin(this->m_txCycleSec * 1000);
        this->m_pirSampleTimer.begin(this->m_pirSampleSec * 1000);
        }

    // start and initialize the PIR sensor
    this->m_pir.begin(gCatena);

    Wire.begin();
    if (this->m_BME280.begin(BME280_ADDRESS, Adafruit_BME280::OPERATING_MODE::Sleep))
        {
        this->m_fBme280 = true;
        }
    else
        {
        this->m_fBme280 = false;
        gCatena.SafePrintf("No BME280 found: check wiring\n");
        }

    if (this->m_si1133.begin())
        {
        this->m_fSi1133 = true;
        this->m_si1133.configure(0, CATENA_SI1133_MODE_SmallIR);
        this->m_si1133.configure(1, CATENA_SI1133_MODE_White);
        this->m_si1133.configure(2, CATENA_SI1133_MODE_UV);
        this->m_si1133.start();
        }
    else
        {
        this->m_fSi1133 = false;
        gCatena.SafePrintf("No Si1133 found: check hardware\n");
        }

    // start (or restart) the FSM.
    if (! this->m_running)
        {
        this->m_exit = false;
        this->m_fsm.init(*this, &cMeasurementLoop::fsmDispatch);
        }
    }

void cMeasurementLoop::end()
    {
    if (this->m_running)
        {
        this->m_exit = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::requestActive(bool fEnable)
    {
    if (fEnable)
        this->m_rqActive = true;
    else
        this->m_rqInactive = true;

    this->m_fsm.eval();
    }

cMeasurementLoop::State
cMeasurementLoop::fsmDispatch(
    cMeasurementLoop::State currentState,
    bool fEntry
    )
    {
    State newState = State::stNoChange;

    if (fEntry && this->isTraceEnabled(this->DebugFlags::kTrace))
        {
        gCatena.SafePrintf("cMeasurementLoop::fsmDispatch: enter %s\n",
                this->getStateName(currentState)
                );
        }

    switch (currentState)
        {
    case State::stInitial:
        newState = State::stInactive;
        break;

    case State::stInactive:
        if (fEntry)
            {
            // turn off anything that should be off while idling.
            }
        if (this->m_rqActive)
            {
            // when going active manually, start the measurement
            // cycle immediately.
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = true;
            this->m_UplinkTimer.retrigger();
            newState = State::stWarmup;
            }
        break;

    case State::stSleeping:
        if (fEntry)
            {
            // reset the counters.
            this->resetPirAccumulation();

            // set the LEDs to flash accordingly.
            gLed.Set(McciCatena::LedPattern::Sleeping);
            }

        if (this->m_rqInactive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = false;
            newState = State::stInactive;
            }
        else if (this->m_UplinkTimer.isready())
            newState = State::stMeasure;
        else if (this->m_UplinkTimer.getRemaining() > 1500)
            this->sleep();
        break;

    // get some data
    case State::stWarmup:
        if (fEntry)
            {
            // reset the counters.
            this->resetPirAccumulation();
            //start the timer
            this->setTimer(5 * 1000);
            }
        if (this->timedOut())
            newState = State::stMeasure;
        break;

    case State::stMeasure:
        if (fEntry)
            {
            // start SI1133 measurement
            this->m_si1133.start(true);
            this->updateSynchronousMeasurements();
            this->setTimer(100);
            }
        
        if (this->m_si1133.isOneTimeReady())
            {
            this->updateLightMeasurements();
            newState = State::stTransmit;
            }
        else if (this->timedOut())
            {
            this->m_si1133.stop();
            newState = State::stTransmit;
            if (this->isTraceEnabled(this->DebugFlags::kError))
                gCatena.SafePrintf("S1133 timed out\n");
            }
        break;

    case State::stTransmit:
        if (fEntry)
            {
            TxBuffer_t b;
            this->fillTxBuffer(b);
            this->startTransmission(b);
            }
        if (this->txComplete())
            {
            newState = State::stSleeping;

            // calculate the new sleep interval.
            this->updateTxCycleTime();
            }
        break;

    case State::stFinal:
        break;

    default:
        break;
        }
    
    return newState;
    }

/****************************************************************************\
|
|   Take a measurement
|
\****************************************************************************/

void cMeasurementLoop::updateSynchronousMeasurements()
    {
    memset((void *) &this->m_data, 0, sizeof(this->m_data));
    this->m_data.flags = Flags(0);

    this->m_data.vBat = gCatena.ReadVbat();
    this->m_data.flags |= Flags::Vbat;

    this->m_data.vBus = gCatena.ReadVbat();
    this->m_data.flags |= Flags::Vbus;

    if (gCatena.getBootCount(this->m_data.BootCount))
        {
        this->m_data.flags |= Flags::Boot;
        }

    if (this->m_fBme280)
        {
        auto m = this->m_BME280.readTemperaturePressureHumidity();
        this->m_data.env.Temperature = m.Temperature;
        this->m_data.env.Pressure = m.Pressure;
        this->m_data.env.Humidity = m.Humidity;
        this->m_data.flags |= Flags::TPH;
        }

    // SI1133 is handled separately

    // update activity
    uint32_t const tDelta = this->m_pirLastTimeMs - this->m_pirBaseTimeMs;
    this->m_data.activity.Avg = this->m_pirSum / tDelta;
    this->m_data.activity.Max = this->m_pirMax;
    this->m_data.activity.Min = this->m_pirMin;
    this->m_data.flags |= Flags::Activity;
    }

void cMeasurementLoop::updateLightMeasurements()
    {
    uint16_t data[3];

    this->m_si1133.readMultiChannelData(data, 3);
    this->m_si1133.stop();

    this->m_data.flags |= Flags::Light;
    this->m_data.light.IR = data[0];
    this->m_data.light.White = data[1];
    this->m_data.light.UV = data[2];
    }

void cMeasurementLoop::resetPirAccumulation()
    {
    this->m_pirMax = -1.0f;
    this->m_pirMin = 1.0f;
    this->m_pirSum = 0.0f;
    this->m_pirBaseTimeMs = millis();
    this->m_pirLastTimeMs = this->m_pirBaseTimeMs;
    }

void cMeasurementLoop::accumulatePirData()
    {
    std::uint32_t thisTimeMs;
    std::uint32_t deltaT;
    float v = this->m_pir.readWithTime(thisTimeMs);

    if (v > this->m_pirMax)
        this->m_pirMax = v;
    if (v < this->m_pirMin)
        this->m_pirMin = v;

    deltaT = thisTimeMs - this->m_pirLastTimeMs;
    this->m_pirSum += v * deltaT;
    this->m_pirLastTimeMs = thisTimeMs;
    }

/****************************************************************************\
|
|   Prepare a buffer to be transmitted.
|
\****************************************************************************/

void cMeasurementLoop::fillTxBuffer(cMeasurementLoop::TxBuffer_t& b)
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Measuring);

    b.begin();

    // insert format byte
    b.put(kMessageFormat);
    b.put(std::uint8_t(this->m_data.flags));

    // send Vbat
    if ((this->m_data.flags & Flags::Vbat) != Flags(0))
        {
        float Vbat = this->m_data.vBat;
        gCatena.SafePrintf("Vbat:    %d mV\n", (int) (Vbat * 1000.0f));
        b.putV(Vbat);
        }

    // send Vdd if we can measure it.

    // vBus is sent as 5000 * v
    if ((this->m_data.flags & Flags::Vbus) != Flags(0))
        {
        float Vbus = this->m_data.vBus;
        gCatena.SafePrintf("Vbus:    %d mV\n", (int) (Vbus * 1000.0f));
        b.putV(Vbus);
        }

    // send boot count
    if ((this->m_data.flags & Flags::Boot) != Flags(0))
        {
        b.putBootCountLsb(this->m_data.BootCount);
        }

    if ((this->m_data.flags & Flags::TPH) != Flags(0))
        {
        gCatena.SafePrintf(
                "BME280:  T: %d P: %d RH: %d\n",
                (int) this->m_data.env.Temperature,
                (int) this->m_data.env.Pressure,
                (int) this->m_data.env.Humidity
                );
        b.putT(this->m_data.env.Temperature);
        b.putP(this->m_data.env.Pressure);
        // no method for 2-byte RH, directly encode it.
        b.put2uf((this->m_data.env.Humidity / 100.0f) * 65535.0f);
        }

    // put light
    if ((this->m_data.flags & Flags::Light) != Flags(0))
        {
        gCatena.SafePrintf(
                "Si1133:  %u IR, %u White, %u UV\n",
                this->m_data.light.IR,
                this->m_data.light.White,
                this->m_data.light.UV
                );

        b.putLux(this->m_data.light.IR);
        b.putLux(this->m_data.light.White);
        b.putLux(this->m_data.light.UV);
        }

    // put activity
    if ((this->m_data.flags & Flags::Activity) != Flags(0))
        {
        // scale to 0..1
        float aMin = this->m_data.activity.Min;
        float aMax = this->m_data.activity.Max;
        float aAvg = this->m_data.activity.Avg;

        gCatena.SafePrintf(
                "Activity [0..1000):  %d min %d max %d Avg\n",
                500 + int(500 * aMin), 
                500 + int(500 * aMax),
                500 + int(500 * aAvg)
                );

        b.put2uf(LMIC_f2sflt16(aAvg));
        b.put2uf(LMIC_f2sflt16(aMin));
        b.put2uf(LMIC_f2sflt16(aMax));
        }

    gLed.Set(savedLed);
    }

/****************************************************************************\
|
|   Start uplink of data
|
\****************************************************************************/

void cMeasurementLoop::startTransmission(
    cMeasurementLoop::TxBuffer_t &b
    )
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Sending);

    // by using a lambda, we can access the private contents
    auto sendBufferDoneCb =
        [](void *pClientData, bool fSuccess)
            {
            auto const pThis = (cMeasurementLoop *)pClientData;
            pThis->m_txpending = false;
            pThis->m_txcomplete = true;
            pThis->m_txerr = ! fSuccess;
            pThis->m_fsm.eval();
            };

    bool fConfirmed = false;
    if (gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fConfirmedUplink))
        {
        gCatena.SafePrintf("requesting confirmed tx\n");
        fConfirmed = true;
        }

    this->m_txpending = true;
    this->m_txcomplete = this->m_txerr = false;

    if (! gLoRaWAN.SendBuffer(b.getbase(), b.getn(), sendBufferDoneCb, (void *)this, fConfirmed, kUplinkPort))
        {
        // uplink wasn't launched.
        this->m_txcomplete = true;
        this->m_txerr = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::sendBufferDone(bool fSuccess)
    {
    this->m_txpending = false;
    this->m_txcomplete = true;
    this->m_txerr = ! fSuccess;
    this->m_fsm.eval();
    }

/****************************************************************************\
|
|   The Polling function -- 
|
\****************************************************************************/

void cMeasurementLoop::poll()
    {
    bool fEvent;

    // no need to evaluate unless something happens.
    fEvent = false;

    // if we're not active, and no request, nothing to do.
    if (! this->m_active)
        {
        if (! this->m_rqActive)
            return;

        // we're asked to go active. We'll want to eval.
        fEvent = true;
        }

    // accumulate PIR data
    if (this->m_pirSampleTimer.isready())
        {
        // timer has fired. grab data
        this->accumulatePirData();
        }

    if (this->m_fTimerActive)
        {
        if ((millis() - this->m_timer_start) >= this->m_timer_delay)
            {
            this->m_fTimerActive = false;
            this->m_fTimerEvent = true;
            fEvent = true;
            }
        }

    // check the transmit time.
    if (this->m_UplinkTimer.peekTicks() != 0)
        {
        fEvent = true;
        }

    if (fEvent)
        this->m_fsm.eval();
    }

/****************************************************************************\
|
|   Update the TxCycle count. 
|
\****************************************************************************/

void cMeasurementLoop::updateTxCycleTime()
    {
    auto txCycleCount = this->m_txCycleCount;

    // update the sleep parameters
    if (txCycleCount > 1)
            {
            // values greater than one are decremented and ultimately reset to default.
            this->m_txCycleCount = txCycleCount - 1;
            }
    else if (txCycleCount == 1)
            {
            // it's now one (otherwise we couldn't be here.)
            gCatena.SafePrintf("resetting tx cycle to default: %u\n", this->m_txCycleSec_Permanent);

            this->setTxCycleTime(this->m_txCycleSec_Permanent, 0);
            }
    else
            {
            // it's zero. Leave it alone.
            }
    }

/****************************************************************************\
|
|   Handle sleep between measurements 
|
\****************************************************************************/

void cMeasurementLoop::sleep()
    {
    const bool fDeepSleep = checkDeepSleep();

    if (! this->m_fPrintedSleeping)
            this->doSleepAlert(fDeepSleep);

    if (fDeepSleep)
            this->doDeepSleep();
    }

// for now, we simply don't allow deep sleep. In the future might want to
// use interrupts on activity to wake us up; then go back to sleep when we've
// seen nothing for a while.
bool cMeasurementLoop::checkDeepSleep()
    {
    bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    bool fDeepSleep;
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (! this->kEnableDeepSleep)
        {
        return false;
        }

    if (sleepInterval < 2)
        fDeepSleep = false;
    else if (fDeepSleepTest)
        {
        fDeepSleep = true;
        }
#ifdef USBCON
    else if (Serial.dtr())
        {
        fDeepSleep = false;
        }
#endif
    else if (gCatena.GetOperatingFlags() &
                static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDisableDeepSleep))
        {
        fDeepSleep = false;
        }
    else if ((gCatena.GetOperatingFlags() &
                static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)) != 0)
        {
        fDeepSleep = true;
        }
    else
        {
        fDeepSleep = false;
        }

    return fDeepSleep;
    }

void cMeasurementLoop::doSleepAlert(bool fDeepSleep)
    {
    this->m_fPrintedSleeping = true;

    if (fDeepSleep)
        {
        bool const fDeepSleepTest =
                gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
        const uint32_t deepSleepDelay = fDeepSleepTest ? 10 : 30;

        gCatena.SafePrintf("using deep sleep in %u secs"
#ifdef USBCON
                            " (USB will disconnect while asleep)"
#endif
                            ": ",
                            deepSleepDelay
                            );

        // sleep and print
        gLed.Set(McciCatena::LedPattern::TwoShort);

        for (auto n = deepSleepDelay; n > 0; --n)
            {
            uint32_t tNow = millis();

            while (uint32_t(millis() - tNow) < 1000)
                {
                gCatena.poll();
                yield();
                }
            gCatena.SafePrintf(".");
            }
        gCatena.SafePrintf("\nStarting deep sleep.\n");
        uint32_t tNow = millis();
        while (uint32_t(millis() - tNow) < 100)
            {
            gCatena.poll();
            yield();
            }
        }
    else
        gCatena.SafePrintf("using light sleep\n");
    }

void cMeasurementLoop::doDeepSleep()
    {
    // bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
    //                         static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval == 0)
        return;

    /* ok... now it's time for a deep sleep */
    gLed.Set(McciCatena::LedPattern::Off);
    this->deepSleepPrepare();

    /* sleep */
    gCatena.Sleep(sleepInterval);

    /* recover from sleep */
    this->deepSleepRecovery();

    /* and now... we're awake again. trigger another measurement */
    this->m_fsm.eval();
    }

void cMeasurementLoop::deepSleepPrepare(void)
    {
    Serial.end();
    Wire.end();
    SPI.end();
    if (this->m_pSPI2)
        this->m_pSPI2->end();
    }

void cMeasurementLoop::deepSleepRecovery(void)
    {
    Serial.begin();
    Wire.begin();
    SPI.begin();
    if (this->m_pSPI2)
        this->m_pSPI2->begin();
    }

/****************************************************************************\
|
|  Time-out asynchronous measurements. 
|
\****************************************************************************/

// set the timer
void cMeasurementLoop::setTimer(std::uint32_t ms)
    {
    this->m_timer_start = millis();
    this->m_timer_delay = ms;
    this->m_fTimerActive = true;
    this->m_fTimerEvent = false;
    }

void cMeasurementLoop::clearTimer()
    {
    this->m_fTimerActive = false;
    this->m_fTimerEvent = false;
    }

bool cMeasurementLoop::timedOut()
    {
    bool result = this->m_fTimerEvent;
    this->m_fTimerEvent = false;
    return result;
    }

