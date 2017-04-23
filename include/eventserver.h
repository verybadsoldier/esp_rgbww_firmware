#pragma once

#include <Wiring/WVector.h>

#include "jsonrpcmessage.h"

class EventServer : public TcpServer{
public:
	virtual ~EventServer();
	void start();
	void stop();
	inline bool isRunning() { return _running; };

	void publishCurrentHsv(const HSVCT& color);
	void publishCurrentRaw(const ChannelOutput& raw);
	void publishTransitionComplete(const String& name);
	void publishKeepAlive();
	void publishClockSlaveStatus(uint32_t offset, uint32_t interval);

private:
	virtual void onClient(TcpClient *client) override;
	virtual void onClientComplete(TcpClient& client, bool succesfull) override;

	void sendToClients(JsonRpcMessage& rpcMsg);

	static const int _tcpPort = 9090;
	static const int _connectionTimeout = 120;
	static const int _keepAliveInterval = 60;

    bool _running = false;
    Timer _keepAliveTimer;
	Vector<TcpClient*> _clients;
	int _nextId = 1;

	HSVCT _lastHsv;
	ChannelOutput _lastRaw;
};
