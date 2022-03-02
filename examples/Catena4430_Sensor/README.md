# Catena4430_Sensor

This "example" sketch is actually a fully implemented sensor that uses a 4610 + 4430 to make a mouse/rat activity tracker with LoRaWAN&reg; backhaul and local SD card data storage. You can learn more about this project at [https://mouserat.org](mouserat.org).

This sketch also includes the feature of getting the network time and configuring the RTC.

Data is acquired continuously driven by the polling loop (in Catena4430_cMeasurementLoop.cpp). Various timers cause the data to be sampled. The main timer fires nominally every six minutes, and causes a sample to be taken of the data for the last 6 minutes. The data is then uplinked via LoRaWAN (if provisioned), and written to the SD card (if time is set).

## Changing SD Cards While Operating

No problem, but wait for the red light to be out for 5 seconds.

## File format

### Typical File Content

```log
Time,DevEUI,Raw,Vbat,Vbus,BootCount,T,RH,P,Light,P[0].delta,P[0].total,P[1].delta,P[1].total,Act[7],Act[6],Act[5],Act[4],Act[3],Act[2],Act[1],Act[0],NetWorkTime
2019-11-19T08:41:03Z,"0002cc010000046b","224afe6d1fbf421d523a1c1af15ea72d7b0062000000000000",4.13,,5.14,28,26.94,17.77,96923.00,98,0,0,0,0,,,,,,,,True
2019-11-19T08:51:26Z,"0002cc010000046b","224afe6f8eff3dd83a7b1c194d5ea432270066000000000000cdd4e61dfc53f47f6ff36d15",3.87,,3.65,28,25.30,19.59,96910.00,102,0,0,0,0,,,0.16,0.25,-0.28,-0.54,-0.10,-0.01,True
```
