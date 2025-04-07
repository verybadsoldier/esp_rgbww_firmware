#pragma once

#include "RGBWWCtrl.h"

class IMasterClockSink;


class AppMqttClient{

public:
    AppMqttClient();
    virtual ~AppMqttClient();

    void init(String mqttDeviceId);
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

    void initHomeAssistant();
    void handleHomeAssistantCommand(const String& message);

private:
    void connectDelayed(int delay = 2000);
    void connect();
    void onComplete(TcpClient& client, bool success);
    int onMessageReceived(MqttClient& client, mqtt_message_t* message);
    void publish(const String& topic, const String& data, bool retain);

    String buildTopic(const String& suffix);

    // Home Assistant specific
    bool _haEnabled = false;
    String _haDiscoveryPrefix;
    String _haNodeId;
    bool _haConfigPublished = false;
    
    // Home Assistant methods
    void publishHomeAssistantConfig();
    String buildHATopic(const String& component, const String& entityId, const String& suffix);
    void publishHAState(const ChannelOutput& raw, const HSVCT* pHsv);
    void processHACommand(const String& message);


    MqttClient* mqtt = nullptr;
    bool _running = false;
    Timer _procTimer;
    String _id;
    bool _firstClock = true;

    HSVCT _lastHsv;
    ChannelOutput _lastRaw;
};
