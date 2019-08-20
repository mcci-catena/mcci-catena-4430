/*

Module: Catena4430-cPIRdigital.h

Function:
    The Catena4430 library: library for the PIR sensor

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cPIRdigital_h_
# define _Catena4430_cPIRdigital_h_

#pragma once

#include <Arduino.h>
#include <CatenaBase_types.h>
#include <Catena_PollableInterface.h>
#include <cstdint>

namespace McciCatena4430 {

/****************************************************************************\
|
|   A simple PIR library -- this uses cPollableObject because it's easier.
|   However, we'll possibly want to switch to an interrupt-driven or counter-
|   driven version to save power.
|
\****************************************************************************/

class cPIRdigital : public McciCatena::cPollableObject
    {
    // the filtering time-constant, in microseconds.
    static const unsigned getTimeConstant() { return 1000000; }
    // the default digital pin used for the PIR.
    static const unsigned kPirData = A0;

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    // constructor
    cPIRdigital(int pin = kPirData) 
        : m_pin(pin)
        { }

    // neither copyable nor movable 
    cPIRdigital(const cPIRdigital&) = delete;
    cPIRdigital& operator=(const cPIRdigital&) = delete;
    cPIRdigital(const cPIRdigital&&) = delete;
    cPIRdigital& operator=(const cPIRdigital&&) = delete;

    //*******************************************
    // The public methods
    //*******************************************
public:
    // initialze
    bool begin(McciCatena::CatenaBase& rCatena);

    // stop operation
    void end();

    // poll function (updates data from PIR input)
    virtual void poll() override;

    // get a reading
    float read();

    // get a reading and a time
    float readWithTime(std::uint32_t &lastMs);

    //*******************************************
    // The instance data
    //*******************************************
private:
    // the input pin.
    unsigned m_pin;
    // last time measured (in micros)
    std::uint32_t m_tLast;
    // last time measured (in millis)
    std::uint32_t m_tLastMs;
    // the running value.
    float m_value;
    // are we registered?
    bool m_fRegistered;
    };

} // namespace McciCatena4430

#endif // _Catena4430_cPIRdigital_h_
