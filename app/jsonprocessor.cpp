#include <RGBWWCtrl.h>


bool JsonProcessor::onColor(const String& json, String& msg, bool relay) {
    debug_e("JsonProcessor::onColor: %s", json.c_str());
    StaticJsonDocument<256> doc;
    Json::deserialize(doc, json);
    return onColor(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onColor(JsonObject root, String& msg, bool relay) {
    bool result = false;
    auto cmds = root["cmds"].as<JsonArray>();
    if (!cmds.isNull()) {
        Vector<String> errors;
        // multi command post (needs testing)
        for(unsigned i=0; i < cmds.size(); ++i) {
            String msg;
            if (!onSingleColorCommand(cmds[i], msg))
                errors.add(msg);
        }

        if (errors.size() == 0)
            result = true;
        else {
            String msg;
            for (unsigned i=0; i < errors.size(); ++i)
                msg += errors[i] + "|";
            result = false;
        }
    }
    else {
        if (onSingleColorCommand(root, msg))
            result = true;
        else
            result = false;
    }

    if (relay)
        app.onCommandRelay("color", root);

    return result;
}

bool JsonProcessor::onStop(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onStop(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onStop(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    JsonProcessor::parseRequestParams(root, params);
    app.rgbwwctrl.clearAnimationQueue(params.channels);
    app.rgbwwctrl.skipAnimation(params.channels);

    onDirect(root, msg, false);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("stop", root);
    }

    return true;
}

bool JsonProcessor::onSkip(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onSkip(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onSkip(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    JsonProcessor::parseRequestParams(root, params);
    app.rgbwwctrl.skipAnimation(params.channels);

    onDirect(root, msg, false);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("skip", root);
    }

    return true;
}

bool JsonProcessor::onPause(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onPause(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onPause(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    JsonProcessor::parseRequestParams(root, params);

    app.rgbwwctrl.pauseAnimation(params.channels);

    onDirect(root, msg, false);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("pause", root);
    }

    return true;
}

bool JsonProcessor::onContinue(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onContinue(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onContinue(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    JsonProcessor::parseRequestParams(root, params);
    app.rgbwwctrl.continueAnimation(params.channels);

    if (relay)
        app.onCommandRelay("continue", root);

    return true;
}

bool JsonProcessor::onBlink(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onBlink(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onBlink(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    params.ramp.value = 500; //default

    JsonProcessor::parseRequestParams(root, params);

    app.rgbwwctrl.blink(params.channels, params.ramp.value, params.queue, params.requeue, params.name);

    if (relay)
        app.onCommandRelay("blink", root);

    return true;
}

bool JsonProcessor::onToggle(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onToggle(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onToggle(JsonObject root, String& msg, bool relay) {
    app.rgbwwctrl.toggle();

    if (relay)
        app.onCommandRelay("toggle", root);

    return true;
}

bool JsonProcessor::onSingleColorCommand(JsonObject root, String& errorMsg) {
    RequestParameters params;
    parseRequestParams(root, params);
    if (params.checkParams(errorMsg) != 0) {
        return false;
    }

    bool queueOk = false;
    if (params.mode == RequestParameters::Mode::Hsv) {
        if(!params.hasHsvFrom) {
            if (params.cmd == "fade") {
                queueOk = app.rgbwwctrl.fadeHSV(params.hsv, params.ramp, params.direction, params.queue, params.requeue, params.name);
            } else {
                queueOk = app.rgbwwctrl.setHSV(params.hsv, params.ramp.value, params.queue, params.requeue, params.name);
            }
        } else {
            app.rgbwwctrl.fadeHSV(params.hsvFrom, params.hsv, params.ramp, params.direction, params.queue);
        }
    } else if (params.mode == RequestParameters::Mode::Raw) {
        if(!params.hasRawFrom) {
            if (params.cmd == "fade") {
                queueOk = app.rgbwwctrl.fadeRAW(params.raw, params.ramp, params.queue);
            } else {
                queueOk = app.rgbwwctrl.setRAW(params.raw, params.ramp.value, params.queue);
            }
        } else {
            app.rgbwwctrl.fadeRAW(params.rawFrom, params.raw, params.ramp, params.queue);
        }
    } else {
        errorMsg = "No color object!";
        return false;
    }

    if (!queueOk)
        errorMsg = "Queue full";
    return queueOk;
}

bool JsonProcessor::onDirect(const String& json, String& msg, bool relay) {
	StaticJsonDocument<256> doc;
	Json::deserialize(doc, json);
    return onDirect(doc.as<JsonObject>(), msg, relay);
}

bool JsonProcessor::onDirect(JsonObject root, String& msg, bool relay) {
    RequestParameters params;
    JsonProcessor::parseRequestParams(root, params);

    if (params.mode == RequestParameters::Mode::Hsv) {
        app.rgbwwctrl.colorDirectHSV(params.hsv);
    } else if (params.mode == RequestParameters::Mode::Raw) {
        app.rgbwwctrl.colorDirectRAW(params.raw);
    } else {
        msg = "No color object!";
    }

    if (relay)
        app.onCommandRelay("direct", root);

    return true;
}

void JsonProcessor::parseRequestParams(JsonObject root, RequestParameters& params) {
	String value;

	JsonObject hsv = root["hsv"];
	if (!hsv.isNull()) {
    	params.mode = RequestParameters::Mode::Hsv;
        if (Json::getValue(hsv["h"], value))
            params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
        if (Json::getValue(hsv["s"], value))
            params.hsv.s = AbsOrRelValue(value);
        if (Json::getValue(hsv["v"], value))
            params.hsv.v = AbsOrRelValue(value);
        if (Json::getValue(hsv["ct"], value))
            params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);

        JsonObject from = hsv["from"];
        if (!from.isNull()) {
            params.hasHsvFrom = true;
            if (Json::getValue(from["h"], value))
                params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
            if (Json::getValue(from["s"], value))
                params.hsv.s = AbsOrRelValue(value);
            if (Json::getValue(from["v"], value))
                params.hsv.v = AbsOrRelValue(value);
            if (Json::getValue(from["ct"], value))
                params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);
        }
    }
    else if (!root["raw"].isNull()) {
    	JsonObject raw = root["raw"];
        params.mode = RequestParameters::Mode::Raw;
        if (Json::getValue(raw["r"], value))
            params.raw.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw["g"], value))
            params.raw.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw["b"], value))
            params.raw.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw["ww"], value))
            params.raw.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw["cw"], value))
            params.raw.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);

        JsonObject from = raw["from"];
        if (!from.isNull()) {
            params.hasRawFrom = true;
            if (Json::getValue(from["r"], value))
                params.rawFrom.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from["g"], value))
                params.rawFrom.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from["b"], value))
                params.rawFrom.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from["ww"], value))
                params.rawFrom.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from["cw"], value))
                params.rawFrom.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        }
    }

    if (Json::getValue(root["t"], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Time;
    }

    if (Json::getValue(root["s"], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Speed;
    }

    if (!root["r"].isNull()) {
        params.requeue = root["r"].as<bool>();
    }

    Json::getValue(root["d"], params.direction);

    Json::getValue(root["name"], params.name);

    Json::getValue(root["cmd"], params.cmd);

    if (!root["q"].isNull()) {
        String q = root["q"];
        if (q == "back")
            params.queue = QueuePolicy::Back;
        else if (q == "front")
            params.queue = QueuePolicy::Front;
        else if (q == "front_reset")
            params.queue = QueuePolicy::FrontReset;
        else if (q == "single")
            params.queue = QueuePolicy::Single;
        else {
            params.queue = QueuePolicy::Invalid;
        }
    }

    JsonArray arr;
    if (Json::getValue(root["channels"], arr)) {
        for(size_t i=0; i < arr.size(); ++i) {
            String str = arr[i];
            if (str == "h") {
                params.channels.add(CtrlChannel::Hue);
            }
            else if (str == "s") {
                params.channels.add(CtrlChannel::Sat);
            }
            else if (str == "v") {
                params.channels.add(CtrlChannel::Val);
            }
            else if (str == "ct") {
                params.channels.add(CtrlChannel::ColorTemp);
            }
            else if (str == "r") {
                params.channels.add(CtrlChannel::Red);
            }
            else if (str == "g") {
                params.channels.add(CtrlChannel::Green);
            }
            else if (str == "b") {
                params.channels.add(CtrlChannel::Blue);
            }
            else if (str == "ww") {
                params.channels.add(CtrlChannel::WarmWhite);
            }
            else if (str == "cw") {
                params.channels.add(CtrlChannel::ColdWhite);
            }
        }
    }
}

int JsonProcessor::RequestParameters::checkParams(String& errorMsg) const {
    if (mode == Mode::Hsv) {
        if (hsv.ct.hasValue()) {
            if (hsv.ct != 0 && (hsv.ct < 100 || hsv.ct > 10000 || (hsv.ct > 500 && hsv.ct < 2000))) {
                errorMsg = "bad param for ct";
                return 1;
            }
        }

        if (!hsv.h.hasValue() && !hsv.s.hasValue() && !hsv.v.hasValue() && !hsv.ct.hasValue()) {
            errorMsg = "Need at least one HSVCT component!";
            return 1;
        }
    }
    else if (mode == Mode::Raw) {
        if (!raw.r.hasValue() && !raw.g.hasValue() && !raw.b.hasValue() && !raw.ww.hasValue() && !raw.cw.hasValue()) {
            errorMsg = "Need at least one RAW component!";
            return 1;
        }
    }

    if (queue == QueuePolicy::Invalid) {
        errorMsg = "Invalid queue policy";
        return 1;
    }

    if (cmd != "fade" && cmd != "solid") {
        errorMsg = "Invalid cmd";
        return 1;
    }

    if (direction < 0 || direction > 1) {
        errorMsg = "Invalid direction";
        return 1;
    }

    if (ramp.type == RampTimeOrSpeed::Type::Speed && ramp.value == 0) {
        errorMsg = "Speed cannot be 0!";
        return 1;
    }

    return 0;
}

bool JsonProcessor::onJsonRpc(const String& json) {
    debug_d("JsonProcessor::onJsonRpc: %s\n", json.c_str());
    JsonRpcMessageIn rpc(json);

    String msg;
    String method = rpc.getMethod();
    if (method == "color") {
        return onColor(rpc.getParams(), msg, false);
    }
    else if (method == "stop") {
        return onStop(rpc.getParams(), msg, false);
    }
    else if (method == "blink") {
        return onBlink(rpc.getParams(), msg, false);
    }
    else if (method == "skip") {
        return onSkip(rpc.getParams(), msg, false);
    }
    else if (method == "pause") {
        return onPause(rpc.getParams(), msg, false);
    }
    else if (method == "continue") {
        return onContinue(rpc.getParams(), msg, false);
    }
    else if (method == "direct") {
        return onDirect(rpc.getParams(), msg, false);
    } else {
    	return false;
    }
}

void JsonProcessor::addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels) {
    switch(app.rgbwwctrl.getMode()) {
    case RGBWWLed::ColorMode::Hsv:
    {
        const HSVCT& c = app.rgbwwctrl.getCurrentColor();
        JsonObject obj = root.createNestedObject("hsv");
        if (channels.count() == 0 || channels.contains(CtrlChannel::Hue))
            obj["h"] = (float(c.h) / float(RGBWW_CALC_HUEWHEELMAX)) * 360.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Sat))
            obj["s"] = (float(c.s) / float(RGBWW_CALC_MAXVAL)) * 100.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Val))
            obj["v"] = (float(c.v) / float(RGBWW_CALC_MAXVAL)) * 100.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::ColorTemp))
            obj["ct"] = c.ct;
        break;
    }
    case RGBWWLed::ColorMode::Raw:
    {
        const ChannelOutput& c = app.rgbwwctrl.getCurrentOutput();
        JsonObject obj = root.createNestedObject("raw");
        if (channels.count() == 0 || channels.contains(CtrlChannel::Red))
            obj["r"] = c.r;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Green))
            obj["g"] = c.g;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Blue))
            obj["b"] = c.b;
        if (channels.count() == 0 || channels.contains(CtrlChannel::WarmWhite))
            obj["ww"] = c.ww;
        if (channels.count() == 0 || channels.contains(CtrlChannel::ColdWhite))
            obj["cw"] = c.cw;
        break;
    }
    }
}
