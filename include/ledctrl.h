/**
 * @file
 * @author  Patrick Jahns http://github.com/patrickjahns
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 *
 */
#pragma once

#include "mqtt.h"
#include "stepsync.h"

#define APP_COLOR_FILE ".color"

struct PinConfig {
    PinConfig() : red(13), green(12), blue(14), warmwhite(5), coldwhite(4) {}

    int red;
    int green;
    int blue;
    int warmwhite;
    int coldwhite;
    bool isValid;
};


class APPLedCtrl: public RGBWWLed {

public:
    virtual ~APPLedCtrl();

    void init();
    void setup();

    void start();
    void stop();
    void colorSave();
    void colorReset();
    void testChannels();
    void toggle();

    void updateLed();
    void onMasterClock(uint32_t steps);
    void onMasterClockReset();
    virtual void onAnimationFinished(const String& name, bool requeued);

    void setClockMaster(bool master, uint32_t interval = 0) {
        clockMaster = master;
        clockMasterInterval = interval;
    };
    void setColorMaster(bool master) {
        colorMaster = master;
    };
    void setTransitionInterval(uint32_t interval) {
        transFinInterval = interval;
    };
    void setColorInterval(uint32_t interval) {
        colorMasterInterval = interval;
    };
    void setColorMinInterval(uint32_t interval) {
        colorMinInterval = interval;
    };
    void setStartupColorLast(bool last) {
        startupColorLast = last;
    };
    void reconfigure();
private:
    static PinConfig parsePinConfigString(String& pinStr);
    static PinConfig parsePinConfigString(const String& pinStr);
    static void updateLedCb(void* pTimerArg);
    static bool isPinValid(int pin);
    void publishToEventServer();
    void publishToMqtt();
    void publishFinishedStepAnimations();
    void publishColorStayedCmds();
    void checkStableColorState();
    void publishStatus();

    bool clockMaster,
         colorMaster,
         startupColorLast;
    uint32_t clockMasterInterval,
             transFinInterval,
             colorMasterInterval,
             colorMinInterval;

    HSVCT _lastHsvct;
    ChannelOutput _lastOutput;

    StepSync* _stepSync = nullptr;

    uint32_t _stepCounter = 0;
    HSVCT _prevColor;
    uint32_t _numStableColorSteps = 0;
    ChannelOutput _prevOutput;

    static const uint32_t _saveAfterStableColorMs = 2000;

    SimpleTimer _ledTimer;
    uint32_t _timerInterval = RGBWW_MINTIMEDIFF;
    HashMap<String, bool> _stepFinishedAnimations;
    uint32_t _lastColorEvent = 0;
};
