/*

Name:   Catena4430_Sensor.h

Function:
    Global linkage for Catena4430_Sensor.ino

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#ifndef _Catena4430_Sensor_h_
# define _Catena4430_Sensor_h_

#pragma once

#include <Catena.h>
#include <Catena_BootloaderApi.h>
#include <Catena_Download.h>
#include <Catena_Led.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_Date.h>
#include <Catena_Timer.h>
#include <Catena4430_c4430Gpios.h>
#include <Catena4430_cPCA9570.h>
#include <Catena4430_cClockDriver_PCF8523.h>
#include <SD.h>
#include <SPI.h>
#include "Catena4430_cMeasurementLoop.h"

// the global clock object
extern  McciCatena4430::cClockDriver_PCF8523    gClock;

extern  McciCatena4430::c4430Gpios              gpio;
extern  McciCatena::Catena                      gCatena;
extern McciCatena::cDate                        gDate;
extern  McciCatena::cTimer                      ledTimer;
extern  McciCatena::Catena::LoRaWAN             gLoRaWAN;
extern  McciCatena::StatusLed                   gLed;

extern  SPIClass                                gSPI2;
extern  McciCatena4430::cMeasurementLoop        gMeasurementLoop;

//   The flash
extern  McciCatena::Catena_Mx25v8035f           gFlash;

// the bootloader
extern  McciCatena::cBootloaderApi              gBootloaderApi;

// the downloaer
extern  McciCatena::cDownload                   gDownload;

// the SD card
extern  SDClass                                 gSD;

#endif // !defined(_Catena4430_Sensor_h_)
