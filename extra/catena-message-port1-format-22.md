# Understanding MCCI Catena data sent on port 1 format 0x22

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [Overall Message Format](#overall-message-format)
    - [Timekeeping](#timekeeping)
- [Optional fields](#optional-fields)
    - [Battery Voltage (field 0)](#battery-voltage-field-0)
    - [System Voltage (field 1)](#system-voltage-field-1)
    - [Bus Voltage (field 2)](#bus-voltage-field-2)
    - [Boot counter (field 3)](#boot-counter-field-3)
    - [Environmental Readings (field 4)](#environmental-readings-field-4)
    - [Ambient Light (field 5)](#ambient-light-field-5)
    - [Pellet consumption (field 6)](#pellet-consumption-field-6)
    - [Activity Indication (field 7)](#activity-indication-field-7)
- [Data Formats](#data-formats)
    - [`uint16`](#uint16)
    - [`int16`](#int16)
    - [`uint8`](#uint8)
    - [`uflt16`](#uflt16)
    - [`sflt16`](#sflt16)
    - [`uint32`](#uint32)
- [Test Vectors](#test-vectors)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->
<!-- due to another bug in Markdown TOC, you need to have Editor>Tab Size set to 4 (even though it will be auto-overridden); it uses that setting rather than the current setting for the file. -->

## Overall Message Format

Port 1 format 0x22 uplink messages are sent by Catena4430_Sensor and related sketches. We use the discriminator byte in the same way as many of the sketches in the Catena-Sketches collection.

Each message has the following layout. There is a fixed part, followed by a variable part. THe maximum message size is 37 bytes, which allows for operation as slow as DR1 (SF9) in the United States.

byte | description
:---:|:---
0    | magic number 0x22
1..4 | four bytes of time-stamp of measurement from RTC, expressed as seconds since the POSIX epoch (1970-01-01 00:00Z). See [timekeeping](#timekeeping) discussion below.  This is expressed as a [`uint32`](#uint32)
5    | a single byte, interpreted as a bit map indicating the fields that follow in bytes 6..*.
6..* | data bytes; use bitmap to map these bytes onto fields.

### Timekeeping

Timekeeping is a thorny topic for scientific investigations, because one day is not exactly 86,400 seconds long. Obviously, the difference between two instants, measured in seconds, is independent of calendar system, but converting the time of each instant into ISO date and time is **not** independent of the calendar. Worse is that computing systems (e.g. POSIX-based systems) focus more on easy, deterministic conversion, and so assume that there are exactly 86400 seconds/day. In UTC time, the solar calendar date is paramount; leap-seconds are inserted or deleted as needed to keep UTC mean solar noon aligned with astronomical mean solar noon.

In effect, the computer observes a sequence of seconds. We need to correlate them to calendar time, and we need to interpret know the interval between instances. Let's call the sequence of seconds _interval time_, as opposed to _calendar time_.

Let's also define an important property of sequences of seconds -- "interval-preserving" sequences are those in which, if T1 and T2 are interval second numbers, (T2 - T1) is equal to the number of ITU seconds between the times T1 and T2.

Since the RTC runs in calendar time, but we do all our work with interval time, we must consider this.

There are (at least) three ways of relating interval time to calendar time.

1. Keep interval time interval-preserving, and convert to calendar time as if days were exactly 86,400 seconds long. (GPS is such a time scale.) Differences between instants (in seconds) are in ITU seconds.
2. Keep interval time interval-preserving, but convert to calendar time accounting for leap seconds (most days are 86,400 seconds long, but some days are 86,399 seconds long, others are 86,401 seconds long). (UTC is such a time scale.) Differences between instants (in seconds) are in ITU seconds.
3. Make interval time _not_ interval-preserving by considering leap seconds. A day with 86,401 ITU seconds will have two seconds numbered 86,399; a day with 86,399 ITU seconds will not have a second numbered 86,399. Convert to date/time as if days were exactly 86,400 seconds long. The difference between two instants (in interval time) is not guaranteed to be accurate in ITU seconds.

Steve Allen's website has a number of good discussions, including:

- [Issues involved in computer time stamps and leap seconds][T1]
- [Two kinds of Time, Two kinds of Time Scales][T2]

[T1]: https://www.ucolick.org/~sla/leapsecs/picktwo.html
[T2]: https://www.ucolick.org/~sla/leapsecs/twokindsoftime.html

The onboard real-time clock counts in "calendar" time, and will be set by people (again in "calendar" time) from watches etc. that run from UTC or a derivative.  We will assume that the user can input the time in UTC and that the battery-backed RTC is recording time in UTC.

Therefore, we use a timescale that simply states that days have 86,400 seconds. In this application, we think that this will be good enough. If we ever start to use LoRaWAN ("GPS") time, we assume that the network will be able to send us the information needed to convert to calendar time as needed. This may add a little complication but it's future complication and we'll deal with all this if the need arises.

## Optional fields

Each bit in byte 5 represents whether a corresponding field in bytes 6..n is present. If all bits are clear, then no data bytes are present. If bit 0 is set, then field 0 is present; if bit 1 is set, then field 1 is present, and so forth. If a field is omitted, all bytes for that field are omitted.

The bitmap byte has the following interpretation. `int16`, `uint16`, etc. are defined after the table.

Bitmap bit | Length of corresponding field (bytes) | Data format |Description
:---:|:---:|:---:|:----
0 | 2 | [`int16`](#int16) | [Battery voltage](#battery-voltage-field-0)
1 | 2 | [`int16`](#int16) | [System voltage](#sys-voltage-field-1)
2 | 2 | [`int16`](#int16) | [Bus voltage](#bus-voltage-field-2)
3 | 1 | [`uint8`](#uint8) | [Boot counter](#boot-counter-field-3)
4 | 6 | [`int16`](#int16), [`uint16`](#uint16), [`uint16`](#uint16) | [Temperature, Pressure, Humidity](environmental-readings-field-4)
5 | 2 | [`uint16`](#uint16) | [Ambient Light](#ambient-light-field-5)
6 | 6 | ([`uint16`](#uint16), `uint8`)\[2] | [Pellet count](#pellet-count-field-6)
7 | 2*n | [`sflt16`](#sflt16)\[n] | [Activity indication](#activity-indication-field-7)

### Battery Voltage (field 0)

Field 0, if present, carries the current battery voltage. To get the voltage, extract the int16 value, and divide by 4096.0. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### System Voltage (field 1)

Field 1, if present, carries the current System voltage. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

_Note:_ this field is not transmitted by some versions of the sketches.

### Bus Voltage (field 2)

Field 2, if present, carries the current voltage from USB VBus. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### Boot counter (field 3)

Field 3, if present, is a counter of number of recorded system reboots, modulo 256.

### Environmental Readings (field 4)

Field 4, if present, has three environmental readings as four bytes of data.

- The first two bytes are a [`int16`](#int16) representing the temperature (divide by 256 to get degrees Celsius).

- The next two bytes are a [`uint16`](#uint16) representing the barometric pressure (divide by 25 to get millibars). This is the station pressure, not the sea-level pressure.

- The next two bytes are a [`uint16`](#uint16) representing the relative humidity (divide by 65535 to get percent). This field can represent humidity from 0% to 100%, in steps of roughly 0.001529%.

### Ambient Light (field 5)

This field represents the ambient "white light" expressed as W/cm^2. If the field is zero, it will not be transmitted.

### Pellet consumption (field 6)

Field six, if present, represents pellet consumption. This field has 6 bytes of data.

- Bytes 0..1 are a [`uint16`] representing the running total from the first counter (GPIO `A1`). This rolls over after 65536 pulses, and is cleared at system boot.
- Byte 2 is the number of pulses on `A1` in the last sample interval. This value saturates at 255 (i.e., if 256 pulses happen in an interval, the transmitted value is 255).
- Bytes 3..4 are a [`uint16`] representing the running total from the second counter (GPIO `A2`). This rolls over after 65536 pulses, and is cleared at system boot.
- Byte 5 is the number of pulses on `A2` in the last sample interval. This value saturates at 255 (i.e., if 256 pulses happen in an interval, the transmitted value is 255).

### Activity Indication (field 7)

Field 7 represents activity. It consists (nominally) of 6 fields, 12 bytes of data. Each field represents the activity for one minute, as a value from -1 (no activity) to 1 (continuous activity). The numbers are each transmitted as [`sflt16`](#sflt16) values.

However, decoding software should be prepared for fewer or more points in the sequence.

The last data point in the array correlates with the timestamp of the message. Preceding points were taken at one-minute intervals prior to the data.

This format allows for software in the devices to upload activity indication at times different than the six-minute interval normally used.

## Data Formats

All multi-byte data is transmitted with the most significant byte first (big-endian format).  Comments on the individual formats follow.

### `uint16`

An integer from 0 to 65,535.

### `int16`

A signed integer from -32,768 to 32,767, in two's complement form. (Thus 0..0x7FFF represent 0 to 32,767; 0x8000 to 0xFFFF represent -32,768 to -1).

### `uint8`

An integer from 0 to 255.

### `uflt16`

An unsigned floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

bits | description
:---:|:---
15..12 | binary exponent `b`
11..0 | fraction `f`

The floating point number is computed by computing `f`/4096 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated.

For example, if the transmitted message contains 0x1A, 0xAB, the equivalent floating point number is found as follows.

1. The full 16-bit number is 0x1AAB.
2. `b`  is therefore 0x1, and `b`-15 is -14.  2^-14 is 1/32768
3. `f` is 0xAAB. 0xAAB/4096 is 0.667
4. `f * 2^(b-15)` is therefore 0.6667/32768 or 0.0000204

Floating point mavens will immediately recognize:

* There is no sign bit; all numbers are positive.
* Numbers do not need to be normalized (although in practice they always are).
* The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048.. 4095, and then transmit only `f - 2048`, saving a bit. However, this complicated the handling of gradual underflow; see next point.)
* Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

### `sflt16`

A signed floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

bits | description
:---:|:---
15   | Sign (negative if set)
14..11 | binary exponent `b`
10..0 | fraction `f`

The floating point number is computed by computing `f`/2048 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated.

Floating point mavens will immediately recognize:

* This is a sign/magnitude format, not a two's complement format.
* Numbers do not need to be normalized (although in practice they always are).
* The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048..4095, and then transmit only `f - 2048`, saving a bit. However, this complicates the handling of gradual underflow; see next point.)
* Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

### `uint32`

An integer from 0 to 4,294,967,295. The first byte is the most-significant 8 bits; the last byte is the least-significant 8 bits.

## Test Vectors

The following input data can be used to test decoders.

`22 00 00 00 00 01 18 00`

```json
{
  "time": 0,
  "vBat": 1.5
}
```

`22 00 00 00 00 02 F8 00`

```json
{
  "time": 0,
  "vSys": -0.5
}```

`22 00 00 00 00 04 7f ff`

```json
{
  "time": 0,
  "vBus": 7.999755859375
}
```

`22 00 00 00 00 08 2a`

```json
{
  "boot": 42,
  "time": 0
}
```

`22 00 00 00 00 10 14 00 5f 8f 99 99`

```json
{
  "p": 978.52,
  "rh": 60,
  "tDewC": 11.999894615745436,
  "tempC": 20,
  "time": 0
}
```

`22 00 00 00 00 10 1e 00 63 54 99 99`

```json
{
  "p": 1017.12,
  "rh": 60,
  "tDewC": 21.390006900020513,
  "tHeatIndexC": 32.83203227777776,
  "tempC": 30,
  "time": 0
}
```

`22 00 00 00 00 20 00 c8`

```json
{
  "irradiance": {
    "White": 200
  },
  "time": 0
}
```

`22 00 00 00 00 80`

```json
{
  "activity": {},
  "time": 0
}
```

`22 00 00 00 00 80 74 52`

```json
{
  "activity": {
    "0": 0.27001953125
  },
  "time": 0
}
```

`22 00 00 00 00 80 7C 3D FF FF 7F FF FC 00 74 00 F4 CD`

```json
{
  "activity": [
    0.52978515625,
    -0.99951171875,
    0.99951171875,
    -0.5,
    0.25,
    -0.300048828125
  ],
  "time": 0
}
```

`22 00 00 00 00 40 00 64 03 00 19 0A`

```json
{
  "pellets": [
    {
      "Delta": 3,
      "Total": 100
    },
    {
      "Delta": 10,
      "Total": 25
    }
  ],
  "time": 0
}
```

`22 4a d5 06 db ff 20 00 34 cd 4e 66 2a 1e 00 63 54 99 99 00 c8 00 64 03 00 19 0a 7c 3d ff ff 7f ff fc 00 74 00 f4 cd`

```json
{
  "activity": [
    0.52978515625,
    -0.99951171875,
    0.99951171875,
    -0.5,
    0.25,
    -0.300048828125
  ],
  "boot": 42,
  "irradiance": {
    "White": 200
  },
  "p": 1017.12,
  "pellets": [
    {
      "Delta": 3,
      "Total": 100
    },
    {
      "Delta": 10,
      "Total": 25
    }
  ],
  "rh": 60,
  "tDewC": 21.390006900020513,
  "tHeatIndexC": 32.83203227777776,
  "tempC": 30,
  "time": 1255474907000,
  "vBat": 2,
  "vBus": 4.89990234375,
  "vSys": 3.300048828125
}
```

### Test vector generator

This repository contains a simple C++ file for generating test vectors.

Build it from the command line. Using Visual C++:

```console
C> cl /EHsc catena-message-port1-format-22-test.cpp
Microsoft (R) C/C++ Optimizing Compiler Version 19.16.27031.1 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

catena-message-port1-format-22-test.cpp
Microsoft (R) Incremental Linker Version 14.16.27031.1
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:catena-message-port1-format-22-test.exe
catena-message-port1-format-22-test.obj
```

Using GCC or Clang on Linux:

```bash
make catena-message-port1-format-22-test
```

(The default make rules should work.)

For usage, read the source or the check the input vector generation file `catena-message-port1-format-22.vec`.

To run it against the test vectors, try:

```console
C> catena-message-port1-format-22-test < catena-message-port1-format-22.vec
Time 1255474907 .
22 4a d5 06 db 00
length: 6
Vbat 1.5 .
22 00 00 00 00 01 18 00
length: 8
Vsys -0.5 .
22 00 00 00 00 02 f8 00
length: 8
Vbus 10 .
22 00 00 00 00 04 7f ff
length: 8
Boot 42 .
22 00 00 00 00 08 2a
length: 7
Env 20 978.5 60 .
22 00 00 00 00 10 14 00 5f 8f 99 99
length: 12
Env 30 1017.1 60 .
22 00 00 00 00 10 1e 00 63 54 99 99
length: 12
Light 200 .
22 00 00 00 00 20 00 c8
length: 8
Activity [ ] .
22 00 00 00 00 80
length: 6
Activity [ 0.27 ] .
22 00 00 00 00 80 74 52
length: 8
Activity [ 0.53 -1 1 -0.5 0.25 -0.3 ] .
22 00 00 00 00 80 7c 3d ff ff 7f ff fc 00 74 00 f4 cd
length: 18
Pellets 100 3 25 10 .
22 00 00 00 00 40 00 64 03 00 19 0a
length: 12
Time 1255474907 Vbat 2 Vsys 3.3 Vbus 4.9 Boot 42 Env 30 1017.1 60 Light 200 Pellets 100 3 25 10 Activity [ 0.53 -1 1 -0.5 0.25 -0.3 ] .
22 4a d5 06 db ff 20 00 34 cd 4e 66 2a 1e 00 63 54 99 99 00 c8 00 64 03 00 19 0a 7c 3d ff ff 7f ff fc 00 74 00 f4 cd
length: 39

C>
```

## The Things Network Console decoding script

The repository contains a generic script that decodes messages in this format, for [The Things Network console](https://console.thethingsnetwork.org).

You can get the latest version on GitHub:

- [in raw form](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/master/extra/catena-message-port1-22-decoder-ttn.js), or
- [view it](https://github.com/mcci-catena/MCCI-Catena-4430/blob/master/extra/catena-message-port1-22-decoder-ttn.js).

## Node-RED Decoding Script

A Node-RED script to decode this data is part of this repository. You can download the latest version from GitHub:

- [in raw form](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/master/extra/catena-message-port1-22-decoder-node-red.js), or
- [view it](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/blob/master/extra/catena-message-port1-22-decoder-node-red.js).
