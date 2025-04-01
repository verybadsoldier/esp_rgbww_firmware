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
#include <Network/Mqtt/MqttBuffer.h>

AppMqttClient::AppMqttClient()
{
}

AppMqttClient::~AppMqttClient()
{
	delete mqtt;
	mqtt = nullptr;
}

void AppMqttClient::onComplete(TcpClient& client, bool success)
{
	if(success == true)
		debug_i("MQTT Broker Disconnected!!");
	else
		debug_e("MQTT Broker Unreachable!!");

	// Restart connection attempt after few seconds
	connectDelayed(2000);
}

void AppMqttClient::connectDelayed(int delay)
{
	debug_d("MQTT::connectDelayed");
	_procTimer.initializeMs(delay, TimerDelegate(&AppMqttClient::connect, this)).startOnce();

	if (mqtt->getConnectionState() == TcpClientState::eTCS_Connected) {
        initHomeAssistant();
        publishHomeAssistantConfig();
    }
}

void AppMqttClient::connect()
{
	if(!mqtt || mqtt->getConnectionState() == TcpClientState::eTCS_Connected ||
	   mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
		return;

	debug_d("MQTT::connect ID: %s\n", _id.c_str());

	if(!mqtt->setWill(F("last/will"), F("The connection from this device is lost:("),
					  MqttClient::getFlags(MQTT_QOS_AT_LEAST_ONCE, MQTT_RETAIN_TRUE))) {
		debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
	}
	//    0);app.cfg.network.mqtt.username, app.cfg.network.mqtt.password);
	//debug_i("MqttClient: Server: %s Port: %d\n", app.cfg.network.mqtt.server.c_str(), app.cfg.network.mqtt.port);
	{
		AppConfig::Network network(*app.cfg);
		Url url = "mqtt://" + network.mqtt.getUsername() + ":" + network.mqtt.getPassword() + "@" +
				  network.mqtt.getServer() + ":" + String(network.mqtt.getPort());
		mqtt->connect(url, _id);
	} // end ConfigDB network context
#ifdef ENABLE_SSL
	// not need i guess? mqtt->addSslOptions(SSL_SERVER_VERIFY_LATER);

#include <ssl/private_key.h>
#include <ssl/cert.h>

	mqtt->setSslKeyCert(default_private_key, default_private_key_len, default_certificate, default_certificate_len,
						NULL, true);

#endif
	// Assign a disconnect callback function
	mqtt->setCompleteDelegate(TcpClientCompleteDelegate(&AppMqttClient::onComplete, this));
	{
		AppConfig::Sync sync(*app.cfg);
		if(sync.getClockSlaveEnabled()) {
			mqtt->subscribe(sync.getClockSlaveTopic());
		}
		if(sync.getCmdSlaveEnabled()) {
			mqtt->subscribe(sync.getCmdSlaveTopic());
		}
		if(sync.getColorSlaveEnabled()) {
			debug_d("Subscribe: %s\n", sync.getColorSlaveTopic().c_str());
			mqtt->subscribe(sync.getColorSlaveTopic());
		}
	} // end ConfigDB sync context
}

// ToDo: rework this so the class is less depending on the app itself but rather the app initializes the calls
void AppMqttClient::init()
{
	AppConfig::General general(*app.cfg);
	if(general.getDeviceName().length() > 0) {
		debug_w("AppMqttClient::init: building MQTT ID from device name: '%s'\n", general.getDeviceName().c_str());
		_id = general.getDeviceName();
	} else {
		debug_w("AppMqttClient::init: building MQTT ID from MAC (device name is: '%s')\n",
				general.getDeviceName().c_str());
		_id = String("rgbww_") + WifiStation.getMAC();
	}
	connectDelayed(1000);
}

void AppMqttClient::start()
{
	debug_i("Start MQTT");

	delete mqtt;
	mqtt = new MqttClient();
	mqtt->setEventHandler(MQTT_TYPE_PUBLISH, MqttDelegate(&AppMqttClient::onMessageReceived, this));
}

void AppMqttClient::stop()
{
	delete mqtt;
	mqtt = nullptr;
}

bool AppMqttClient::isRunning() const
{
	return (mqtt != nullptr);
}

int AppMqttClient::onMessageReceived(MqttClient& client, mqtt_message_t* msg)
{
	String topic = MqttBuffer(msg->publish.topic_name);
    String message = MqttBuffer(msg->publish.content);
    
    // Check if this is a Home Assistant command
    if (_haEnabled && topic == buildHATopic("light", _haNodeId, "set")) {
        handleHomeAssistantCommand(message);
        return 0;
    }

	AppConfig::Sync sync(*app.cfg);
	if(sync.getClockSlaveEnabled() && (topic == sync.getClockSlaveTopic())) {
		if(message == F("reset")) {
			app.rgbwwctrl.onMasterClockReset();
		} else {
			uint32_t clock = message.toInt();
			app.rgbwwctrl.onMasterClock(clock);
		}
	} else if(sync.getCmdSlaveEnabled() && topic == sync.getCmdSlaveTopic()) {
		app.jsonproc.onJsonRpc(message);
	} else if(sync.getColorSlaveEnabled() && (topic == sync.getColorSlaveTopic())) {
		String error;
		app.jsonproc.onColor(message, error, false);
	}
	return 0;
}

void AppMqttClient::publish(const String& topic, const String& data, bool retain)
{
	//Serial.printf("AppMqttClient::publish: Topic: %s | Data: %s\n", topic.c_str(), data.c_str());

	if(!mqtt) {
		debug_w("ApplicationMQTTClient::publish: no MQTT object\n");
		return;
	}

	TcpClientState state = mqtt->getConnectionState();
	if(state == TcpClientState::eTCS_Connected) {
		mqtt->publish(topic, data, retain);
	} else {
		debug_w("ApplicationMQTTClient::publish: not connected.\n");
	}

	/*
	if (_haEnabled) {
        publishHAState(app.rgbwwctrl.getCurrentOutput(), &color);
    }
	*/	
}

void AppMqttClient::publishCurrentRaw(const ChannelOutput& raw)
{
	if(raw == _lastRaw)
		return;
	_lastRaw = raw;

	debug_d("ApplicationMQTTClient::publishCurrentRaw\n");

	StaticJsonDocument<200> doc;
	JsonObject root = doc.to<JsonObject>();
	JsonObject rawJson = root.createNestedObject("raw");
	rawJson[F("r")] = raw.r;
	rawJson[F("g")] = raw.g;
	rawJson[F("b")] = raw.b;
	rawJson[F("cw")] = raw.cw;
	rawJson[F("ww")] = raw.ww;

	root[F("t")] = 0;
	root[F("cmd")] = "solid";

	String jsonMsg = Json::serialize(root);
	publish(buildTopic(F("color")), jsonMsg, true);
}

void AppMqttClient::publishCurrentHsv(const HSVCT& color)
{
	if(color == _lastHsv)
		return;
	_lastHsv = color;

	debug_d("ApplicationMQTTClient::publishCurrentHsv\n");

	float h, s, v;
	int ct;
	color.asRadian(h, s, v, ct);

	StaticJsonDocument<200> doc;
	JsonObject root = doc.to<JsonObject>();
	JsonObject hsv = root.createNestedObject("hsv");
	hsv[F("h")] = h;
	hsv[F("s")] = s;
	hsv[F("v")] = v;
	hsv[F("ct")] = ct;

	root[F("t")] = 0;
	root[F("cmd")] = "solid";

	String jsonMsg = Json::serialize(root);
	publish(buildTopic(F("color")), jsonMsg, true);
}

String AppMqttClient::buildTopic(const String& suffix)
{
	AppConfig::Network network(*app.cfg);
	String topic = network.mqtt.getTopicBase();
	topic += _id + "/";
	return topic + suffix;
}

void AppMqttClient::publishClock(uint32_t steps)
{
	if(_firstClock) {
		this->publishClockReset();
		_firstClock = false;
	} else {
		String msg;
		msg += steps;

		publish(buildTopic(F("clock")), msg, false);
	}
}

void AppMqttClient::publishClockReset()
{
	publish(buildTopic(F("clock")), "reset", false);
}

void AppMqttClient::publishClockInterval(uint32_t curInterval)
{
	String msg;
	msg += curInterval;

	publish(buildTopic(F("clock_interval")), msg, false);
}

void AppMqttClient::publishClockSlaveOffset(int offset)
{
	String msg;
	msg += offset;

	publish(buildTopic(F("clock_slave_offset")), msg, false);
}

void AppMqttClient::publishCommand(const String& method, const JsonObject& params)
{
	debug_d("ApplicationMQTTClient::publishCommand: %s\n", method.c_str());

	JsonRpcMessage msg(method);

	if(params.size() > 0)
		msg.getRoot()[F("params")] = params;

	String msgStr = Json::serialize(msg.getRoot());
	publish(buildTopic(F("command")), msgStr, false);
}

void AppMqttClient::publishTransitionFinished(const String& name, bool requeued)
{
	debug_d("ApplicationMQTTClient::publishTransitionFinished: %s\n", name.c_str());

	StaticJsonDocument<200> doc;
	JsonObject root = doc.to<JsonObject>();
	root[F("name")] = name;
	root[F("requequed")] = requeued;

	String jsonMsg = Json::serialize(root);
	publish(buildTopic(F("transition_finished")), jsonMsg, true);
}

void AppMqttClient::initHomeAssistant() {
    AppConfig::Network network(*app.cfg);
    _haEnabled = network.mqtt.homeassistant.getEnable();
    
    if (!_haEnabled) {
        return;
    }
    
    _haDiscoveryPrefix = network.mqtt.homeassistant.getDiscoveryPrefix();
    _haNodeId = network.mqtt.homeassistant.getNodeId();
    
    // If node_id is empty, use the device ID
    if (_haNodeId.length() == 0) {
        _haNodeId = _id;
    }
    
    // Subscribe to the HA command topic
    if (mqtt && mqtt->getConnectionState() == TcpClientState::eTCS_Connected) {
        mqtt->subscribe(buildHATopic("light", _haNodeId, "set"));
    }
}

void AppMqttClient::publishHomeAssistantConfig() {
    if (!_haEnabled || _haConfigPublished) {
        return;
    }
    
    AppConfig::General general(*app.cfg);
    String deviceName = general.getDeviceName();
    
    // Create config document
    StaticJsonDocument<512> doc;
    
    // Basic configuration
    doc["name"] = deviceName;
    doc["unique_id"] = _haNodeId;
    doc["state_topic"] = buildHATopic("light", _haNodeId, "state");
    doc["command_topic"] = buildHATopic("light", _haNodeId, "set");
    doc["schema"] = "json";
    
    // Light features
    doc["brightness"] = true;
    doc["brightness_scale"] = 255;  // Standard HA scale
    doc["color_mode"] = true;
    doc["supported_color_modes"] = JsonArray().add("hs");
    
    // Optimization
    doc["optimistic"] = false;
    doc["retain"] = true;
    
    // Device info
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"] = JsonArray().add(_id);
    device["name"] = deviceName;
    device["model"] = "RGBWW Controller";
    device["manufacturer"] = "ESP RGBWW Firmware";
    device["sw_version"] = GIT_VERSION;
    
    // Publish discovery message
    String configTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/config";
    publish(configTopic, Json::serialize(doc), true);
    
    _haConfigPublished = true;
    
    // Publish initial state
    publishCurrentHsv(app.rgbwwctrl.getCurrentColor());
}

String AppMqttClient::buildHATopic(const String& component, const String& entityId, const String& suffix) {
    return component + "/" + entityId + "/" + suffix;
}

void AppMqttClient::publishHAState(const ChannelOutput& raw, const HSVCT* pHsv) {
    if (!_haEnabled) {
        return;
    }
    
    StaticJsonDocument<256> doc;
    
    // Get HSV values
    HSVCT color = pHsv ? *pHsv : app.rgbwwctrl.getCurrentColor();
    float h, s, v;
    int ct;
    color.asRadian(h, s, v, ct);
    
    // Convert to HA format
    doc["state"] = v > 0 ? "ON" : "OFF";
    doc["brightness"] = (uint8_t)(v * 2.55f);  // Scale 0-100 to 0-255
    
    // Only include color if light is on
    if (v > 0) {
        JsonObject color_obj = doc.createNestedObject("color");
        color_obj["h"] = h;
        color_obj["s"] = s * 100;  // HA expects 0-100%
    }
    
    publish(buildHATopic("light", _haNodeId, "state"), Json::serialize(doc), true);
}

void AppMqttClient::handleHomeAssistantCommand(const String& message) {
    if (!_haEnabled) {
        return;
    }
    
    StaticJsonDocument<256> doc;
    deserializeJson(doc, message);
    
    String state = doc["state"];
    
    // Create a JSON command that works with your existing system
    StaticJsonDocument<256> cmdDoc;
    JsonObject root = cmdDoc.to<JsonObject>();
    JsonObject hsv = root.createNestedObject("hsv");
    
    if (state == "ON") {
        // Handle brightness
        float brightness = 100.0f;  // Default to 100%
        if (doc.containsKey("brightness")) {
            brightness = (doc["brightness"].as<float>() / 255.0f) * 100.0f;  // Convert 0-255 to 0-100
        }
        
        // Handle color
        if (doc.containsKey("color")) {
            hsv["h"] = doc["color"]["h"];
            hsv["s"] = doc["color"]["s"];
            hsv["v"] = brightness;
        } else {
            // Just brightness change
            hsv["v"] = brightness;
        }
    } else {
        // Turn off
        hsv["v"] = 0;
    }
    
    root["cmd"] = "fade";
    root["t"] = 500;  // 500ms transition
    
    // Process the command through your existing system
    String error;
    app.jsonproc.onColor(Json::serialize(root), error, false);
}