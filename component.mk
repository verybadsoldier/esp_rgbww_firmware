COMPONENT_SEARCH_DIRS := $(PROJECT_DIR)/Components
COMPONENT_DEPENDS += MDNS RGBWWLed LittleFS ConfigDB ArduinoJson6 OtaNetwork 

# Set default number of jobs to twice the number of available processors
NUM_JOBS := $(shell echo $(($(nproc) * 2)))
MAKEFLAGS += -j$(NUM_JOBS)

HWCONFIG :=two_roms_two_lfs_$(SMING_ARCH)

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
//COM_SPEED = 921600
//COM_SPEED = 2000000
//COM_PORT=/dev/ttyUSB0
//COM_PORT=/dev/ttyACM0

ifeq ($(SMING_ARCH), Esp8266)
    $(info arch Esp8266)
    ifeq ($(strip $(COM_PORT)),)
        override COM_PORT=/dev/serial/by-id/usb-1a86_USB_Single_Serial_5647014434-if00
    endif
    ifeq ($(strip $(COM_SPEED)),)
        override COM_SPEED=921600
    endif
    $(info COM_PORT is $(COM_PORT)@$(COM_SPEED) for $(SMING_ARCH))
else ifeq ($(SMING_ARCH), Esp32)
    ifeq ($(strip $(COM_PORT)),)
        override COM_PORT=/dev/serial/by-id/usb-1a86_USB_Single_Serial_5647022450-if00
    endif
    ifeq ($(strip $(COM_SPEED)),)
        override COM_SPEED=115200
    endif
    $(info COM_PORT is $(COM_PORT)@$(COM_SPEED) for $(SMING_ARCH))
endif

CUSTOM_TARGETS += check_versions

#### GIT VERSION Information #####
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)"-["$(shell git rev-parse --abbrev-ref HEAD)"]"
GIT_DATE = $(firstword $(shell git --no-pager show --date=short --format="%ad" --name-only))
SMING_GITVERSION =	$(shell git -C $(SMING_HOME)/.. describe --abbrev=4 --dirty --always --tags)"-["$(shell git -C $(SMING_HOME)/.. rev-parse --abbrev-ref HEAD)"]"
WEBAPP_VERSION = $(shell cat $(PROJECT_DIR)/webapp/VERSION)
USER_CFLAGS = -DGITVERSION=\"$(GIT_VERSION)\" -DGITDATE=\"$(GIT_DATE)\" -DWEBAPP_VERSION=\"$(WEBAPP_VERSION)\" -DSMING_GITVERSION=\"$(SMING_GITVERSION)\"

$(info using firmware version $(GIT_VERSION))
$(info using WEBapp $(WEBAPP_VERSION))
$(info using SMING $(SMING_GITVERSION))

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
