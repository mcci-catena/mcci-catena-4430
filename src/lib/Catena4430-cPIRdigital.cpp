/*

Module: Catena4430-cPIRdigital.cpp

Function:
    The Catena4430 library: implementation for the Catena PIR sensor library

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430-cPIRdigital.h"

#include <Arduino.h>
#include <CatenaBase.h>

using namespace McciCatena4430;

bool cPIRdigital::begin(McciCatena::CatenaBase& rCatena)
    {
    pinMode(this->m_pin, INPUT);
    this->m_value = 0.0;
    this->m_tLast = micros();

    // set up for polling.
    rCatena.registerObject(this);
    }

void cPIRdigital::poll() /* override */
    {
    // these two lines take the measurement.
    auto const v = digitalRead(this->m_pin);
    std::uint32_t tNow = micros();

    // this is for outsiders reference
    this->m_tLastMs = millis();

    // now compute the result
    float delta = v ? 1.0f : -1.0f;

    float m = float(tNow - this->m_tLast) / this->getTimeConstant(); 

    // The formaula for a classic unity-gain one-pole IIR filter is g*new + (1-g)*old,
    // and that's what this is, if g is 1-(the effective decay value).
    // Note that g is adjusted based on the variable sampling
    // rate so that the overall time constant is this->getTimeConstant().
    this->m_value = this->m_value + m * (delta  - this->m_value);

    // we clamp the output to the range [-1, 1].
    if (this->m_value > 1.0f) 
        this->m_value = 1.0f;
    else if (this->m_value < -1.0f)
        this->m_value = -1.0f;

    this->m_tLast = tNow;
    }

float cPIRdigital::read()
    {
    return this->m_value;
    }

float cPIRdigital::readWithTime(std::uint32_t& lastMs)
    {
    lastMs = this->m_tLastMs;
    return this->m_value;
    }
