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
//onComplete handler for mqtt 
{
	// lost connection
		connectDelayed(2000);
	
	// Restart connection attempt after few seconds
}

void AppMqttClient::connectDelayed(int delay)
{
	debug_d("MQTT::connectDelayed");
	_procTimer.initializeMs(delay, TimerDelegate(&AppMqttClient::connect, this)).startOnce();
/*
	if (mqtt->getConnectionState() == TcpClientState::eTCS_Connected) {
        initHomeAssistant();
        publishHomeAssistantConfig();
    }
*/
}

void AppMqttClient::connect()
{
	if(!mqtt ){
		debug_i("no mqtt client object");
		return;
	}
	if(mqtt->getConnectionState() == TcpClientState::eTCS_Connected ||
	   mqtt->getConnectionState() == TcpClientState::eTCS_Connecting)
	   {
		debug_i("mqtt already connecting");
		return;
	   }
	
	if(!mqtt->setWill(F("last/will"), F("The connection from this device is lost:("),
					  MqttClient::getFlags(MQTT_QOS_AT_LEAST_ONCE, MQTT_RETAIN_TRUE))) {
		debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
	}
	//    0);app.cfg.network.mqtt.username, app.cfg.network.mqtt.password);
	//debug_i("MqttClient: Server: %s Port: %d\n", app.cfg.network.mqtt.server.c_str(), app.cfg.network.mqtt.port);
	Url url;
	{
		AppConfig::Network network(*app.cfg);
		url = "mqtt://" + network.mqtt.getUsername() + ":" + network.mqtt.getPassword() + "@" +
				  network.mqtt.getServer() + ":" + String(network.mqtt.getPort());
		debug_i("mqtt url: %s, id: %s",url.toString().c_str(), _id);
		
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
	
	mqtt->connect(url, _id);
	// Initialize Home Assistant after connection
	
}

// ToDo: rework this so the class is less depending on the app itself but rather the app initializes the calls
void AppMqttClient::init()
{
	debug_i("mqtt - init");
	AppConfig::General general(*app.cfg);
	if(general.getDeviceName().length() > 0) {
		debug_w("AppMqttClient::init: building MQTT ID from device name: '%s'\n", general.getDeviceName().c_str());
		_id = general.getDeviceName();
	} else {
		debug_w("AppMqttClient::init: building MQTT ID from chip id (device name is: '%s')\n",
				general.getDeviceName().c_str());
		_id = String("rgbww_") + system_get_chip_id();
		debug_i("AppMqttClient::init: ID: %s\n", _id.c_str());
	}
	
	debug_i("finished mqtt init\n");
}

void AppMqttClient::start()
{
	debug_i("Start MQTT");

	init();
	if(mqtt){
		debug_i("mqttclient start - deleting existing mqtt client");
		stop();
	}
	mqtt = new MqttClient();
	mqtt->setEventHandler(MQTT_TYPE_PUBLISH, MqttDelegate(&AppMqttClient::onMessageReceived, this));
	mqtt->setConnectedHandler(MqttDelegate(&AppMqttClient::onConnected,this));
	connect();
}

int AppMqttClient::onConnected(MqttClient& client, mqtt_message_t* message){
	debug_i("MQTT Broker connected!!");
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
	}
	{
		AppConfig::Network network(*app.cfg);
		if(network.mqtt.homeassistant.getEnable()){
			initHomeAssistant();
		}else{
			debug_i("home assistant compatibility disabled");
		}
	} 
	return 0;
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
    
    debug_i("MQTT: Received message on topic: %s", topic.c_str());
    debug_i("MQTT: Message content: %s", message.c_str());
    
    // Check if this is a Home Assistant command
    if (_haEnabled) {
        String haCommandTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/set";
        if (topic == haCommandTopic) {
            debug_i("HA: Main light command received on topic: %s", topic.c_str());
            handleHomeAssistantCommand(message);
            return 0;
        }
        
        // Check for individual channel commands
        Vector<String> activeChannels;
        getActiveChannelNames(activeChannels);
        for (unsigned i = 0; i < activeChannels.count(); i++) {
            String channelCommandTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + activeChannels[i] + "/set";
            if (topic == channelCommandTopic) {
                debug_i("HA: Channel command received for '%s' on topic: %s", activeChannels[i].c_str(), topic.c_str());
                handleChannelCommand(activeChannels[i], message);
                return 0;
            }
        }
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
	
	// Also publish to Home Assistant
	if (_haEnabled) {
		publishHAState(raw, nullptr);
	}
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
	
	// Also publish to Home Assistant
	if (_haEnabled) {
		publishHAState(app.rgbwwctrl.getCurrentOutput(), &color);
	}
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
    debug_i("intialize Home Assistant topics");
    if (!_haEnabled) {
        return;
    }
    
    _haDiscoveryPrefix = network.mqtt.homeassistant.getDiscoveryPrefix();
    _haNodeId = network.mqtt.homeassistant.getNodeId();
    debug_i("HA::discoveryPrefix: %s",_haDiscoveryPrefix.c_str());
	debug_i("HA::NodeId: %s", _haNodeId.c_str());
    
    // If node_id is empty, use the device ID (controller name)
    if (_haNodeId.length() == 0) {
        _haNodeId = _id;  // This is the controller name
    }
    
    // Clean up node ID: trim spaces and replace spaces with underscores for MQTT topic compatibility
    _haNodeId.trim();
    _haNodeId.replace(" ", "_");
    
    // Generate unique_id from chip ID
    _haUniqueId = String("rgbww_") + system_get_chip_id();
    
    // Object ID for this light entity
    _haObjectId = "1";
    
    debug_i("HA::NodeId (device): %s", _haNodeId.c_str());
    debug_i("HA::UniqueId (chip): %s", _haUniqueId.c_str());
    debug_i("HA::ObjectId (entity): %s", _haObjectId.c_str());
    
    // Subscribe to the HA command topic
    if (mqtt && mqtt->getConnectionState() == TcpClientState::eTCS_Connected) {
        String commandTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/set";
        mqtt->subscribe(commandTopic);
        debug_i("Subscribed to HA command topic: %s", commandTopic.c_str());
    }
    
    // Publish the discovery configuration
    publishHomeAssistantConfig();
}

void AppMqttClient::publishHomeAssistantConfig() {
    if (!_haEnabled || _haConfigPublished) {
        return;
    }
    
    AppConfig::General general(*app.cfg);
    String deviceName = general.getDeviceName();
    
    // Clean device name for use in MQTT topics (trim spaces and replace with underscores)
    String cleanDeviceName = deviceName;
    cleanDeviceName.trim();
    cleanDeviceName.replace(" ", "_");
    
    // Update node_id with clean name if it was auto-generated
    if (_haNodeId == deviceName || _haNodeId.indexOf(' ') >= 0) {
        _haNodeId = cleanDeviceName;
        debug_i("Updated HA node_id to clean version: %s", _haNodeId.c_str());
    }
    
    // Create config document
    StaticJsonDocument<512> doc;
    
    // Basic configuration
    doc["name"] = deviceName;  // Display name can have spaces
    doc["unique_id"] = _haUniqueId;  // Use chip-based unique ID
    doc["state_topic"] = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/state";
    doc["command_topic"] = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/set";
    doc["schema"] = "json";
    
    // Light features - Main light uses HSV scaling
    doc["brightness"] = true;
    doc["brightness_scale"] = 100;  // HSV V is 0-100%
    doc["color_mode"] = true;
    
    // Supported color modes - dynamic based on current configuration
    JsonArray colorModes = doc.createNestedArray("supported_color_modes");
    Vector<String> supportedModes;
    getSupportedColorModes(supportedModes);
    for (unsigned i = 0; i < supportedModes.count(); i++) {
        colorModes.add(supportedModes[i]);
    }
    
    // Add white temperature support for modes with warm white
    int colorMode = getCurrentColorMode();
    if (colorMode == 1 || colorMode == 3) { // RGBWW or RGBWWCW
        doc["min_mireds"] = 153;  // ~6500K
        doc["max_mireds"] = 370;  // ~2700K
    }
    
    // Optimization
    doc["optimistic"] = false;
    doc["retain"] = true;
    
    // Device info
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(_haUniqueId);  // Use chip-based unique ID for device identification
    device["name"] = deviceName;  // Display name can have spaces
    device["model"] = "RGBWW Controller";
    device["manufacturer"] = "ESP RGBWW Firmware";
    device["sw_version"] = GITVERSION;
    
    // Publish discovery message
    String configTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/config";
    
    debug_i("Publishing HA config to topic: %s", configTopic.c_str());
    debug_i("Device name: '%s', Node ID: '%s', Unique ID: '%s'", deviceName.c_str(), _haNodeId.c_str(), _haUniqueId.c_str());
    
    publish(configTopic, Json::serialize(doc), true);
    
    _haConfigPublished = true;
    
    // Publish individual channel configurations
    Vector<String> activeChannels;
    getActiveChannelNames(activeChannels);
    for (unsigned i = 0; i < activeChannels.count(); i++) {
        publishChannelConfig(activeChannels[i]);
    }
    
    // Publish initial state
    publishHAState(app.rgbwwctrl.getCurrentOutput(), &app.rgbwwctrl.getCurrentColor());
}

void AppMqttClient::publishChannelConfig(const String& channelName) {
    if (!_haEnabled || !mqtt) return;
    
    AppConfig::General general(*app.cfg);
    String deviceName = general.getDeviceName();
    
    // Topic: homeassistant/light/node_id/channel_name/config
    String configTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + channelName + "/config";
    
    // Create channel discovery JSON
    StaticJsonDocument<512> doc;
    
    // Basic configuration
    doc["name"] = deviceName + " " + channelName.c_str();  // Display name with channel
    doc["unique_id"] = _haUniqueId + "_" + channelName;    // Unique ID per channel
    doc["state_topic"] = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + channelName + "/state";
    doc["command_topic"] = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + channelName + "/set";
    doc["schema"] = "json";
    
    // Channel only supports brightness - use raw channel scaling
    doc["brightness"] = true;
    doc["brightness_scale"] = 1023;  // Raw channel scaling 0-1023
    doc["color_mode"] = false;  // Individual channels don't support color
    
    // Device info - link to same device as main light
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(_haUniqueId);  // Same device as main light
    device["name"] = deviceName;
    device["model"] = "RGBWW Controller";
    device["manufacturer"] = "ESP RGBWW Firmware";
    device["sw_version"] = GITVERSION;
    
    // Subscribe to channel command topic
    if (mqtt && mqtt->getConnectionState() == TcpClientState::eTCS_Connected) {
        String commandTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + channelName + "/set";
        mqtt->subscribe(commandTopic);
        debug_i("Subscribed to channel command topic: %s", commandTopic.c_str());
    }
    
    // Publish channel discovery message
    debug_i("Publishing channel config for %s to: %s", channelName.c_str(), configTopic.c_str());
    publish(configTopic, Json::serialize(doc), true);
}

void AppMqttClient::publishHAState(const ChannelOutput& raw, const HSVCT* pHsv) {
    if (!_haEnabled) {
        return;
    }
    
    debug_i("HA: Publishing main light state");
    debug_i("HA: Raw values - R:%d G:%d B:%d WW:%d CW:%d", raw.r, raw.g, raw.b, raw.ww, raw.cw);
    
    StaticJsonDocument<256> doc;
    
    // Get HSV values
    HSVCT color = pHsv ? *pHsv : app.rgbwwctrl.getCurrentColor();
    float h, s, v;
    int ct;
    color.asRadian(h, s, v, ct);
    
    debug_i("HA: HSV values - H:%.3f deg, S:%.1f%%, V:%.1f%%, CT:%d", h, s, v, ct);
    
    // asRadian() actually returns H in degrees (0-360°) and S,V in percentages (0-100%)
    float hue_degrees = h;      // Already in degrees
    float sat_percent = s;      // Already in percent  
    float val_percent = v;      // Already in percent
    
    debug_i("HA: Converted for HA - H:%.1f°, S:%.1f%%, V:%.1f%%", hue_degrees, sat_percent, val_percent);
    
    // State and brightness - Use 0-100 scale as configured in discovery
    doc["state"] = v > 0 ? "ON" : "OFF";
    doc["brightness"] = (uint8_t)val_percent;  // V is already 0-100%, use directly
    doc["color_mode"] = "hs";  // Always include color_mode
    
    // Only include color if light is on
    if (v > 0) {
        JsonObject color_obj = doc.createNestedObject("color");
        color_obj["h"] = hue_degrees;   // 0-360 degrees
        color_obj["s"] = sat_percent;   // 0-100 percent
    }
    
    String stateTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + _haObjectId + "/state";
    String statePayload = Json::serialize(doc);
    debug_i("HA: Publishing to topic '%s': %s", stateTopic.c_str(), statePayload.c_str());
    publish(stateTopic, statePayload, true);
    
    // Publish individual channel states
    Vector<String> activeChannels;
    getActiveChannelNames(activeChannels);
    for (unsigned i = 0; i < activeChannels.count(); i++) {
        publishChannelState(activeChannels[i], raw);
    }
}

void AppMqttClient::publishChannelState(const String& channelName, const ChannelOutput& raw) {
    if (!_haEnabled) {
        return;
    }
    
    StaticJsonDocument<128> doc;
    
    int channelValue = 0;
    
    // Map channel name to raw channel value (0-1023)
    // Check both configured names and standard fallback names
    if (channelName == "red") {
        channelValue = raw.r;
    } else if (channelName == "green") {
        channelValue = raw.g;
    } else if (channelName == "blue") {
        channelValue = raw.b;
    } else if (channelName == "warmwhite" || channelName == "warm_white") {
        channelValue = raw.ww;
    } else if (channelName == "coldwhite" || channelName == "cool_white" || channelName=="cold_white") {
        channelValue = raw.cw;
    }
    
    debug_i("HA: Publishing channel '%s' state: %d (0-1023 scale)", channelName.c_str(), channelValue);
    
    // Use raw 0-1023 scaling as configured in discovery
    doc["state"] = channelValue > 0 ? "ON" : "OFF";
    doc["brightness"] = channelValue;  // Direct 0-1023 value
    
    String stateTopic = _haDiscoveryPrefix + "/light/" + _haNodeId + "/" + channelName + "/state";
    String statePayload = Json::serialize(doc);
    debug_i("HA: Publishing to topic '%s': %s", stateTopic.c_str(), statePayload.c_str());
    publish(stateTopic, statePayload, true);
}

void AppMqttClient::handleChannelCommand(const String& channelName, const String& message) {
    debug_i("HA: Processing channel command for '%s': %s", channelName.c_str(), message.c_str());
    
    // Parse JSON command
    DynamicJsonDocument root(256);
    auto error = deserializeJson(root, message);
    if (error) {
        debug_e("HA: Failed to parse channel command JSON: %s", error.c_str());
        return;
    }
    
    // Get current raw values
    ChannelOutput currentRaw = app.rgbwwctrl.getCurrentOutput();
    debug_i("HA: Current raw values - R:%d G:%d B:%d WW:%d CW:%d", 
            currentRaw.r, currentRaw.g, currentRaw.b, currentRaw.ww, currentRaw.cw);
    
    // Handle state command
    if (root.containsKey("state")) {
        String state = root["state"].as<String>();
        debug_i("HA: Channel state command: %s", state.c_str());
        if (state == "OFF") {
            // Turn off this channel
            if (channelName == "red") {
                currentRaw.r = 0;
                debug_i("HA: Setting red channel to 0");
            } else if (channelName == "green") {
                currentRaw.g = 0;
                debug_i("HA: Setting green channel to 0");
            } else if (channelName == "blue") {
                currentRaw.b = 0;
                debug_i("HA: Setting blue channel to 0");
            } else if (channelName == "warmwhite" || channelName == "warm_white") {
                currentRaw.ww = 0;
                debug_i("HA: Setting warm white channel to 0");
            } else if (channelName == "coldwhite" || channelName == "cool_white") {
                currentRaw.cw = 0;
                debug_i("HA: Setting cool white channel to 0");
            }
        }
    }
    
    // Handle brightness command (0-1023 scale as configured in discovery)
    if (root.containsKey("brightness")) {
        int brightness = root["brightness"].as<int>();
        debug_i("HA: Channel brightness command: %d (0-1023 scale)", brightness);
        
        // Clamp to valid range 0-1023
        int originalBrightness = brightness;
        brightness = (brightness < 0) ? 0 : ((brightness > 1023) ? 1023 : brightness);
        if (brightness != originalBrightness) {
            debug_w("HA: Clamped brightness from %d to %d", originalBrightness, brightness);
        }
        
        if (channelName == "red") {
            currentRaw.r = brightness;
            debug_i("HA: Setting red channel to %d", brightness);
        } else if (channelName == "green") {
            currentRaw.g = brightness;
            debug_i("HA: Setting green channel to %d", brightness);
        } else if (channelName == "blue") {
            currentRaw.b = brightness;
            debug_i("HA: Setting blue channel to %d", brightness);
        } else if (channelName == "warmwhite" || channelName == "warm_white") {
            currentRaw.ww = brightness;
            debug_i("HA: Setting warm white channel to %d", brightness);
        } else if (channelName == "coldwhite" || channelName == "cool_white") {
            currentRaw.cw = brightness;
            debug_i("HA: Setting cool white channel to %d", brightness);
        }
    }
    
    debug_i("HA: New raw values - R:%d G:%d B:%d WW:%d CW:%d", 
            currentRaw.r, currentRaw.g, currentRaw.b, currentRaw.ww, currentRaw.cw);
    
    // Apply the changes
    debug_i("HA: Applying changes to LED controller");
    app.rgbwwctrl.setRAW(currentRaw);
    
    // Publish state update for this channel only
    debug_i("HA: Publishing state update for channel '%s'", channelName.c_str());
    publishChannelState(channelName, currentRaw);

    // Also publish the main light state to keep them in sync
    debug_i("HA: Publishing main light state update");
    publishHAState(currentRaw, nullptr);
}

void AppMqttClient::handleHomeAssistantCommand(const String& message) {
    if (!_haEnabled) {
        return;
    }
    
    debug_i("HA: Processing main light command: %s", message.c_str());
    
    StaticJsonDocument<256> doc;
    DeserializationError parseError = deserializeJson(doc, message);
    if (parseError) {
        debug_e("HA: Failed to parse command JSON: %s", parseError.c_str());
        return;
    }
    
    String state = doc["state"];
    debug_i("HA: Command state: %s", state.c_str());
    
    // Create a JSON command that works with your existing system
    StaticJsonDocument<256> cmdDoc;
    JsonObject root = cmdDoc.to<JsonObject>();
    JsonObject hsv = root.createNestedObject("hsv");
    
    if (state == "ON") {
        // Handle brightness
        float brightness = 100.0f;  // Default to 100%
        if (doc.containsKey("brightness")) {
            float brightness_raw = doc["brightness"].as<float>();
            brightness = brightness_raw;  // Now using 0-100 scale directly
            debug_i("HA: Brightness from HA: %.1f (0-100 scale)", brightness);
        }
        
        // Handle color
        if (doc.containsKey("color")) {
            float h = doc["color"]["h"];  // HA sends 0-360 degrees
            float s = doc["color"]["s"];  // HA sends 0-100 percent
            
            debug_i("HA: Color from HA - H: %.1f°, S: %.1f%%", h, s);
            
            // LED controller expects H: 0-360°, S: 0-100%, V: 0-100%
            hsv["h"] = h;                 // Keep as 0-360 degrees
            hsv["s"] = s;                 // Keep as 0-100 percentage
            hsv["v"] = brightness;        // Keep as 0-100 percentage
            
            debug_i("HA: Converted to internal - H: %.1f°, S: %.1f%%, V: %.1f%%", 
                    hsv["h"].as<float>(), hsv["s"].as<float>(), hsv["v"].as<float>());
        } else {
            // Just brightness change - keep current color
            HSVCT currentColor = app.rgbwwctrl.getCurrentColor();
            float cur_h, cur_s, cur_v;
            int cur_ct;
            currentColor.asRadian(cur_h, cur_s, cur_v, cur_ct);
            
            hsv["h"] = cur_h;             // Keep as 0-360 degrees
            hsv["s"] = cur_s;             // Keep as 0-100 percentage  
            hsv["v"] = brightness;        // Use brightness as 0-100 percentage
            
            debug_i("HA: Brightness only change - keeping current color, new V: %.1f%%", brightness);
        }
    } else {
        // Turn off - keep current color but set brightness to 0
        HSVCT currentColor = app.rgbwwctrl.getCurrentColor();
        float cur_h, cur_s, cur_v;
        int cur_ct;
        currentColor.asRadian(cur_h, cur_s, cur_v, cur_ct);
        
        hsv["h"] = cur_h;             // Keep as 0-360 degrees
        hsv["s"] = cur_s;             // Keep as 0-100 percentage
        hsv["v"] = 0;                 // Turn off (0%)
        
        debug_i("HA: Turning OFF - keeping current color, setting V to 0%%");
    }
    
    root["cmd"] = "fade";
    
    // Handle transition time (HA sends in seconds, we expect milliseconds)
    int transition_ms = 500;  // Default 500ms
    if (doc.containsKey("transition")) {
        transition_ms = doc["transition"].as<int>() * 1000;  // Convert seconds to milliseconds
        debug_i("HA: Transition time: %d ms", transition_ms);
    }
    root["t"] = transition_ms;
    
    String ledCommand = Json::serialize(root);
    debug_i("HA: Sending to LED controller: %s", ledCommand.c_str());
    
    // Process the command through your existing system
    String errorMsg;
    app.jsonproc.onColor(ledCommand, errorMsg, false);
    
    if (errorMsg.length() > 0) {
        debug_e("HA: LED controller error: %s", errorMsg.c_str());
    } else {
        debug_i("HA: LED controller command processed successfully");
    }
    
    // Publish state update after processing
    publishHAState(app.rgbwwctrl.getCurrentOutput(), nullptr);
}

// Helper methods for dynamic color mode support
int AppMqttClient::getCurrentColorMode() {
    AppConfig::Color color(*app.cfg);
    return color.getColorMode();
}

void AppMqttClient::getActiveChannelNames(Vector<String>& channels) {
    channels.clear();

    // Use pin config logic from ledctrl.cpp
    AppConfig::General general(*app.cfg);
    AppConfig::Hardware hardware(*app.cfg);

    String pinConfigName = general.getCurrentPinConfigName();
    String SoC = SOC;
    bool found = false;

    if (hardware.pinconfigs.getItemCount() > 0) {
        for (auto pinconfig : hardware.pinconfigs) {
            if (pinconfig.getName() == pinConfigName && pinconfig.getSoc() == SoC) {
                found = true;
                for (auto channel : pinconfig.channels) {
                    String channelName = channel.getName();
                    if (channelName.length() > 0) {
                        channels.addElement(channelName);
                    }
                }
                break;
            }
        }
    }

    // Fallback to default names if not found
    if (!found || channels.count() == 0) {
        int colorMode = getCurrentColorMode();

        // All modes have RGB
        channels.addElement("red");
        channels.addElement("green");
        channels.addElement("blue");

        // Add white channels based on mode
        switch (colorMode) {
            case 1: // RGBWW
                channels.addElement("warm_white");
                break;
            case 2: // RGBCW
                channels.addElement("cool_white");
                break;
            case 3: // RGBWWCW
                channels.addElement("warm_white");
                channels.addElement("cool_white");
                break;
            default: // RGB
                break;
        }
    }
}

void AppMqttClient::getSupportedColorModes(Vector<String>& modes) {
    modes.clear();
    
    // Primary mode is HSV (hue + saturation)
    modes.addElement("hs");
    
    int colorMode = getCurrentColorMode();
    
    // Add white color modes for modes that support white channels
    if (colorMode == 1 || colorMode == 3) { // RGBWW or RGBWWCW
        modes.addElement("color_temp");
    }
}