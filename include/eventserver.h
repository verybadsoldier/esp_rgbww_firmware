#pragma once

#include <SmingCore/SmingCore.h>
#include <Wiring/WVector.h>

#include <RGBWWLed/RGBWWLed.h>

class TcpConnection;

class JsonRpcMessage {
public:
    JsonRpcMessage(const String& name);
    JsonObjectStream& getStream();
    void setId(int id);
    JsonObject& getParams();

private:
    const String _data;

    JsonObjectStream _stream;
    JsonObject* _pParams;
};

class EventServer : public TcpServer{
public:
	virtual ~EventServer();
	void start();
	void stop();
	inline bool isRunning() { return _running; };

	void publishCurrentColor(const HSVCT& color);
	void publishTransitionComplete();
	void publishKeepAlive();

private:
	virtual void onClient(TcpClient *client) override;
	virtual void onClientComplete(TcpClient& client, bool succesfull) override;

	void sendToClients(JsonRpcMessage& rpcMsg);

	static const int _tcpPort = 9090;
	static const int _keepAliveInterval = 60;

    bool _running = false;
    Timer _keepAliveTimer;
	Vector<TcpClient*> _clients;
	Vector<int*> _tests;
	int _nextId = 1;
};

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
