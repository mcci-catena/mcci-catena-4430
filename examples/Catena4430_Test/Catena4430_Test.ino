/*

Module: catean4430-test.ino

Function:
    Simple test program for the Catena 4430.

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
#include <SD.h>
#include <Catena_Timer.h>
#include <Catena4430_cPCA9570.h>
#include <Catena4430_c4430Gpios.h>
#include <Catena4430_cPIRdigital.h>

extern McciCatena::Catena gCatena;
using namespace McciCatena4430;


/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

using namespace McciCatena;

cPCA9570 i2cgpio    { &Wire };
c4430Gpios gpio     { &i2cgpio };
Catena gCatena;

cTimer pirPrintTimer;
cTimer ledTimer;
cPIRdigital pir;

RTC_PCF8523 rtc;

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

//   The flash
Catena_Mx25v8035f gFlash;
bool gfFlash;

unsigned ledCount;

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup() {
    Wire.begin();
    Serial.begin(115200);
    while (! Serial)
        delay(1);
    Serial.println("\nCatena4430_Test");

    gCatena.begin();
    setup_flash();

    if (! gpio.begin())
        Serial.println("GPIO failed to initialize");

    pirPrintTimer.begin(2000);
    ledTimer.begin(200);

    if (! pir.begin(gCatena))
        Serial.println("PIR failed to initialize");

    if (! rtc.begin())
        Serial.println("RTC failed to intiailize");

    DateTime now = rtc.now();
    gCatena.SafePrintf("RTC is %s. Date: %d-%02d-%02d %02d:%02d:%02d\n",
        rtc.initialized() ? "running" : "not initialized",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second()
        );

    // test the card
    CardInfo(D5);

    ledCount = 0;

    // set up the digital I/Os

    // first off, power to the GPIOs, so we have pull-up current.
    pinMode(D11, OUTPUT);
    digitalWrite(D11, HIGH);

    // second, enable the two inputs.
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
}

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gfFlash = true;
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gfFlash = false;
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop() {
    gCatena.poll();

    // copy current PIR state to the blue LED.
    gpio.setBlue(digitalRead(A0));

    // if it's time to update the LEDs, advance to the next step.
    if (ledTimer.isready())
        {
        const std::uint8_t ledMask = (gpio.kDisplayMask | gpio.kRedMask | gpio.kGreenMask);
        std::uint8_t ledValue;

        unsigned iGpio = 3 - ledCount;

        ledCount = (ledCount + 1) % 3;

        if (ledCount == 0)
            ledValue = gpio.kDisplayMask;
        else if (ledCount == 1)
            ledValue = gpio.kRedMask;
        else
            ledValue = gpio.kGreenMask;
    
        gpio.setLeds(ledMask, ledValue);
        }

    // if it's time to print the PIR state, print it out.
    if (pirPrintTimer.isready())
        {
        // for reaons having to do with rouding and symmetry, pir.read()
	// returns a value in [-1, 1],
        // so we first map it to 0 to 1.
        float v = (pir.read() + 1.0f) / 2.0f;
        // get the current time from the RTC
        DateTime now = rtc.now();


        // print it out.
        gCatena.SafePrintf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        Serial.print(" PIR: ");
        Serial.print(v * 100.0f);
        Serial.print("%");

        gCatena.SafePrintf("  A1: %s   A2: %s\n",
            digitalRead(A1) ? "+" : "-",
            digitalRead(A2) ? "+" : "-"
            );
        }
}


/****************************************************************************\
|
|   the cardinfo script
|
\****************************************************************************/

Sd2Card card;
SdVolume volume;
SdFile root;

// turn on power to the SD card
void sdPowerUp(bool fOn)
    {
    gpio.setVsdcard(fOn);
    }

void CardInfo(int chipSelect)
    {
    Serial.print("\nInitializing SD card...");

    digitalWrite(chipSelect, 1);
    digitalWrite(D7, 1);  // disable SX1276, wh

    pinMode(chipSelect, OUTPUT);

    sdPowerUp(true);
    delay(300);

    // we'll use the initialization code from the utility libraries
    // since we're just testing if the card is working!
    if (!card.init(gSPI2, SPI_HALF_SPEED, chipSelect))
    {
        Serial.println("initialization failed. Things to check:");
        Serial.println("* is a card inserted?");
        Serial.println("* is your wiring correct?");
        Serial.println("* did you change the chipSelect pin to match your shield or module?");
        sdPowerUp(false);
        return;
    }
    else
    {
        Serial.println("Wiring is correct and a card is present.");
    }

    // print the type of card
    Serial.println();
    Serial.print("Card type:         ");
    switch (card.type())
    {
    case SD_CARD_TYPE_SD1:
        Serial.println("SD1");
        break;
    case SD_CARD_TYPE_SD2:
        Serial.println("SD2");
        break;
    case SD_CARD_TYPE_SDHC:
        Serial.println("SDHC");
        break;
    default:
        Serial.println("Unknown");
    }

    // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
    if (!volume.init(card))
    {
        Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
        sdPowerUp(false);
        return;
    }

    Serial.print("Clusters:          ");
    Serial.println(volume.clusterCount());
    Serial.print("Blocks x Cluster:  ");
    Serial.println(volume.blocksPerCluster());

    Serial.print("Total Blocks:      ");
    Serial.println(volume.blocksPerCluster() * volume.clusterCount());
    Serial.println();

    // print the type and size of the first FAT-type volume
    uint32_t volumesize;
    Serial.print("Volume type is:    FAT");
    Serial.println(volume.fatType(), DEC);

    volumesize = volume.blocksPerCluster(); // clusters are collections of blocks
    volumesize *= volume.clusterCount();    // we'll have a lot of clusters
    volumesize /= 2;                        // SD card blocks are always 512 bytes (2 blocks are 1KB)
    Serial.print("Volume size (Kb):  ");
    Serial.println(volumesize);
    Serial.print("Volume size (Mb):  ");
    volumesize /= 1024;
    Serial.println(volumesize);
    Serial.print("Volume size (Gb):  ");
    Serial.println((float)volumesize / 1024.0);

    Serial.println("\nFiles found on the card (name, date and size in bytes): ");
    root.openRoot(volume);

    // list all files in the card with date and size
    root.ls(LS_R | LS_DATE | LS_SIZE);

    // clean things up.
    sdPowerUp(false);
    }
