#pragma once

#include "RGBWWCtrl.h"

class IMasterClockSink;


class AppMqttClient{

public:
	AppMqttClient();
	virtual ~AppMqttClient();

	void init();
	void start();
	void stop();
	bool isRunning() const;

	void publishCurrentHsv(const HSVCT& color);
    void publishCurrentRaw(const ChannelOutput& color);
    void publishClock(uint32_t steps);
    void publishCommand(const String& method, const JsonObject& params);

private:
	void connectDelayed(int delay = 2000);
    void connect();
    void onComplete(TcpClient& client, bool success);
    void onMessageReceived(String topic, String message);
    void publish(const String& topic, const String& data, bool retain);

    String buildTopic(const String& suffix);

	MqttClient* mqtt = nullptr;
	bool _running = false;
	Timer _procTimer;
	String _id;
};
