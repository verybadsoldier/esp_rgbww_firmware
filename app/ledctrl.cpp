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

APPLedCtrl::~APPLedCtrl()
{
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
PinConfig APPLedCtrl::parsePinConfigString(const String& pinStr)
{
	Vector<int> pins;
	bool isValid=true;
	{
		String _pinStr = pinStr;

		splitString(_pinStr, ',', pins);
	}
	
	// sanity check
	for(int i = 0; i < 5; ++i) {
		if(pins[i] == 0||!isPinValid(pins[i])) {
			isValid = false;
		}
	}

	if(pins.size() != 5)
		isValid = false;

	if(!isValid) {
		debug_e("APPLedCtrl::parsePinConfigString - Error in pin configuration - Using default pin values");
		return PinConfig();
	}

	PinConfig cfg;
	cfg.red = pins[0];
	cfg.green = pins[1];
	cfg.blue = pins[2];
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

void APPLedCtrl::init()
{
	debug_i("APPLedCtrl::init");

	_stepSync = new StepSync();

	reconfigure();

	PinConfig pins;
	pins.isValid=true;
	{
		debug_i("APPLedCtrl::init - reading pin config");
		AppConfig::General general(*app.cfg);
		if(general.channels.getItemCount() != 0) {
			// prefer the channels config
			debug_i("cannels array configured");
			for(auto channel : general.channels) {
				int pin=channel.getPin();
				if (isPinValid(pin)){
					if(channel.getName() == "red") {
						pins.red = channel.getPin();
					} else if(channel.getName() == "green") {
						pins.green = channel.getPin();
					} else if(channel.getName() == "blue") {
						pins.blue = channel.getPin();
					} else if(channel.getName() == "warmwhite") {
						pins.warmwhite = channel.getPin();
					} else if(channel.getName() == "coldwhite") {
						pins.coldwhite = channel.getPin();
					}
				}else{
					debug_e("APPLedCtrl::init - invalid pin %i for SoC %s", pin, SOC);
					pins.isValid=false;
				}
			}
		} else {
			//fall back on the old pin config string if necessary
			debug_i("no channels array configured");
			pins = APPLedCtrl::parsePinConfigString(general.getPinConfig());
			//populate the pin array for next time
			if(auto generalUpdate = general.update()) {
				{
					auto pin = generalUpdate.channels.addItem();
					pin.setName("red");
					pin.setPin(pins.red);
				}
				{
					auto pin = generalUpdate.channels.addItem();
					pin.setName("green");
					pin.setPin(pins.green);
				}
				{
					auto pin = generalUpdate.channels.addItem();
					pin.setName("blue");
					pin.setPin(pins.blue);
				}
				{
					auto pin = generalUpdate.channels.addItem();
					pin.setName("warmwhite");
					pin.setPin(pins.warmwhite);
				}
				{
					auto pin = generalUpdate.channels.addItem();
					pin.setName("coldwhite");
					pin.setPin(pins.coldwhite);
				}
			} // end AppConfig::General::update context
			else {
				debug_e("APPLedCtrl::init - failed to update pin config");
			}
		}
	} //end ConfigDB::General context

	debug_i(
		"APPLedCtrl::init - initializing RGBWWLed\n   red: %i | green: %i | blue: %i | warmwhite: %i | coldwhite: %i, valid: [%s]",
		pins.red, pins.green, pins.blue, pins.warmwhite, pins.coldwhite, pins.isValid?"true":"false");
	if(pins.isValid)
		{
			RGBWWLed::init(pins.red, pins.green, pins.blue, pins.warmwhite, pins.coldwhite, PWM_FREQUENCY);
		debug_i("APPLedCtrl::init - finished setting up RGBWWLed");
		setup();
		}

	HSVCT startupColor;
	{
		debug_i("APPLedCtrl::init - reading startup color");
		AppConfig::Color color(*app.cfg);
		if(color.getStartupColor() == "last") {
			AppData::Root data(*app.data);
			debug_i("H: %i | s: %i | v: %i | ct: %i", data.lastColor.getH(), data.lastColor.getS(),
					data.lastColor.getV(), data.lastColor.getCt());

			startupColor.h = data.lastColor.getH();
			startupColor.s = data.lastColor.getS();
			startupColor.v = data.lastColor.getV();
			startupColor.ct = data.lastColor.getCt();
		} else {
			// interpret as color string
			String tempStartupColor = color.getStartupColor();
			startupColor = tempStartupColor;
		}
	}
	// boot from off to startup color
	HSVCT startupColorDark = startupColor;
	startupColorDark.v = 0;
	fadeHSV(startupColorDark, startupColor, 2000); //fade to color in 700ms
}

bool APPLedCtrl::isPinValid(int currentPin)
{
	AppConfig::Hardware hardware(*app.cfg);
	for (auto pinconfig : hardware.availablePins){
		if (strcmp(pinconfig.getSoc().c_str(),SOC)==0){
			for (auto pin : pinconfig.pins){
				if(pin==currentPin){
					return true;
				}
			}
		debug_e("APPLedCtrl::isPinValid - invalid pin %i for SoC %s", currentPin, SOC);
		return false;
		}
	}
	debug_e("APPLedCtrl::isPinValid - invalid SoC %s", SOC);
	return false;
}
/**
 * @brief read local configuration from ConfigDB
 * 
 */
void APPLedCtrl::reconfigure()
{
	debug_i("APPLedCtrl::reconfigure");
	{
		AppConfig::Sync sync(*app.cfg);
		clockMaster = sync.getClockMasterEnabled();
		clockMasterInterval = sync.getClockMasterInterval();
		colorMaster = sync.getColorMasterEnabled();
		colorMasterInterval = sync.getColorMasterIntervalMs();

	} // end AppConfig::Sync context
	{
		AppConfig::Root config(*app.cfg);
		transFinInterval = config.events.getTransFinIntervalMs();
		colorMinInterval = config.events.getColorMinIntervalMs();
	} //close configdb root context
	{
		AppConfig::Color color(*app.cfg);
		startupColorLast = (color.getStartupColor() == "last");
	} //close configdb context for color
}
/**
 * @brief Initializes the LED controller.
 * 
 * This function sets up the LED controller by configuring the brightness correction,
 * HSV correction, color mode, HSV model, and white temperature based on the 
 * configuration settings stored in the `app` object.
 */
void APPLedCtrl::setup()
{
	debug_i("APPLedCtrl::setup");

	{
		debug_i("APPLedCtrl::setup - reading color config");
		AppConfig::Color color(*app.cfg);

		colorutils.setBrightnessCorrection(color.brightness.getRed(), color.brightness.getGreen(),
										   color.brightness.getBlue(), color.brightness.getWw(),
										   color.brightness.getCw());
		colorutils.setHSVcorrection(color.hsv.getRed(), color.hsv.getYellow(), color.hsv.getGreen(),
									color.hsv.getCyan(), color.hsv.getBlue(), color.hsv.getMagenta());

		colorutils.setColorMode((RGBWW_COLORMODE)color.getColorMode());
		colorutils.setHSVmodel((RGBWW_HSVMODEL)color.hsv.getModel());
		debug_i("set RGBWW_COLORMOD %i and RGBWW_HSVMODEL %i", color.getColorMode(), color.hsv.getModel());

		colorutils.setWhiteTemperature(color.colortemp.getWw(), color.colortemp.getCw());
	} // end configdb context for color
}

/**
 * @brief Publishes the current state of the LED controller to the event server.
 * 
 * This function checks if the event server is enabled and updates the clients accordingly.
 * It publishes the current output and color information to the event server.
 */
void APPLedCtrl::publishToEventServer()
{
	if(!app.eventserver.isEnabled()) {
		debug_i("APPLEDCtrl - eventserver is disabled");
		return;
	}
	HSVCT const* pHsv = NULL;
	if(_mode == ColorMode::Hsv)
		pHsv = &getCurrentColor();

	app.eventserver.publishCurrentState(getCurrentOutput(), pHsv);
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
void APPLedCtrl::publishToMqtt()
{
	AppConfig::Network network(*app.cfg);
	if(network.mqtt.getEnabled()) {
		if(colorMaster) {
			switch(_mode) {
			case ColorMode::Hsv:
				app.mqttclient.publishCurrentHsv(getCurrentColor());
				break;
			case ColorMode::Raw:
				app.mqttclient.publishCurrentRaw(getCurrentOutput());
				break;
			}
		}
	}
}

void APPLedCtrl::updateLedCb(void* pTimerArg)
{
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
void APPLedCtrl::updateLed()
{
	// arm next timer
	_ledTimer.startOnce();

	/************************** 
     * those rely on the mqtt master/secodary toggles to be set
     * but really, they should also look at the main mqtt enable flag
     **************************/
	const bool animFinished = show();

	++_stepCounter;

	// publish _stepCounter if this is the clockMaster
	if(clockMaster) {
		if((_stepCounter % clockMasterInterval * RGBWW_UPDATEFREQUENCY) == 0) {
			app.mqttclient.publishClock(_stepCounter);
		}
	}

	const static uint32_t stepLenMs = 1000 / RGBWW_UPDATEFREQUENCY;

	if(colorMasterInterval >= 0) {
		if(animFinished || colorMasterInterval == 0 || ((stepLenMs * _stepCounter) % colorMasterInterval) < stepLenMs) {
			uint32_t now = millis();
			if(now - _lastColorEvent >= (uint32_t)colorMinInterval) {
				// debug_i("APPLedCtrl::updateLed - publishing color event");
				_lastColorEvent = now;
				publishToEventServer();
			}
		}
	}

	if(colorMaster) {
		if(animFinished || colorMasterInterval == 0 || ((stepLenMs * _stepCounter) % colorMasterInterval) < stepLenMs) {
			publishToMqtt();
		}
	}

	checkStableColorState();

	if(transFinInterval >= 0) {
		if(transFinInterval == 0 || ((stepLenMs * _stepCounter) % transFinInterval) < stepLenMs) {
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
void APPLedCtrl::checkStableColorState()
{
	if(!startupColorLast)
		return;

	if(_prevColor == getCurrentColor()) {
		++_numStableColorSteps;
	} else {
		_prevColor = getCurrentColor();
		_numStableColorSteps = 0;
	}

	// save if color was stable for _saveAfterStableColorMs
	if(abs(_saveAfterStableColorMs - (_numStableColorSteps * RGBWW_MINTIMEDIFF)) <= (uint32_t)(RGBWW_MINTIMEDIFF / 2))
		colorSave();
}

/**
 * @brief Publishes the finished step animations.
 * 
 * This function publishes the finished step animations to the MQTT client and event server.
 * It iterates through the list of step animations and publishes each animation's name and requeued status.
 * After publishing, the list of step animations is cleared.
 */
void APPLedCtrl::publishFinishedStepAnimations()
{
	for(unsigned int i = 0; i < _stepFinishedAnimations.count(); i++) {
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
void APPLedCtrl::onMasterClockReset()
{
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
void APPLedCtrl::onMasterClock(uint32_t stepsMaster)
{
	_timerInterval = _stepSync->onMasterClock(_stepCounter, stepsMaster);

	// limit interval to sane values (just for safety)
	_timerInterval = std::min(std::max(_timerInterval, static_cast<uint32_t>(RGBWW_MINTIMEDIFF_US / 2u)),
							  static_cast<uint32_t>(RGBWW_MINTIMEDIFF_US * 1.5));
	_ledTimer.setIntervalUs(_timerInterval);
	publishStatus();
}

void APPLedCtrl::publishStatus()
{
	app.eventserver.publishClockSlaveStatus(_stepSync->getCatchupOffset(), _timerInterval);
	app.mqttclient.publishClockSlaveOffset(_stepSync->getCatchupOffset());
	app.mqttclient.publishClockInterval(_timerInterval);
}

void APPLedCtrl::start()
{
	debug_i("APPLedCtrl::start");

	_ledTimer.setCallback(APPLedCtrl::updateLedCb, this);
	_ledTimer.setIntervalMs(_timerInterval);
	debug_i("_timerInterval", _timerInterval);
	_ledTimer.startOnce();
}

void APPLedCtrl::stop()
{
	debug_i("APPLedCtrl::stop");
	_ledTimer.stop();
}

void APPLedCtrl::colorSave()
{
	AppData::Root data(*app.data);
	{
		auto update = data.update();
		auto current = getCurrentColor();
		update.lastColor.setH(current.h);
		update.lastColor.setS(current.s);
		update.lastColor.setV(current.v);
		update.lastColor.setCt(current.ct);
	}
}

void APPLedCtrl::colorReset()
{
	debug_i("APPLedCtrl::colorReset");
	AppData::Root data(*app.data);
	{
		auto update = data.update();
		update.lastColor.setH(0);
		update.lastColor.setS(0);
		update.lastColor.setV(0);
		update.lastColor.setCt(0);
	}
}

void APPLedCtrl::onAnimationFinished(const String& name, bool requeued)
{
	debug_d("APPLedCtrl::onAnimationFinished: %s", name.c_str());

	if(name.length() > 0) {
		_stepFinishedAnimations[name] = requeued;
	}
}

void APPLedCtrl::toggle()
{
	static const int toggleFadeTime = 1000;
	switch(_mode) {
	case ColorMode::Hsv: {
		HSVCT current = getCurrentColor();
		if(current.v > 0) {
			debug_d("APPLedCtrl::toggle - off");
			_lastHsvct = current;
			current.v = 0;
			fadeHSV(_lastHsvct, current, toggleFadeTime);
		} else {
			debug_d("APPLedCtrl::toggle - on");
			if(_lastHsvct.v == 0)
				_lastHsvct.v = 100; // we were off before but force some light
			fadeHSV(current, _lastHsvct, toggleFadeTime);
		}
		break;
	}
	case ColorMode::Raw: {
		ChannelOutput current = getCurrentOutput();
		if(current.isOn()) {
			debug_d("APPLedCtrl::toggle - off");
			_lastOutput = current;
			current.r = current.g = current.b = 0;
			current.ww = current.cw = 0;
			fadeRAW(_lastOutput, current, toggleFadeTime);
		} else {
			debug_d("APPLedCtrl::toggle - on");
			if(!_lastOutput.isOn())
				_lastOutput.r = _lastOutput.g = _lastOutput.b = _lastOutput.cw = _lastOutput.ww =
					255; // was off before but force light

			fadeRAW(current, _lastOutput, toggleFadeTime);
		}
	}
	}
}
