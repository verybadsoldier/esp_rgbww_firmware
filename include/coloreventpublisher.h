#pragma once

class ColorEventPublisher {
public:
	virtual ~ColorEventPublisher();

	void start();
	void stop();
	void init(EventServer& eventServer, RGBWWLed& rgbctrl, ApplicationSettings& cfg);
	inline bool isRunning() { return _running; };

private:
	void publishCurrentColor();
	Timer _ledTimer;

	RGBWWLed* _rgbCtrl = nullptr;;
	EventServer* _eventServer;
	ApplicationSettings const * _cfg = nullptr;

	HSVCT _lastColor;
	bool _running = false;
};
