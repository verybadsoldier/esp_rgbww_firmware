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
#include <cstdlib>
#include <algorithm>

APPLedCtrl::~APPLedCtrl() {
    delete _stepSync;
    _stepSync = nullptr;
}
void APPLedCtrl::init() {
	debugapp("APPLedCtrl::init");

    _stepSync = new ClockCatchUp3();

    // default pin layout
    int pinRed = 13;
    int pinGreen = 12;
    int pinBlue = 14;
    int pinWw = 5;
    int pinCw = 4;

    switch(app.cfg.general.chip_type) {
    case ApplicationSettings::ChipEsp12e:
        pinWw = 4;
        pinCw = 5;
        break;
    default:
        break;
    }

	RGBWWLed::init(pinRed, pinGreen, pinBlue, pinWw, pinCw, PWM_FREQUENCY);

	setup();
	colorStorage.load();
	debugapp("H: %i | s: %i | v: %i | ct: %i", colorStorage.current.h, colorStorage.current.s, colorStorage.current.v, colorStorage.current.ct);

	// boot from off to current color
	HSVCT dark = colorStorage.current;
	dark.h = 0;
	fadeHSV(dark, colorStorage.current, 700); //fade to color in 700ms
}

void APPLedCtrl::setup() {
	debugapp("APPLedCtrl::setup");

	colorutils.setBrightnessCorrection(app.cfg.color.brightness.red,
			app.cfg.color.brightness.green, app.cfg.color.brightness.blue,
			app.cfg.color.brightness.ww, app.cfg.color.brightness.cw);
	colorutils.setHSVcorrection(app.cfg.color.hsv.red, app.cfg.color.hsv.yellow,
			app.cfg.color.hsv.green, app.cfg.color.hsv.cyan,
			app.cfg.color.hsv.blue, app.cfg.color.hsv.magenta);

	colorutils.setColorMode((RGBWW_COLORMODE) app.cfg.color.outputmode);
	colorutils.setHSVmodel((RGBWW_HSVMODEL) app.cfg.color.hsv.model);

}

void APPLedCtrl::publishToEventServer() {
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
    ets_timer_arm_new(&_ledTimer, _timerInterval, 0, 0);

	const bool animFinished = show();

    ++_stepCounter;

	if (app.cfg.sync.clock_master_enabled) {
        if ((_stepCounter % (app.cfg.sync.clock_master_interval * RGBWW_UPDATEFREQUENCY)) == 0) {
            app.mqttclient.publishClock(_stepCounter);
        }
	}

	const static uint32_t stepLenMs = 1000 / RGBWW_UPDATEFREQUENCY;

    if (animFinished || app.cfg.events.color_interval_ms == 0 ||
            ((stepLenMs * _stepCounter) % app.cfg.events.color_interval_ms) < stepLenMs) {
        publishToEventServer();
    }

    if (animFinished || app.cfg.sync.color_master_interval_ms == 0 ||
            ((stepLenMs * _stepCounter) % app.cfg.sync.color_master_interval_ms) < stepLenMs) {
        publishToMqtt();
    }

    checkStableColorState();

    publishFinishedStepAnimations();
}

void APPLedCtrl::checkStableColorState() {
    if (_prevColor == getCurrentColor())
    {
        ++_numStableColorSteps;
    }
    else {
        _prevColor = getCurrentColor();
        _numStableColorSteps = 0;
    }

    // save if color was stable for _saveAfterStableColorMs
    if (abs(_saveAfterStableColorMs - (_numStableColorSteps * RGBWW_MINTIMEDIFF)) <= (RGBWW_MINTIMEDIFF / 2))
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

void APPLedCtrl::onMasterClock(uint32_t stepsMaster) {
    if (_stepSync->getCatchupOffset() > 1000)
        _stepSync->resetCatchupOffset();

    _timerInterval = _stepSync->onMasterClock(_stepCounter, stepsMaster);

    _timerInterval = std::min(std::max(_timerInterval, RGBWW_MINTIMEDIFF_US / 2u), static_cast<uint32_t>(RGBWW_MINTIMEDIFF_US * 1.5));

    app.eventserver.publishClockSlaveStatus(_stepSync->getCatchupOffset(), _timerInterval);
    app.mqttclient.publishClockSlaveOffset(_stepSync->getCatchupOffset());
    app.mqttclient.publishClockInterval(_timerInterval);
}

void APPLedCtrl::start() {
    debugapp("APPLedCtrl::start");

    ets_timer_setfn(&_ledTimer, APPLedCtrl::updateLedCb, this);
    ets_timer_arm_new(&_ledTimer, _timerInterval, 0, 0);
}

void APPLedCtrl::stop() {
    debugapp("APPLedCtrl::stop");
    ets_timer_disarm(&_ledTimer);
}

void APPLedCtrl::colorSave() {
	colorStorage.current = getCurrentColor();
	colorStorage.save();
}

void APPLedCtrl::colorReset() {
	debugapp("APPLedCtrl::colorReset");
	colorStorage.current.h = 0;
	colorStorage.current.s = 0;
	colorStorage.current.v = 0;
	colorStorage.current.ct = 0;
	colorStorage.save();
}

void APPLedCtrl::testChannels() {
//	debugapp("APPLedCtrl::test_channels");
//	ChannelOutput red = ChannelOutput(1023, 0, 0, 0, 0);
//	ChannelOutput green = ChannelOutput(0, 1023, 0, 0, 0);
//	ChannelOutput blue = ChannelOutput(0, 0, 1023, 0, 0);
//	ChannelOutput ww = ChannelOutput(0, 0, 0, 1023, 0);
//	ChannelOutput cw = ChannelOutput(0, 0, 0, 0, 1023);
//	ChannelOutput black = ChannelOutput(0, 0, 0, 0, 0);
//	setRAW(black);
//	fadeRAW(red, 1000, QueuePolicy::Back);
//	fadeRAW(black, 1000, QueuePolicy::Back);
//	fadeRAW(green, 1000, QueuePolicy::Back);
//	fadeRAW(black, 1000, QueuePolicy::Back);
//	fadeRAW(blue, 1000, QueuePolicy::Back);
//	fadeRAW(black, 1000, QueuePolicy::Back);
//	fadeRAW(ww, 1000, QueuePolicy::Back);
//	fadeRAW(black, 1000, QueuePolicy::Back);
//	fadeRAW(cw, 1000, QueuePolicy::Back);
//	fadeRAW(black, 1000, QueuePolicy::Back);
}

void APPLedCtrl::onAnimationFinished(const String& name, bool requeued) {
	debugapp("APPLedCtrl::onAnimationFinished: %s", name.c_str());

	if (name.length() > 0) {
	    _stepFinishedAnimations[name] = requeued;
	}
}

uint32_t ClockCatchUp3::onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) {
    uint32_t nextInt = _constBaseInt;
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        int curOffset = masterDiff - diff;
        _catchupOffset += curOffset;
        Serial.printf("Diff: %d | Master Diff: %d | CurOffset: %d | Catchup Offset: %d\n", diff, masterDiff, curOffset, _catchupOffset);

        double curSteering = 1.0 - static_cast<double>(_catchupOffset) / masterDiff;
        curSteering = std::min(std::max(curSteering, 0.5), 1.5);
        _steering = 0.5 *_steering + 0.5 * curSteering;
        nextInt *= _steering;
        Serial.printf("New Int: %d | CurSteering: %f | Steering: %f\n", nextInt, curSteering, _steering);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;

    return nextInt;
}

uint32_t ClockCatchUp3::getCatchupOffset() const {
    return _catchupOffset;
}

void StepSync::resetCatchupOffset() {
	_catchupOffset = 0;
}
