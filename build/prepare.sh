BUILDROOT=/build
cd $BUILDROOT
#cloning and installing current Sming version
git clone https://github.com/SmingHub/Sming.git
cd $BUILDROOT/Sming
git checkout develop 
Tools/install.sh ESP8266 && Tools/install.sh ESP32

cd $BUILDROOT
# Set environment variables
export NODE_VERSION=18

npm install npm@latest -g 
wget -qO- https://deb.nodesource.com/setup_$NODE_VERSION.x|/bin/bash - 
apt-get install nodejs -y 
echo "nvm installed" 

export NODE_PATH=$NVM_DIR/v$NODE_VERSION/lib/node_modules
export PATH=$NVM_DIR/v$NODE_VERSION/bin:$PATH

# Clone your project repository and checkout the 'devel' branch
git clone https://github.com/pljakobs/esp_rgbww_firmware.git
git clone https://github.com/pljakobs/esp_rgb_webapp2.git

cd $BUILDROOT/esp_rgbww_firmware
git pull origin
git checkout devel
rm -rf Sming
ln -s $BUILDROOT/Sming Sming
export SMING_HOME=/Sming
export ESP_HOME=/opt/esp-quick-toolchain
export IDF_PATH=/opt/esp-idf
export ESP32_PYTHON_PATH=/usr/bin

cd $BUILDROOT/esp_rgb_webapp2
git pull origin 
git checkout devel

# Install app dependencies
npm install

