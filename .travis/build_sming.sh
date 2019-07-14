#!/bin/sh

echo "Downloading esp-open-sdk from $ESP_OPEN_SDK_BIN"
wget --no-verbose $ESP_OPEN_SDK_BIN
echo "Extracting..."
tar -Jxvf esp-open-sdk.tar.xz
ls esp-open-sdk
export ESP_HOME=$TRAVIS_BUILD_DIR/esp-open-sdk
export SMING_HOME=$TRAVIS_BUILD_DIR/Sming/Sming
export SDK_BASE=$TRAVIS_BUILD_DIR/Sming/Sming/third-party/ESP8266_NONOS_SDK
cd $SMING_HOME
echo "Removing Sming examples..."
rm -rf samples
echo "Building..."
make -j2
