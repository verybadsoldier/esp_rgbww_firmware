#pragma once

#include <SmingCore/SmingCore.h>
#include <Wiring/WVector.h>

#include <RGBWWLed/RGBWWLed.h>

#include "eventserver.h"


class ColorEventPublisher {
public:
	virtual ~ColorEventPublisher();

	void start();
	void stop();
	void init(EventServer& eventServer, RGBWWLed& rgbctrl);
	inline bool isRunning() { return _running; };

private:
	void publishCurrentColor();
	Timer _ledTimer;

	RGBWWLed* _rgbCtrl;
	EventServer* _eventServer;

	HSVCT _lastColor;
	bool _running = false;
};
