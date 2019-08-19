/*

Module: Catena4430-c4430Gpios.h

Function:
    The Catena4430 library: library for the PCA9570 I2C output buffer

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_c4430Gpios_h_
# define _Catena4430_c4430Gpios_h_

#pragma once

#include <Catena4430-cPCA9570.h>

namespace McciCatena4430 {

/****************************************************************************\
|
|   The 4430 GPIO cluster -- I2C GPIOs plus other LEDs
|
\****************************************************************************/

class c4430Gpios
    {
private:
    static const unsigned kRedLed = D12;
    static const unsigned kDisplayLed = D13;

public:
    static const std::uint8_t kDisplayMask  = (1 << 0);
    static const std::uint8_t kRedMask      = (1 << 1);
    static const std::uint8_t kBlueMask     = (1 << 2);
    static const std::uint8_t kGreenMask    = (1 << 3);

private:
    static const std::uint8_t kVout1Mask   = (1 << 0);
    static const std::uint8_t kVout2Mask   = (1 << 1);

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    c4430Gpios(cPCA9570 *pPCA9570) : m_gpio(pPCA9570) {};

    // neither copyable nor movable
    c4430Gpios(const c4430Gpios&) = delete;
    c4430Gpios& operator=(const c4430Gpios&) = delete;
    c4430Gpios(const c4430Gpios&&) = delete;
    c4430Gpios& operator=(const c4430Gpios&&) = delete;

    //*******************************************
    // The public methods
    //*******************************************
public:
    bool begin();
    void end();

    bool setBlue(bool fOn)  { return this->m_gpio->modify(kBlueMask, fOn ? 0xFF : 0); }
    bool setGreen(bool fOn) { return this->m_gpio->modify(kGreenMask, fOn ? 0xFF : 0); }
    bool setRed(bool fOn)   { digitalWrite(kRedLed, fOn); return true; }
    bool setDisplay(bool fOn) { digitalWrite(kDisplayLed, fOn); return true; }

    bool setVout1(bool fOn) { return this->m_gpio->modify(kVout1Mask, fOn ? 0xFF : 0); }
    bool getVout1() const   { return this->m_gpio->get() & kVout1Mask; }
    bool setVsdcard(bool fOn) { return this->m_gpio->modify(kVout2Mask, fOn ? 0xFF : 0); }
    bool getVsdcard() const { return this->m_gpio->get() & kVout2Mask; }

    bool setLeds(std::uint8_t mask, std::uint8_t v);
    std::uint8_t getLeds();

private:
    cPCA9570 *m_gpio;
    };

} // namespace McciCatena4430

#endif // _Catena4430_c4430Gpios_h_
