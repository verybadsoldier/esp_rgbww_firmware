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


void APPLedCtrl::init() {
	debugapp("APPLedCtrl::init");

    _stepSync = new ClockCatchUp3();
	//_stepSync = new ClockAdaption();
	RGBWWLed::init(REDPIN, GREENPIN, BLUEPIN, WWPIN, CWPIN, PWM_FREQUENCY);

	setup();
	color.load();
	debugapp("H: %i | s: %i | v: %i | ct: %i", color.h, color.s, color.v, color.ct);
	HSVCT s = HSVCT(color.h, color.s, 0, color.ct);
	HSVCT c = HSVCT(color.h, color.s, color.v, color.ct);
	fadeHSV(s, c, 700); //fade to color in 700ms
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
	switch(_mode) {
	case ColorMode::Hsv:
        app.eventserver.publishCurrentHsv(getCurrentColor());
	    break;
    case ColorMode::Raw:
        app.eventserver.publishCurrentRaw(getCurrentOutput());
        break;
	}
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

void APPLedCtrl::updateLed() {
	const bool animFinished = show();

    ++_stepCounter;

	if (app.cfg.sync.clock_master_enabled) {
        if ((_stepCounter % (app.cfg.sync.clock_master_interval * RGBWW_UPDATEFREQUENCY)) == 0) {
            Serial.printf("Send Master Clock\n");
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
}

void APPLedCtrl::onMasterClock(uint32_t stepsMaster) {
    if (_stepSync) {
        _stepSync->onMasterClock(ledTimer, _stepCounter, stepsMaster);
    }
}

void APPLedCtrl::start() {
	debugapp("APPLedCtrl::start");
	ledTimer.initializeMs(RGBWW_MINTIMEDIFF, TimerDelegate(&APPLedCtrl::updateLed, this)).start();
}

void APPLedCtrl::stop() {
	debugapp("APPLedCtrl::stop");
	ledTimer.stop();
}

void APPLedCtrl::colorSave() {
	debugapp("APPLedCtrl::colorSave");
	HSVCT c = getCurrentColor();
	color.h = c.h;
	color.s = c.s;
	color.v = c.v;
	color.ct = c.ct;
	color.save();
}

void APPLedCtrl::colorReset() {
	debugapp("APPLedCtrl::colorReset");
	color.h = 0;
	color.s = 0;
	color.v = 0;
	color.ct = 0;
	color.save();
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

void APPLedCtrl::onAnimationFinished(RGBWWLedAnimation* anim) {
	debugapp("APPLedCtrl::onAnimationFinished");
	app.rgbwwctrl.colorSave();

	app.eventserver.publishTransitionComplete(anim->getName());
//	app.eventserver.publishCurrentHsv(getCurrentColor());
//
//	app.mqttclient.publishCurrentHsv(getCurrentColor());
}

void ClockCatchUp::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        _catchupOffset += masterDiff - diff;

        // if _catchupOffset is positive then we are too slow

        double perc = static_cast<double>(_catchupOffset) / masterDiff;
        uint32_t newInt = (1000 * RGBWW_MINTIMEDIFF) * (1.0 - perc);
        Serial.printf("Step diff to master: %d Perc: %f | Catchup Offset: %d | New interval: %d us\n", masterDiff, perc, _catchupOffset, newInt);
        timer.setIntervalUs(newInt);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;
}

void ClockCatchUp3::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        int curDiff = masterDiff - diff;
        _catchupOffset += curDiff;
        Serial.printf("Step Diff: %d | Master Step Diff: %d | Catchup: %d\n", diff, masterDiff, _catchupOffset);

        const uint32_t curInt = timer.getIntervalUs();
        Serial.printf("Current Interval: %d | Current Base Interval: %d\n", curInt, _baseInt);

        // update base interval
        const int diffCorrected = ((curInt - _steering)/ static_cast<double>(_constBaseInt)) * diff; // normalize towards current interval
        const int calcNewBase = curInt * (diffCorrected / static_cast<double>(masterDiff));
        _baseInt =  (0.9 * _baseInt) + (0.1 * calcNewBase);
        Serial.printf("New Base Interval: %d | Cal New Base: %d\n", _baseInt, calcNewBase);

        double perc = static_cast<double>(_catchupOffset) / _baseInt;
        perc = std::max(-0.2, std::min(perc, 0.2));
        _steering = static_cast<int>((perc / 2.0) * _baseInt + 0.5f);
        uint32_t newInt = _baseInt + _steering;

        Serial.printf("New Int: %d | Perc: %f | Steering: %d\n", newInt, perc, _steering);
        timer.setIntervalUs(newInt);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;
}

void ClockCatchUpSteering::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        int curDiff = masterDiff - diff;
        _catchupOffset += curDiff;
        Serial.printf("Step Diff: %d | Master Step Diff: %d | Catchup: %d\n", diff, masterDiff, _catchupOffset);

        double perc = 1 + static_cast<double>(_catchupOffset) / masterDiff;
        _steering = (_steering * 0.9 + 0.1 * _steering * std::max(1.01, std::min(0.99, perc)));
        uint32_t newInt = _constBaseInt * _steering;
        Serial.printf("New Int: %d | Perc: %f | Steering: %d\n", newInt, perc, _steering);
        timer.setIntervalUs(newInt);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;
}

void ClockCatchUp2::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        int curDiff = masterDiff - diff;
        _catchupOffset += curDiff;

        double perc = static_cast<double>(_catchupOffset) / masterDiff;
        uint32_t curInt = timer.getIntervalUs();
        uint32_t newInt = (0.9 * curInt) + (0.1 * curInt * (1.0 - perc));

        Serial.printf("Step diff to master: %d Perc: %f | Current Diff: %d | Catchup Offset: %d | New interval: %d us\n", masterDiff, perc, curDiff, _catchupOffset, newInt);
        timer.setIntervalUs(newInt);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;
}

void ClockAdaption::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        if (masterDiff < 5000) {
            return;
        }

        int diffDiff = masterDiff - diff;
        double diffPerc = static_cast<double>(diffDiff) / masterDiff;
        double absDiffPerc = (static_cast<double>(abs(diffDiff)) / static_cast<double>(masterDiff));
        Serial.printf("Master: %d MasterPrev: %d DiffMaster: %d DiffSelf: %d | Perc: %f\n", stepsMaster, _stepsSyncMasterLast, masterDiff, diff, diffPerc);

        uint32_t curInt = timer.getIntervalUs();
        uint32_t newInt = (1.0 - diffPerc/2) * curInt;
        Serial.printf("Step diff to master: %d Cur Int: %d New interval: %d us\n", masterDiff, curInt, newInt);
        timer.setIntervalUs(newInt);
    }

    _stepsSyncMasterLast = stepsMaster;
    _stepsSyncLast = stepsCurrent;
    _firstMasterSync = false;
}
