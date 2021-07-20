/*

Module: Catena4430_cMeasurementLoop.h

Function:
    cMeasurementLoop definitions.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cMeasurementLoop_h_
# define _Catena4430_cMeasurementLoop_h_

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Catena_Download.h>
#include <Catena_FSM.h>
#include <Catena_Led.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Catena_Si1133.h>
#include <Catena_Timer.h>
#include <Catena_TxBuffer.h>
#include <Catena.h>
#include <mcciadk_baselib.h>
#include <stdlib.h>
#include "Catena4430_cPelletFeeder.h"
#include "Catena4430_cPIRdigital.h"
#include <Catena_Date.h>

#include <cstdint>

extern McciCatena::Catena gCatena;
extern McciCatena::Catena::LoRaWAN gLoRaWAN;
extern McciCatena::StatusLed gLed;

namespace McciCatena4430 {

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

class cMeasurementBase
    {

    };

class cMeasurementFormat21
    {
public:
    static constexpr uint8_t kMessageFormat = 0x22;
    enum class Flags : uint8_t
        {
        Vbat = 1 << 0,      // vBat
        Vcc = 1 << 1,       // system voltage
        Vbus = 1 << 2,      // Vbus input
        Boot = 1 << 3,      // boot count
        TPH = 1 << 4,       // temperature, pressure, humidity
        Light = 1 << 5,     // light (IR, white, UV)
        Activity = 1 << 6,  // Activity (min/max/avg)
        };

    // buffer size for uplink data
    static constexpr size_t kTxBufferSize = 36;

    // the structure of a measurement
    struct Measurement
        {
        McciCatena::cDate           DateTime;
        cMeasurementFormat21::Flags  flags;
        float                       vBat;
        float                       vSystem;
        float                       vBus;
        uint32_t                    BootCount;
        struct Env
            {
            float                   Temperature;
            float                   Pressure;
            float                   Humidity;
            } env;
        struct Light
            {
            std::uint16_t           IR;
            std::uint16_t           White;
            std::uint16_t           UV;
            } light;
        struct Activity
            {
            float                   Min;
            float                   Max;
            float                   Avg;
            } activity;
        };
    };


template <unsigned a_kMaxActivityEntries>
class cMeasurementFormat22 : public cMeasurementBase
    {
public:
    static constexpr uint8_t kMessageFormat = 0x22;

    enum class Flags : uint8_t
            {
            Vbat = 1 << 0,      // vBat
            Vcc = 1 << 1,       // system voltage
            Vbus = 1 << 2,      // Vbus input
            Boot = 1 << 3,      // boot count
            TPH = 1 << 4,       // temperature, pressure, humidity
            Light = 1 << 5,     // light (IR, white, UV)
            Pellets = 1 << 6,   // Pellet count
            Activity = 1 << 7,  // Activity (min/max/avg)
            };

    static constexpr unsigned kMaxActivityEntries = a_kMaxActivityEntries;
    static constexpr unsigned kMaxPelletEntries = 2;
    static constexpr size_t kTxBufferSize = (1 + 4 + 1 + 2 + 2 + 2 + 1 + 6 + 2 + 6 + kMaxActivityEntries * 2);

    // the structure of a measurement
    struct Measurement
        {
        //----------------
        // the subtypes:
        //----------------

        // environmental measurements
        struct Env
            {
            // temperature (in degrees C)
            float                   Temperature;
            // pressure (in millibars/hPa)
            float                   Pressure;
            // humidity (in % RH)
            float                   Humidity;
            };

        // ambient light measurements
        struct Light
            {
            // "white" light, in w/m^2
            float                   White;
            };

        // count of food pellets
        struct Pellets
            {
            // the running total since boot.
            std::uint32_t           Total;
            // count of pellets since last measurement.
            std::uint8_t            Recent;
            };

        // activity: -1 to 1 (for inactive to active)
        struct Activity
            {
            float                   Avg;
            };

        //---------------------------
        // the actual members as POD
        //---------------------------

        // time of most recent activity measurement
        McciCatena::cDate           DateTime;

        // flags of entries that are valid.
        Flags                       flags;
        // count of valid activity measurements
        std::uint8_t                nActivity;

        // measured battery voltage, in volts
        float                       Vbat;
        // measured system Vdd voltage, in volts
        float                       Vsystem;
        // measured USB bus voltage, in volts.
        float                       Vbus;
        // boot count
        uint32_t                    BootCount;
        // environmental data
        Env                         env;
        // ambient light
        Light                       light;
        // food pellet tracking.
        Pellets                     pellets[kMaxPelletEntries];
        // array of potential activity measurements.
        Activity                    activity[kMaxActivityEntries];
        };
    };

class cMeasurementLoop : public McciCatena::cPollableObject
    {
public:
    // some parameters
    static constexpr std::uint8_t kUplinkPort = 2;
    static constexpr bool kEnableDeepSleep = false;
    static constexpr unsigned kMaxActivityEntries = 8;
    using MeasurementFormat = cMeasurementFormat22<kMaxActivityEntries>;
    static constexpr unsigned kMaxPelletEntries = MeasurementFormat::kMaxPelletEntries;
    using Measurement = MeasurementFormat::Measurement;
    using Flags = MeasurementFormat::Flags;
    static constexpr std::uint8_t kMessageFormat = MeasurementFormat::kMessageFormat;
    static constexpr std::uint8_t kSdCardCSpin = D5;

    void deepSleepPrepare();
    void deepSleepRecovery();

    enum OPERATING_FLAGS : uint32_t
        {
        fUnattended = 1 << 0,
        fManufacturingTest = 1 << 1,
        fConfirmedUplink = 1 << 16,
        fDisableDeepSleep = 1 << 17,
        fQuickLightSleep = 1 << 18,
        fDeepSleepTest = 1 << 19,
        fDisableLed = 1 << 30,
        };

    enum DebugFlags : std::uint32_t
        {
        kError      = 1 << 0,
        kWarning    = 1 << 1,
        kTrace      = 1 << 2,
        kInfo       = 1 << 3,
        };

    // constructor
    cMeasurementLoop(
            )
        : m_pirSampleSec(2)                 // PIR sample timer
        , m_txCycleSec_Permanent(6 * 60)    // default uplink interval
        , m_txCycleSec(60)                  // initial uplink interval
        , m_txCycleCount(10)                // initial count of fast uplinks
        , m_DebugFlags(DebugFlags(kError | kTrace))
        , m_ActivityTimerSec(60)            // the activity time sample interval
        {};

    // neither copyable nor movable
    cMeasurementLoop(const cMeasurementLoop&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&) = delete;
    cMeasurementLoop(const cMeasurementLoop&&) = delete;
    cMeasurementLoop& operator=(const cMeasurementLoop&&) = delete;

    enum class State : std::uint8_t
        {
        stNoChange = 0, // this name must be present: indicates "no change of state"
        stInitial,      // this name must be present: it's the starting state.
        stInactive,     // parked; not doing anything.
        stSleeping,     // active; sleeping between measurements
        stWarmup,       // transition from inactive to measure, get some data.
        stMeasure,      // take measurents
        stTransmit,     // transmit data
        stWriteFile,    // write file data
        stTryToUpdate,  // try to update firmware
        stTryToMigrate, // try to migrate device to TTN V3
        stAwaitCard,    // wait for a card to show up.
        stRebootForUpdate, // reboot system to complete firmware update

        stFinal,        // this name must be present, it's the terminal state.
        };

    static constexpr const char *getStateName(State s)
        {
        switch (s)
            {
        case State::stNoChange: return "stNoChange";
        case State::stInitial:  return "stInitial";
        case State::stInactive: return "stInactive";
        case State::stSleeping: return "stSleeping";
        case State::stWarmup:   return "stWarmup";
        case State::stMeasure:  return "stMeasure";
        case State::stTransmit: return "stTransmit";
        case State::stWriteFile: return "stWriteFile";
        case State::stTryToUpdate: return "stTryToUpdate";
        case State::stTryToMigrate: return "stTryToMigrate";
        case State::stAwaitCard: return "stAwaitCard";
        case State::stRebootForUpdate: return "stRebootForUpdate";
        case State::stFinal:    return "stFinal";
        default:                return "<<unknown>>";
            }
        }

    // concrete type for uplink data buffer
    using TxBuffer_t = McciCatena::AbstractTxBuffer_t<MeasurementFormat::kTxBufferSize>;

    // flag to disable LED
    bool fDisableLED;

    // initialize measurement FSM.
    void begin();
    void end();
    void setTxCycleTime(
        std::uint32_t txCycleSec,
        std::uint32_t txCycleCount
        )
        {
        this->m_txCycleSec = txCycleSec;
        this->m_txCycleCount = txCycleCount;

        this->m_UplinkTimer.setInterval(txCycleSec * 1000);
        if (this->m_UplinkTimer.peekTicks() != 0)
            this->m_fsm.eval();
        }
    std::uint32_t getTxCycleTime()
        {
        return this->m_txCycleSec;
        }
    virtual void poll() override;
    void setBme280(bool fEnable)
        {
        this->m_fBme280 = fEnable;
        }
    void setVbus(float Vbus)
        {
        // set threshold value as 4.0V as there is reverse voltage
        // in vbus(~3.5V) while powered from battery in 4610. 
        this->m_fUsbPower = (Vbus > 4.0f) ? true : false;
        }

    // request that the measurement loop be active/inactive
    void requestActive(bool fEnable);

    // return true if a given debug mask is enabled.
    bool isTraceEnabled(DebugFlags mask) const
        {
        return this->m_DebugFlags & mask;
        }

    // register an additional SPI for sleep/resume
    // can be called before begin().
    void registerSecondSpi(SPIClass *pSpi)
        {
        this->m_pSPI2 = pSpi;
        }

    /// bring up the SD card, if possible.
    bool checkSdCard();
    /// tear down the SD card.
    void sdFinish();

private:
    // sleep handling
    void sleep();
    bool checkDeepSleep();
    void doSleepAlert(bool fDeepSleep);
    void doDeepSleep();
    // void deepSleepPrepare();
    // void deepSleepRecovery();

    // read data
    void updateSynchronousMeasurements();
    void updateLightMeasurements();
    void resetMeasurements();
    void measureActivity();

    // telemetry handling.
    void fillTxBuffer(TxBuffer_t &b, Measurement const & mData);
    void startTransmission(TxBuffer_t &b);
    void sendBufferDone(bool fSuccess);
    bool txComplete()
        {
        return this->m_txcomplete;
        }
    static std::uint16_t activity2uf(float v)
        {
        return McciCatena::TxBuffer_t::f2uflt16(v);
        }
    void updateTxCycleTime();

    // SD card handling
    bool initSdCard();

    bool writeSdCard(TxBuffer_t &b, Measurement const &mData);
    bool handleSdFirmwareUpdate();
    bool handleSdFirmwareUpdateCardUp();
    bool updateFromSd(const char *sFile, McciCatena::cDownload::DownloadRq_t rq);
    void handleSdTTNv3Migrate();
    void rejoinNetwork();
    void sdPowerUp(bool fOn);
    void sdPrep();

    // pir handling
    void resetPirAccumulation(void);
    void accumulatePirData(void);

    // timeout handling

    // set the timer
    void setTimer(std::uint32_t ms);
    // clear the timer
    void clearTimer();
    // test (and clear) the timed-out flag.
    bool timedOut();

    // instance data
private:
    McciCatena::cFSM<cMeasurementLoop, State> m_fsm;
    // evaluate the control FSM.
    State fsmDispatch(State currentState, bool fEntry);

    Adafruit_BME280                 m_BME280;
    McciCatena::Catena_Si1133       m_si1133;

    // second SPI class
    SPIClass                        *m_pSPI2;

    // debug flags
    DebugFlags                      m_DebugFlags;

    // true if object is registered for polling.
    bool                            m_registered : 1;
    // true if object is running.
    bool                            m_running : 1;
    // true to request exit
    bool                            m_exit : 1;
    // true if in active uplink mode, false otehrwise.
    bool                            m_active : 1;

    // set true to request transition to active uplink mode; cleared by FSM
    bool                            m_rqActive : 1;
    // set true to request transition to inactive uplink mode; cleared by FSM
    bool                            m_rqInactive : 1;

    // set true if event timer times out
    bool                            m_fTimerEvent : 1;
    // set true while evenet timer is active.
    bool                            m_fTimerActive : 1;
    // set true if USB power is present.
    bool                            m_fUsbPower : 1;
    // set true if BME280 is present
    bool                            m_fBme280 : 1;
    // set true if SI1133 is present
    bool                            m_fSi1133: 1;

    // set true while a transmit is pending.
    bool                            m_txpending : 1;
    // set true when a transmit completes.
    bool                            m_txcomplete : 1;
    // set true when a transmit complete with an error.
    bool                            m_txerr : 1;
    // set true when we've printed how we plan to sleep
    bool                            m_fPrintedSleeping : 1;
    // set true when SPI2 is active
    bool                            m_fSpi2Active: 1;
    // set true when we've BIN file in SD card to update
    bool                            m_fFwUpdate : 1;

    // PIR sample control
    cPIRdigital                     m_pir;
    McciCatena::cTimer              m_pirSampleTimer;
    float                           m_pirMin;
    float                           m_pirMax;
    float                           m_pirSum;
    std::uint32_t                   m_pirBaseTimeMs;
    std::uint32_t                   m_pirLastTimeMs;
    std::uint32_t                   m_pirSampleSec;

    // Pellet Feeder
    cPelletFeeder                   m_PelletFeeder;

    // activity time control
    McciCatena::cTimer              m_ActivityTimer;
    std::uint32_t                   m_ActivityTimerSec;

    // uplink time control
    McciCatena::cTimer              m_UplinkTimer;
    std::uint32_t                   m_txCycleSec;
    std::uint32_t                   m_txCycleCount;
    std::uint32_t                   m_txCycleSec_Permanent;

    // simple timer for timing-out sensors.
    std::uint32_t                   m_timer_start;
    std::uint32_t                   m_timer_delay;

    // the current measurement
    Measurement                     m_data;

    // the data to write to the file
    Measurement                     m_FileData;
    TxBuffer_t                      m_FileTxBuffer;
    };

//
// operator overloads for ORing structured flags
//
static constexpr cMeasurementLoop::Flags operator| (const cMeasurementLoop::Flags lhs, const cMeasurementLoop::Flags rhs)
        {
        return cMeasurementLoop::Flags(uint8_t(lhs) | uint8_t(rhs));
        };

static constexpr cMeasurementLoop::Flags operator& (const cMeasurementLoop::Flags lhs, const cMeasurementLoop::Flags rhs)
        {
        return cMeasurementLoop::Flags(uint8_t(lhs) & uint8_t(rhs));
        };

static cMeasurementLoop::Flags operator|= (cMeasurementLoop::Flags &lhs, const cMeasurementLoop::Flags &rhs)
        {
        lhs = lhs | rhs;
        return lhs;
        };

} // namespace McciCatena4430

#endif /* _Catena4430_cMeasurementLoop_h_ */
