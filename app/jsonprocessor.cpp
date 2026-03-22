#include <RGBWWCtrl.h>

bool JsonProcessor::onColor(const String& json, String& errorMsg, bool relay) {
    debug_e("JsonProcessor::onColor: %s", json.c_str());
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onColor(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onColor(JsonObject root, String& errorMsg, bool relay) {
    bool result = false;
    auto cmds = root[F("cmds")].as<JsonArray>();
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
        app.onCommandRelay(F("color"), root);

    return result;
}

bool JsonProcessor::onStop(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onStop(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onStop(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, true, errorMsg))
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
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onSkip(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onSkip(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, true, errorMsg))
        return false;
    app.rgbwwctrl.skipAnimation(params.channels);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay(F("skip"), root);
    }

    return true;
}

bool JsonProcessor::onPause(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onPause(doc.as<JsonObject>(), errorMsg);
}

bool JsonProcessor::onPause(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, true, errorMsg))
        return false;

    app.rgbwwctrl.pauseAnimation(params.channels);

    if (relay) {
        addChannelStatesToCmd(root, params.channels);
        app.onCommandRelay(F("pause"), root);
    }

    return true;
}

bool JsonProcessor::onContinue(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onContinue(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onContinue(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, true, errorMsg))
        return false;
    app.rgbwwctrl.continueAnimation(params.channels);

    if (relay)
        app.onCommandRelay(F("continue"), root);

    return true;
}

bool JsonProcessor::onBlink(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onBlink(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onBlink(JsonObject root, String& errorMsg, bool relay) {
    RequestParameters params;
    params.ramp.value = 500; // default

    if (!JsonProcessor::parseRequestParams(root, params, true, errorMsg))
        return false;

    app.rgbwwctrl.blink(params.channels, params.ramp.value, params.queue, params.requeue, params.name);

    if (relay)
        app.onCommandRelay(F("blink"), root);

    return true;
}

bool JsonProcessor::onToggle(const String& json, String& errorMsg, bool relay) {
    StaticJsonDocument<_jsonDocumentMaxSize> doc;
    if (!Json::deserialize(doc, json)) {
        errorMsg = F("JSON deserialization error");
        return false;
    }
    return onToggle(doc.as<JsonObject>(), errorMsg, relay);
}

bool JsonProcessor::onToggle(JsonObject root, String& errorMsg, bool relay) {
    app.rgbwwctrl.toggle();

    if (relay)
        app.onCommandRelay(F("toggle"), root);

    return true;
}

bool JsonProcessor::onSingleColorCommand(JsonObject root, String& errorMsg) {
    RequestParameters params;
    if (!JsonProcessor::parseRequestParams(root, params, false, errorMsg))
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
        errorMsg = F("No color object!");
        return false;
    }

    if (!queueOk)
        errorMsg = F("Queue full");
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
            errorMsg = F("Unsupported param: ") + it->key().c_str() + F(" in node ") + rootName;
            return false;
        }
    }
    return true;
}

bool JsonProcessor::parseRequestParams(JsonObject root, RequestParameters& params, bool allowChannelsParam,
                                       String& errorMsg) {
    String value;

    if (allowChannelsParam) {
        if (!JsonProcessor::checkUnsupportedParams(
                root, F("root"),
                {F("hsv"), F("raw"), F("t"), F("s"), F("stay"), F("r"), F("d"), F("name"), F("q"), F("channels")},
                errorMsg))
            return false;
    } else {
        if (!JsonProcessor::checkUnsupportedParams(
                root, F("root"), {F("hsv"), F("raw"), F("t"), F("s"), F("stay"), F("r"), F("d"), F("name"), F("q")},
                errorMsg))
            return false;
    }

    JsonObject hsv = root["hsv"];
    if (!hsv.isNull()) {
        if (!JsonProcessor::JsonProcessor::checkUnsupportedParams(
                hsv, F("hsv"), {F("h"), F("s"), F("v"), F("ct"), F("from")}, errorMsg))
            return false;

        params.mode = RequestParameters::Mode::Hsv;
        if (Json::getValue(hsv[F("h")], value))
            params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
        if (Json::getValue(hsv[F("s")], value))
            params.hsv.s = AbsOrRelValue(value);
        if (Json::getValue(hsv[F("v")], value))
            params.hsv.v = AbsOrRelValue(value);
        if (Json::getValue(hsv[F("ct")], value))
            params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);

        JsonObject from = hsv[F("from")];
        if (!JsonProcessor::checkUnsupportedParams(from, F("hsv.from"), {F("h"), F("s"), F("v"), F("ct")}, errorMsg))
            return false;

        if (!from.isNull()) {
            params.hasHsvFrom = true;
            if (Json::getValue(from[F("h")], value))
                params.hsv.h = AbsOrRelValue(value, AbsOrRelValue::Type::Hue);
            if (Json::getValue(from[F("s")], value))
                params.hsv.s = AbsOrRelValue(value);
            if (Json::getValue(from[F("v")], value))
                params.hsv.v = AbsOrRelValue(value);
            if (Json::getValue(from[F("ct")], value))
                params.hsv.ct = AbsOrRelValue(value, AbsOrRelValue::Type::Ct);
        }
    } else if (!root["raw"].isNull()) {
        if (!JsonProcessor::checkUnsupportedParams(root["raw"], F("raw"),
                                                   {F("r"), F("g"), F("b"), F("ww"), F("cw"), F("from")}, errorMsg))
            return false;

        JsonObject raw = root["raw"];
        params.mode = RequestParameters::Mode::Raw;
        if (Json::getValue(raw[F("r")], value))
            params.raw.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw[F("g")], value))
            params.raw.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw[F("b")], value))
            params.raw.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw[F("ww")], value))
            params.raw.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        if (Json::getValue(raw[F("cw")], value))
            params.raw.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);

        JsonObject from = raw[F("from")];

        if (!from.isNull()) {
            if (!JsonProcessor::checkUnsupportedParams(from, F("raw.from"), {F("r"), F("g"), F("b"), F("ww"), F("cw")},
                                                       errorMsg))
                return false;

            params.hasRawFrom = true;
            if (Json::getValue(from[F("r")], value))
                params.rawFrom.r = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from[F("g")], value))
                params.rawFrom.g = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from[F("b")], value))
                params.rawFrom.b = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from[F("ww")], value))
                params.rawFrom.ww = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
            if (Json::getValue(from[F("cw")], value))
                params.rawFrom.cw = AbsOrRelValue(value, AbsOrRelValue::Type::Raw);
        }
    }

    if (root.containsKey("t") && root.containsKey("s")) {
        errorMsg = "Cannot use both t (time) and s (speed) parameters simultaneously!";
        return false;
    }

    if (Json::getValue(root[F("t")], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Time;
    }

    if (Json::getValue(root[F("s")], params.ramp.value)) {
        params.ramp.type = RampTimeOrSpeed::Type::Speed;
    }

    Json::getValue(root[F("stay")], params.stay);

    if (!root[F("r")].isNull()) {
        params.requeue = root[F("r")].as<bool>();
    }

    String direction;
    if (Json::getValue(root[F("d")], direction)) {
        if (direction == F("short")) {
            params.direction = HueTransitionDirection::dir_short;
        } else if (direction == F("long")) {
            params.direction = HueTransitionDirection::dir_long;
        } else {
            errorMsg = F("Invalid hue direction");
            return false;
        }
    }

    Json::getValue(root[F("name")], params.name);

    if (!root[F("q")].isNull()) {
        String q = root[F("q")];
        if (q == F("back"))
            params.queue = QueuePolicy::Back;
        else if (q == F("front"))
            params.queue = QueuePolicy::Front;
        else if (q == F("front_reset"))
            params.queue = QueuePolicy::FrontReset;
        else if (q == F("single"))
            params.queue = QueuePolicy::Single;
        else {
            params.queue = QueuePolicy::Invalid;
        }
    }

    JsonArray arr;
    if (Json::getValue(root[F("channels")], arr)) {
        debug_w("JsonProcessor::parseRequestParams - channels specified: %d\n", arr.size());
        for (size_t i = 0; i < arr.size(); ++i) {
            // checkUnsupportedParams(arr, "channels", {"h", "s", "v", "ct", "r", "g", "b", "ww", "cw"});

            String str = arr[i];
            if (str == F("h")) {
                params.channels.add(CtrlChannel::Hue);
            } else if (str == F("s")) {
                params.channels.add(CtrlChannel::Sat);
            } else if (str == F("v")) {
                params.channels.add(CtrlChannel::Val);
            } else if (str == F("ct")) {
                params.channels.add(CtrlChannel::ColorTemp);
            } else if (str == F("r")) {
                params.channels.add(CtrlChannel::Red);
            } else if (str == F("g")) {
                params.channels.add(CtrlChannel::Green);
            } else if (str == F("b")) {
                params.channels.add(CtrlChannel::Blue);
            } else if (str == F("ww")) {
                params.channels.add(CtrlChannel::WarmWhite);
            } else if (str == F("cw")) {
                params.channels.add(CtrlChannel::ColdWhite);
            }
        }
    }

    return true;
}

bool JsonProcessor::RequestParameters::checkParams(String& errorMsg, const ApplicationSettings& settings) const {
    if (mode == Mode::Hsv) {
        if (hsv.ct.hasValue() && hsv.ct.getValue().getMode() == AbsOrRelValue::Mode::Absolute &&
            !settings.isColortempInRange(hsv.ct.getValue())) {
            errorMsg = F("ct param out of range");
            return false;
        }

        if (!hsv.h.hasValue() && !hsv.s.hasValue() && !hsv.v.hasValue() && !hsv.ct.hasValue()) {
            errorMsg = F("Need at least one HSVCT component!");
            return false;
        }
    } else if (mode == Mode::Raw) {
        if (!raw.r.hasValue() && !raw.g.hasValue() && !raw.b.hasValue() && !raw.ww.hasValue() && !raw.cw.hasValue()) {
            errorMsg = F("Need at least one RAW component!");
            return false;
        }
    }

    if (queue == QueuePolicy::Invalid) {
        errorMsg = F("Invalid queue policy");
        return false;
    }

    if (ramp.type == RampTimeOrSpeed::Type::Speed && ramp.value == 0) {
        errorMsg = F("Speed cannot be 0!");
        return false;
    }

    return true;
}

bool JsonProcessor::onJsonRpc(const String& json, String& errorMsg) {
    debug_d("JsonProcessor::onJsonRpc: %s\n", json.c_str());
    JsonRpcMessageIn rpc(json);

    String method = rpc.getMethod();
    if (method == F("color")) {
        return onColor(rpc.getParams(), errorMsg);
    } else if (method == F("stop")) {
        return onStop(rpc.getParams(), errorMsg);
    } else if (method == F("blink")) {
        return onBlink(rpc.getParams(), errorMsg);
    } else if (method == F("skip")) {
        return onSkip(rpc.getParams(), errorMsg);
    } else if (method == F("pause")) {
        return onPause(rpc.getParams(), errorMsg);
    } else if (method == F("continue")) {
        return onContinue(rpc.getParams(), errorMsg);
    } else if (method == F("toggle")) {
        return onToggle(rpc.getParams(), errorMsg);
    } else {
        return false;
    }
}

void JsonProcessor::addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels) {
    switch (app.rgbwwctrl.getMode()) {
    case RGBWWLed::ColorMode::Hsv: {
        const HSVCT& c = app.rgbwwctrl.getCurrentColor();
        JsonObject obj = root.createNestedObject(F("hsv"));
        if (channels.count() == 0 || channels.contains(CtrlChannel::Hue))
            obj[F("h")] = (float(c.h) / float(RGBWW_CALC_HUEWHEELMAX)) * 360.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Sat))
            obj[F("s")] = (float(c.s) / float(RGBWW_CALC_MAXVAL)) * 100.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Val))
            obj[F("v")] = (float(c.v) / float(RGBWW_CALC_MAXVAL)) * 100.0;
        if (channels.count() == 0 || channels.contains(CtrlChannel::ColorTemp))
            obj[F("ct")] = c.ct;
        break;
    }
    case RGBWWLed::ColorMode::Raw: {
        const ChannelOutput& c = app.rgbwwctrl.getCurrentOutput();
        JsonObject obj = root.createNestedObject(F("raw"));
        if (channels.count() == 0 || channels.contains(CtrlChannel::Red))
            obj[F("r")] = c.r;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Green))
            obj[F("g")] = c.g;
        if (channels.count() == 0 || channels.contains(CtrlChannel::Blue))
            obj[F("b")] = c.b;
        if (channels.count() == 0 || channels.contains(CtrlChannel::WarmWhite))
            obj[F("ww")] = c.ww;
        if (channels.count() == 0 || channels.contains(CtrlChannel::ColdWhite))
            obj[F("cw")] = c.cw;
        break;
    }
    }
}
