/*

Module: Catena4430.h

Function:
    The Catena4430 library

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019

*/

#ifndef _Catena4430_h_
# define _Catena4430_h_

#pragma once

#include "Catena4430-version.h"
#include <Catena_FSM.h>
#include <Catena_PollableInterface.h>

namespace McciCatena4430 {

/****************************************************************************\
|
|   PMS7003 sensor
|
\****************************************************************************/

class Catena4430 : public McciCatena::cPollableObject
    {
    //*******************************************
    // Forward references, etc.
    //*******************************************
private:

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    Catena4430(cSerial &port, Catena4430Hal &hal)
        : m_port    (&port)
        , m_hal     (&hal)
        {};

    // neither copyable nor movable
    Catena4430(const Catena4430&) = delete;
    Catena4430& operator=(const Catena4430&) = delete;
    Catena4430(const Catena4430&&) = delete;
    Catena4430& operator=(const Catena4430&&) = delete;

    //*******************************************
    // The public methods
    //*******************************************
public:
    // start the wing services.
    bool begin();

    // stop the sensor.
    void end();

    virtual void poll(void) override;
    void suspend();
    void resume();

    //*******************************************
    // Internal utilities
    //*******************************************
private:

    //*******************************************
    // The instance data
    //*******************************************
private:
    };  // class Catena4430

} // namespace McciCatena4430

#endif // defined _Catena4430_h_
