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
#include <RGBWWCtrl.h>

#include <Wiring/WVector.h>
#include <Wiring/SplitString.h>

#include <cstdlib>
#include <algorithm>

APPLedCtrl::~APPLedCtrl() {
    delete _stepSync;
    _stepSync = nullptr;
}

PinConfig APPLedCtrl::parsePinConfigString(String& pinStr) {
    Vector<int> pins;
    splitString(pinStr, ',', pins);

    bool isCorrect = true;
    // sanity check
    for(int i=0; i < 5; ++i) {
        if (pins[i] == 0) {
            isCorrect = false;
        }
    }

    if (pins.size() != 5)
        isCorrect = false;

    if (!isCorrect) {
        debug_e("APPLedCtrl::parsePinConfigString - Error in pin configuration - Using default pin values");
        return PinConfig();
    }

    PinConfig cfg;
    cfg.red       = pins[0];
    cfg.green     = pins[1];
    cfg.blue      = pins[2];
    cfg.warmwhite = pins[3];
    cfg.coldwhite = pins[4];
    return cfg;
}

void APPLedCtrl::init() {
    debug_i("APPLedCtrl::init");

    _stepSync = new StepSync();

    const PinConfig pins = APPLedCtrl::parsePinConfigString(app.cfg.general.pin_config);

    RGBWWLed::init(pins.red, pins.green, pins.blue, pins.warmwhite, pins.coldwhite, PWM_FREQUENCY);

    setup();

    HSVCT startupColor;
    if (app.cfg.color.startup_color == "last") {
        colorStorage.load();
        debug_i("H: %i | s: %i | v: %i | ct: %i", colorStorage.current.h, colorStorage.current.s, colorStorage.current.v, colorStorage.current.ct);

        startupColor = colorStorage.current;
    } else {
        // interpret as color string
        startupColor = app.cfg.color.startup_color;
    }

    // boot from off to startup color
    HSVCT startupColorDark = startupColor;
    startupColorDark.v = 0;
    fadeHSV(startupColorDark, startupColor, 2000); //fade to color in 700ms
}

void APPLedCtrl::setup() {
    debug_i("APPLedCtrl::setup");

    colorutils.setBrightnessCorrection(app.cfg.color.brightness.red,
            app.cfg.color.brightness.green, app.cfg.color.brightness.blue,
            app.cfg.color.brightness.ww, app.cfg.color.brightness.cw);
    colorutils.setHSVcorrection(app.cfg.color.hsv.red, app.cfg.color.hsv.yellow,
            app.cfg.color.hsv.green, app.cfg.color.hsv.cyan,
            app.cfg.color.hsv.blue, app.cfg.color.hsv.magenta);

    colorutils.setColorMode((RGBWW_COLORMODE) app.cfg.color.outputmode);
    colorutils.setHSVmodel((RGBWW_HSVMODEL) app.cfg.color.hsv.model);

    colorutils.setWhiteTemperature(app.cfg.color.colortemp.ww, app.cfg.color.colortemp.cw);
}

void APPLedCtrl::publishToEventServer() {
    if (!app.cfg.events.server_enabled)
        return;

    HSVCT const * pHsv = NULL;
    if (_mode == ColorMode::Hsv)
        pHsv = &getCurrentColor();

    app.eventserver.publishCurrentState(getCurrentOutput(), pHsv);
}

void APPLedCtrl::publishToMqtt() {
    if (!app.cfg.sync.color_master_enabled)
        return;

    switch(_mode) {
    case ColorMode::Hsv:
        app.mqttclient.publishCurrentHsv(getCurrentColor());
        break;
    case ColorMode::Raw:
        app.mqttclient.publishCurrentRaw(getCurrentOutput());
        break;
    }
}

void APPLedCtrl::updateLedCb(void* pTimerArg) {
    APPLedCtrl* pThis = static_cast<APPLedCtrl*>(pTimerArg);
    pThis->updateLed();
}

void APPLedCtrl::updateLed() {
    // arm next timer
    _ledTimer.startOnce();

    const bool animFinished = show();

    ++_stepCounter;

    if (app.cfg.sync.clock_master_enabled) {
        if ((_stepCounter % (app.cfg.sync.clock_master_interval * RGBWW_UPDATEFREQUENCY)) == 0) {
            app.mqttclient.publishClock(_stepCounter);
        }
    }

    const static uint32_t stepLenMs = 1000 / RGBWW_UPDATEFREQUENCY;

    if (app.cfg.events.color_interval_ms >= 0) {
        if (animFinished || app.cfg.events.color_interval_ms == 0 ||
                ((stepLenMs * _stepCounter) % app.cfg.events.color_interval_ms) < stepLenMs) {

            uint32_t now = millis();
            if (now - _lastColorEvent >= (uint32_t) app.cfg.events.color_mininterval_ms) {
                _lastColorEvent = now;
                publishToEventServer();
            }
        }
    }

    if (animFinished || app.cfg.sync.color_master_interval_ms == 0 ||
            ((stepLenMs * _stepCounter) % app.cfg.sync.color_master_interval_ms) < stepLenMs) {
        publishToMqtt();
    }

    checkStableColorState();

    if (app.cfg.events.transfin_interval_ms >= 0) {
        if (app.cfg.events.transfin_interval_ms == 0 ||
                ((stepLenMs * _stepCounter) % app.cfg.events.transfin_interval_ms) < stepLenMs) {
            publishFinishedStepAnimations();
        }
    }
}

void APPLedCtrl::checkStableColorState() {
	if (app.cfg.color.startup_color != "last")
		return;

    if (_prevColor == getCurrentColor())
    {
        ++_numStableColorSteps;
    }
    else {
        _prevColor = getCurrentColor();
        _numStableColorSteps = 0;
    }

    // save if color was stable for _saveAfterStableColorMs
    if (abs(_saveAfterStableColorMs - (_numStableColorSteps * RGBWW_MINTIMEDIFF)) <= (uint32_t)(RGBWW_MINTIMEDIFF / 2))
        colorSave();
}

void APPLedCtrl::publishFinishedStepAnimations() {
    for(unsigned int i=0; i < _stepFinishedAnimations.count(); i++) {
        const String& name = _stepFinishedAnimations.keyAt(i);
        const bool requeued = _stepFinishedAnimations.valueAt(i);
        app.mqttclient.publishTransitionFinished(name, requeued);
        app.eventserver.publishTransitionFinished(name, requeued);
    }
    _stepFinishedAnimations.clear();
}

void APPLedCtrl::onMasterClockReset() {
    _timerInterval = _stepSync->reset();
    publishStatus();
}

void APPLedCtrl::onMasterClock(uint32_t stepsMaster) {
    _timerInterval = _stepSync->onMasterClock(_stepCounter, stepsMaster);

    // limit interval to sane values (just for safety)
    _timerInterval = std::min(std::max(_timerInterval, static_cast<uint32_t>(RGBWW_MINTIMEDIFF_US / 2u)), static_cast<uint32_t>(RGBWW_MINTIMEDIFF_US * 1.5));
    _ledTimer.setIntervalUs(_timerInterval);
    publishStatus();
}

void APPLedCtrl::publishStatus() {
    app.eventserver.publishClockSlaveStatus(_stepSync->getCatchupOffset(), _timerInterval);
    app.mqttclient.publishClockSlaveOffset(_stepSync->getCatchupOffset());
    app.mqttclient.publishClockInterval(_timerInterval);
}

void APPLedCtrl::start() {
    debug_i("APPLedCtrl::start");

    _ledTimer.setCallback(APPLedCtrl::updateLedCb, this);
    _ledTimer.setIntervalMs(_timerInterval);
    _ledTimer.startOnce();
}

void APPLedCtrl::stop() {
    debug_i("APPLedCtrl::stop");
    _ledTimer.stop();
}

void APPLedCtrl::colorSave() {
    colorStorage.current = getCurrentColor();
    colorStorage.save();
}

void APPLedCtrl::colorReset() {
    debug_i("APPLedCtrl::colorReset");
    colorStorage.current.h = 0;
    colorStorage.current.s = 0;
    colorStorage.current.v = 0;
    colorStorage.current.ct = 0;
    colorStorage.save();
}

void APPLedCtrl::onAnimationFinished(const String& name, bool requeued) {
    debug_d("APPLedCtrl::onAnimationFinished: %s", name.c_str());

    if (name.length() > 0) {
        _stepFinishedAnimations[name] = requeued;
    }
}

void APPLedCtrl::toggle() {
    static const int toggleFadeTime = 1000;
    switch (_mode) {
    case ColorMode::Hsv: {
        HSVCT current = getCurrentColor();
        if (current.v > 0) {
            debug_d("APPLedCtrl::toggle - off");
            _lastHsvct = current;
            current.v = 0;
            fadeHSV(_lastHsvct, current, toggleFadeTime);
        } else {
            debug_d("APPLedCtrl::toggle - on");
            if (_lastHsvct.v == 0)
                _lastHsvct.v = 100; // we were off before but force some light
            fadeHSV(current, _lastHsvct, toggleFadeTime);
        }
        break;
    }
    case ColorMode::Raw: {
        ChannelOutput current = getCurrentOutput();
        if (current.isOn()) {
            debug_d("APPLedCtrl::toggle - off");
            _lastOutput = current;
            current.r = current.g = current.b = 0;
            current.ww = current.cw = 0;
            fadeRAW(_lastOutput, current, toggleFadeTime);
        } else {
            debug_d("APPLedCtrl::toggle - on");
            if (!_lastOutput.isOn())
                _lastOutput.r = _lastOutput.g = _lastOutput.b = _lastOutput.cw = _lastOutput.ww = 255; // was off before but force light

            fadeRAW(current, _lastOutput, toggleFadeTime);
        }
    }
    }
}
