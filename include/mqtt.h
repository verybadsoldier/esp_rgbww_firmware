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
    void publishCurrentRaw(const ChannelOutput& raw);
    void publishClock(uint32_t steps);
    void publishClockReset();
    void publishClockInterval(uint32_t curInterval);
    void publishClockSlaveOffset(int offset);
    void publishCommand(const String& method, const JsonObject& params);
    void publishTransitionFinished(const String& name, bool requeued);

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
    bool _firstClock = true;

    HSVCT _lastHsv;
    ChannelOutput _lastRaw;
};
