FROM ubuntu:20.04

ARG SMING_REPO=https://github.com/SmingHub/Sming
ARG SMING_BRANCH=develop
ARG INSTALL_ARGS="Esp32 Esp8266"

# ------------------------------------------------------------------------------
# Pre-requisites
# ------------------------------------------------------------------------------

RUN apt-get -y update \
    && DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Berlin \
    apt-get install -y \
    git \
    sudo \
    tzdata \
    build-essential \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    python3-future \
    cmake \
    python3.8-venv

# ------------------------------------------------------------------------------
# Fetch Sming and install tools
# ------------------------------------------------------------------------------

RUN git clone $SMING_REPO -b $SMING_BRANCH /opt/Sming 

WORKDIR "/opt/Sming"
RUN Tools/install.sh $INSTALL_ARGS
RUN echo ". $(pwd)/Tools/export.sh" >> ~/.bashrc

WORKDIR "/opt/Sming/Sming/"
RUN make python-requirements 
RUN make submodules

WORKDIR "/opt/Sming/samples/Basic_WiFi"
RUN source ~/.bashrc && make SMING_SOC=esp8266
RUN source ~/.bashrc && make SMING_SOC=esp32
RUN source ~/.bashrc && make SMING_SOC=esp32c3

