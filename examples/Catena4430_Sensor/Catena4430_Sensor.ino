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
#include <Catena_Mx25v8035f.h>
#include <SPI.h>
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
    CATENA_ARDUINO_PLATFORM_VERSION >= CATENA_ARDUINO_PLATFORM_VERSION_CALC(0, 17, 0, 40),
    "This sketch requires Catena-Arduino-Platform v0.17.0.40 or later"
    );

constexpr std::uint32_t kAppVersion = makeVersion(0,3,4,0);

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

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

cMeasurementLoop gMeasurementLoop;

//   The flash
Catena_Mx25v8035f gFlash;

unsigned ledCount;

// required time check for provisioning
uint32_t provisionStartTime;
uint32_t delay_ms;
bool fProvision;
bool fInitial;

/* flag to check application mode */
bool fMode;

/****************************************************************************\
|
|   User commands
|
\****************************************************************************/

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
        {
        { "date", cmdDate },
        { "mode", cmdMode },
        { "provision", cmdProvision },
        // { "debugmask", cmdDebugMask },
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
    setup_platform();
    setup_printSignOn();

    setup_flash();
    setup_measurement();
    setup_gpio();
    setup_rtc();
    setup_radio();
    setup_mode();
    setup_commands();
    setup_start();
    }

void setup_platform()
    {
    gCatena.begin();

    // if running unattended, don't wait for USB connect.
    if (! (gCatena.GetOperatingFlags() &
            static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)))
            {
            while (!Serial)
                    /* wait for USB attach */
                    yield();
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

    gCatena.SafePrintf("This is %s v%d.%d.%d.%d.\n",
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
    gCatena.SafePrintf("(remember to select 'Line Ending: Newline' at the bottom of the monitor window.)\n");

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

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    LMIC_setClockError(5 * MAX_CLOCK_ERROR / 100);
    }
 
void setup_mode()
    {
    fMode = !(fMode);
    
    if (fMode)
        gCatena.SafePrintf("Application configured in LPTIM Mode\n");
    else
        gCatena.SafePrintf("Application configured in Normal Mode\n");
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
    
    gCatena.SafePrintf("To switch between LPTIM and Normal mode, use command 'mode'\n");
    /* wait time for command from user */
    delay(3000);
    gCatena.SafePrintf("Timeout to enter command 'mode', RESET device to switch mode\n");

    /* set flag to check for Provisioning in LPTIM mode */
    fInitial = true;
    }

void start_provisioning(uint32_t delay_s)
    {
    delay_ms = delay_s * 1000;
    provisionStartTime = millis();

    /* set flag to enable provisioning */
    fProvision = true;
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

    if (fMode && fInitial)
        {
        gCatena.SafePrintf("To start provision, use command 'provision' uses 60 second timeout\n");
        gCatena.SafePrintf("For desired timeout to provision, use command 'provision <secs>'\n");
        /* wait for 5seconds to receive command from user */
        delay(10000);
        gCatena.SafePrintf("Timeout to start provision, RESET the device to provision\n");
        }
    
    if (fProvision)
        {
        if ((millis() - provisionStartTime) < delay_ms)
            fProvision = true;        
        else
            {
            /* clear flag to end provision time */
            fProvision = false;
            gCatena.SafePrintf("Requested time for provisioning has been end, RESET to Provision\n");
            }
        }
        
    fInitial = false;    

    // copy current PIR state to the blue LED.
    gpio.setBlue(digitalRead(A0));

    // if it's time to update the LEDs, advance to the next step.
    if (ledTimer.isready())
        {
        const std::uint8_t ledMask = (gpio.kRedMask | gpio.kGreenMask);
        std::uint8_t ledValue;

        unsigned iGpio = 2 - ledCount;

        ledCount = (ledCount + 1) % 2;

        if (ledCount == 0)
            ledValue = gpio.kRedMask;
        else
            ledValue = gpio.kGreenMask;

        gpio.setLeds(ledMask, ledValue);
        }
    }
