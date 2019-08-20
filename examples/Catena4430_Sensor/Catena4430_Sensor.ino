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
#include <RTClib.h>
#include <SPI.h>
#include <arduino_lmic.h>
#include <Catena_Timer.h>
#include <Catena4430-cPCA9570.h>
#include <Catena4430-c4430Gpios.h>
#include <Catena4430-cPIRdigital.h>
#include "Catena4430_cMeasurementLoop.h"

extern McciCatena::Catena gCatena;
using namespace McciCatena4430;
using namespace McciCatena;

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/


cPCA9570 i2cgpio    { &Wire };
c4430Gpios gpio     { &i2cgpio };
Catena gCatena;
cTimer ledTimer;
Catena::LoRaWAN gLoRaWAN;
StatusLed gLed (Catena::PIN_STATUS_LED);

RTC_PCF8523 rtc;

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

cMeasurementLoop gMeasurementLoop;

//   The flash
Catena_Mx25v8035f gFlash;

unsigned ledCount;

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    Wire.begin();
    Serial.begin(115200);
    while (! Serial)
        delay(1);
    Serial.println("\n" "Catena4430_Sensor");

    gCatena.begin();
    setup_flash();

    if (! gpio.begin())
        Serial.println("GPIO failed to initialize");

    if (! rtc.begin())
        Serial.println("RTC failed to intiailize");

    DateTime now = rtc.now();
    gCatena.SafePrintf("RTC is %s. Date: %d-%02d-%02d %02d:%02d:%02d\n",
        rtc.initialized() ? "running" : "not initialized",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second()
        );

    /* set up the radio */
    setup_radio();

    /* set up the measurement loop */
    setup_measurement();

    ledCount = 0;
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

void setup_measurement()
    {
    gMeasurementLoop.begin();
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();

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
