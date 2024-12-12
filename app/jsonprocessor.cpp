#include <RGBWWCtrl.h>

/**
 * @brief Processes the color JSON data.
 *
 * This function deserializes the JSON data and calls the overloaded onColor function
 * with the deserialized JsonObject.
 *
 * @param json The JSON data to be processed.
 * @param msg The output message.
 * @param relay A flag indicating whether to relay the message.
 * @return True if the processing is successful, false otherwise.
 */
bool JsonProcessor::onColor(const String& json, String& msg, bool relay)
{
	debug_e("JsonProcessor::onColor: %s", json.c_str());
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onColor(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Processes the color command from a JSON object.
 * 
 * This function is responsible for processing the color command from a JSON object.
 * It can handle both single command and multi-command posts.
 * 
 * @param root The JSON object containing the color command.
 * @param msg A reference to a string that will hold any error messages.
 * @param relay A boolean indicating whether to relay the command to the app.
 * @return A boolean indicating the success of the color command processing.
 */
bool JsonProcessor::onColor(JsonObject root, String& msg, bool relay)
{
	bool result = false;
	auto cmds = root[F("cmds")].as<JsonArray>();
	if(!cmds.isNull()) {
		Vector<String> errors;
		// multi command post (needs testing)
		debug_i("  multi command post");
		for(unsigned i = 0; i < cmds.size(); ++i) {
			String msg;
			if(!onSingleColorCommand(cmds[i], msg))
				errors.add(msg);
		}

		if(errors.size() == 0)
			result = true;
		else {
			String msg;
			debug_i("  multi command post, %s", msg.c_str());
			for(unsigned i = 0; i < errors.size(); ++i)
				msg += errors[i] + "|";
			result = false;
		}
	} else {
		debug_i("  single command post %s", msg.c_str());
		if(onSingleColorCommand(root, msg))
			result = true;
		else
			result = false;
	}

	if(relay)
		app.onCommandRelay(F("color"), root);

	return result;
}

/**
 * @brief Handles the "onStop" event for the JsonProcessor class.
 * 
 * This function deserializes the given JSON string into a StaticJsonDocument object
 * and calls the overloaded onStop function with the deserialized JsonObject, a message string,
 * and a boolean flag indicating whether to use the relay.
 * 
 * @param json The JSON string to be deserialized.
 * @param msg The output message string.
 * @param relay Flag indicating whether to use the relay.
 * @return True if the onStop function is successfully called, false otherwise.
 */
bool JsonProcessor::onStop(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onStop(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Handles the "stop" command in the JSON message.
 * 
 * This function stops the animation and performs other necessary actions based on the provided JSON message.
 * 
 * @param root The JSON object containing the command and parameters.
 * @param msg A reference to a string where error messages can be stored.
 * @param relay A boolean indicating whether the command should be relayed to another device.
 * @return true if the command was successfully processed, false otherwise.
 */
bool JsonProcessor::onStop(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	JsonProcessor::parseRequestParams(root, params);
	app.rgbwwctrl.clearAnimationQueue(params.channels);
	app.rgbwwctrl.skipAnimation(params.channels);

	onDirect(root, msg, false);

	if(relay) {
		addChannelStatesToCmd(root, params.channels);
		app.onCommandRelay(F("stop"), root);
	}

	return true;
}

/**
 * @brief Skips the animation and performs additional actions based on the provided parameters.
 *
 * This function deserializes the given JSON string into a StaticJsonDocument object and then calls the overloaded
 * onSkip function with the deserialized JsonObject, a message string, and a relay flag.
 *
 * @param json The JSON string to be skipped.
 * @param msg The message string to be passed to the onSkip function.
 * @param relay The relay flag to be passed to the onSkip function.
 * @return True if the onSkip function is successfully called, false otherwise.
 */
bool JsonProcessor::onSkip(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onSkip(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Skips the animation and performs additional actions based on the provided parameters.
 *
 * This function parses the request parameters from the given JSON object and skips the animation
 * for the specified channels. It then calls the onDirect function to perform additional actions.
 * If the relay flag is set to true, it adds the channel states to the command and calls the
 * onCommandRelay function.
 *
 * @param root The JSON object containing the request data.
 * @param msg A reference to a string that will be modified to store any error messages.
 * @param relay A boolean flag indicating whether to relay the command.
 * @return True if the animation was skipped successfully, false otherwise.
 */
bool JsonProcessor::onSkip(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	JsonProcessor::parseRequestParams(root, params);
	app.rgbwwctrl.skipAnimation(params.channels);

	onDirect(root, msg, false);

	if(relay) {
		addChannelStatesToCmd(root, params.channels);
		app.onCommandRelay(F("skip"), root);
	}

	return true;
}

/**
 * @brief Pauses the animation and performs additional actions based on the provided parameters.
 *
 * This function deserializes the given JSON string into a StaticJsonDocument,
 * and then calls the onPause function with the deserialized JsonObject.
 *
 * @param json The JSON string to be deserialized.
 * @param msg The output message.
 * @param relay The relay flag.
 * @return True if the onPause function is successfully called, false otherwise.
 */
bool JsonProcessor::onPause(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onPause(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Pauses the animation and performs additional actions based on the provided parameters.
 * 
 * This function pauses the animation by calling `app.rgbwwctrl.pauseAnimation()` with the specified channels.
 * It also calls `onDirect()` with the provided `root` and `msg` parameters.
 * 
 * If the `relay` parameter is true, it adds the channel states to the command and calls `app.onCommandRelay()`
 * with the command "pause" and the `root` parameter.
 * 
 * @param root The JsonObject containing the request data.
 * @param msg A reference to a String object to store any additional message.
 * @param relay A boolean indicating whether to perform additional relay actions.
 * @return true if the function executed successfully, false otherwise.
 */
bool JsonProcessor::onPause(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	JsonProcessor::parseRequestParams(root, params);

	app.rgbwwctrl.pauseAnimation(params.channels);

	onDirect(root, msg, false);

	if(relay) {
		addChannelStatesToCmd(root, params.channels);
		app.onCommandRelay(F("pause"), root);
	}

	return true;
}

/**
 * @brief Continues the animation and relays the command if specified.
 * 
 * This function deserializes the JSON data using the StaticJsonDocument class
 * and calls the overloaded onContinue function with the deserialized JsonObject.
 * 
 * @param json The JSON data to be processed.
 * @param msg Output parameter to store any error message.
 * @param relay Flag indicating whether to relay the data or not.
 * @return True if the operation is successful, false otherwise.
 */
bool JsonProcessor::onContinue(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onContinue(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Continues the animation and relays the command if specified.
 *
 * This function is called to continue the animation based on the provided JSON object.
 * It parses the request parameters, continues the animation, and relays the command if the relay flag is set.
 *
 * @param root The JSON object containing the animation parameters.
 * @param msg A reference to a string that will hold any error message.
 * @param relay A boolean flag indicating whether to relay the command or not.
 * @return True if the animation was continued successfully, false otherwise.
 */
bool JsonProcessor::onContinue(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	JsonProcessor::parseRequestParams(root, params);
	app.rgbwwctrl.continueAnimation(params.channels);

	if(relay)
		app.onCommandRelay(F("continue"), root);

	return true;
}

/**
 * @brief Handles the "blink" command in the JSON payload.
 * 
 * This function deserializes the JSON data into a StaticJsonDocument,
 * and then calls the overloaded onBlink function with the deserialized
 * JsonObject, message string, and relay flag.
 * 
 * @param json The JSON data to process.
 * @param msg The output message string.
 * @param relay The relay flag.
 * @return True if the blink command was processed successfully, false otherwise.
 */
bool JsonProcessor::onBlink(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onBlink(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Handles the "blink" command in the JSON payload.
 * 
 * This function parses the JSON payload and extracts the necessary parameters for the "blink" command.
 * It then calls the `blink` function of the `rgbwwctrl` object with the extracted parameters.
 * If the `relay` flag is set to true, it also calls the `onCommandRelay` function with the "blink" command.
 * 
 * @param root The JSON object containing the command and its parameters.
 * @param msg A reference to a string that will hold any error message generated during the processing.
 * @param relay A boolean flag indicating whether to relay the command or not.
 * @return Returns true if the command was successfully processed, false otherwise.
 */
bool JsonProcessor::onBlink(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	params.ramp.value = 500; //default

	JsonProcessor::parseRequestParams(root, params);

	app.rgbwwctrl.blink(params.channels, params.ramp.value, params.queue, params.requeue, params.name);

	if(relay)
		app.onCommandRelay(F("blink"), root);

	return true;
}

/**
 * @brief Toggles the RGBWW control and sends a command relay if specified.
 *
 * This function deserializes the JSON data and calls the overloaded onToggle function
 * with the deserialized JsonObject, message string, and relay flag.
 *
 * @param json The JSON data to be processed.
 * @param msg The output message string.
 * @param relay The relay flag.
 * @return True if the toggle operation is successful, false otherwise.
 */
bool JsonProcessor::onToggle(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onToggle(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Toggles the RGBWW control and sends a command relay if specified.
 * 
 * @param root The JSON object containing the command.
 * @param msg The message to be modified.
 * @param relay Flag indicating whether to send a command relay.
 * @return true if the toggle was successful, false otherwise.
 */
bool JsonProcessor::onToggle(JsonObject root, String& msg, bool relay)
{
	app.rgbwwctrl.toggle();

	if(relay)
		app.onCommandRelay(F("toggle"), root);

	return true;
}

/**
 * @brief Handles a single color command from a JSON object.
 * 
 * This function parses the request parameters from the JSON object and performs the corresponding action
 * based on the parameters. It supports both HSV and RAW color modes. If the parameters are valid and the
 * action is successfully executed, it returns true. Otherwise, it returns false and sets the errorMsg
 * parameter with an appropriate error message.
 * 
 * @param root The JSON object containing the command parameters.
 * @param errorMsg A reference to a string variable to store the error message, if any.
 * @return Returns true if the command is executed successfully, false otherwise.
 */
bool JsonProcessor::onSingleColorCommand(JsonObject root, String& errorMsg)
{
	RequestParameters params;
	parseRequestParams(root, params);
	if(params.checkParams(errorMsg) != 0) {
		return false;
	}

	bool queueOk = false;
	if(params.mode == RequestParameters::Mode::Hsv) {
		if(!params.hasHsvFrom) {
			if(params.cmd == "fade") {
				queueOk = app.rgbwwctrl.fadeHSV(params.hsv, params.ramp, params.direction, params.queue, params.requeue,
												params.name);
			} else {
				queueOk =
					app.rgbwwctrl.setHSV(params.hsv, params.ramp.value, params.queue, params.requeue, params.name);
			}
		} else {
			app.rgbwwctrl.fadeHSV(params.hsvFrom, params.hsv, params.ramp, params.direction, params.queue);
		}
	} else if(params.mode == RequestParameters::Mode::Raw) {
		if(!params.hasRawFrom) {
			if(params.cmd == "fade") {
				queueOk = app.rgbwwctrl.fadeRAW(params.raw, params.ramp, params.queue);
			} else {
				queueOk = app.rgbwwctrl.setRAW(params.raw, params.ramp.value, params.queue);
			}
		} else {
			app.rgbwwctrl.fadeRAW(params.rawFrom, params.raw, params.ramp, params.queue);
		}
	} else {
		errorMsg = "No color object!";
		debug_i("no color object");
		return false;
	}

	if(!queueOk) {
		debug_i("queue full");
		errorMsg = "Queue full";
	}
	return queueOk;
}

/**
 * @brief Handles a direct JSON command.
 *
 * This function processes a direct JSON command and performs the corresponding action based on the provided parameters.
 * 
 * This function deserializes the given JSON string into a JSON document and calls the overloaded
 * `onDirect` function with the deserialized JSON object.
 *
 * @param json The JSON string to be processed.
 * @param msg Output parameter to store any error message.
 * @param relay Flag indicating whether to relay the message.
 * @return True if the processing is successful, false otherwise.
 */
bool JsonProcessor::onDirect(const String& json, String& msg, bool relay)
{
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
	return onDirect(doc.as<JsonObject>(), msg, relay);
}

/**
 * @brief Handles a direct JSON command.
 *
 * This function processes a direct JSON command and performs the corresponding action based on the provided parameters.
 *
 * @param root The JSON object containing the command parameters.
 * @param msg A reference to a string that will be updated with a message indicating the result of the command.
 * @param relay A boolean value indicating whether the command should be relayed to another component.
 * @return Returns true if the command was successfully processed, false otherwise.
 */
bool JsonProcessor::onDirect(JsonObject root, String& msg, bool relay)
{
	RequestParameters params;
	JsonProcessor::parseRequestParams(root, params);

	if(params.mode == RequestParameters::Mode::Hsv) {
		app.rgbwwctrl.colorDirectHSV(params.hsv);
	} else if(params.mode == RequestParameters::Mode::Raw) {
		app.rgbwwctrl.colorDirectRAW(params.raw);
	} else {
		msg = "No color object!";
	}

	if(relay)
		app.onCommandRelay(F("direct"), root);

	return true;
}

/**
 * @brief Parses the request parameters from a JSON object.
 *
 * This function extracts the request parameters from the provided JSON object and populates
 * the RequestParameters object accordingly.
 *
 * @param root The JSON object containing the request parameters.
 * @param params The RequestParameters object to be populated.
 */
void JsonProcessor::parseRequestParams(JsonObject root, RequestParameters& params)
{
	String value;

	JsonObject hsv = root[F("hsv")];
	if(!hsv.isNull()) {
		params.mode = RequestParameters::Mode::Hsv;
		if(Json::getValue(hsv[F("h")], value))
			params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
		if(Json::getValue(hsv[F("s")], value))
			params.hsv.s = AbsOrRelValue(value);
		if(Json::getValue(hsv[F("v")], value))
			params.hsv.v = AbsOrRelValue(value);
		if(Json::getValue(hsv[F("ct")], value))
			params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);

		JsonObject from = hsv[F("from")];
		if(!from.isNull()) {
			params.hasHsvFrom = true;
			if(Json::getValue(from[F("h")], value))
				params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
			if(Json::getValue(from[F("s")], value))
				params.hsv.s = AbsOrRelValue(value);
			if(Json::getValue(from[F("v")], value))
				params.hsv.v = AbsOrRelValue(value);
			if(Json::getValue(from[F("ct")], value))
				params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);
		}
	} else if(!root[F("raw")].isNull()) {
		JsonObject raw = root[F("raw")];
		params.mode = RequestParameters::Mode::Raw;
		if(Json::getValue(raw[F("r")], value))
			params.raw.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
		if(Json::getValue(raw[F("g")], value))
			params.raw.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
		if(Json::getValue(raw[F("b")], value))
			params.raw.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
		if(Json::getValue(raw[F("ww")], value))
			params.raw.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
		if(Json::getValue(raw[F("cw")], value))
			params.raw.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);

		JsonObject from = raw[F("from")];
		if(!from.isNull()) {
			params.hasRawFrom = true;
			if(Json::getValue(from[F("r")], value))
				params.rawFrom.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
			if(Json::getValue(from[F("g")], value))
				params.rawFrom.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
			if(Json::getValue(from[F("b")], value))
				params.rawFrom.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
			if(Json::getValue(from[F("ww")], value))
				params.rawFrom.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
			if(Json::getValue(from[F("cw")], value))
				params.rawFrom.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
		}
	}

	if(Json::getValue(root[F("t")], params.ramp.value)) {
		params.ramp.type = RampTimeOrSpeed::Type::Time;
	}

	if(Json::getValue(root[F("s")], params.ramp.value)) {
		params.ramp.type = RampTimeOrSpeed::Type::Speed;
	}

	if(!root[F("r")].isNull()) {
		params.requeue = root[F("r")].as<bool>();
	}

	Json::getValue(root[F("d")], params.direction);

	Json::getValue(root[F("name")], params.name);

	Json::getValue(root[F("cmd")], params.cmd);

	if(!root[F("q")].isNull()) {
		String q = root[F("q")];
		if(q == "back")
			params.queue = QueuePolicy::Back;
		else if(q == "front")
			params.queue = QueuePolicy::Front;
		else if(q == "front_reset")
			params.queue = QueuePolicy::FrontReset;
		else if(q == "single")
			params.queue = QueuePolicy::Single;
		else {
			params.queue = QueuePolicy::Invalid;
		}
	}

	JsonArray arr;
	if(Json::getValue(root[F("channels")], arr)) {
		for(size_t i = 0; i < arr.size(); ++i) {
			String str = arr[i];
			if(str == "h") {
				params.channels.add(CtrlChannel::Hue);
			} else if(str == "s") {
				params.channels.add(CtrlChannel::Sat);
			} else if(str == "v") {
				params.channels.add(CtrlChannel::Val);
			} else if(str == "ct") {
				params.channels.add(CtrlChannel::ColorTemp);
			} else if(str == "r") {
				params.channels.add(CtrlChannel::Red);
			} else if(str == "g") {
				params.channels.add(CtrlChannel::Green);
			} else if(str == "b") {
				params.channels.add(CtrlChannel::Blue);
			} else if(str == "ww") {
				params.channels.add(CtrlChannel::WarmWhite);
			} else if(str == "cw") {
				params.channels.add(CtrlChannel::ColdWhite);
			}
		}
	}
}

/**
 * @brief Check the parameters of the RequestParameters object.
 *
 * This function checks the parameters of the RequestParameters object and returns an error message if any parameter is invalid.
 *
 * @param errorMsg The error message to be returned if any parameter is invalid.
 * @return An integer indicating the result of the parameter check. 0 if all parameters are valid, non-zero otherwise.
 */
int JsonProcessor::RequestParameters::checkParams(String& errorMsg) const
{
	if(mode == Mode::Hsv) {
		if(hsv.ct.hasValue()) {
			if(hsv.ct != 0 && (hsv.ct < 100 || hsv.ct > 10000 || (hsv.ct > 500 && hsv.ct < 2000))) {
				errorMsg = "bad param for ct";
				return 1;
			}
		}

		if(!hsv.h.hasValue() && !hsv.s.hasValue() && !hsv.v.hasValue() && !hsv.ct.hasValue()) {
			errorMsg = "Need at least one HSVCT component!";
			return 1;
		}
	} else if(mode == Mode::Raw) {
		if(!raw.r.hasValue() && !raw.g.hasValue() && !raw.b.hasValue() && !raw.ww.hasValue() && !raw.cw.hasValue()) {
			errorMsg = "Need at least one RAW component!";
			return 1;
		}
	}

	if(queue == QueuePolicy::Invalid) {
		errorMsg = "Invalid queue policy";
		return 1;
	}

	if(cmd != "fade" && cmd != "solid") {
		errorMsg = "Invalid cmd";
		return 1;
	}

	if(direction < 0 || direction > 1) {
		errorMsg = "Invalid direction";
		return 1;
	}

	if(ramp.type == RampTimeOrSpeed::Type::Speed && ramp.value == 0) {
		errorMsg = "Speed cannot be 0!";
		return 1;
	}

	return 0;
}

/**
 * @brief Handles the JSON-RPC request.
 *
 * This function processes the JSON-RPC request and performs the necessary actions based on the received JSON data.
 *
 * @param json The JSON string containing the request.
 * @return True if the request was successfully processed, false otherwise.
 */
bool JsonProcessor::onJsonRpc(const String& json)
{
	debug_d("JsonProcessor::onJsonRpc: %s\n", json.c_str());
	JsonRpcMessageIn rpc(json);

	String msg;
	String method = rpc.getMethod();
	if(method == "color") {
		return onColor(rpc.getParams(), msg, false);
	} else if(method == "stop") {
		return onStop(rpc.getParams(), msg, false);
	} else if(method == "blink") {
		return onBlink(rpc.getParams(), msg, false);
	} else if(method == "skip") {
		return onSkip(rpc.getParams(), msg, false);
	} else if(method == "pause") {
		return onPause(rpc.getParams(), msg, false);
	} else if(method == "continue") {
		return onContinue(rpc.getParams(), msg, false);
	} else if(method == "direct") {
		return onDirect(rpc.getParams(), msg, false);
	} else {
		return false;
	}
}

/**
 * @brief Adds channel states to the command JSON object.
 * 
 * This function adds channel states to the command JSON object based on the current color mode.
 * If the color mode is HSV, the function adds the hue, saturation, value, and color temperature channels.
 * If the color mode is Raw, the function adds the red, green, blue, warm white, and cold white channels.
 * 
 * @param root The root JSON object to which the channel states will be added.
 * @param channels The list of channels for which the states will be added.
 */
void JsonProcessor::addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels)
{
	switch(app.rgbwwctrl.getMode()) {
	case RGBWWLed::ColorMode::Hsv: {
		const HSVCT& c = app.rgbwwctrl.getCurrentColor();
		JsonObject obj = root.createNestedObject(F("hsv"));
		if(channels.count() == 0 || channels.contains(CtrlChannel::Hue))
			obj[F("h")] = (float(c.h) / float(RGBWW_CALC_HUEWHEELMAX)) * 360.0;
		if(channels.count() == 0 || channels.contains(CtrlChannel::Sat))
			obj[F("s")] = (float(c.s) / float(RGBWW_CALC_MAXVAL)) * 100.0;
		if(channels.count() == 0 || channels.contains(CtrlChannel::Val))
			obj[F("v")] = (float(c.v) / float(RGBWW_CALC_MAXVAL)) * 100.0;
		if(channels.count() == 0 || channels.contains(CtrlChannel::ColorTemp))
			obj[F("ct")] = c.ct;
		break;
	}
	case RGBWWLed::ColorMode::Raw: {
		const ChannelOutput& c = app.rgbwwctrl.getCurrentOutput();
		JsonObject obj = root.createNestedObject(F("raw"));
		if(channels.count() == 0 || channels.contains(CtrlChannel::Red))
			obj[F("r")] = c.r;
		if(channels.count() == 0 || channels.contains(CtrlChannel::Green))
			obj[F("g")] = c.g;
		if(channels.count() == 0 || channels.contains(CtrlChannel::Blue))
			obj[F("b")] = c.b;
		if(channels.count() == 0 || channels.contains(CtrlChannel::WarmWhite))
			obj[F("ww")] = c.ww;
		if(channels.count() == 0 || channels.contains(CtrlChannel::ColdWhite))
			obj[F("cw")] = c.cw;
		break;
	}
	}
}
