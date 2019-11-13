/*

Module: Catena4430_cClockDriver_PCF8523.h

Function:
    The Catena4430 library: concrete class for PCF8523 clock.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   November 2019

*/

#ifndef _Catena4430_cClockDriver_PCF8523_h_
# define _Catena4430_cClockDriver_PCF8523_h_

#pragma once

#include "Catena4430_cClockDriver.h"
#include <Wire.h>
#include <type_traits>

namespace McciCatena4430 {

class cBitFields
    {
public:
    template <typename Tout, typename Tin>
    static Tout constexpr getField(Tin v, Tin mask)
            {
            using Tu = typename std::make_unsigned<Tin>::type;
            const Tu uMask = (Tu)(mask);
            return static_cast<Tout>((static_cast<Tu>(v) & uMask) / (uMask & (~uMask + 1u)));
            }

    template <typename T>
    static T constexpr getField(T v, T mask) { return getField<T, T>(v, mask); }

    template <typename Tout, typename Tfield>
    static Tout constexpr setField(
            Tout oldv, Tout mask, Tfield fv
            )
            {
            using Tu = typename std::make_unsigned<Tout>::type;
            using Tf = typename std::make_unsigned<Tfield>::type;
            const Tu uMask = (Tu)(mask);
            return (Tout)(((Tu)(oldv) & ~uMask) +
                          ((Tf)(fv) * (uMask & (~uMask + 1u))));
            }

    template <typename T>
    static T constexpr setField(
            T oldv, T fv, T mask
            )
            {
            return setField<T, T>(oldv, fv, mask);
            }

    // get the maximum value for a field
    template <typename Tout, typename Tin>
    static Tout constexpr getMaxValue(Tin mask)
            {
            return getField<Tout, Tin>(mask, mask);
            }

    template <typename T>
    static T constexpr getMaxValue(T mask)
            {
            return getMaxValue<T, T>(mask);
            }

    template <typename T, T firstReg, T lastReg>
    class RegisterImage
        {
    private:
        static constexpr unsigned nRegs = unsigned(lastReg) - unsigned(firstReg) + 1;
        static_assert(nRegs < McciCatena::cNumericLimits<std::uint8_t>::numeric_limits_max(),
            "number of registers must fit in uint8_t");

    public:
        RegisterImage(){};

        std::uint8_t length() const { return nRegs; }
        std::uint8_t buffer() const { return &m_buf[0]; }
        void putraw(unsigned index, std::uint8_t b)
            {
            if (index < nRegs)
                this->m_buf[index] = b;
            }
        std::uint8_t getraw(unsigned index)
            {
            if (index < nRegs)
                return this->m_buf[index];
            else
                return 0;
            }

        template <typename Tresult = std::uint8_t>
        Tresult get(T regname)
            {
            if (firstReg <= regname && regname <= lastReg)
                {
                return (Tresult)(this->m_buf[unsigned(regname) - unsigned(firstReg)]);
                }
            else
                return (Tresult) 0;
            }

        template <typename Tvalue = std::uint8_t>
        void put(T regname, Tvalue v)
            {
            if (firstReg <= regname && regname <= lastReg)
                this->m_buf[unsigned(regname) - unsigned(firstReg)] = (std::uint8_t)v;
            }

    private:
        std::uint8_t m_buf[nRegs];
        };
    };

class cClockDriver_PCF8523 : public cClockDriver, cBitFields
    {
public:
    static constexpr std::uint8_t kI2cAddress = 0x68;
    // register indices, using datasheet names.
    enum class kReg : std::uint8_t {
        Control_1 = 0,
        Control_2,
        Control_3,
        Seconds,
        Minutes,
        Hours,
        Days,
        Weekdays,
        Months,
        Years,
        Minute_alarm,
        Hour_alarm,
        Day_alarm,
        Weekday_alarm,
        Offset,
        Tmr_CLKOUT_ctrl,
        Tmr_A_freq_ctrl,
        Tmr_A_reg,
        Tmr_B_freq_ctrl,
        Tmr_B_reg,
        };

    enum class kRegControl_1 : std::uint8_t {
        CAP_SEL = 1 << 7,
        T = 1 << 6,
        STOP = 1 << 5,
        SR = 1 << 4,
        k12_24 = 1 << 3,
        SIE = 1 << 2,
        AIE = 1 << 1,
        CIE = 1 << 0
        };

    enum class kRegControl_2 : std::uint8_t
        {
        WTAF = 1 << 7,
        CTAF = 1 << 6,
        CTBF = 1 << 5,
        SF = 1 << 4,
        AF = 1 << 3,
        WTAIE = 1 << 2,
        CTAIE = 1 << 1,
        CTBIE = 1 << 0,
        };

    enum class kRegControl_3 : std::uint8_t
        {
        PM = 7 << 5,
        RSV4 = 1 << 4,
        BSF = 1 << 3,
        BLF = 1 << 2,
        BSIE = 1 << 1,
        BLIE = 1 << 0,
        };

    enum class kRegControl_3_PM : std::uint8_t
        {
        StandardLowBattDetect = 0,
        DirectLowBattDetect = 1,
        NoSwitchLowBattDetect = 2,
        Rsv3 = 3,
        StandardNoLowBattDetect = 4,
        DirectNoLowBattDetect = 5,
        Rsv6 = 6,
        NoSwitchNoLowBattDetect = 7,
        };

    enum class kRegSeconds : std::uint8_t
        {
        OS = 1 << 7,
        SECONDS = 0x7F,
        };

    enum class kRegMinutes : std::uint8_t
        {
        RSV7 = 1 << 7,
        MINUTES = 0x7F,
        };

    enum class kRegHours : std::uint8_t
        {
        RSV6 = 3 << 6,
        AMPM = 1 << 5,
        HOURS12 = 0x1F,
        HOURS24 = 0x3F,
        };

    enum class kRegDays : std::uint8_t
        {
        RSV6 = 3 << 6,
        DAYS = 0x3F,
        };

    enum class kRegWeekdays : std::uint8_t
        {
        RSV3 = 0xF8,
        WEEKDAYS = 0x7,
        };

    enum class kRegMonths : std::uint8_t
        {
        RSV5 = 0xE0,
        MONTHS = 0x1F,
        };

    // TODO(tmm@mcci.com) add remaining bits (that we don't use)

public:
    cClockDriver_PCF8523(TwoWire *pWire) : m_i2caddr(kI2cAddress), m_wire(pWire) {};
    // neither copyable nor movable
    cClockDriver_PCF8523(const cClockDriver_PCF8523&) = delete;
    cClockDriver_PCF8523& operator=(const cClockDriver_PCF8523&) = delete;
    cClockDriver_PCF8523(const cClockDriver_PCF8523&&) = delete;
    cClockDriver_PCF8523& operator=(const cClockDriver_PCF8523&&) = delete;

    // must have all of the following
    virtual bool begin() override;
    virtual void end() override;
    virtual bool isInitialized() override;
    virtual bool get(McciCatena::cDate &d, unsigned *pError = nullptr) override;
    virtual bool set(const McciCatena::cDate &d, unsigned *pError = nullptr) override;

protected:
    static bool checkInitialized(kRegControl_1 rControl1, kRegControl_3 rControl3);
    bool readBuffer(std::uint8_t *pBuffer, unsigned nBytes);
    bool writeBuffer(std::uint8_t const *pBuffer, unsigned nBytes);

private:
    TwoWire *m_wire;
    std::uint8_t m_i2caddr;
    };

} // namespace McciCatena4430

#endif // !defined(_Catena4430_cClockDriver_PCF8523_h_)
