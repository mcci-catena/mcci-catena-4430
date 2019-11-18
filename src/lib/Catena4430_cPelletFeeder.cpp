/*

Module: Catena4430_cPelletFeeder.cpp

Function:
    The Catena4430 library: implementation for the Catena Pellet Feeder sensor

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430_cPelletFeeder.h"

#include <Arduino.h>
#include <CatenaBase.h>

using namespace McciCatena4430;
using namespace McciCatena;

bool cPelletFeeder::begin(McciCatena::CatenaBase& rCatena)
    {
    // if already up, do nothing.
    if (this->m_fActive)
        return true;

    // Power up the connector, so we have pull-up current.
    // due to an unexpected anomaly in the STM32 BSP, we must
    // set the level *after* making it an output.
    pinMode(this->m_vddpin, OUTPUT);
    digitalWrite(this->m_vddpin, HIGH);

    // Enable the inputs.
    for (auto & feeder : this->m_data)
        {
        pinMode(feeder.pin, INPUT);
        }

    // set up the data
    for (auto & feeder : this->m_data)
        {
        feeder.current = 0;
        feeder.total = 0;
        feeder.lastObservation = digitalRead(feeder.pin);
        }

    // state that we're active
    this->m_fActive = true;

    // set up for polling.
    if (! this->m_fRegistered)
        {
        this->m_fRegistered = true;
        rCatena.registerObject(this);
        }

    return true;
    }

void cPelletFeeder::end()
    {
    this->m_fActive = false;
    }

void cPelletFeeder::poll() /* override */
    {
    // if not active, do nothing.
    if (! this->m_fActive)
        return;

    for (auto & feeder : this->m_data)
        {
        auto const last = feeder.lastObservation;
        feeder.lastObservation = digitalRead(feeder.pin);
        if (last != feeder.lastObservation &&
            /* count falling edges */ last)
            {
            ++feeder.total;
            auto const current = feeder.current;
            if (current < cNumericLimits<decltype(current)>::numeric_limits_max())
                {
                feeder.current = current + 1;
                }
            }
        }
    }

void cPelletFeeder::read(cPelletFeeder::PelletFeederData &m) const
    {
    for (unsigned i = 0; i < kNumFeeders; ++i)
        {
        m.feeder[i].total = this->m_data[i].total;
        m.feeder[i].current = this->m_data[i].current;
        }
    }

void cPelletFeeder::resetCurrent()
    {
    for (auto & feeder : this->m_data)
        {
        feeder.current = 0;
        }
    }
