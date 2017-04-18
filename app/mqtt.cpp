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
        Serial.println("MQTT Broker Disconnected!!");
    else
        Serial.println("MQTT Broker Unreachable!!");

    // Restart connection attempt after few seconds
    connectDelayed(2000);
}

void AppMqttClient::connectDelayed(int delay) {
    Serial.println("MQTT::connectDelayed");
    _procTimer.initializeMs(delay, TimerDelegate(&AppMqttClient::connect, this)).startOnce();
}

void AppMqttClient::connect() {
    if (!mqtt || mqtt->getConnectionState() == TcpClientState::eTCS_Connected || mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
        return;

    Serial.printf("MQTT::connect ID: %s\n", _id.c_str());
    if(!mqtt->setWill("last/will","The connection from this device is lost:(", 1, true)) {
        debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
    }
    mqtt->connect(_id, "", "", false);
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
}

void AppMqttClient::init() {
    if (app.cfg.general.device_name.length() > 0) {
        _id = app.cfg.general.device_name;
    }
}

void AppMqttClient::start() {
	Serial.println("Start MQTT");

	delete mqtt;
	Serial.printf("MqttClient: Server: %s Port: %d\n", app.cfg.network.mqtt.server.c_str(), app.cfg.network.mqtt.port);
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
	Serial.print(topic);
	Serial.print(":\r\n\t"); // Prettify alignment for printing
	Serial.println(message);

	if (app.cfg.sync.clock_slave_enabled && (topic == app.cfg.sync.clock_slave_topic)) {
		uint32_t clock = message.toInt();
		app.rgbwwctrl.onMasterClock(clock);
	}
	else if (app.cfg.sync.cmd_slave_enabled && (topic == app.cfg.sync.cmd_slave_topic)) {
		app.jsonproc.onJsonRpc(message);
	}
}

void AppMqttClient::publish(const String& topic, const String& data, bool retain) {
    //Serial.printf("AppMqttClient::publish: Topic: %s | Data: %s\n", topic.c_str(), data.c_str());

    if (!mqtt) {
        Serial.printf("ApplicationMQTTClient::publish: no MQTT object\n");
        return;
    }

    TcpClientState state = mqtt->getConnectionState();
    if (state == TcpClientState::eTCS_Connected) {
        mqtt->publish(topic, data, retain);
    }
    else {
        Serial.printf("ApplicationMQTTClient::publish: not connected.\n");
    }
}

void AppMqttClient::publishCurrentRaw(const ChannelOutput& color) {
    Serial.printf("ApplicationMQTTClient::publishCurrentRaw\n");

    String msg;
    msg += color.r;
    msg += ",";
    msg += color.g;
    msg += ",";
    msg += color.b;
    msg += ",";
    msg += color.cw;
    msg += ",";
    msg += color.ww;

    publish(buildTopic("raw"), msg, true);
}

void AppMqttClient::publishCurrentHsv(const HSVCT& color) {
    //Serial.printf("ApplicationMQTTClient::publishCurrentHsv\n");

    float h, s, v;
    int ct;
    color.asRadian(h, s, v, ct);

    String msg;
    msg += h;
    msg += ",";
    msg += s;
    msg += ",";
    msg += v;
    msg += ",";
    msg += ct;

    publish(buildTopic("hsv"), msg, true);
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

void AppMqttClient::publishCommand(const String& method, const JsonObject& params) {
    Serial.printf("ApplicationMQTTClient::publishCommand: %s\n", method.c_str());

	JsonRpcMessage msg(method);

    String topic = buildTopic("command");

    if (params.size() > 0)
        msg.getRoot()["params"] = params;

    String msgStr;
    msg.getRoot().printTo(msgStr);
    Serial.printf("ApplicationMQTTClient::publishCommand22: %s\n", method.c_str());
    publish(topic, msgStr, false);
}
