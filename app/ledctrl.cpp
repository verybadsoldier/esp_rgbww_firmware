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

/**
 * @struct PinConfig
 * @brief Structure representing the pin configuration for controlling LEDs.
 * 
 * The PinConfig structure holds the pin numbers for controlling different color channels of LEDs.
 * It is used by the APPLedCtrl class to parse and store the pin configuration.
 * 
 * The structure has the following members:
 * - red: Pin number for the red color channel.
 * - green: Pin number for the green color channel.
 * - blue: Pin number for the blue color channel.
 * - warmwhite: Pin number for the warm white color channel.
 * - coldwhite: Pin number for the cold white color channel.
 */
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

PinConfig APPLedCtrl::parsePinConfigRGBWW(std::vector<channel> channels){
    PinConfig cfg;

    debug_i("getting pin configuration from channels array");
    for(int i=0;i<channels.size();i++){
        if(channels[i].name == "red"){
            cfg.red = channels[i].pin;
        }else if(channels[i].name == "green"){
            cfg.green = channels[i].pin;
        }else  if(channels[i].name == "blue"){
            cfg.blue = channels[i].pin;
        }else if(channels[i].name == "warmwhite"){
            cfg.warmwhite = channels[i].pin;
        }else if(channels[i].name == "coldwhite"){
            cfg.coldwhite = channels[i].pin;
        }
    }
    return cfg;
}

/**
 * @brief Initializes the APPLedCtrl class.
 *
 * This function initializes the APPLedCtrl class by creating a StepSync object,
 * parsing the pin configuration string, initializing the RGBWWLed, setting up
 * the LED controller, and setting the startup color.
 */
void APPLedCtrl::init() {
    debug_i("APPLedCtrl::init");

    _stepSync = new StepSync();

    PinConfig pins;
    if(app.cfg.general.channels.size()!=0){
        pins = APPLedCtrl::parsePinConfigRGBWW(app.cfg.general.channels);
    }else{
        pins = APPLedCtrl::parsePinConfigString(app.cfg.general.pin_config);
    }

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

/**
 * @brief Initializes the LED controller.
 * 
 * This function sets up the LED controller by configuring the brightness correction,
 * HSV correction, color mode, HSV model, and white temperature based on the 
 * configuration settings stored in the `app` object.
 */
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

/**
 * @brief Publishes the current state of the LED controller to the event server.
 * 
 * This function checks if the event server is enabled and updates the clients accordingly.
 * It publishes the current output and color information to the event server.
 */
void APPLedCtrl::publishToEventServer() {
    if (!app.cfg.events.server_enabled){
        debug_i("APPLEDCtrl - eventserver is disabled");
        return;
    }else{
    //debug_i("APPLEDCtrl - eventserver is enabled, updating clients");
        
    HSVCT const * pHsv = NULL;
    if (_mode == ColorMode::Hsv)
        pHsv = &getCurrentColor();

    app.eventserver.publishCurrentState(getCurrentOutput(), pHsv);
    }
}

/**
 * @brief Publishes the current LED color or output to MQTT.
 * 
 * This function is responsible for publishing the current LED color or output to MQTT.
 * It checks if the color master is enabled and based on the current color mode, it publishes
 * the corresponding data to the MQTT broker.
 * 
 * @note This function does nothing if the color master is disabled.
 */
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

/**
 * @brief Updates the LED state and performs various actions based on the configuration settings.
 *
 * This function is responsible for updating the LED state and performing actions such as publishing clock values,
 * publishing to MQTT, checking for stable color state, and publishing finished step animations.
 * The function takes into account the configuration settings for synchronization, events, and intervals.
 * It also checks if the animation has finished and if the color or transition interval has elapsed.
 *
 * @note This function relies on the configuration settings provided by the `app` object.
 */
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
                // debug_i("APPLedCtrl::updateLed - publishing color event");
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

/**
 * @brief Checks if the current color state is stable and saves it if necessary.
 *
 * This function checks if the current color state is stable by comparing it with the previous color state.
 * If the current color state is the same as the previous color state, the number of stable color steps is incremented.
 * If the current color state is different from the previous color state, the previous color state is updated and the number of stable color steps is reset.
 * If the number of stable color steps reaches a certain threshold, the current color state is saved.
 */
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

/**
 * @brief Publishes the finished step animations.
 * 
 * This function publishes the finished step animations to the MQTT client and event server.
 * It iterates through the list of step animations and publishes each animation's name and requeued status.
 * After publishing, the list of step animations is cleared.
 */
void APPLedCtrl::publishFinishedStepAnimations() {
    for(unsigned int i=0; i < _stepFinishedAnimations.count(); i++) {
        const String& name = _stepFinishedAnimations.keyAt(i);
        const bool requeued = _stepFinishedAnimations.valueAt(i);
        app.mqttclient.publishTransitionFinished(name, requeued);
        app.eventserver.publishTransitionFinished(name, requeued);
    }
    _stepFinishedAnimations.clear();
}

/**
 * @brief Resets the master clock and updates the timer interval.
 * 
 * This function is called when the master clock is reset. It resets the step synchronization
 * and updates the timer interval accordingly. After updating the timer interval, it publishes
 * the status.
 */
void APPLedCtrl::onMasterClockReset() {
    _timerInterval = _stepSync->reset();
    publishStatus();
}

/**
 * @brief Handles the master clock event and updates the timer interval for LED control.
 * 
 * This function is called when the master clock event occurs. It calculates the new timer interval
 * based on the current step counter and the number of steps in the master clock. The timer interval
 * is then limited to ensure it falls within a safe range. Finally, the timer interval is set and the
 * status is published.
 * 
 * @param stepsMaster The number of steps in the master clock.
 */
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
