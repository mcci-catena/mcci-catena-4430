/*

Module: Catena4430_cPCA9570.h

Function:
    The Catena4430 library: library for the PCA9570 I2C output buffer

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cPCA9570_h_
# define _Catena4430_cPCA9570_h_

#pragma once

#include <cstdint>
#include <Wire.h>

namespace McciCatena4430 {

/****************************************************************************\
|
|   PCA9570 I2C output buffer
|
\****************************************************************************/

class cPCA9570
    {
public:
    // the default i2c address
    static constexpr std::uint8_t kI2cAddress = 0x24;

private:
    // the mask of bits in the PCA9570 output register that are active.
    static constexpr std::uint8_t kActiveBits = 0xF;

    //*******************************************
    // Constructor, etc.
    //*******************************************
public:
    cPCA9570(TwoWire *pWire) : m_i2caddr(kI2cAddress), m_wire(pWire), m_inversion(0) {};

    // neither copyable nor movable
    cPCA9570(const cPCA9570&) = delete;
    cPCA9570& operator=(const cPCA9570&) = delete;
    cPCA9570(const cPCA9570&&) = delete;
    cPCA9570& operator=(const cPCA9570&&) = delete;

    //*******************************************
    // The public methods
    //*******************************************
public:
    // initialize the PCA9570.
    bool begin();

    // stop using the PCA9570.
    void end();

    // set the output of the PCA9579 to value.
    bool set(std::uint8_t value);

    // modify the output of the PCA9579; use mask to enable writing of bits
    bool modify(std::uint8_t mask, std::uint8_t bits);

    // set the polarity of each output; 0 == normal, 1 == inverting.
    bool setPolarity(std::uint8_t mask)
        { this->m_inversion = (~mask) & kActiveBits; }

    // get the polarity of each output.
    std::uint8_t getPolarity() const
        { return this->m_inversion ^ kActiveBits; }

    // get the current (cached) value.
    std::uint8_t get() const
        { return this->m_value & kActiveBits; }

    // read the output register of the PCA9570. Takes into
    // account inversion, so result is conmensurate with 
    // cPCA9570::get().
    bool read(std::uint8_t &value) const;

    //*******************************************
    // The instance data
    //*******************************************
private:
    // the i2c bus used to access the device.
    TwoWire *m_wire;
    // the i2c address.
    std::uint8_t m_i2caddr;
    // the most recent value.
    std::uint8_t m_value;
    // the inversion mask. For each bit, 0 ==> non-inverting.
    std::uint8_t m_inversion;
    };

} // namespace McciCatena4430

#endif // _Catena4430_cPCA9570_h_
