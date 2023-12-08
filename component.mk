COMPONENT_SEARCH_DIRS := $(PROJECT_DIR)/Components

ARDUINO_LIBRARIES := RGBWWLed ArduinoJson6 OtaNetwork

HWCONFIG := spiffs-two-roms

# These are defined in hardware config or no longer required
# SPI_SIZE = 4M
# SPIFF_SIZE ?= 786432 #~768KB spiffs size
# RBOOT_SPIFFS_0  = 0x100000
# RBOOT_SPIFFS_1  = 0x300000
# RBOOT_BIG_FLASH = 1

#### SPIFFS options ####
# folder with files to include
SPIFF_FILES = Storage

#### rBoot options ####
# use rboot build mode
# RBOOT_ENABLED = 1

#  enable tmp rom switching
RBOOT_RTC_ENABLED = 1

# two rom mode (where two roms sit in the same 1mb block of flash)
RBOOT_TWO_ROMS  = 0

ENABLE_CUSTOM_PWM = 0
#ENABLE_CUSTOM_PWM = 0

COM_SPEED = 230400
COM_PORT=/dev/ttyUSB0

CUSTOM_TARGETS += check_versions

#### GIT VERSION Information #####
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)
GIT_DATE = $(firstword $(shell git --no-pager show --date=short --format="%ad" --name-only))
WEBAPP_VERSION = `cat $(PROJECT_DIR)/webapp/VERSION`
USER_CFLAGS = -DGITVERSION=\"$(GIT_VERSION)\" -DGITDATE=\"$(GIT_DATE)\" -DWEBAPP_VERSION=\"$(WEBAPP_VERSION)\"

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
