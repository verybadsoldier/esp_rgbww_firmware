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
#ifndef APP_LEDCTRL_H_
#define APP_LEDCTRL_H_

#include <limits>

#include "mqtt.h"

#define APP_COLOR_FILE ".color"

struct ColorStorage {
	int h = 0;
	int s = 0;
	int v = 0;
	int ct = 0;

	void load(bool print = false) {
		StaticJsonBuffer < 72 > jsonBuffer;
		if (exist()) {
			int size = fileGetSize(APP_COLOR_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(APP_COLOR_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);
			h = root["h"];
			s = root["s"];
			v = root["v"];
			ct = root["ct"];
			if (print) {
				root.prettyPrintTo(Serial);
			}
			delete[] jsonString;
		}
	}

	void save(bool print = false) {
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		root["h"] = h;
		root["s"] = s;
		root["v"] = v;
		root["ct"] = ct;
		String rootString;
		if (print) {
			root.prettyPrintTo(Serial);
		}
		root.printTo(rootString);
		fileSetContent(APP_COLOR_FILE, rootString);
	}
	bool exist() {
		return fileExist(APP_COLOR_FILE);
	}
};

typedef Delegate<bool(void)> ledctrlDelegate;

class IMasterClockSink {
public:
    virtual void onMasterClock(uint32_t steps) = 0;
};

class StepSync {
public:
    virtual void onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster) = 0;

protected:
    template<typename T>
    static T calcOverflowVal(T prevValue, T curValue) {
        if (curValue < prevValue) {
            //overflow
            return std::numeric_limits<T>::max() - prevValue + curValue;
        }
        else {
            return curValue - prevValue;
        }
    }
};

class ClockAdaption : public StepSync {
public:
    virtual void onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster);

private:
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
};

class ClockCatchUp : public StepSync {
public:
    virtual void onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster);

private:
    int _catchupOffset = 0;
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
};

class ClockCatchUp2 : public StepSync {
public:
    virtual void onMasterClock(Timer& timer, uint32_t stepsCurrent, uint32_t stepsMaster);

private:
    int _catchupOffset = 0;
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
    uint32_t _baseInt = RGBWW_MINTIMEDIFF * 1000;
};

class APPLedCtrl: public RGBWWLed, IMasterClockSink {

public:
	void init(const ApplicationSettings& cfg, ApplicationMQTTClient& mqtt);
	void setup();

	void start();
	void stop();
	void color_save();
	void color_reset();
	void test_channels();

	void show_led();
	virtual void onMasterClock(uint32_t steps);
	static void led_callback(RGBWWLed* rgbwwctrl, RGBWWLedAnimation* anim);

private:
	ColorStorage color;
	Timer ledTimer;
    ApplicationSettings const * _cfg;
    ApplicationMQTTClient* _mqtt;
    StepSync* _stepSync = nullptr;

    uint32_t _stepCounter = 0;
};

#endif
