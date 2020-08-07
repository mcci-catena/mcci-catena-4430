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

/*

Name:   McciCatena4430::cMeasurementLoop::fillTxBuffer()

Function:
    Prepare a messages in a TxBuffer with data from current measurements.

Definition:
    void McciCatena4430::cMeasurementLoop::fillTxBuffer(
            cMeasurementLoop::TxBuffer_t& b
            );

Description:
    A format 0x22 message is prepared from the data in the cMeasurementLoop
    object.

*/

void
cMeasurementLoop::fillTxBuffer(
    cMeasurementLoop::TxBuffer_t& b, Measurement const &mData
    )
    {
    auto const savedLed = gLed.Set(McciCatena::LedPattern::Off);
    if (!(this->fDisableLED))
        {
        gLed.Set(McciCatena::LedPattern::Measuring);
        }

    // initialize the message buffer to an empty state
    b.begin();

    // insert format byte
    b.put(kMessageFormat);

    // insert the timestamp from the data
    // stuff zero if time is not valid.
    b.put4u(std::uint32_t(mData.DateTime.getGpsTime()));

    // the flags in Measurement correspond to the over-the-air flags.
    b.put(std::uint8_t(mData.flags));

    // send Vbat
    if ((mData.flags & Flags::Vbat) != Flags(0))
        {
        float Vbat = mData.Vbat;
        gCatena.SafePrintf("Vbat:    %d mV\n", (int) (Vbat * 1000.0f));
        b.putV(Vbat);
        }

    // send Vdd if we can measure it.

    // Vbus is sent as 5000 * v
    if ((mData.flags & Flags::Vbus) != Flags(0))
        {
        float Vbus = mData.Vbus;
        gCatena.SafePrintf("Vbus:    %d mV\n", (int) (Vbus * 1000.0f));
        b.putV(Vbus);
        }

    // send boot count
    if ((mData.flags & Flags::Boot) != Flags(0))
        {
        b.putBootCountLsb(mData.BootCount);
        }

    if ((mData.flags & Flags::TPH) != Flags(0))
        {
        gCatena.SafePrintf(
                "BME280:  T: %d P: %d RH: %d\n",
                (int) mData.env.Temperature,
                (int) mData.env.Pressure,
                (int) mData.env.Humidity
                );
        b.putT(mData.env.Temperature);
        b.putP(mData.env.Pressure);
        // no method for 2-byte RH, directly encode it.
        b.put2uf((mData.env.Humidity / 100.0f) * 65535.0f);
        }

    // put light
    if ((mData.flags & Flags::Light) != Flags(0))
        {
        gCatena.SafePrintf(
                "Si1133:  %u White\n",
                mData.light.White
                );

        b.putLux(mData.light.White);
        }

    // put pellets
    if ((mData.flags & Flags::Pellets) != Flags(0))
        {
        for (unsigned i = 0; i < MeasurementFormat::kMaxPelletEntries; ++i)
            {
            b.put2(mData.pellets[i].Total & 0xFFFFu);
            b.put(mData.pellets[i].Recent);
            }
        }

    // put activity
    if ((mData.flags & Flags::Activity) != Flags(0))
        {
        for (unsigned i = 0; i < mData.nActivity; ++i)
            {
            // scale to 0..1
            float aAvg = mData.activity[i].Avg;

            gCatena.SafePrintf(
                "Activity[%u] [0..1000):  %d Avg\n",
                i,
                500 + int(500 * aAvg)
                );

            b.put2uf(LMIC_f2sflt16(aAvg));
            }
        }

    if (!(this->fDisableLED))
        gLed.Set(savedLed);
    }
