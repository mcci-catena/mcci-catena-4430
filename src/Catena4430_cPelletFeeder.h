/*

Module: Catena4430_cPelletFeeder.h

Function:
    The Catena4430 library: library for the PIR sensor

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cPelletFeeder_h_
# define _Catena4430_cPelletFeeder_h_

#pragma once

#include <Arduino.h>
#include <CatenaBase_types.h>
#include <Catena_PollableInterface.h>
#include <cstdint>

namespace McciCatena4430 {

/****************************************************************************\
|
|   A simple library for monitoring the pellet feeder inputs.
|
\****************************************************************************/

class cPelletFeeder : public McciCatena::cPollableObject
    {
    // the default digital pins used for the Pellet Feeders
    static const unsigned kVddEnable = D11;
    static const unsigned kPelletFeeder0 = A1;
    static const unsigned kPelletFeeder1 = A2;
    static const unsigned kNumFeeders = 2;

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    // constructor
    cPelletFeeder()
        : m_vddpin(kVddEnable)
        , m_data({ {.pin = kPelletFeeder0}, {.pin = kPelletFeeder1 } })
        { }

    // neither copyable nor movable
    cPelletFeeder(const cPelletFeeder&) = delete;
    cPelletFeeder& operator=(const cPelletFeeder&) = delete;
    cPelletFeeder(const cPelletFeeder&&) = delete;
    cPelletFeeder& operator=(const cPelletFeeder&&) = delete;

    //*******************************************
    // The measurement structure
    //*******************************************
private:
    // the internal representation of a feeder.
    struct PelletFeederDataInternal
        {
        // pin
        std::uint8_t    pin;
        // total since last call to reset
        std::uint8_t    current;
        // last observation
        std::uint8_t    lastObservation;
        // spare for alignment
        std::uint8_t    spare;

        // total pellets measured since zero.
        std::uint32_t   total;
        };

public:
    // the external representation of data about a feeder.
    struct PelletFeederData
        {
        struct PelletFeederDatum
            {
            // total pellets measured since zero.
            std::uint32_t   total;
            // total since last call to reset
            std::uint8_t    current;
            }   feeder[kNumFeeders];
        };

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

    // get a sample
    void read(PelletFeederData &m) const;
    void readAndReset(PelletFeederData &m)
        {
        this->read(m);
        this->resetCurrent();
        }
    void resetCurrent();

    //*******************************************
    // The instance data
    //*******************************************
private:
    // the input pin.
    unsigned m_vddpin;

    // are we registered?
    bool m_fRegistered;

    // are we active?
    bool m_fActive;

    // the data
    PelletFeederDataInternal    m_data[kNumFeeders];
    };

} // namespace McciCatena4430

#endif // _Catena4430_cPelletFeeder_h_
