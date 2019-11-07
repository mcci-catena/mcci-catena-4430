/*

Module: Catena4430_c4430Gpios.cpp

Function:
    The Catena4430 library: implementation for the Catena GPIOs

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430_c4430Gpios.h"

using namespace McciCatena4430;

bool c4430Gpios::begin()
    {
    bool fResult;

    this->m_gpio->setPolarity(this->m_gpio->getPolarity() & ~(kBlueMask | kGreenMask));

    fResult = this->m_gpio->begin();

    digitalWrite(kRedLed, 0);
    pinMode(kRedLed, OUTPUT);

    digitalWrite(kDisplayLed, 0);
    pinMode(kDisplayLed, OUTPUT);

    return fResult;
    }

void c4430Gpios::end()
    {
    this->m_gpio->end();
    }

bool c4430Gpios::setLeds(std::uint8_t mask, std::uint8_t v)
    {
    bool fResult = true;

    fResult &= this->m_gpio->modify(mask & (kBlueMask | kGreenMask), v);
    if (mask & kRedMask)
        fResult &= this->setRed(v & kRedMask);
    if (mask & kDisplayMask)
        fResult &= this->setDisplay(v & kDisplayMask);

    return fResult;
    }

std::uint8_t c4430Gpios::getLeds()
    {
    std::uint8_t v;

    v = this->m_gpio->get() & (kBlueMask | kGreenMask);

    if (digitalRead(kRedLed))
        v |= kRedMask;
    if (digitalRead(kDisplayLed))
        v |= kDisplayMask;

    return v;
    }
