#pragma once

class IMasterClockSink;


class ApplicationMQTTClient{

public:
	ApplicationMQTTClient();
	virtual ~ApplicationMQTTClient();

	void start();
	void stop();
	bool isRunning() const;

	void publishCurrentColor(const HSVCT& color);
    void publishClock(uint32_t steps);
    void setMasterClockSink(IMasterClockSink* pSink);

private:
	void connectDelayed(int delay = 2000);
    void connect();
    void onComplete(TcpClient& client, bool success);
    void onMessageReceived(String topic, String message);
    void publish(const String& topic, const String& data, bool retain);


	MqttClient* mqtt = nullptr;
	bool _running = false;
	Timer _procTimer;
	String _topicPrefix;
	String _id;
	IMasterClockSink* _masterClockSink = nullptr;
};
