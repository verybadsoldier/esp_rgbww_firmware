# ESP RGBWW Firmware
## Firmware for RGBWW controller
This repository provides an open-source firmware for ESP8266-based RGBWWCW controllers (up to 5 channels). The firmware is based on Sming (https://github.com/SmingHub/Sming).

This firmware is a fork of Patrick Jahns original firmware (https://github.com/patrickjahns/esp_rgbww_firmware). Thanks for founding it!

#### Features
 * Smooth and programmable on-board fades and animations
 * Independent animation channels
 * Suitable for different PCBs (easily configurable by config options)
 * Highly customizable configuration
 * Various network communication options: HTTP - MQTT - TCP (events only)
 * Highly accurate synchronization of multiple controllers
 * [Easy setup and configuration via a feature rich webapplication]
 * [OTA updates]
 * [Simple JSON API for configuration]
 * Security (change default AP password and Password for accessing API endpoints)
 * Hardware push button support

## Index
Most information about installation (flashing), setup and usage guides are provided via the Wiki
https://github.com/verybadsoldier/esp_rgbww_firmware/wiki

__Quicklinks__
- __Setup guides__
  - [Installation on a fresh controller](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/1.1-Flashing)
  - [Setup guide](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/1.2-Initial-Setup)
  - [Web interface](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/1.3-Web-Interface)
  - [OTA] (https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/3.-OTA)
  - Troubleshooting
  - [Limitations/Known issues](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/4.-Limitations-and-known-issues)
- __API Documentation__
  - [JSON Api](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/2.1-JSON-API-reference)
  - [TCP/UDP](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/2.2-TCP-UDP-reference)
  - [MQTT](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/2.3-MQTT-reference)

- [Contributing](#contributing)
- [Useful Links and Sources](#links)


## Firmware installation/flashing
Precompiled binaries are provided via github. It is also possible to compile the firmware images yourself. 
For more information and instructions please see [the Wiki](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/1.1-Flashing)


## Contributing

I encourage you to contribute to this project. All ideas, thoughts, issues and of course code is welcomed.  

For more information on how to contribute, please check [contribution guidelines](CONTRIBUTING.md)  
<br><br>

## Links

- [FHEM Forum](https://forum.fhem.de/index.php?topic=70738.0)
- [Sming Framework](https://github.com/SmingHub/Sming)
- [RGBWWLed Library](https://github.com/verybadsoldier/RGBWWLed)
