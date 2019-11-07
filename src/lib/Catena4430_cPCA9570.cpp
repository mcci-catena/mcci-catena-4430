/*

Module: Catena4430_cPCA9570.cpp

Function:
    The Catena4430 library: implementation for the PCA9570 I2C output buffer

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430_cPCA9570.h"

using namespace McciCatena4430;

bool cPCA9570::begin()
    {
    this->m_wire->begin();

    return this->set(0);
    }

void cPCA9570::end()
    {
    (void) this->set(0);
    }

bool cPCA9570::modify(std::uint8_t mask, std::uint8_t bits)
    {
    return this->set((this->get() & ~mask) | (bits & mask));
    }

bool cPCA9570::set(std::uint8_t value)
    {
    int error;

    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write((value & kActiveBits) ^ this->m_inversion);
    error = this->m_wire->endTransmission();

    if (error != 0)
        return false;
    else
        {
        this->m_value = value & kActiveBits;
        return true;
        }
    }

bool cPCA9570::read(std::uint8_t &value) const
    {
    unsigned nRead;

    nRead = this->m_wire->requestFrom(this->m_i2caddr, uint8_t(1));

    if (nRead == 0)
        return false;
    else
        {
        value = (this->m_wire->read() ^ this->m_inversion) & kActiveBits;
        return true;
        }
    }
