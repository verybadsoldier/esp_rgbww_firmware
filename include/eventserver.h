#pragma once

#include <Wiring/WVector.h>

#include "jsonrpcmessage.h"

class EventServer : public TcpServer{
public:
	virtual ~EventServer();
	void start();
	void stop();

	void publishCurrentState(const ChannelOutput& raw, const HSVCT* pColor = NULL);
	void publishTransitionFinished(const String& name, bool requeued = false);
	void publishKeepAlive();
	void publishClockSlaveStatus(int offset, uint32_t interval);

private:
	virtual void onClient(TcpClient *client) override;
	virtual void onClientComplete(TcpClient& client, bool succesfull) override;

	void sendToClients(JsonRpcMessage& rpcMsg);

	static const int _tcpPort = 9090;
	static const int _connectionTimeout = 120;
	static const int _keepAliveInterval = 60;

    Timer _keepAliveTimer;
	int _nextId = 1;

	ChannelOutput _lastRaw;
};
