/*

Module: Catena4430_cClockDriver.h

Function:
    The Catena4430 library: base class for RTC drivers

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#ifndef _Catena4430_cClockDriver_h_
# define _Catena4430_cClockDriver_h_

#pragma once

#include <cstdint>
#include <Wire.h>
#include <Catena_Date.h>

namespace McciCatena4430 {

class cClockDriver
    {
public:
    cClockDriver()  {};

    // neither copyable nor movable
    cClockDriver(const cClockDriver&) = delete;
    cClockDriver& operator=(const cClockDriver&) = delete;
    cClockDriver(const cClockDriver&&) = delete;
    cClockDriver& operator=(const cClockDriver&&) = delete;

    // must have all of the following
    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual bool isInitialized() = 0;
    virtual bool get(McciCatena::cDate &d, unsigned *pError = nullptr) = 0;
    virtual bool set(const McciCatena::cDate &d, unsigned *pError = nullptr) = 0;
    };

} // namespace McciCatena4430

#endif // !defined(_Catena4430_cClockDriver_h_)
