COMPONENT_SEARCH_DIRS := $(PROJECT_DIR)/Components
COMPONENT_DEPENDS += MDNS RGBWWLed LittleFS ConfigDB ArduinoJson6 OtaNetwork
ARDUINO_LIBRARIES := RGBWWLed ArduinoJson6 OtaNetwork

GLOBAL_CFLAGS += \
  -DIP_REASSEMBLY=1 \
  -DIP_FRAG=1 \
  -DIP_REASS_MAXAGE=3 \
  -DLWIP_NETIF_TX_SINGLE_PBUF=0 \
  -DIP_REASS_DEBUG=1 \
  -DIP_DEBUG=1 \
  -DTCP_DEBUG=1 \
  -DTCP_INPUT_DEBUG=1


#HWCONFIG := two-spiffs-two-roms
#HWCONFIG := old_layout
HWCONFIG := flash_only
#ENABLE_CUSTOM_LWIP = 0
#ENABLE_LWIP_DEBUG = 1


#### rBoot options ####
# use rboot build mode
RBOOT_ENABLED = 1

#  enable tmp rom switching
RBOOT_RTC_ENABLED = 1

# two rom mode (where two roms sit in the same 1mb block of flash)
RBOOT_TWO_ROMS  = 0
RBOOT_BIG_FLASH = 1

ENABLE_CUSTOM_PWM = 0
#ENABLE_CUSTOM_PWM = 0

//COM_SPEED = 230400
//COM_SPEED = 460800
COM_SPEED = 115200
//COM_SPEED = 921600
//COM_SPEED = 2000000
//COM_PORT=/dev/ttyUSB0
COM_PORT=/dev/ttyACM0
#usb-1a86_USB2.0-Serial-if00-port0
#usb-1a86_USB_Single_Serial_5647014434-if00
#usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_01A7B447-if00-port0

CUSTOM_TARGETS += check_versions

#### GIT VERSION Information #####
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)"-["$(shell git rev-parse --abbrev-ref HEAD)"]"
GIT_DATE = $(firstword $(shell git --no-pager show --date=short --format="%ad" --name-only))
WEBAPP_VERSION = `cat $(PROJECT_DIR)/webapp/VERSION`
USER_CFLAGS = -DGITVERSION=\"$(GIT_VERSION)\" -DGITDATE=\"$(GIT_DATE)\" -DWEBAPP_VERSION=\"$(WEBAPP_VERSION)\" -DPARTLAYOUT=\"$(PART_LAYOUT)\"

# include partition file for initial OTA
EXTRA_LDFLAGS := $(call Wrap,user_pre_init)
USER_CFLAGS += -DPARTITION_TABLE_OFFSET=$(PARTITION_TABLE_OFFSET)

.PHONY: check_versions
check_versions:
ifndef GIT_VERSION
	$(info no GIT_VERSION available, using unknown)
	GIT_VERSION = "unknown"
endif
ifndef GIT_DATE
	$(info no GIT_DATE available, using unknown)
	GIT_DATE = "unknown"
endif
ifndef WEBAPP_VERSION
	$(error can not find webapp/VERSION file - please ensure the source code is complete)
endif
ifndef PART_LAYOUT
	$(info partition layout not defined, defaulting to v1)
	PART_LAYOUT=v1
endif
