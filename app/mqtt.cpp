/**
 * @file
 * @author  Patrick Jahns http://github.com/patrickjahns
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 *
 */
#include <RGBWWCtrl.h>

AppMqttClient::AppMqttClient() {
    _id = String("rgbww_") + WifiStation.getMAC();
}

AppMqttClient::~AppMqttClient() {
    delete mqtt;
    mqtt = nullptr;
}

void AppMqttClient::onComplete(TcpClient& client, bool success) {
    if (success == true)
        debug_i("MQTT Broker Disconnected!!");
    else
        debug_e("MQTT Broker Unreachable!!");

    // Restart connection attempt after few seconds
    connectDelayed(2000);
}

void AppMqttClient::connectDelayed(int delay) {
    debug_d("MQTT::connectDelayed");
    _procTimer.initializeMs(delay, TimerDelegate(&AppMqttClient::connect, this)).startOnce();
}

void AppMqttClient::connect() {
    if (!mqtt || mqtt->getConnectionState() == TcpClientState::eTCS_Connected || mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
        return;

    debug_d("MQTT::connect ID: %s\n", _id.c_str());
    if(!mqtt->setWill("last/will","The connection from this device is lost:(", 1, true)) {
        debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
    }
    mqtt->connect(_id, app.cfg.network.mqtt.username, app.cfg.network.mqtt.password, false);
#ifdef ENABLE_SSL
    mqtt->addSslOptions(SSL_SERVER_VERIFY_LATER);

#include <ssl/private_key.h>
#include <ssl/cert.h>

    mqtt->setSslClientKeyCert(default_private_key, default_private_key_len,
            default_certificate, default_certificate_len, NULL, true);

#endif
    // Assign a disconnect callback function
    mqtt->setCompleteDelegate(TcpClientCompleteDelegate(&AppMqttClient::onComplete, this));

    if (app.cfg.sync.clock_slave_enabled) {
        mqtt->subscribe(app.cfg.sync.clock_slave_topic);
    }
    if (app.cfg.sync.cmd_slave_enabled) {
        mqtt->subscribe(app.cfg.sync.cmd_slave_topic);
    }
    if (app.cfg.sync.color_slave_enabled) {
        debug_d("Subscribe: %s\n", app.cfg.sync.color_slave_topic.c_str());
        mqtt->subscribe(app.cfg.sync.color_slave_topic);
    }

    if (app.cfg.sync.clock_master_enabled) {
        this->publishClockReset();
    }
}

void AppMqttClient::init() {
    if (app.cfg.general.device_name.length() > 0) {
        _id = app.cfg.general.device_name;
    }
}

void AppMqttClient::start() {
    debug_i("Start MQTT");

    delete mqtt;
    debug_i("MqttClient: Server: %s Port: %d\n", app.cfg.network.mqtt.server.c_str(), app.cfg.network.mqtt.port);
    mqtt = new MqttClient(app.cfg.network.mqtt.server, app.cfg.network.mqtt.port, MqttStringSubscriptionCallback(&AppMqttClient::onMessageReceived, this));
    connectDelayed(2000);
}

void AppMqttClient::stop() {
    delete mqtt;
    mqtt = nullptr;
}

bool AppMqttClient::isRunning() const {
    return (mqtt != nullptr);
}

void AppMqttClient::onMessageReceived(String topic, String message) {
    if (app.cfg.sync.clock_slave_enabled && (topic == app.cfg.sync.clock_slave_topic)) {
        if (message == "reset") {
            app.rgbwwctrl.onMasterClockReset();
        }
        else  {
            uint32_t clock = message.toInt();
            app.rgbwwctrl.onMasterClock(clock);
        }
    }
    else if (app.cfg.sync.cmd_slave_enabled && topic == app.cfg.sync.cmd_slave_topic) {
        app.jsonproc.onJsonRpc(message);
    }
    else if (app.cfg.sync.color_slave_enabled && (topic == app.cfg.sync.color_slave_topic)) {
        String error;
        app.jsonproc.onColor(message, error, false);
    }
}

void AppMqttClient::publish(const String& topic, const String& data, bool retain) {
    //Serial.printf("AppMqttClient::publish: Topic: %s | Data: %s\n", topic.c_str(), data.c_str());

    if (!mqtt) {
        debug_w("ApplicationMQTTClient::publish: no MQTT object\n");
        return;
    }

    TcpClientState state = mqtt->getConnectionState();
    if (state == TcpClientState::eTCS_Connected) {
        mqtt->publish(topic, data, retain);
    }
    else {
        debug_w("ApplicationMQTTClient::publish: not connected.\n");
    }
}

void AppMqttClient::publishCurrentRaw(const ChannelOutput& raw) {
    if (raw == _lastRaw)
        return;
    _lastRaw = raw;

    debug_d("ApplicationMQTTClient::publishCurrentRaw\n");

    DynamicJsonBuffer jsonBuffer(200);
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& rawJson = root.createNestedObject("raw");
    rawJson["r"] = raw.r;
    rawJson["g"] = raw.g;
    rawJson["b"] = raw.b;
    rawJson["cw"] = raw.cw;
    rawJson["ww"] = raw.ww;

    root["t"] = 0;
    root["cmd"] = "solid";

    String jsonMsg;
    root.printTo(jsonMsg);
    publish(buildTopic("color"), jsonMsg, true);
}

void AppMqttClient::publishCurrentHsv(const HSVCT& color) {
    if (color == _lastHsv)
        return;
    _lastHsv = color;

    debug_d("ApplicationMQTTClient::publishCurrentHsv\n");

    float h, s, v;
    int ct;
    color.asRadian(h, s, v, ct);

    DynamicJsonBuffer jsonBuffer(200);
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& hsv = root.createNestedObject("hsv");
    hsv["h"] = h;
    hsv["s"] = s;
    hsv["v"] = v;
    hsv["ct"] = ct;

    root["t"] = 0;
    root["cmd"] = "solid";

    String jsonMsg;
    root.printTo(jsonMsg);
    publish(buildTopic("color"), jsonMsg, true);
}

String AppMqttClient::buildTopic(const String& suffix) {
    String topic = app.cfg.network.mqtt.topic_base;
    topic += _id + "/";
    return topic + suffix;
}

void AppMqttClient::publishClock(uint32_t steps) {
    String msg;
    msg += steps;

    String topic = buildTopic("clock");
    publish(topic, msg, false);
}

void AppMqttClient::publishClockReset() {
    String topic = buildTopic("clock");
    publish(topic, "reset", false);
}

void AppMqttClient::publishClockInterval(uint32_t curInterval) {
    String msg;
    msg += curInterval;

    String topic = buildTopic("clock_interval");
    publish(topic, msg, false);
}

void AppMqttClient::publishClockSlaveOffset(uint32_t offset) {
    String msg;
    msg += offset;

    String topic = buildTopic("clock_slave_offset");
    publish(topic, msg, false);
}

void AppMqttClient::publishCommand(const String& method, const JsonObject& params) {
    debug_d("ApplicationMQTTClient::publishCommand: %s\n", method.c_str());

    JsonRpcMessage msg(method);

    String topic = buildTopic("command");

    if (params.size() > 0)
        msg.getRoot()["params"] = params;

    String msgStr;
    msg.getRoot().printTo(msgStr);
    publish(topic, msgStr, false);
}

void AppMqttClient::publishTransitionFinished(const String& name, bool requeued) {
    debug_d("ApplicationMQTTClient::publishTransitionFinished: %s\n", name.c_str());

    DynamicJsonBuffer jsonBuffer(200);
    JsonObject& root = jsonBuffer.createObject();
    root["name"] = name;
    root["requequed"] = requeued;

    String jsonMsg;
    root.printTo(jsonMsg);
    publish(buildTopic("transition_finished"), jsonMsg, true);
}
