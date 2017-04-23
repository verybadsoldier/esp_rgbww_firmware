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

class StepSync;

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

	void updateLed();
	void onMasterClock(uint32_t steps);
	virtual void onAnimationFinished(RGBWWLedAnimation* anim);
private:
    static void updateLedCb(void* pTimerArg);
    void publishToEventServer();
    void publishToMqtt();

	ColorStorage color;

    StepSync* _stepSync = nullptr;

    uint32_t _stepCounter = 0;
    HSVCT _prevColor;
    ChannelOutput _prevOutput;

    ETSTimer _ledTimer;
    uint32_t _timerInterval = RGBWW_MINTIMEDIFF;
};


class StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) = 0;
    virtual uint32_t getCatchupOffset() const;
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


class ClockCatchUp3 : public StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) override;
    virtual uint32_t getCatchupOffset() const;

private:
    int _catchupOffset = 0;
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
    double _steering = 1.0;
    const uint32_t _constBaseInt = RGBWW_MINTIMEDIFF * 1000;
};
