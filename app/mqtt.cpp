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

ApplicationMQTTClient::ApplicationMQTTClient() {
    _id = String("rgbww_") + WifiStation.getMAC();
}

ApplicationMQTTClient::~ApplicationMQTTClient() {
    delete mqtt;
    mqtt = nullptr;
}

void ApplicationMQTTClient::onComplete(TcpClient& client, bool success) {
    if (success == true)
        Serial.println("MQTT Broker Disconnected!!");
    else
        Serial.println("MQTT Broker Unreachable!!");

    // Restart connection attempt after few seconds
    connectDelayed(2000);
}

void ApplicationMQTTClient::connectDelayed(int delay) {
    Serial.println("MQTT::connectDelayed");
    _procTimer.initializeMs(delay, TimerDelegate(&ApplicationMQTTClient::connect, this)).startOnce();
}

void ApplicationMQTTClient::connect() {
    if (!mqtt || mqtt->getConnectionState() == TcpClientState::eTCS_Connected )//|| mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
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
    mqtt->setCompleteDelegate(TcpClientCompleteDelegate(&ApplicationMQTTClient::onComplete, this));

    if (_cfg->sync.syncToMaster) {
        mqtt->subscribe(_cfg->sync.syncToMasterTopic);
    }
}

void ApplicationMQTTClient::init(const ApplicationSettings& cfg) {
    _cfg = &cfg;
    if (_cfg->general.device_name.length() > 0) {
        _id = _cfg->general.device_name;
    }
}

void ApplicationMQTTClient::start() {
	Serial.println("Start MQTT");

	delete mqtt;
	Serial.printf("MqttClient: Server: %s Port: %d\n", _cfg->network.mqtt.server.c_str(), _cfg->network.mqtt.port);
	mqtt = new MqttClient(_cfg->network.mqtt.server, _cfg->network.mqtt.port, MqttStringSubscriptionCallback(&ApplicationMQTTClient::onMessageReceived, this));
	connectDelayed(10000);
}

void ApplicationMQTTClient::stop() {
	 delete mqtt;
	 mqtt = nullptr;
}

bool ApplicationMQTTClient::isRunning() const {
	return (mqtt != nullptr);
}

void ApplicationMQTTClient::onMessageReceived(String topic, String message) {
	Serial.print(topic);
	Serial.print(":\r\n\t"); // Prettify alignment for printing
	Serial.println(message);

	if (topic == _cfg->sync.syncToMasterTopic) {
	    if (_masterClockSink) {
	        uint32_t clock = message.toInt();
	        _masterClockSink->onMasterClock(clock);
	    }
	}
}

void ApplicationMQTTClient::publish(const String& topic, const String& data, bool retain) {
    if (!mqtt) {
        Serial.printf("ApplicationMQTTClient::publish: no MQTT object\n");
        return;
    }

    TcpClientState state = mqtt->getConnectionState();
    if (state == TcpClientState::eTCS_Connected) {
        mqtt->publish(topic, data, retain);
    }
    else {
        Serial.printf("ApplicationMQTTClient::publish: not connected. Connecting now\n");
    //    connectDelayed(500);
    }
}

void ApplicationMQTTClient::publishCurrentRaw(const ChannelOutput& color) {
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

void ApplicationMQTTClient::publishCurrentHsv(const HSVCT& color) {
    Serial.printf("ApplicationMQTTClient::publishCurrentHsv\n");

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

String ApplicationMQTTClient::buildTopic(const String& suffix) {
    String topic = _cfg->network.mqtt.topic_base;
    topic += _id + "/";
    return topic + suffix;
}

void ApplicationMQTTClient::publishClock(uint32_t steps) {
    String msg;
    msg += steps;

    String topic = buildTopic("clock");
    publish(topic, msg, false);
}

void ApplicationMQTTClient::publishColorCommand(String json) {
    String topic = buildTopic("command");
    publish(topic, json, false);
}

void ApplicationMQTTClient::setMasterClockSink(IMasterClockSink* pSink) {
    _masterClockSink = pSink;
}
