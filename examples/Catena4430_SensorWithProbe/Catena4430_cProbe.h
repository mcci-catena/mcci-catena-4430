/*

Module: Catena4430_cProbe.h

Function:
    cProbe definitions.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   August 2019

*/

#ifndef _Catena4430_cProbe_h_
# define _Catena4430_cProbe_h_

#pragma once

#include <DallasTemperature.h>

class cProbe
    {
public:
    static constexpr std::uint32_t kPowerOnMillis = 100;
    static constexpr std::uint32_t kConversionMillis = 750;

    cProbe() {}

    // neither copyable nor movable
    cProbe(const cProbe&) = delete;
    cProbe& operator=(const cProbe&) = delete;
    cProbe(const cProbe&&) = delete;
    cProbe& operator=(const cProbe&&) = delete;

private:
    // the one-wire object
    DallasTemperature               m_ds;
    uint32_t                        m_startTime;
    uint32_t                        m_powerTime;
    bool                            m_addressValid;
    bool                            m_powerRequested;
    bool                            m_powerIsOn;
    DeviceAddress                   m_probeAddress;
    std::uint8_t                    m_probeScratchPad[SCRATCHPAD_CRC + 1];

public:
    // initialize the temperature probe.
    void begin(OneWire &oneWire)
        {
        this->m_ds.setOneWire(&oneWire);
        this->m_powerIsOn = this->m_powerRequested = false;
        this->m_addressValid = false;
        }

    // record that power has been turned on -- do it before calling.
    void powerUp()
        {
        if (! this->m_powerIsOn && ! this->m_powerRequested)
            {
            this->m_powerTime = millis();
            this->m_powerRequested = true;
            }
        }

    bool pollPower()
        {
        if (! this->m_powerIsOn && millis() - this->m_powerTime < kPowerOnMillis)
            return false;
        else
            {
            this->m_powerIsOn = true;
            return true;
            }
        }

    void powerDown()
        {
        this->m_powerIsOn = this->m_powerRequested = false;
        }

    bool startMeasurement()
        {
        if (! this->pollPower())
            {
            this->m_addressValid = false;
            return false;
            }

        // search for connected device, if needed.
        if (! this->m_addressValid)
            {
            this->m_ds.begin();
            if (this->m_ds.getDeviceCount() != 0)
                {
                this->m_addressValid = this->m_ds.getAddress(this->m_probeAddress, 0);
                if (this->m_addressValid && this->m_probeAddress[0] != DS18B20MODEL)
                    this->m_addressValid = false;
                }
            }
        // now, if we have a valid address for an 18B20, set things up
        if (this->m_addressValid)
            {
            // set asynch mode.
            this->m_ds.setWaitForConversion(false);
            // always set resoltion to 12
            if (! this->m_ds.setResolution(this->m_probeAddress, 12, true))
                // device not responding.
                this->m_addressValid = false;
            // start a conversion
            else if (! this->m_ds.requestTemperaturesByAddress(this->m_probeAddress))
                // device not responding.
                this->m_addressValid = false;
            else
                // record conversion start time.
                this->m_startTime = millis();
            }
        // at this point, if we have a valid address, we started a conversion.
        return this->m_addressValid;
        }

    bool pollMeasurement() const
        {
        // if no valid address, no conversion, say "no need to wait"
        if (! this->m_addressValid)
            return true;
        // valid address -> conversion in progress, check whether enough time has elapsed.
        else if (millis() - this->m_startTime > kConversionMillis)
            return true;
        // valid address, but not enough time, need to wait.
        else
            return false;
        }

    // collect result from device, and convert to celsius. false means
    // device wasn't found; in that case v is forced to zero.
    bool finishMeasurement(float &v)
        {
        if (this->m_addressValid)
            {
            auto const tRaw = this->m_ds.getTemp(this->m_probeAddress);

            if (tRaw == DEVICE_DISCONNECTED_RAW)
                this->m_addressValid = false;
            else
                {
                // v = tRaw / 128.0f;
                float const tRawf = tRaw;
                v = ldexp(tRawf, -7);
                }
            }

        if (! this->m_addressValid)
            {
            v = 0.0f;
            }

        return this->m_addressValid;
        }
    };

#endif /* Catena4430_cProbe */
