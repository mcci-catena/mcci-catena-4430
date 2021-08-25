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

#include <Catena4430.h>
#include <arduino_lmic.h>
#include <Catena4430_Sensor.h>
#include <Catena_Si1133.h>

using namespace McciCatena4430;
using namespace McciCatena;

extern c4430Gpios gpio;
extern cMeasurementLoop gMeasurementLoop;

static constexpr uint8_t kVddPin = D11;

void lptimSleep(uint32_t timeOut);
uint32_t HAL_AddTick(uint32_t delta);

uint32_t timeOut = 200;

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
        this->m_ActivityTimer.begin(this->m_ActivityTimerSec * 1000);
        }

    // start and initialize the PIR sensor
    this->m_pir.begin(gCatena);

    // start and initialize pellet feeder monitoring.
    this->m_PelletFeeder.begin(gCatena);

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
        this->m_fLowLight = true;

        auto const measConfig =	Catena_Si1133::ChannelConfiguration_t()
            .setAdcMux(Catena_Si1133::InputLed_t::LargeWhite)
            .setSwGainCode(7)
            .setHwGainCode(4)
            .setPostShift(1)
            .set24bit(true);

        this->m_si1133.configure(0, measConfig, 0);
        }
    else
        {
        this->m_fSi1133 = false;
        gCatena.SafePrintf("No Si1133 found: check hardware\n");
        }

    // start (or restart) the FSM.
    if (! this->m_running)
        {
        this->m_fFwUpdate = false;
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
        this->resetMeasurements();
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

            if (!(this->fDisableLED && this->m_fLowLight)))
                {
                // set the LEDs to flash accordingly.
                gLed.Set(McciCatena::LedPattern::Sleeping);
                }
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

    // get some data. This is only called while booting up.
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

    // fill in the measurement
    case State::stMeasure:
        if (fEntry)
            {
            // start SI1133 measurement (one-time)
            this->m_si1133.start(true);
            this->updateSynchronousMeasurements();
            this->setTimer(1000);
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
            this->fillTxBuffer(b, this->m_data);
            this->m_FileData = this->m_data;

            this->m_FileTxBuffer.begin();
            for (auto i = 0; i < b.getn(); ++i)
                this->m_FileTxBuffer.put(b.getbase()[i]);

            this->resetMeasurements();

            if (gLoRaWAN.IsProvisioned())
                this->startTransmission(b);
            }
        if (! gLoRaWAN.IsProvisioned())
            {
            newState = State::stWriteFile;
            }
        if (this->txComplete())
            {
            newState = State::stWriteFile;

            // calculate the new sleep interval.
            this->updateTxCycleTime();
            }
        break;

    // if there's an SD card, append to file
    case State::stWriteFile:
        if (fEntry)
            {
            }

        if (this->writeSdCard(this->m_FileTxBuffer, this->m_FileData))
            newState = State::stTryToUpdate;
        else if (gLoRaWAN.IsProvisioned())
            newState = State::stTryToUpdate;
        else
            newState = State::stAwaitCard;
        break;

    // try to update firmware
    case State::stTryToUpdate:
        if (this->handleSdFirmwareUpdate())
            newState = State::stRebootForUpdate;
        else
            newState = State::stTryToMigrate;
        this->m_fFwUpdate = false;
        break;

    // try to migrate to TTN V3
    case State::stTryToMigrate:
        if (fEntry)
            {
            this->handleSdTTNv3Migrate();
            }
        newState = State::stSleeping;
        break;

    // no SD card....
    case State::stAwaitCard:
        if (fEntry)
            {
            gCatena.SafePrintf("** no SD card and not provisioned!\n");
            }

        newState = State::stSleeping;
        break;

    // reboot for update
    case State::stRebootForUpdate:
        if (fEntry)
            {
            gLog.printf(gLog.kInfo, "Rebooting to apply firmware\n");
            this->setTimer(1 * 1000);
            }
        if (this->timedOut())
            {
            NVIC_SystemReset();
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

void cMeasurementLoop::resetMeasurements()
    {
    memset((void *) &this->m_data, 0, sizeof(this->m_data));
    this->m_data.flags = Flags(0);
    }

void cMeasurementLoop::updateSynchronousMeasurements()
    {
    this->m_data.Vbat = gCatena.ReadVbat();
    this->m_data.flags |= Flags::Vbat;

    this->m_data.Vbus = gCatena.ReadVbus();
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

    // update activity -- this is is already handled elsewhere

    // grab data on pellets.
    cPelletFeeder::PelletFeederData data;
    this->m_PelletFeeder.readAndReset(data);
    this->m_data.flags |= Flags::Pellets;

    // fill in the measurement.
    for (unsigned i = 0; i < kMaxPelletEntries; ++i)
        {
        this->m_data.pellets[i].Total = data.feeder[i].total;
        this->m_data.pellets[i].Recent = data.feeder[i].current;
        }

    // grab time of last activity update.
    gClock.get(this->m_data.DateTime);
    }

void cMeasurementLoop::measureActivity()
    {
    if (this->m_data.nActivity == this->kMaxActivityEntries)
        {
        // make room by deleting first entry
        for (unsigned i = 0; i < this->kMaxActivityEntries - 1; ++i)
            this->m_data.activity[i] = this->m_data.activity[i+1];

        this->m_data.nActivity = this->kMaxActivityEntries - 1;
        }

    // get another measurement.
    uint32_t const tDelta = this->m_pirLastTimeMs - this->m_pirBaseTimeMs;
    this->m_data.activity[this->m_data.nActivity++].Avg = this->m_pirSum / tDelta;
    this->m_data.flags |= Flags::Activity;

    // record time. Since a zero timevalue is always invalid, we don't
    // need to check validity.
    (void) gClock.get(this->m_data.DateTime);

    // start new measurement.
    this->m_pirBaseTimeMs = this->m_pirLastTimeMs;
    this->m_pirMax = -1.0f;
    this->m_pirMin = 1.0f;
    this->m_pirSum = 0.0f;
    }

void cMeasurementLoop::updateLightMeasurements()
    {
    uint32_t data[1];

    this->m_si1133.readMultiChannelData(data, 1);
    this->m_si1133.stop();

    this->m_data.flags |= Flags::Light;
    this->m_data.light.White = (float) data[0];

    if (data[0] <= 500)
        m_fLowLight = true;
    else
        m_fLowLight = false;
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
|   Start uplink of data
|
\****************************************************************************/

void cMeasurementLoop::startTransmission(
    cMeasurementLoop::TxBuffer_t &b
    )
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Off);
    if (!(this->fDisableLED && this->m_fLowLight))
        {
        gLed.Set(McciCatena::LedPattern::Sending);
        }

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

    // record PIR m_pirSampleSec
    if (this->m_ActivityTimer.isready())
        {
        // time to record another minute of data.
        this->measureActivity();
        if (this->m_data.nActivity == this->kMaxActivityEntries)
            fEvent = true;
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

    this->m_data.Vbus = gCatena.ReadVbus();
    setVbus(this->m_data.Vbus);

    if (!(this->m_fUsbPower) && !(this->m_fFwUpdate) && !(os_queryTimeCriticalJobs(ms2osticks(timeOut))))
        lptimSleep(timeOut);
    }

static void setup_lptim(uint32_t msec)
    {
    // enable clock to LPTIM1
    __HAL_RCC_LPTIM1_CLK_ENABLE();
    __HAL_RCC_LPTIM1_CLK_SLEEP_ENABLE();

    auto const pLptim = LPTIM1;

    // set LPTIM1 clock to LSE clock.
    __HAL_RCC_LPTIM1_CONFIG(RCC_LPTIM1CLKSOURCE_LSE);

    // disable everything so we can tweak the CFGR
    pLptim->CR = 0;

    // upcount from selected internal clock (which is LSE)
    auto rCfg = pLptim->CFGR & ~0x01FEEEDF;
    rCfg |=  0;
    pLptim->CFGR = rCfg;

    // enable the counter but don't start it
    pLptim->CR = LPTIM_CR_ENABLE;
    delayMicroseconds(100);

    // Clear ICR and ISR registers
    pLptim->ICR |= 0x3F;
    pLptim->ISR &= 0x00;

    // Auto-Reload Register is a 16-bit register
    // set ARR to value between 0 to 0xFFFF ( < 1999 ms )
    // must be done after enabling.
    uint32_t timeoutCount;
    timeoutCount = ((32768 * msec) / 1000);
    pLptim->ARR = timeoutCount;

    // Autoreload match interrupt
    pLptim->IER |= LPTIM_IER_ARRMIE;

    NVIC_SetPriority(LPTIM1_IRQn, 1);
    NVIC_DisableIRQ(LPTIM1_IRQn);

    // start in continuous mode.
    pLptim->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;

    // enable LPTIM interrupt routine
    NVIC_EnableIRQ(LPTIM1_IRQn);
    }

void lptimSleep(uint32_t timeOut)
    {
    uint32_t sleepTimeMS;
    sleepTimeMS = timeOut;

    setup_lptim(sleepTimeMS);

    gMeasurementLoop.deepSleepPrepare();

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(
          PWR_LOWPOWERREGULATOR_ON,
          PWR_STOPENTRY_WFI
          );

    HAL_IncTick();
    HAL_ResumeTick();
    HAL_AddTick(sleepTimeMS);

    gMeasurementLoop.deepSleepRecovery();
    }

uint32_t HAL_AddTick(
   uint32_t delta
    )
    {
    extern __IO uint32_t uwTick;
    // copy old interrupt-enable state to flags.
    uint32_t const flags = __get_PRIMASK();

    // disable interrupts
    __set_PRIMASK(1);

    // observe uwTick, and advance it.
    uint32_t const tickCount = uwTick + delta;

    // save uwTick
    uwTick = tickCount;

    // restore interrupts (does nothing if ints were disabled on entry)
    __set_PRIMASK(flags);

    // return the new value of uwTick.
    return tickCount;
    }

extern "C" {
void LPTIM1_IRQHandler(void)
    {
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    if(LPTIM1->ISR & LPTIM_ISR_ARRM) //If there was a compare match
        {
        /* If the interrupt was enabled */
        LPTIM1->ICR |= LPTIM_ICR_ARRMCF;
        LPTIM1->ICR |= LPTIM_ICR_CMPOKCF;
        LPTIM1->CR = 0;
        }
    }
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

        if (!(this->fDisableLED && this->m_fLowLight))
            {
            // sleep and print
            gLed.Set(McciCatena::LedPattern::TwoShort);
            }

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
    pinMode(kVddPin, INPUT);

    Serial.end();
    Wire.end();
    SPI.end();
    if (this->m_pSPI2 && this->m_fSpi2Active)
        {
        this->m_pSPI2->end();
        this->m_fSpi2Active = false;
        }
    }

void cMeasurementLoop::deepSleepRecovery(void)
    {
    pinMode(kVddPin, OUTPUT);
    digitalWrite(kVddPin, HIGH);
    
    Serial.begin();
    Wire.begin();
    SPI.begin();
    //if (this->m_pSPI2)
    //    this->m_pSPI2->begin();
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
