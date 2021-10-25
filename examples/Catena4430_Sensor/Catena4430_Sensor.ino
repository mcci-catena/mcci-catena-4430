/*

Module: Catena4430_Sensor.ino

Function:
    Remote sensor example for the Catena 4430.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#include <Arduino.h>
#include <Wire.h>
#include <Catena.h>
#include "Catena4430_Sensor.h"
#include <arduino_lmic.h>
#include <Catena_Timer.h>
#include <Catena4430.h>
#include <Catena4430_cPCA9570.h>
#include <Catena4430_c4430Gpios.h>
#include <Catena4430_cPIRdigital.h>
#include "Catena4430_cMeasurementLoop.h"
#include "Catena4430_cmd.h"
#include <Catena4430_cClockDriver_PCF8523.h>

extern McciCatena::Catena gCatena;
using namespace McciCatena4430;
using namespace McciCatena;

static_assert(
    CATENA_ARDUINO_PLATFORM_VERSION_COMPARE_GE(
        CATENA_ARDUINO_PLATFORM_VERSION, 
        CATENA_ARDUINO_PLATFORM_VERSION_CALC(0, 21, 0, 5)
        ),
    "This sketch requires Catena-Arduino-Platform v0.21.0-5 or later"
    );

constexpr std::uint32_t kAppVersion = makeVersion(0,5,1,1);
constexpr std::uint32_t kDoubleResetWaitMs = 500;
constexpr std::uint32_t kSetDoubleResetMagic = 0xCA44301;
constexpr std::uint32_t kClearDoubleResetMagic = 0xCA44300;

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

// the global I2C GPIO object
cPCA9570                i2cgpio    { &Wire };

// the global clock object
cClockDriver_PCF8523    gClock      { &Wire };

c4430Gpios gpio     { &i2cgpio };
Catena gCatena;
cTimer ledTimer;
Catena::LoRaWAN gLoRaWAN;
StatusLed gLed (Catena::PIN_STATUS_LED);

cMeasurementLoop gMeasurementLoop;

/* instantiate the bootloader API */
cBootloaderApi gBootloaderApi;

/* instantiate SPI */
SPIClass gSPI2(
		Catena::PIN_SPI2_MOSI,
		Catena::PIN_SPI2_MISO,
		Catena::PIN_SPI2_SCK
		);

/* instantiate the flash */
Catena_Mx25v8035f gFlash;

/* instantiate the downloader */
cDownload gDownload;

unsigned ledCount;
bool fAnalogPin1;
bool fAnalogPin2;
bool fCheckPinA1;
bool fCheckPinA2;
bool fToggle;

/****************************************************************************\
|
|   User commands
|
\****************************************************************************/

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
        {
        { "date", cmdDate },
        { "dir", cmdDir },
        { "log", cmdLog },
        { "tree", cmdDir },
        // other commands go here....
        };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyExtraCommands_top(
        sMyExtraCommmands,          /* this is the pointer to the table */
        sizeof(sMyExtraCommmands),  /* this is the size of the table */
        nullptr                     /* this is no "first word" for all the commands in this table */
        );


/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    setup_double_reset();

    setup_platform();
    setup_printSignOn();

    setup_flash();
    setup_download();
    setup_measurement();
    setup_gpio();
    setup_rtc();
    setup_radio();
    setup_commands();
    setup_start();
    }

void setup_double_reset()
    {
    const uint32_t resetReason = READ_REG(RCC->CSR);
    if (resetReason & RCC_CSR_PINRSTF)
        {
        if (RTC->BKP0R == kSetDoubleResetMagic)
            {
            fToggle = true;
            RTC->BKP0R = kClearDoubleResetMagic;
            }
        else
            {
            RTC->BKP0R = kSetDoubleResetMagic;
            delay(kDoubleResetWaitMs);
            RTC->BKP0R = kClearDoubleResetMagic;
            }
        }
    }

void setup_platform()
    {
    const uint32_t setDisableLedFlag = 0x40000000;
    const uint32_t clearDisableLedFlag = 0x0FFFFFFF;
    uint32_t savedFlag;

    gCatena.begin();
    savedFlag = gCatena.GetOperatingFlags();

    // if running unattended, don't wait for USB connect.
    if (! (gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)))
        {
        while (!Serial)
            /* wait for USB attach */
            yield();
        }

    // check for pin reset and second reset
    if (fToggle)
        {
        // check if LEDs are disabled
        if (savedFlag & static_cast<uint32_t>(gMeasurementLoop.OPERATING_FLAGS::fDisableLed))
            {
            // clear flag to disable LEDs
            savedFlag &= clearDisableLedFlag;
            gCatena.getFram()->saveField(
                cFramStorage::StandardKeys::kOperatingFlags,
                savedFlag
                );
            }

        // if LEDs are not disabled
        else
            {
            // set flag to disable LEDs
            savedFlag |= setDisableLedFlag;
            gCatena.getFram()->saveField(
                cFramStorage::StandardKeys::kOperatingFlags,
                savedFlag
                );
            }

        // update operatingFlag in the library
        gCatena.SetOperatingFlags(savedFlag);
        }
    }

static constexpr const char *filebasename(const char *s)
    {
    const char *pName = s;

    for (auto p = s; *p != '\0'; ++p)
        {
        if (*p == '/' || *p == '\\')
            pName = p + 1;
        }
    return pName;
    }

void setup_printSignOn()
    {
    static const char dashes[] = "------------------------------------";

    gCatena.SafePrintf("\n%s%s\n", dashes, dashes);

    gCatena.SafePrintf("This is %s v%d.%d.%d-%d.\n",
        filebasename(__FILE__),
        getMajor(kAppVersion), getMinor(kAppVersion), getPatch(kAppVersion), getLocal(kAppVersion)
        );

    do
        {
        char sRegion[16];
        gCatena.SafePrintf("Target network: %s / %s\n",
                        gLoRaWAN.GetNetworkName(),
                        gLoRaWAN.GetRegionString(sRegion, sizeof(sRegion))
                        );
        } while (0);

    gCatena.SafePrintf("System clock rate is %u.%03u MHz\n",
        ((unsigned)gCatena.GetSystemClockRate() / (1000*1000)),
        ((unsigned)gCatena.GetSystemClockRate() / 1000 % 1000)
        );
    gCatena.SafePrintf("Enter 'help' for a list of commands.\n");

    gCatena.SafePrintf("%s%s\n" "\n", dashes, dashes);
    }

void setup_gpio()
    {
    if (! gpio.begin())
        Serial.println("GPIO failed to initialize");

    ledTimer.begin(400);

    // set up the LED
    gLed.begin();
    gCatena.registerObject(&gLed);
    gLed.Set(LedPattern::FastFlash);

    if ((gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gMeasurementLoop.OPERATING_FLAGS::fDisableLed)))
        {
        gMeasurementLoop.fDisableLED = true;
        gLed.Set(McciCatena::LedPattern::Off);
        }
    }

void setup_rtc()
    {
    if (! gClock.begin())
        gCatena.SafePrintf("RTC failed to intialize\n");

    cDate d;
    if (! gClock.get(d))
        gCatena.SafePrintf("RTC is not running\n");
    else
        {
        gCatena.SafePrintf("RTC is running. Date: %d-%02d-%02d %02d:%02d:%02dZ\n",
            d.year(), d.month(), d.day(),
            d.hour(), d.minute(), d.second()
            );
        }
    }

void setup_flash(void)
    {
    gSPI2.begin();
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gMeasurementLoop.registerSecondSpi(&gSPI2);
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

void setup_download()
    {
    gDownload.begin(gFlash, gBootloaderApi);
    }

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    LMIC_setClockError(5 * MAX_CLOCK_ERROR / 100);
    }

void setup_measurement()
    {
    gMeasurementLoop.begin();
    }

void setup_commands()
    {
    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyExtraCommands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );
    }

void setup_start()
    {
    gMeasurementLoop.requestActive(true);
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();
    
    if (gMeasurementLoop.fDisableLED)
        {
        gpio.setGreen(false);
        gpio.setRed(false);

        // set flags of Pin A1 and A2 to false.
        // this used to check A1/A2 when disabling the flag fDisableLed
        fAnalogPin1 = false;
        fAnalogPin2 = false;
        fCheckPinA1 = false;
        fCheckPinA2 = false;
        }
    else
        {
        // copy current PIR state to the blue LED.
        gpio.setBlue(digitalRead(A0));

        if (!fCheckPinA1)
            {
            // check the connection pin A1
            if (digitalRead(A1) == 0)
                {
                fAnalogPin1 = true;
                fCheckPinA1 = true;
                }
            }

        if (fAnalogPin1)
            {
            // copy current state of Pin A1 to the Green LED.
            gpio.setGreen(digitalRead(A1));
            }
        else
            gpio.setGreen(false);

        if (!fCheckPinA2)
            {
            // check the connection pin A2
            if (digitalRead(A2) == 0)
                {
                fAnalogPin2 = true;
                fCheckPinA2 = true;
                }
            }

        if (fAnalogPin2)
            {
            // copy current state of Pin A2 to the Green LED.
            gpio.setRed(digitalRead(A2));
            }
        else
            gpio.setRed(false);
        }
    }
