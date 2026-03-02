# ESP RGBWW Firmware

This repository hosts the firmware for the [open hardware controller esp_rgbww](https://github.com/patrickjahns/esp_rgbww_controller)  
  

Link for OTA update 
http://rgbww.dronezone.de/release/version.json

Seperate files
- [Bootloader (rboot)] (http://rgbww.dronezone.de/release/rboot.bin)
- [Rom] (http://rgbww.dronezone.de/release/rom0.bin)
- [Fileystem](http://rgbww.dronezone.de/release/spiff_rom.bin)

Filesystem includes the latest version of [web interface](https://github.com/verybadsoldier/esp_rgbww_webinterface)


For more information on flashing your controller, [please refer to the wiki](https://github.com/verybadsoldier/esp_rgbww_firmware/wiki/Flashing-&-Compiling)

# Firmware Channels

There are three channels available that can be used for OTA updates directly to the controllers.

These version are online right now:

| Channel  | Version      | OTA-URL                                          |
| -------- | ------------ | ------------------------------------------------ |
| release  | 4.3.1-rc1    | http://rgbww.dronezone.de/release/version.json   |
| testing  | 6.0.0-rc1    | http://rgbww.dronezone.de/testing/version.json   |
| unstable | 6.0.0-alpha5 | http://rgbww.dronezone.de/unstable/version.json  |

# Flashing the Firmware
## Installation from FHEM

FHEM command to update a controller from one of the URLs:

`set myLedDevice fw_update http://rgbww.dronezone.de/testing/version.json`

If the URL is omitted then the FHEM module will use the URL configured by configuration parameter `config-ota-url`.

## Serial Flash

Flashing via serial is done using `esptool` (https://github.com/espressif/esptool). Easiest way to run this is by using `uv` (Installation instructions: https://docs.astral.sh/uv/guides/install-python/). No need to install or download `esptool` manually then.

Wire up the controller and bring it to serial flash mode.

Then run:

```uvx esptool -p /dev/ttyUSB0 write_flash 0x00000 rboot.bin 0x02000 rom0.bin 0x100000 spiff_rom.bin```