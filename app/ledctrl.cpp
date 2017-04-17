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


void APPLedCtrl::init(const ApplicationSettings& cfg, ApplicationMQTTClient& mqtt) {
	debugapp("APPLedCtrl::init");
	_cfg = &cfg;
	_mqtt = &mqtt;

    _stepSync = new ClockCatchUp();
	//_stepSync = new ClockAdaption();
	RGBWWLed::init(REDPIN, GREENPIN, BLUEPIN, WWPIN, CWPIN, PWM_FREQUENCY);
	setAnimationCallback(led_callback);
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

void APPLedCtrl::show_led() {
	show();

    ++_stepCounter;

	if (_cfg->sync.clockSendEnabled) {
        if ((_stepCounter % (_cfg->sync.clockSendInterval * RGBWW_UPDATEFREQUENCY)) == 0) {
            Serial.printf("Send Master Clock\n");
            _mqtt->publishClock(_stepCounter);
        }
	}
}

void APPLedCtrl::onMasterClock(uint32_t stepsMaster) {
    if (_stepSync) {
        _stepSync->onMasterClock(ledTimer, _stepCounter, stepsMaster);
    }
}

void APPLedCtrl::start() {
	debugapp("APPLedCtrl::start");
	ledTimer.initializeMs(RGBWW_MINTIMEDIFF, TimerDelegate(&APPLedCtrl::show_led, this)).start();
}

void APPLedCtrl::stop() {
	debugapp("APPLedCtrl::stop");
	ledTimer.stop();
}

void APPLedCtrl::color_save() {
	debugapp("APPLedCtrl::save_color");
	HSVCT c = getCurrentColor();
	color.h = c.h;
	color.s = c.s;
	color.v = c.v;
	color.ct = c.ct;
	color.save();
}

void APPLedCtrl::color_reset() {
	debugapp("APPLedCtrl::reset_color");
	color.h = 0;
	color.s = 0;
	color.v = 0;
	color.ct = 0;
	color.save();
}

void APPLedCtrl::test_channels() {
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

void APPLedCtrl::led_callback(RGBWWLed* rgbwwctrl, RGBWWLedAnimation* anim) {
	debugapp("APPLedCtrl::led_callback");
	app.rgbwwctrl.color_save();

	app.eventserver.publishTransitionComplete(anim->getName());
	app.eventserver.publishCurrentColor(app.rgbwwctrl.getCurrentColor());

	//if (_cfg.network.mqtt.enabled)
	app.mqttclient.publishCurrentColor(app.rgbwwctrl.getCurrentColor());
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

void ClockAdaption::onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) {
    if (!_firstMasterSync) {
        int diff = StepSync::calcOverflowVal(_stepsSyncLast, stepsCurrent);
        int masterDiff = StepSync::calcOverflowVal(_stepsSyncMasterLast, stepsMaster);

        if (masterDiff < 60000) {
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
