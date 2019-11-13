/*

Module: Catena4430_cPCA9570.cpp

Function:
    The Catena4430 library: implementation for the PCA9570 I2C output buffer

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#include "../Catena4430_cClockDriver_PCF8523.h"
#include <Arduino.h>

using namespace McciCatena4430;
using namespace McciCatena;

static constexpr std::uint8_t bcd2bin(std::uint8_t val)
    {
    return val - /* excess 6 in encoding for each decade */ 6 * (val >> 4);
    }
static constexpr uint8_t bin2bcd(uint8_t val)
    {
    // divide by ten, and then multiply by 6 to add excess encoding to make bcd.
    return val + 6 * ((val * 0x19A) >> 12);
    }


bool cClockDriver_PCF8523::begin()
    {
    this->m_wire->begin();
    }

void cClockDriver_PCF8523::end()
    {
    }

bool cClockDriver_PCF8523::checkInitialized(kRegControl_1 rControl_1, kRegControl_3 rControl_3)
    {
    if (getField<bool, kRegControl_1>(rControl_1, kRegControl_1::STOP))
        {
        // clock is stopped!
        return false;
        }
    if (getField<bool, kRegControl_1>(rControl_1, kRegControl_1::k12_24))
        {
        // clock is in 12-hour mode, not initialized by us!
        return false;
        }

    kRegControl_3_PM pmBits = getField<kRegControl_3_PM, kRegControl_3>(rControl_3, kRegControl_3::PM);

    if (pmBits != kRegControl_3_PM::NoSwitchNoLowBattDetect)
        return true;
    else
        return false;
    }

bool cClockDriver_PCF8523::isInitialized()
    {
    bool fStatus;

    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write(std::uint8_t(kReg::Control_1));
    fStatus = this->m_wire->endTransmission() == 0;

    if (! fStatus)
        return false;

    if (this->m_wire->requestFrom(this->m_i2caddr, std::uint8_t(3)) != 3)
        return false;

    // get the byte and check the status.
    kRegControl_1 rControl_1 = kRegControl_1(this->m_wire->read());
    (void) this->m_wire->read();
    kRegControl_3 rControl_3 = kRegControl_3(this->m_wire->read());
    return checkInitialized(rControl_1, rControl_3);
    }

bool cClockDriver_PCF8523::get(McciCatena::cDate &d, unsigned *pError)
    {
    unsigned nonceError;
    if (pError == nullptr)
        pError = &nonceError;

    auto seterror = [pError](unsigned ecode) ->bool { *pError = ecode; return false; };

    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write(std::uint8_t(kReg::Control_1));
    bool fStatus = this->m_wire->endTransmission() == 0;

    if (! fStatus)
        return seterror(1);

    RegisterImage<kReg, kReg::Control_1, kReg::Years> regs;

    if (this->m_wire->requestFrom(this->m_i2caddr, regs.length()) != regs.length())
        return seterror(2);

    for (unsigned i = 0; i < regs.length(); ++i)
        regs.putraw(i, this->m_wire->read());

    if (! checkInitialized(
            regs.get<kRegControl_1>(kReg::Control_1),
            regs.get<kRegControl_3>(kReg::Control_3)
            ))
        return seterror(3);

    // if the oscillator is stopped, say we're not happy
    if (getField<bool, kRegSeconds>(regs.get<kRegSeconds>(kReg::Seconds), kRegSeconds::OS))
        return seterror(4);

    cDate::Year_t y = 2000 + bcd2bin(regs.get(kReg::Years));
    cDate::Month_t mon = bcd2bin(getField<std::uint8_t, kRegMonths>(regs.get<kRegMonths>(kReg::Months), kRegMonths::MONTHS));
    cDate::Day_t day = bcd2bin(getField<std::uint8_t, kRegDays>(regs.get<kRegDays>(kReg::Days), kRegDays::DAYS));
    cDate::Hour_t h = bcd2bin(getField<std::uint8_t, kRegHours>(regs.get<kRegHours>(kReg::Hours), kRegHours::HOURS24));
    cDate::Minute_t min = bcd2bin(getField<std::uint8_t, kRegMinutes>(regs.get<kRegMinutes>(kReg::Minutes), kRegMinutes::MINUTES));
    cDate::Second_t s = bcd2bin(getField<std::uint8_t, kRegSeconds>(regs.get<kRegSeconds>(kReg::Seconds), kRegSeconds::SECONDS));

    if (cDate::isValidYearMonthDay(y, mon, day) && cDate::isValidHourMinuteSecond(h, min, s))
        {
        d.setDate(y, mon, day);
        d.setTime(h, min, s);
        return true;
        }
    else
        {
        return seterror(5);
        }
    }

bool cClockDriver_PCF8523::set(const McciCatena::cDate &d, unsigned *pError)
    {
    unsigned nonceError;
    if (pError == nullptr)
        pError = &nonceError;

    auto seterror = [pError](unsigned ecode) ->bool { *pError = ecode; return false; };

    if (! d.isValid())
        return seterror(1);

    if (! (2000 <= d.year() && d.year() <= 2099))
        return seterror(2);

    // make sure 24-hour mode is selected
    RegisterImage<kReg, kReg::Control_1, kReg::Control_3> ctrlregs;
    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write(std::uint8_t(kReg::Control_1));
    bool fStatus = this->m_wire->endTransmission() == 0;
    if (! fStatus)
        return seterror(3);

    if (this->m_wire->requestFrom(this->m_i2caddr, ctrlregs.length()) != ctrlregs.length())
        return seterror(4);

    for (unsigned i = 0; i < ctrlregs.length(); ++i)
        ctrlregs.putraw(i, this->m_wire->read());

    kRegControl_1 rControl_1 = ctrlregs.get<kRegControl_1>(kReg::Control_1);
    if (getField<bool, kRegControl_1>(rControl_1, kRegControl_1::k12_24))
        {
        // reset the bit and write it back
        ctrlregs.put<kRegControl_1>(kReg::Control_1, setField<kRegControl_1, unsigned>(rControl_1, kRegControl_1::k12_24, 0u));
        this->m_wire->beginTransmission(this->m_i2caddr);
        this->m_wire->write(std::uint8_t(kReg::Control_1));
        this->m_wire->write(ctrlregs.get(kReg::Control_1));
        fStatus = this->m_wire->endTransmission() == 0;

        if (! fStatus)
            return seterror(5);
        }

    // now, write the values.
    RegisterImage<kReg, kReg::Seconds, kReg::Years> regs;
    regs.put(kReg::Years, bin2bcd(d.year() - 2000));
    regs.put<kRegMonths>(kReg::Months, setField<kRegMonths, std::uint8_t>(kRegMonths(0), kRegMonths::MONTHS, bin2bcd(d.month())));
    regs.put<kRegDays>(kReg::Days, setField<kRegDays, std::uint8_t>(kRegDays(0), kRegDays::DAYS, bin2bcd(d.day())));
    regs.put<kRegHours>(kReg::Hours, setField<kRegHours, std::uint8_t>(kRegHours(0), kRegHours::HOURS24, bin2bcd(d.hour())));
    regs.put<kRegMinutes>(kReg::Minutes, setField<kRegMinutes, std::uint8_t>(kRegMinutes(0), kRegMinutes::MINUTES, bin2bcd(d.minute())));
    regs.put<kRegSeconds>(kReg::Seconds, setField<kRegSeconds, std::uint8_t>(kRegSeconds(0), kRegSeconds::SECONDS, bin2bcd(d.second())));

    this->m_wire->beginTransmission(this->m_i2caddr);
    this->m_wire->write(std::uint8_t(kReg::Seconds));
    for (unsigned i = 0; i < regs.length(); ++i)
        this->m_wire->write(regs.getraw(i));
    fStatus = this->m_wire->endTransmission() == 0;

    if (! fStatus)
        return seterror(6);

    // finally, make sure the battery mode is set.
    if (getField<kRegControl_3_PM, kRegControl_3>(ctrlregs.get<kRegControl_3>(kReg::Control_3), kRegControl_3::PM) ==
        kRegControl_3_PM::NoSwitchNoLowBattDetect)
        {
        this->m_wire->beginTransmission(this->m_i2caddr);
        this->m_wire->write(std::uint8_t(kReg::Control_3));
        this->m_wire->write(0);
        fStatus = this->m_wire->endTransmission() == 0;
        if (! fStatus)
            return seterror(7);
        }

    return true;
    }
