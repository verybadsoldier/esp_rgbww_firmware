FROM ubuntu:20.04

# ------------------------------------------------------------------------------
# Pre-requisites
# ------------------------------------------------------------------------------

RUN apt-get -y update \
    && DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/London \
    apt-get install -y \
    git \
    sudo \
    tzdata \
    python3.8-venv

# ------------------------------------------------------------------------------
# Fetch Sming and install tools
# ------------------------------------------------------------------------------

ARG SMING_REPO=https://github.com/SmingHub/Sming
ARG SMING_BRANCH=develop
ARG INSTALL_ARGS="Esp32 Esp8266"

RUN git clone $SMING_REPO -b $SMING_BRANCH /opt/sming \
    git submodule update --init --recursive

WORKDIR "/opt/sming"

RUN Tools/install.sh $INSTALL_ARGS
RUN echo ". $(pwd)/Tools/export.sh" >> ~/.bashrc