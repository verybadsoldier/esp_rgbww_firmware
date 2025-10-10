#include <RGBWWCtrl.h>

bool JsonProcessor::onColor(const String& json, String& errorMsg, bool relay) {
    debug_e("JsonProcessor::onColor: %s", json.c_str());
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onColor(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onColor(JsonObject root, String& errorMsg, bool relay) {
    bool result = false;
    auto cmds = root["cmds"].as<JsonArray>();
    if (!cmds.isNull()) {
        Vector<String> errors;
        // multi command post (needs testing)
        for (unsigned i = 0; i < cmds.size(); ++i) {
            String cmdErrMsg;
            if (!onSingleColorCommand(cmds[i], cmdErrMsg))
                errors.add(cmdErrMsg);
        }

        if (errors.size() == 0)
            result = true;
        else {
            errorMsg = "";
            for (unsigned i = 0; i < errors.size(); ++i)
                errorMsg += errors[i] + "|";
            result = false;
        }
    } else {
        if (onSingleColorCommand(root, errorMsg))
            result = true;
        else
            result = false;
    }

   if (relay)
        app.onCommandRelay("color", root);

    return result;
}

bool JsonProcessor::onStop(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onStop(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onStop(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;

    app.rgbwwctrl.clearAnimationQueue(params.channels);
    app.rgbwwctrl.skipAnimation(params.channels); // also stops current animation

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("stop", root);
    }

    return true;
}

bool JsonProcessor::onSkip(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onSkip(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onSkip(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;
    app.rgbwwctrl.skipAnimation(params.channels);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("skip", root);
    }

    return true;
}

bool JsonProcessor::onPause(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onPause(doc.as<JsonObject>(), errorMsg);
}

bool JsonProcessor::onPause(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;

    app.rgbwwctrl.pauseAnimation(params.channels);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay("pause", root);
    }

    return true;
}

bool JsonProcessor::onContinue(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onContinue(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onContinue(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;
    app.rgbwwctrl.continueAnimation(params.channels);

    if (relay)
        app.onCommandRelay("continue", root);

    return true;
}

bool JsonProcessor::onBlink(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onBlink(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onBlink(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    params.ramp.value = 500; // default

    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;

    app.rgbwwctrl.blink(params.channels, params.ramp.value, params.queue, params.requeue, params.name);

    if (relay)
        app.onCommandRelay("blink", root);

    return true;
}

bool JsonProcessor::onToggle(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = "JSON deserialization error";
        return false;
    }
    return onToggle(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onToggle(JsonObject root, String& errorMsg, bool relay) {
    app.rgbwwctrl.toggle();

    if (relay)
        app.onCommandRelay("toggle", root);

    return true;
}

bool JsonProcessor::onSingleColorCommand(JsonObject root, String& errorMsg) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, errorMsg))
        return false;

    if (!params.checkParams(errorMsg, _settings)) {
        return false;
    }

    bool queueOk = false;
    if (params.mode == RequestParameters::Mode::Hsv) {
        if (!params.hasHsvFrom) {
            queueOk = app.rgbwwctrl.fadeHSV(params.hsv, params.ramp, params.stay, params.direction, params.queue,
                                            params.requeue, params.name);
        } else {
            queueOk = app.rgbwwctrl.fadeHSV(params.hsvFrom, params.hsv, params.ramp, params.stay, params.direction,
                                            params.queue);
        }
    } else if (params.mode == RequestParameters::Mode::Raw) {
        if (!params.hasRawFrom) {
            queueOk = app.rgbwwctrl.fadeRAW(params.raw, params.ramp, params.stay, params.queue);
        } else {
            queueOk = app.rgbwwctrl.fadeRAW(params.rawFrom, params.raw, params.ramp, params.stay, params.queue);
        }
    } else {
        errorMsg = "No color object!";
        return false;
    }

    if (!queueOk)
        errorMsg = "Queue full";
    return queueOk;
}

/*
 * Check for unsupported parameters in the JSON object, by list of allowed fields. Raise an expection if any unsupported
 * field is found.
 */
bool JsonProcessor::checkUnsupportedParams(JsonObject obj, const String& rootName, const std::set<String>& allowed,
                                           String& errorMsg) {
    for (JsonObject::iterator it = obj.begin(); it != obj.end(); ++it) {
        if (allowed.find(it->key().c_str()) == allowed.end()) {
            debug_w("JsonProcessor::checkUnsupportedParams - unsupported param: %s in node %s\n", it->key().c_str(),
                    rootName.c_str());
            errorMsg = String("Unsupported param: ") + it->key().c_str() + " in node " + rootName;
            return false;
        }
    }
    return true;
}

bool JsonProcessor::parseRequestParams(JsonObject root, RequestParameters& params, String& errorMsg) {
    String value;

    if (!JsonProcessor::checkUnsupportedParams(root, "root", {"hsv", "raw", "t", "s", "stay", "r", "d", "name", "q"},
                                               errorMsg))
        return false;

    JsonObject hsv = root["hsv"];
    if (!hsv.isNull()) {
        if (!JsonProcessor::JsonProcessor::checkUnsupportedParams(hsv, "hsv", {"h", "s", "v", "ct", "from"}, errorMsg))
            return false;

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
        if (!JsonProcessor::checkUnsupportedParams(from, "hsv.from", {"h", "s", "v", "ct"}, errorMsg))
            return false;

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
    } else if (!root["raw"].isNull()) {
        if (!JsonProcessor::checkUnsupportedParams(root["raw"], "raw", {"r", "g", "b", "ww", "cw", "from"}, errorMsg))
            return false;

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
            if (!JsonProcessor::checkUnsupportedParams(from, "raw.from", {"r", "g", "b", "ww", "cw"}, errorMsg))
                return false;

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

    if (root.containsKey("t") && root.containsKey("s")) {
        errorMsg = "Cannot use both t (time) and s (speed) parameters simultaneously!";
        return false;
    }

    if (Json::getValue(root["t"], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Time;
    }

    if (Json::getValue(root["s"], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Speed;
    }

    Json::getValue(root["stay"], params.stay);

    if (!root["r"].isNull()) {
        params.requeue = root["r"].as<bool>();
    }

    String direction;
    if (Json::getValue(root["d"], direction)) {
        if (direction == "short") {
            params.direction = HueTransitionDirection::dir_short;
        }
        else if (direction == "long") {
            params.direction = HueTransitionDirection::dir_long;
        }
        else {
            errorMsg = "Invalid hue direction";
            return false;
        }
    }

    Json::getValue(root["name"], params.name);

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
        debug_w("JsonProcessor::parseRequestParams - channels specified: %d\n", arr.size());
        for (size_t i = 0; i < arr.size(); ++i) {
            // checkUnsupportedParams(arr, "channels", {"h", "s", "v", "ct", "r", "g", "b", "ww", "cw"});

            String str = arr[i];
            if (str == "h") {
                params.channels.add(CtrlChannel::Hue);
            } else if (str == "s") {
                params.channels.add(CtrlChannel::Sat);
            } else if (str == "v") {
                params.channels.add(CtrlChannel::Val);
            } else if (str == "ct") {
                params.channels.add(CtrlChannel::ColorTemp);
            } else if (str == "r") {
                params.channels.add(CtrlChannel::Red);
            } else if (str == "g") {
                params.channels.add(CtrlChannel::Green);
            } else if (str == "b") {
                params.channels.add(CtrlChannel::Blue);
            } else if (str == "ww") {
                params.channels.add(CtrlChannel::WarmWhite);
            } else if (str == "cw") {
                params.channels.add(CtrlChannel::ColdWhite);
            }
        }
    }

    return true;
}

bool JsonProcessor::RequestParameters::checkParams(String& errorMsg, const ApplicationSettings& settings) const {
    if (mode == Mode::Hsv) {
        if (hsv.ct.hasValue() && !settings.isColortempInRange(hsv.ct.getValue())) {
            errorMsg = "bad param for ct";
            return false;
        }

        if (!hsv.h.hasValue() && !hsv.s.hasValue() && !hsv.v.hasValue() && !hsv.ct.hasValue()) {
            errorMsg = "Need at least one HSVCT component!";
            return false;
        }
    } else if (mode == Mode::Raw) {
        if (!raw.r.hasValue() && !raw.g.hasValue() && !raw.b.hasValue() && !raw.ww.hasValue() && !raw.cw.hasValue()) {
            errorMsg = "Need at least one RAW component!";
            return false;
        }
    }

    if (queue == QueuePolicy::Invalid) {
        errorMsg = "Invalid queue policy";
        return false;
    }

    if (ramp.type == RampTimeOrSpeed::Type::Speed && ramp.value == 0) {
        errorMsg = "Speed cannot be 0!";
        return false;
    }

    return true;
}

bool JsonProcessor::onJsonRpc(const String& json, String& errorMsg) {
    debug_d("JsonProcessor::onJsonRpc: %s\n", json.c_str());
    JsonRpcMessageIn rpc(json);

    String method = rpc.getMethod();
    if (method == "color") {
        return onColor(rpc.getParams(), errorMsg);
    } else if (method == "stop") {
        return onStop(rpc.getParams(), errorMsg);
    } else if (method == "blink") {
        return onBlink(rpc.getParams(), errorMsg);
    } else if (method == "skip") {
        return onSkip(rpc.getParams(), errorMsg);
    } else if (method == "pause") {
        return onPause(rpc.getParams(), errorMsg);
    } else if (method == "continue") {
        return onContinue(rpc.getParams(), errorMsg);
    } else if (method == "toggle") {
        return onToggle(rpc.getParams(), errorMsg);
    } else {
        return false;
    }
}

void JsonProcessor::addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels) {
    switch (app.rgbwwctrl.getMode()) {
    case RGBWWLed::ColorMode::Hsv: {
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
    case RGBWWLed::ColorMode::Raw: {
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
