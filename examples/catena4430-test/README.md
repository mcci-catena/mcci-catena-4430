# catena4430-test.ino

Simple demonstration program for the Catena 4430.

## Functions performed by this sketch

This sketch has the following features.

- During startup, the sketch queries and displays the RTC value.

- It monitors the PIR sensor and continually computes an "activity fraction", by low-pass filtering the PIR with a time constant of one second. Every two seconds, it outputs that data to the USB console. The RTC is used to time-stamp the data.  Although the data is logged only once every two seconds, a real time estimate is continually updated.

- The sketch dynamically transfers the PIR signal to the blue LED. This gives a real-time indication of the output of the PIR sensor.

- The sketch flashes the Arduino standard LED (D13), the red LED on the 4430, and the green LED on the 4430. It does this in a circular chasing pattern.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always available while the sketch is running. This also allows user commands to be added if desired.

- Furthermore, the `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Libraries required by this sketch

- MCCI's fork of the standard SD card library: [mcci-catena/SD](https://github.com/mcci-catena/SD/)
- MCCI's fork of the Adafruit RTClib library (no changes at present, just a known-working version): [mcci-catena/RTClib](https://github.com/mcci-catena/RTClib/)

Libraries to support the Catena 4610:

- [mcci-catena/Catena-Arduino-Platform](https://github.com/mcci-catena/Catena-Arduino-Platform/)
- [mcci-catena/arduino-lorawan](https://github.com/mcci-catena/arduino-lorawan/)
- [mcci-catena/Catena-mcciadk](https://github.com/mcci-catena/Catena-mcciadk/)
- [mcci-catena/arduino-lmic](https://github.com/mcci-catena/arduino-lmic/)
- [mcci-catena/Adafruit_FRAM_I2C](https://github.com/mcci-catena/Adafruit_FRAM_I2C/)
- [mcci-catena/Adafruit_BME280_Library](https://github.com/mcci-catena/Adafruit_BME280_Library/)
