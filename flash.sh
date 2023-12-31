COM_PORT=/dev/serial/by-id/usb-1a86_USB2.0-Serial-if00-port0
COM_SPEED=$(grep -e "^COM_SPEED" component.mk |cut -d "=" -f 2)
//COM_SPEED=921600
python3 /home/pjakobs/devel/esp_rgbww_firmware/Sming/Sming/Components/esptool/esptool/esptool.py -p $COM_PORT -b $COM_SPEED --chip esp8266 --before default_reset --after hard_reset write_flash -z -fs 4MB -ff 40m -fm dio 0x00000 out/Esp8266/debug/firmware/rboot.bin 0x003fa000 out/Esp8266/debug/firmware/partitions.bin 0x00002000 out/Esp8266/debug/firmware/rom0.bin 0x00102000 out/Esp8266/debug/firmware/rom0.bin 0x00200000 out/Esp8266/debug/firmware/spiff_rom.bin  0x003fc000 /home/pjakobs/devel/esp_rgbww_firmware/Sming/Sming/Arch/Esp8266/Components/esp8266/sdk/bin/esp_init_data_default.bin
#usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_01A7B447-if00-port0
python3 -m serial.tools.miniterm --raw --encoding ascii --rts 0 --dtr 0 $COM_PORT $COM_SPEED

