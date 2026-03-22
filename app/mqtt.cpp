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

#include "homeassistant.h"

AppMqttClient::AppMqttClient() {}

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
    if (!mqtt || mqtt->getConnectionState() == TcpClientState::eTCS_Connected ||
        mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
        return;

    debug_d("MQTT::connect ID: %s\n", _id.c_str());
    if (!mqtt->setWill(F("last/will"), F("The connection from this device is lost:("), 1, true)) {
        debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
    }
    //    0);app.cfg.network.mqtt.username, app.cfg.network.mqtt.password);
    // debug_i("MqttClient: Server: %s Port: %d\n", app.cfg.network.mqtt.server.c_str(), app.cfg.network.mqtt.port);

    Url url = "mqtt://" + app.cfg.network.mqtt.username + ":" + app.cfg.network.mqtt.password + "@" +
              app.cfg.network.mqtt.server + ":" + String(app.cfg.network.mqtt.port);
    mqtt->connect(url, _id, 0);
#ifdef ENABLE_SSL
    // not need i guess? mqtt->addSslOptions(SSL_SERVER_VERIFY_LATER);

#include <ssl/cert.h>
#include <ssl/private_key.h>

    mqtt->setSslKeyCert(default_private_key, default_private_key_len, default_certificate, default_certificate_len,
                        NULL, true);

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

    if (app.cfg.network.mqtt.homeassistant_discovery_enabled) {
        mqtt->subscribe(HOMEASSISTANT_STATUS_TOPIC);
    }
}

void AppMqttClient::init() {
    if (app.cfg.general.device_name.length() > 0) {
        debug_w("AppMqttClient::init: building MQTT ID from device name: '%s'\n", app.cfg.general.device_name.c_str());
        _id = app.cfg.general.device_name;
    } else {
        debug_w("AppMqttClient::init: building MQTT ID from MAC (device name is: '%s')\n",
                app.cfg.general.device_name.c_str());
        _id = String("rgbww_") + WifiStation.getMAC();
    }
}

void AppMqttClient::start() {
    debug_i("Start MQTT");

    delete mqtt;
    mqtt = new MqttClient();
    mqtt->setCallback(MqttStringSubscriptionCallback(&AppMqttClient::onMessageReceived, this));
    mqtt->setConnectedHandler(MqttDelegate(&AppMqttClient::onMqttConnected, this));

    connectDelayed(2000);
}

void AppMqttClient::stop() {
    delete mqtt;
    mqtt = nullptr;
}

bool AppMqttClient::isConnected() const {
    return (mqtt != nullptr && mqtt->getConnectionState() == TcpClientState::eTCS_Connected);
}

void AppMqttClient::onMessageReceived(String topic, String message) {
    if (app.cfg.sync.clock_slave_enabled && (topic == app.cfg.sync.clock_slave_topic)) {
        if (message == F("reset")) {
            app.rgbwwctrl.onMasterClockReset();
        } else {
            uint32_t clock = message.toInt();
            app.rgbwwctrl.onMasterClock(clock);
        }
    } else if (app.cfg.sync.cmd_slave_enabled && topic == app.cfg.sync.cmd_slave_topic) {
        String errorMsg;
        app.jsonproc.onJsonRpc(message, errorMsg);
        // todo: handle errorMsg
    } else if (app.cfg.sync.color_slave_enabled && (topic == app.cfg.sync.color_slave_topic)) {
        String error;
        app.jsonproc.onColor(message, error);
    } else if (topic == HOMEASSISTANT_STATUS_TOPIC) {
        if (message == F("online")) {
            this->publishHomeAssistantDiscovery();
        }
    }
}

int AppMqttClient::onMqttConnected(MqttClient& client, mqtt_message_t* message) {
    debug_i("MQTT Connected\n");
    return app.onMqttConnected(client, message);
}

void AppMqttClient::publish(const String& topic, const String& data, bool retain) {
    if (!mqtt) {
        debug_w("AppMqttClient::publish: no MQTT object\n");
        return;
    }

    TcpClientState state = mqtt->getConnectionState();
    if (state == TcpClientState::eTCS_Connected) {
        mqtt->publish(topic, data, retain);
    } else {
        debug_w("AppMqttClient::publish: not connected.\n");
    }
}

void AppMqttClient::publishCurrentRaw(const ChannelOutput& raw) {
    if (!mqtt) {
        return;
    }

    if (raw == _lastRaw)
        return;
    _lastRaw = raw;

    debug_d("AppMqttClient::publishCurrentRaw\n");

    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    JsonObject rawJson = root.createNestedObject(F("raw"));
    rawJson[F("r")] = raw.r;
    rawJson[F("g")] = raw.g;
    rawJson[F("b")] = raw.b;
    rawJson[F("cw")] = raw.cw;
    rawJson[F("ww")] = raw.ww;

    root[F("t")] = 0;
    root[F("cmd")] = F("solid");

    String jsonMsg = Json::serialize(root);
    publish(buildTopic(F("color")), jsonMsg, true);
}

void AppMqttClient::publishCurrentHsv(const HSVCT& color) {
    if (!mqtt) {
        return;
    }

    if (color == _lastHsv)
        return;
    _lastHsv = color;

    debug_d("AppMqttClient::publishCurrentHsv\n");

    float h, s, v;
    int ct;
    color.asRadian(h, s, v, ct);

    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    JsonObject hsv = root.createNestedObject(F("hsv"));
    hsv[F("h")] = h;
    hsv[F("s")] = s;
    hsv[F("v")] = v;
    hsv[F("ct")] = ct;

    root[F("t")] = 0;
    root[F("cmd")] = F("solid");

    String jsonMsg = Json::serialize(root);
    publish(buildTopic(F("color")), jsonMsg, true);
}

String AppMqttClient::buildTopic(const String& suffix) {
    String topic = app.cfg.network.mqtt.topic_base;
    topic += _id + "/";
    return topic + suffix;
}

String AppMqttClient::buildHaDiscoveryTopic(const String& deviceName) {
    return F("homeassistant/fhem_rgbwwcontroller/discovery/") + deviceName;
}

void AppMqttClient::publishClock(uint32_t steps) {
    if (_firstClock) {
        this->publishClockReset();
        _firstClock = false;
    } else {
        String msg;
        msg += steps;

        publish(buildTopic(F("clock")), msg, false);
    }
}

void AppMqttClient::publishClockReset() {
    if (!mqtt) {
        return;
    }

    publish(buildTopic(F("clock")), F("reset"), false);
}

void AppMqttClient::publishClockInterval(uint32_t curInterval) {
    if (!mqtt) {
        return;
    }

    String msg;
    msg += curInterval;

    publish(buildTopic(F("clock_interval")), msg, false);
}

void AppMqttClient::publishClockSlaveOffset(int offset) {
    if (!mqtt) {
        return;
    }
    String msg;
    msg += offset;

    publish(buildTopic(F("clock_slave_offset")), msg, false);
}

void AppMqttClient::publishCommand(const String& method, const JsonObject& params) {
    debug_d("AppMqttClient::publishCommand: %s\n", method.c_str());

    if (!mqtt) {
        return;
    }

    JsonRpcMessage msg(method);

    if (params.size() > 0)
        msg.getRoot()[F("params")] = params;

    String msgStr = Json::serialize(msg.getRoot());
    publish(buildTopic(F("command")), msgStr, false);
}

void AppMqttClient::publishTransitionFinished(const String& name, bool requeued) {
    debug_d("AppMqttClient::publishTransitionFinished: %s\n", name.c_str());

    if (!mqtt) {
        return;
    }

    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root[F("name")] = name;
    root[F("requeued")] = requeued;

    String jsonMsg = Json::serialize(root);
    publish(buildTopic(F("transition_finished")), jsonMsg, true);
}

void AppMqttClient::publishConfigEvent(const JsonObject& jsonObj) {
    debug_d("AppMqttClient::publishConfigEvent");

    if (!mqtt) {
        return;
    }

    JsonRpcMessage msg(F("config_event"), CONFIG_MAX_LENGTH);
    JsonObject root = msg.getParams();

    root.set(jsonObj);

    debug_d("EventServer::publishConfigEvent\n");

    String jsonMsg = Json::serialize(root);
    publish(buildTopic(F("config_event")), jsonMsg, true);
}

void AppMqttClient::publishHomeAssistantDiscovery() {
    debug_d("AppMqttClient::publishHomeAssistantDiscovery");

    if (!mqtt) {
        return;
    }

    JsonRpcMessage msg(F("ha_discovery"));
    StaticJsonDocument<200> doc;
    JsonObject root = doc.to<JsonObject>();
    root[F("device_name")] = app.cfg.general.device_name;
    root[F("ip_address")] = WifiStation.getIP().toString();
    root[F("mac_address")] = WifiStation.getMAC();

    String jsonMsg = Json::serialize(root);
    publish(buildHaDiscoveryTopic(app.cfg.general.device_name), jsonMsg, false);
}