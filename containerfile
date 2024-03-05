# Use an Ubuntu base image
FROM ubuntu:20.04
# Set environment variables to automate tzdata setup
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin

# Replace shell with bash so we can source files
RUN rm /bin/sh && ln -s /bin/bash /bin/sh

# Install necessary dependencies
RUN apt-get update && apt-get install -y \
    tzdata \
    git \
    make \
    g++ \
    gperf \
    binutils \
    python3 \
    python3-serial \
    srecord \
    bc \
    xz-utils \
    flex \
    bison \
    libtool-bin \
    wget \
    unzip \
    libncurses-dev \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    python3-future \
    cmake \
    ninja-build \
    ccache \
    dfu-util \
    sudo \
    npm \
    clang-format \
    apt-transport-https \
    ca-certificates \
    curl \
    libssl-dev \
    libusb-1.0-0

WORKDIR /
#cloning and installing current Sming version
RUN git clone https://github.com/SmingHub/Sming.git
workdir Sming
RUN Tools/install.sh ESP8266
RUN Tools/install.sh ESP32

WORKDIR /
# Set environment variables
ENV NODE_VERSION 18

RUN npm install npm@latest -g

RUN wget -qO- https://deb.nodesource.com/setup_$NODE_VERSION.x|/bin/bash - 
RUN apt-get install nodejs -y

RUN echo "nvm installed" 

#ENV NODE_PATH=$NVM_DIR/v$NODE_VERSION/lib/node_modules
#ENV PATH      $NVM_DIR/v$NODE_VERSION/bin:$PATH

# Clone your project repository and checkout the 'devel' branch
RUN git clone https://github.com/pljakobs/esp_rgbww_firmware.git
RUN git clone https://github.com/pljakobs/esp_rgb_webapp2.git

WORKDIR /esp_rgbww_firmware
RUN git pull origin
RUN git checkout devel
RUN rm -rf Sming
RUN ln -s /Sming Sming
ENV SMING_HOME=/Sming
ENV ESP_HOME=/opt/esp-quick-toolchain
ENV IDF_PATH=/opt/esp-idf
ENV ESP32_PYTHON_PATH=/usr/bin

WORKDIR /esp_rgb_webapp2
RUN git pull origin
RUN git checkout devel

# Install app dependencies
RUN npm install

# run actual quasar frontend build
RUN git status
RUN npx quasar build
RUN ./minifyFontnames.sh
RUN ./gzipSPA.sh
#RUN rm -rf /esp_rgbww_firmware/spiffs/*
RUN echo $(git describe --abbrev=4 --dirty --always --tags) > dist/spa/VERSION
RUN cp -a dist/spa/ /esp_rgbww_firmware/spiffs

WORKDIR /esp_rgbww_firmware
RUN git submodule update --remote --init Components/RGBWWLed

WORKDIR /
RUN mkdir html

#WORKDIR /esp_rgbww_firmware
#RUN make -j8 SMING_SOC=esp8266 PART_LAYOUT=v1
#RUN make clean
#RUN make SMING_SOC=esp8266 PART_LAYOUT=v2
#RUN make clean
#RUN make SMING_SOC=esp32c3
