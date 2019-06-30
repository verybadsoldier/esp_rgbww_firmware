COMPONENT_SEARCH_DIRS := $(PROJECT_DIR)/Components

ARDUINO_LIBRARIES := RGBWWLed

# SPI EEPROM SIZE
SPI_SIZE = 4M

#### SPIFFS options ####
# folder with files to include
SPIFF_FILES = webapp

# size of filesystem
SPIFF_SIZE ?= 786432 #~768KB spiffs size
#SPIFF_SIZE      = 284288 #~512KB spiffs size


#### rBoot options ####
# use rboot build mode
RBOOT_ENABLED = 1

#  enable tmp rom switching
RBOOT_RTC_ENABLED = 1

# enable big flash support (for multiple roms, each in separate 1mb block of flash)
RBOOT_BIG_FLASH = 1

# two rom mode (where two roms sit in the same 1mb block of flash)
RBOOT_TWO_ROMS  = 0

# where does the filesystem reside
RBOOT_SPIFFS_0  = 0x100000
RBOOT_SPIFFS_1  = 0x300000 

ENABLE_CUSTOM_PWM = 0
#ENABLE_CUSTOM_PWM = 0
## output file for first rom (.bin will be appended)
#RBOOT_ROM_0     ?= rom0
## input linker file for first rom
#RBOOT_LD_0      ?= rom0.ld


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
