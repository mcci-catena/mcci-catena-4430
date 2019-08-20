# Understanding MCCI Catena data sent on port 1 format 0x21

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [Overall Message Format](#overall-message-format)
- [Bitmap fields and associated fields](#bitmap-fields-and-associated-fields)
	- [Battery Voltage (field 0)](#battery-voltage-field-0)
	- [System Voltage (field 1)](#system-voltage-field-1)
	- [Bus Voltage (field 2)](#bus-voltage-field-2)
	- [Boot counter (field 3)](#boot-counter-field-3)
	- [Environmental Readings (field 4)](#environmental-readings-field-4)
	- [Particle Concentrations (field 5)](#particle-concentrations-field-5)
- [Data Formats](#data-formats)
	- [uint16](#uint16)
	- [int16](#int16)
	- [uint8](#uint8)
	- [uflt16](#uflt16)
	- [sflt16](#sflt16)
- [Test Vectors](#test-vectors)
	- [Test vector generator](#test-vector-generator)
- [The Things Network Console decoding script](#the-things-network-console-decoding-script)
- [Node-RED Decoding Script](#node-red-decoding-script)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Overall Message Format

Port 1 format 0x21 uplink messages are sent by Catena4430_Sensor and related sketches. We use the discriminator byte in the same way as many of the sketches in the Catena-Sketches collection.

Each message has the following layout.

byte | description
:---:|:---
0    | magic number 0x21
1    | bitmap encoding the fields that follow
2..n | data bytes; use bitmap to map these bytes onto fields.

Each bit in byte 1 represents whether a corresponding field in bytes 2..n is present. If all bits are clear, then no data bytes are present. If bit 0 is set, then field 0 is present; if bit 1 is set, then field 1 is present, and so forth. If a field is omitted, all bytes for that field are omitted.

## Bitmap fields and associated fields

The bitmap byte has the following interpretation. `int16`, `uint16`, etc. are defined after the table.

Bitmap bit | Length of corresponding field (bytes) | Data format |Description
:---:|:---:|:---:|:----
0 | 2 | [int16](#int16) | [Battery voltage](#battery-voltage-field-0)
1 | 2 | [int16](#int16) | [System voltage](#sys-voltage-field-1)
2 | 2 | [int16](#int16) | [Bus voltage](#bus-voltage-field-2)
3 | 1 | [uint8](#uint8) | [Boot counter](#boot-counter-field-3)
4 | 6 | [int16](#int16), [uint16](#uint16), [uint16](#uint16) | [Temperature, Pressure, Humidity](environmental-readings-field-4)
5 | 6 | [uint16](#uint16), [uint16](#uint16), [uint16](#uint16) | [Ambient Light](#ambient-light-field-5)
6 | 6 | [sflt16](#sflt16), [sflt16](#sflt16), [sflt16](#sflt16) | [Activity indication](#activity-indication-field-6)
7 | n/a | _reserved_ | Reserved for future use.

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

### Particle Concentrations (field 5)

Field 5, if present, has nine particle concentrations as 18 bytes of data, each as a [`uflt16`](#uflt16).  `uflt16` values respresent values in [0, 1).

The fields in order are:

- PM1.0, PM2.5 and PM10 concentrations. Multiply by 65536 to get concentrations in &mu;g per cubic meter.
- Dust concentrations for particles of size 0.3, 0.5, 1.0, 2.5, 5.0 and 10 microns. Multiply by 65536 to get particle counts per 0.1L of air.

## Data Formats

All multi-byte data is transmitted with the most significant byte first (big-endian format).  Comments on the individual formats follow.

### uint16

an integer from 0 to 65536.

### int16

a signed integer from -32,768 to 32,767, in two's complement form. (Thus 0..0x7FFF represent 0 to 32,767; 0x8000 to 0xFFFF represent -32,768 to -1).

### uint8

an integer from 0 to 255.

### uflt16

A unsigned floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

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

### sflt16

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

## Test Vectors

The following input data can be used to test decoders.

`21 01 18 00`

```json
{
  "vBat": 1.5
}
```

`21 02 F8 00`

```json
{
  "vSys": -0.5
}
```

`21 04 7f ff`

```json
{
  "vBus": 7.999755859375
}
```

`21 08 2a`

```json
{
  "boot": 42
}
```

`21 10 14 00 5f 8f 99 99`

```json
{
  "p": 978.52,
  "rh": 60,
  "tDewC": 11.999894615745436,
  "tempC": 20
}
```

`21 10 1e 00 63 54 99 99`

```json
{
  "p": 1017.12,
  "rh": 60,
  "tDewC": 21.390006900020513,
  "tHeatIndexF": 91.09765809999998,
  "tempC": 30
}
```

`21 20 00 64 00 c8 01 2c`

```json
{
  "irradiance": {
    "IR": 100,
    "UV": 300,
    "White": 200
  }
}
```

`21 40 7c 3d ff ff 7f ff`

```json
{
  "activity": {
    "Avg": 0.52978515625,
    "Max": 0.99951171875,
    "Min": -0.99951171875
  }
}
```

`21 7f 20 00 34 cd 4e 66 2a 1e 00 63 54 99 99 00 64 00 c8 01 2c 67 f0 ff ff 7f 2f`

```json
{
  "activity": {
    "Avg": 0.1240234375,
    "Max": 0.89794921875,
    "Min": -0.99951171875
  },
  "boot": 42,
  "irradiance": {
    "IR": 100,
    "UV": 300,
    "White": 200
  },
  "p": 1017.12,
  "rh": 60,
  "tDewC": 21.390006900020513,
  "tHeatIndexC": 32.83203227777776,
  "tempC": 30,
  "vBat": 2,
  "vBus": 4.89990234375,
  "vSys": 3.300048828125
}
```

### Test vector generator

This repository contains a simple C++ file for generating test vectors.

Build it from the command line. Using Visual C++:

```console
C> cl /EHsc catena-message-port1-format-21-test.cpp
Microsoft (R) C/C++ Optimizing Compiler Version 19.16.27031.1 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

catena-message-port1-format-21-test.cpp
Microsoft (R) Incremental Linker Version 14.16.27031.1
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:catena-message-port1-format-21-test.exe
catena-message-port1-format-21-test.obj
```

Using GCC or Clang on Linux:

```bash
make catena-message-port1-format-21-test
```

(The default make rules should work.)

For usage, read the source or the check the input vector generation file `catena-message-port1-format-21.vec`.

To run it against the test vectors, try:

```console
$ catena-message-port1-format-21-test < catena-message-port1-format-21.vec
Input a line with name/values pairs
Vbat 1.5 .
21 01 18 00
Vsys -0.5 .
21 02 f8 00
Vbus 10 .
21 04 7f ff
Boot 2a .
21 08 2a
Env 20 978.5 60 .
21 10 14 00 5f 8f 99 99
Env 30 1017.1 60 .
21 10 1e 00 63 54 99 99
Light 100 200 300 .
21 20 00 64 00 c8 01 2c
Activity 0.53 -1 1 .
21 40 7c 3d ff ff 7f ff
Vbat 2 Vsys 3.3 Vbus 4.9 Boot 2a Env 30 1017.1 60 Light 100 200 300 Activity 0.124 -1 0.898 .
21 7f 20 00 34 cd 4e 66 2a 1e 00 63 54 99 99 00 64 00 c8 01 2c 67 f0 ff ff 7f 2f
```

## The Things Network Console decoding script

The repository contains a generic script that decodes messages in this format, for [The Things Network console](https://console.thethingsnetwork.org).

You can get the latest version on GitHub:

- [in raw form](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/master/extra/catena-message-port1-21-decoder-ttn.js), or
- [view it](https://github.com/mcci-catena/MCCI-Catena-4430/blob/master/extra/catena-message-port1-21-decoder-ttn.js).

## Node-RED Decoding Script

A Node-RED script to decode this data is part of this repository. You can download the latest version from GitHub:

- [in raw form](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/master/extra/catena-message-port1-21-decoder-node-red.js), or
- [view it](https://raw.githubusercontent.com/mcci-catena/MCCI-Catena-4430/blob/master/extra/catena-message-port1-21-decoder-node-red.js).
