#!/bin/sh

BUILD_DIR=$1
ESP_OPEN_SDK_BIN=$2

echo "Downloading esp-open-sdk from $ESP_OPEN_SDK_BIN"
wget --no-verbose $ESP_OPEN_SDK_BIN
echo "Extracting..."
tar -Jxvf esp-open-sdk.tar.xz
ls esp-open-sdk
export ESP_HOME=$BUILD_DIR/esp-open-sdk
export SMING_HOME=$BUILD_DIR/Sming/Sming
export SDK_BASE=$BUILD_DIR/Sming/Sming/third-party/ESP8266_NONOS_SDK
cd $SMING_HOME
echo "Removing Sming examples..."
rm -rf samples
echo "Building..."
make
