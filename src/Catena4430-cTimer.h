/*

Module: Catena4430-cTimer.h

Function:
    The Catena4430 library: library for simple timer

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cTimer_h_
# define _Catena4430_cTimer_h_

#pragma once

#include <Arduino.h>
#include <CatenaBase_types.h>
#include <Catena_PollableInterface.h>
#include <cstdint>

namespace McciCatena4430 {

/****************************************************************************\
|
|   A simple timer -- this uses cPollableObject because it's easier
|
\****************************************************************************/

class cTimer : public McciCatena::cPollableObject
    {
public:
    // constructor
    cTimer() {}

    // neither copyable nor movable 
    cTimer(const cTimer&) = delete;
    cTimer& operator=(const cTimer&) = delete;
    cTimer(const cTimer&&) = delete;
    cTimer& operator=(const cTimer&&) = delete;

    // initialze to fire every nMillis
    bool begin(std::uint32_t nMillis);

    // stop operation
    void end();

    // poll function (updates data)
    virtual void poll() override;

    bool isready();
    std::uint32_t readTicks();
    std::uint32_t peekTicks() const;

    void debugDisplay() const
        {
        Serial.print("time="); Serial.print(this->m_time);
        Serial.print(" interval="); Serial.print(this->m_interval);
        Serial.print(" events="); Serial.print(this->m_events);
        Serial.print(" overrun="); Serial.println(this->m_overrun); 
        }

private:
    std::uint32_t   m_time;
    std::uint32_t   m_interval;
    std::uint32_t   m_events;
    std::uint32_t   m_overrun;
    };

} // namespace McciCatena4430

#endif // _Catena4430_cPIRdigital_h_
