# MCCI-Catena-4430 library

This library provides Arduino support for the MCCI Catena&reg; 4430 Feather Wing.

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [Introduction](#introduction)
- [Key classes](#key-classes)
	- [`cPCA9570` I2C GPIO controller](#cpca9570-i2c-gpio-controller)
	- [`c4430Gpios` Catena 4430 GPIO Control](#c4430gpios-catena-4430-gpio-control)
	- [`cPIRdigital` PIR monitor class](#cpirdigital-pir-monitor-class)
	- [`cTimer` simple periodic timer class](#ctimer-simple-periodic-timer-class)
- [Integration with Catena 4610](#integration-with-catena-4610)
- [Example Sketches](#example-sketches)
- [Additional material](#additional-material)
- [Meta](#meta)
	- [License](#license)
	- [Support Open Source Hardware and Software](#support-open-source-hardware-and-software)
	- [Trademarks](#trademarks)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Introduction

The MCCI Catena 4430 Feather Wing is an accessory for Adafruit Feather-compatible CPU boards such as the Adafruit Feather M0 or the MCCI Catena 4610.

With an MCCI Catena 4610 mounted, the assembly looks like this:

![Picture of Catena 4430](assets/Catean4430-with-main-Catena.jpg)

Prior to assembly, the Catena 4430 looks like this:

![Picture of Catena 4430 without Feather](assets/AnnotatedCatena4430.-1278x864.jpg)

The Catena 4430 adds the following functions to any compatible Feather-like board.

- Passive Infrared (PIR) motion sensor. This can be mounted on the front or back or the board.

- A battery-backed real-time clock (compatible with Adafruit's [Adalogger FeatherWing](https://www.adafruit.com/product/2922)).

- An SD-card slot similar to the Adalogger, but different in a couple of ways. First the SD-card is electrically buffered going to and from the system; it can't interfere with the SPI bus when you're not using it. Second, the SD-card slot has a dedicated power supply that is off by default.

- Three additional lights (red, green, and blue).

- A GPIO screw terminal with four posts: two I/O signals, power, and ground. The power at the screw terminal can also be turned on and off under software control.

## Key classes

### `cPCA9570` I2C GPIO controller

This class models the hardware of PCA9570 I2C GPIO expander. It has no knowledge of how the PCA9570 is wired up.

### `c4430Gpios` Catena 4430 GPIO Control

This class models the GPIOs of the Catena 4430. It understands the wiring and polarities so that clients can use method like `c4330Gpios::setBlue()` to turn on the blue LED.

### `cPIRdigital` PIR monitor class

This class monitors the digital output from the PIR and accumulates an activity estimate.

### `cTimer` simple periodic timer class

This class simplifies the coding of periodic events driven from the Arduino `loop()` routine.

## Integration with Catena 4610

The Catena 4610 has the following features.

- LoRaWAN-compatible radio and software

- Bosch BME280 temperature/humidity/pressure sensor

- Silicon Labs Si1133 ambient light sensor

## Example Sketches

The [`Catena4430_Test`](examples/Catena4430_Test/Catena4430_Test.ino) example sketch tests various features of the Catena 4430.

The [`Catena4430_Sensor`](examples/Catena4430_Sensor/Catena4430_Sensor.ino) example is a completely worked remote sensor sketch with power management.

## Additional material

Check the [extra](./extra) directory for information about [decoding data from LoRaWAN messages](./extra/catena-message-port1-format-21.md), JavaScript decoding scripts for [The Things Network Console](extra/catena-message-port1-format-21-decoder-ttn.js) and [Node-RED](extra/catena-message-port1-format-21-decoder-node-red.js), and complete Node-RED [flows](extra/wakefield-nodered-flow.json) and [Grafana dashboards](extra/washington-university-catena4430-grafana.json) for saving and presenting the data using the MCCI Catena [`docker-ttn-dashboard`](https://github.com/mcci-catena/docker-ttn-dashboard).

## Meta

### License

This repository is released under the [MIT](./LICENSE) license. Commercial licenses are also available from MCCI Corporation.

### Support Open Source Hardware and Software

MCCI invests time and resources providing this open source code, please support MCCI and open-source hardware by purchasing products from MCCI, Adafruit and other open-source hardware/software vendors!

For information about MCCI's products, please visit [store.mcci.com](https://store.mcci.com/).

### Trademarks

MCCI and MCCI Catena are registered trademarks of MCCI Corporation. LoRaWAN is a registered trademark of the LoRa Alliance. LoRa is a registered trademark of Semtech Corporation. All other marks are the property of their respective owners.
