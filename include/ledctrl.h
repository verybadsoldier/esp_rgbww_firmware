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

struct PinConfig {
	PinConfig() : red(13), green(12), blue(14), warmwhite(5), coldwhite(4) {}

	int red;
	int green;
	int blue;
	int warmwhite;
	int coldwhite;
};

struct ColorStorage {
    HSVCT current;

	void load(bool print = false) {
		StaticJsonBuffer < 72 > jsonBuffer;
		if (exist()) {
			int size = fileGetSize(APP_COLOR_FILE);
			char* jsonString = new char[size + 1];
			fileGetContent(APP_COLOR_FILE, jsonString, size + 1);
			JsonObject& root = jsonBuffer.parseObject(jsonString);
			current.h = root["h"];
			current.s = root["s"];
			current.v = root["v"];
			current.ct = root["ct"];
			if (print) {
				root.prettyPrintTo(Serial);
			}
			delete[] jsonString;
		}
	}

	void save(bool print = false) {
	    debugapp("Saving ColorStorage to file...");
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		root["h"] = current.h;
		root["s"] = current.s;
		root["v"] = current.v;
		root["ct"] = current.ct;
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
	virtual void onAnimationFinished(const String& name, bool requeued);
private:
	static PinConfig parsePinConfigString(String& pinStr);
    static void updateLedCb(void* pTimerArg);
    void publishToEventServer();
    void publishToMqtt();
    void publishFinishedStepAnimations();
    void publishColorStayedCmds();
    void checkStableColorState();

	ColorStorage colorStorage;

    StepSync* _stepSync = nullptr;

    uint32_t _stepCounter = 0;
    HSVCT _prevColor;
    uint32_t _numStableColorSteps = 0;
    ChannelOutput _prevOutput;

    static const uint32_t _saveAfterStableColorMs = 2000;

    ETSTimer _ledTimer;
    uint32_t _timerInterval = RGBWW_MINTIMEDIFF_US;
    HashMap<String, bool> _stepFinishedAnimations;
};


class StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) = 0;
    virtual uint32_t getCatchupOffset() const;
    void resetCatchupOffset();

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

    int _catchupOffset = 0;
};


class ClockCatchUp3 : public StepSync {
public:
    virtual uint32_t onMasterClock(uint32_t stepsCurrent, uint32_t stepsMaster) override;
    virtual uint32_t getCatchupOffset() const;

private:
    uint32_t _stepsSyncMasterLast = 0;
    uint32_t _stepsSyncLast = 0;
    bool _firstMasterSync = true;
    double _steering = 1.0;
    const uint32_t _constBaseInt = RGBWW_MINTIMEDIFF_US;
};
