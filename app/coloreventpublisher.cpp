#include "coloreventpublisher.h"


ColorEventPublisher::~ColorEventPublisher() {
	stop();
}

void ColorEventPublisher::init(EventServer& evServer, RGBWWLed& rgbctrl) {
	_eventServer = &evServer;
	_rgbCtrl = &rgbctrl;
}

void ColorEventPublisher::start() {
	if (_running)
		return;

	Serial.printf("Starting ColorEventPublisher\n");

	_running = true;

	Serial.printf("Starting event timer\n");
	_ledTimer.initializeMs(2000, TimerDelegate(&ColorEventPublisher::publishCurrentColor, this)).start();
}

void ColorEventPublisher::stop() {
	if (not _running)
		return;

	_ledTimer.stop();

	_running = false;
}

void ColorEventPublisher::publishCurrentColor() {
	const HSVCT& curColor = _rgbCtrl->getCurrentColor();

	if (curColor == _lastColor)
		return;

	_eventServer->publishCurrentColor(curColor);
	_lastColor = curColor;
}
