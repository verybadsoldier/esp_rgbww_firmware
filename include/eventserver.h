#pragma once

#include <memory>
#include <set>

#include <SmingCore/SmingCore.h>
#include <Wiring/WVector.h>

#include <RGBWWLed/RGBWWLed.h>

class TcpConnection;

class EventServer: private TcpServer {
public:
	virtual ~EventServer();
	void start();
	void stop();
	inline bool isRunning() { return _running; };

	void publishCurrentColor(const HSVCT& color);

private:
	virtual void onClient(TcpClient *client) override;
	virtual void onClientComplete(TcpClient& client, bool succesfull) override;

	void sendToClients(IDataSourceStream* stream);

	bool _running = false;
	static const int _tcpPort = 9452;

	Vector<TcpClient*> _clients;

	TcpClient* _client = nullptr;
};

class ColorEventPublisher {
public:
	virtual ~ColorEventPublisher();

	void start();
	void stop();
	void init(EventServer& eventServer, RGBWWLed& rgbctrl);
	inline bool isRunning() { return _running; };

private:
	Timer _ledTimer;

	RGBWWLed* _rgbCtrl;
	EventServer* _eventServer;

	HSVCT _lastColor;
};
