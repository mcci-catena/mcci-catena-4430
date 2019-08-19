/*

Module: Catena4430-cTimer.cpp

Function:
    The Catena4430 library: implementation for the simple timer library

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430-cTimer.h"

#include <Arduino.h>
#include <Catena.h>

extern McciCatena::Catena gCatena;

using namespace McciCatena4430;

bool cTimer::begin(std::uint32_t nMillis)
    {
    this->m_interval = nMillis;
    this->m_time = millis();
    this->m_events = 0;

    // set up for polling.
    gCatena.registerObject(this);

    return true;
    }

void cTimer::poll() /* override */
    {
    auto const tNow = millis();

    if (tNow - this->m_time >= this->m_interval)
        {
        this->m_time += this->m_interval;
        ++this->m_events;

        /* if this->m_time is now in the future, we're done */
        if (std::int32_t(tNow - this->m_time) < std::int32_t(this->m_interval))
            return;

        // rarely, we need to do arithmetic. time and events are in sync.
        // arrange for m_time to be greater than tNow, and adjust m_events
        // accordingly. 
        std::uint32_t const tDiff = tNow - this->m_time;
        std::uint32_t const nTicks = tDiff / this->m_interval;
        this->m_events += nTicks;
        this->m_time += nTicks * this->m_interval;
        this->m_overrun += nTicks;
        }
    }

bool cTimer::isready()
    {
    return this->readTicks() != 0;
    }

std::uint32_t cTimer::readTicks()
    {
    auto const result = this->m_events;
    this->m_events = 0;
    return result; 
    }

std::uint32_t cTimer::peekTicks() const
    {
    return this->m_events;
    }

