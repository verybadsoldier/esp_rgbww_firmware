#pragma once

#include <memory>

#include <Wiring/WVector.h>

#include "jsonrpcmessage.h"

class EventServer : public TcpServer{
public:
	EventServer(std::function<void()> connectCallback);
	virtual ~EventServer();
	void start();
	void stop();

	void publishColorEvent(const ChannelOutput& raw, const HSVCT* pColor = NULL, bool force=false);
	void publishTransitionFinished(const String& name, bool requeued = false);
	void publishKeepAlive();
	void publishClockSlaveStatus(int offset, uint32_t interval);
	void publishConfigEvent(const DynamicJsonDocument& config);
	void publishInfo(std::shared_ptr<JsonObjectStream> pInfo);
	void publishStateCompleted();

private:
	virtual void onClient(TcpClient *client) override;
	virtual void onClientComplete(TcpClient& client, bool succesfull) override;

	void sendToClients(JsonRpcMessage& rpcMsg); 

	static const int _tcpPort = 9090;
	static const int _connectionTimeout = 120;
	static const int _keepAliveInterval = 60;

	std::function<void()> _connectCallback;

    Timer _keepAliveTimer;
	int _nextId = 1;

	ChannelOutput _lastRaw;
	HSVCT _lastHsv;
};
