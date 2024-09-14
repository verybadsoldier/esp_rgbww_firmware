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
PinConfig APPLedCtrl::parsePinConfigString(const String& pinStr) {
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

/**
 * @brief Initializes the APPLedCtrl class.
 *
 * This function initializes the APPLedCtrl class by creating a StepSync object,
 * parsing the pin configuration string, initializing the RGBWWLed, setting up
 * the LED controller, and setting the startup color.
 * 
 * ToDo:
 * in the future, this should be expanded to be able to use more pins than just five (some platforms make 12 or more pwm pins available)
 * the idea is to introduce a layer of abstraction above the pins that can be configured as lights. This way, the user can define a light
 * to be RGB, RGBW, RGBWW, WW or just a single channel and use one controller to drive multiple "virtual" lights.
 * For now, however, we will stick to the current implementation. 
 * 
 * ToDo:
 * a nearer term feature I'm, thinking about is named channels. Currently, the channels are named for the RGBWW colors the drive in the HSV 
 * color model. Named channels would be an extension of the RAW API and allow the user to a) name a channel and b) use that name in the API
 * to set the intensity for that channel. This will not require a change of the underlying code, but will necessitate a change to the PinConfig 
 * structure, namely rather than having the color names hardcoded there, I'll switch to channel numbers. 
 * Once named mode has been fully implemented and a controller has been configured with channel names other than [ red, green, blue, warmwhite, 
 * coldwhite ], I believe the controller shall not allow calls to the /color api with the HSV or RAW structure but with a new, yet to define
 * CHANNELS structure that will provide the channel name and the channel brightness.
 */

void APPLedCtrl::init() {
    debug_i("APPLedCtrl::init");

    _stepSync = new StepSync();

    PinConfig pins;
    {
        AppConfig::General general(*app.cfg);
        if(general.channels.getItemCount()!=0){
            // prefer the channels config
            for(unsigned int i=0;i<general.channels.getItemCount();i++){
                if(general.channels[i].getName() == "red"){
                    pins.red = general.channels[i].getPin();
                }else if(general.channels[i].getName() == "green"){
                    pins.green = general.channels[i].getPin();
                }else if(general.channels[i].getName() == "blue"){
                    pins.blue = general.channels[i].getPin();
                }else if(general.channels[i].getName() == "warmwhite"){
                    pins.warmwhite = general.channels[i].getPin();
                }else if(general.channels[i].getName() == "coldwhite"){
                    pins.coldwhite = general.channels[i].getPin();
                }
            }
        }else{
            //fall back on the old pin config string if necessary
            pins = APPLedCtrl::parsePinConfigString(general.getPinConfig());
        }
    }
    RGBWWLed::init(pins.red, pins.green, pins.blue, pins.warmwhite, pins.coldwhite, PWM_FREQUENCY);

    setup();

    HSVCT startupColor;
    {
        AppConfig::Color color(*app.cfg);
        if (color.getStartupColor() == "last") {
            colorStorage.load();
            debug_i("H: %i | s: %i | v: %i | ct: %i", colorStorage.current.h, colorStorage.current.s, colorStorage.current.v, colorStorage.current.ct);

            startupColor = colorStorage.current;
        } else {
            // interpret as color string
            startupColor = color.getStartupColor();
        }
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

    {
        AppConfig::Color color(*app.cfg);

        colorutils.setBrightnessCorrection(
            color.brightness.getRed(),
            color.brightness.getGreen(),
            color.brightness.getBlue(),
            color.brightness.getWw(), 
            color.brightness.getCw()
            );
        colorutils.setHSVcorrection(
            color.hsv.getRed(), 
            color.hsv.getYellow(),
            color.hsv.getGreen(), 
            color.hsv.getCyan(),
            color.hsv.getBlue(), 
            color.hsv.getMagenta()
            );

        colorutils.setColorMode((RGBWW_COLORMODE) color.getOutputmode());
        colorutils.setHSVmodel((RGBWW_HSVMODEL) color.hsv.getModel());

        colorutils.setWhiteTemperature(
            color.colortemp.getWw(),
            color.colortemp.getCw()
            );
    } // end configdb context for color
}

/**
 * @brief Publishes the current state of the LED controller to the event server.
 * 
 * This function checks if the event server is enabled and updates the clients accordingly.
 * It publishes the current output and color information to the event server.
 */
void APPLedCtrl::publishToEventServer() {
    /* 
     * ToDo: there might be a better way to do this. 
     *       This is called relatively often and the config is read every time.
     *       since eventserver is a class, maybe it should have it's state locally
     */
    AppConfig::Root config(*app.cfg); 
    if (!config.events.getServerEnabled()){
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
    /* 
     * ToDo: this is called frequently, it might be good to hold this config in app directly
     */
    {
        AppConfig::Sync sync(*app.cfg);
        if (!sync.getColorMasterEnabled())
        return;
    }
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
    /* ToDo profile this and see if it works with database access. 
     */
    {
        AppConfig::Sync sync(*app.cfg);
        
        if (sync.getClockMasterEnabled()) {
            if ((_stepCounter % (sync.getClockMasterInterval() * RGBWW_UPDATEFREQUENCY)) == 0) {
                app.mqttclient.publishClock(_stepCounter);
            }
        }
    } //close configdb context for sync
    const static uint32_t stepLenMs = 1000 / RGBWW_UPDATEFREQUENCY;
    {
        AppConfig::Root config(*app.cfg);
        if (config.events.getColorIntervalMs() >= 0) {
            if (animFinished || config.events.getColorIntervalMs() == 0 ||
                    ((stepLenMs * _stepCounter) % config.events.getColorIntervalMs()) < stepLenMs) {

                uint32_t now = millis();
                if (now - _lastColorEvent >= (uint32_t) config.events.getColorMinintervalMs()) {
                    // debug_i("APPLedCtrl::updateLed - publishing color event");
                    _lastColorEvent = now;
                    publishToEventServer();
                }
            }
        }
    } //close configdb context for events

    {
        AppConfig::Sync sync(*app.cfg);
        if (sync.getColorMasterEnabled()) {
            if (animFinished || sync.getColorMasterIntervalMs() == 0 ||
                    ((stepLenMs * _stepCounter) % sync.getColorMasterIntervalMs()) < stepLenMs) {
                publishToMqtt();
            }
        }
    } //close configdb context for sync

    checkStableColorState();
    {
        AppConfig::Root config(*app.cfg);
        if (config.events.getTransfinIntervalMs() >= 0) {
            if (config.events.getTransfinIntervalMs() == 0 ||
                    ((stepLenMs * _stepCounter) % config.events.getTransfinIntervalMs()) < stepLenMs) {
                publishFinishedStepAnimations();
            }
        }
    } //close configdb context for events
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
    {
        AppConfig::Color color(*app.cfg);
        if (color.getStartupColor() != "last")
            return;
    } //close configdb context for color
	
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
