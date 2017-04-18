#include <RGBWWCtrl.h>


bool JsonProcessor::onColor(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onColor(root, msg);
}

bool JsonProcessor::onColor(JsonObject& root, String& msg) {
    if (root["cmds"].success()) {
        Vector<String> errors;
        // multi command post (needs testing)
        const JsonArray& cmds = root["cmds"].asArray();
        for(int i=0; i < cmds.size(); ++i) {
            String msg;
            if (!onSingleColorCommand(cmds[i], msg)) {
                errors.add(msg);
            }
        }

        if (errors.size() == 0)
            return true;
        else {
            String msg;
            for (int i=0; i < errors.size(); ++i) {
                msg += errors[i] + "|";
            }
            return false;
        }
    }
    else {
        if (onSingleColorCommand(root, msg)) {
            return true;
        }
        else {
            return false;
        }
    }

    app.onCommand("color", root);
}

bool JsonProcessor::onStop(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onStop(root, msg);
}

bool JsonProcessor::onStop(JsonObject& root, String& msg) {
    RequestParameters params;
    JsonProcessor::parseChannelRequestParams(root, params);
    app.rgbwwctrl.clearAnimationQueue(params.channels);
    app.rgbwwctrl.skipAnimation(params.channels);
    return true;
}

bool JsonProcessor::onSkip(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onSkip(root, msg);
}

bool JsonProcessor::onSkip(JsonObject& root, String& msg) {
    RequestParameters params;
    JsonProcessor::parseChannelRequestParams(root, params);
    app.rgbwwctrl.skipAnimation(params.channels);
    return true;
}

bool JsonProcessor::onPause(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onPause(root, msg);
}

bool JsonProcessor::onPause(JsonObject& root, String& msg) {
    RequestParameters params;
    JsonProcessor::parseChannelRequestParams(root, params);
    app.rgbwwctrl.pauseAnimation(params.channels);
    return true;
}

bool JsonProcessor::onContinue(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onContinue(root, msg);
}

bool JsonProcessor::onContinue(JsonObject& json, String& msg) {
    RequestParameters params;
    JsonProcessor::parseChannelRequestParams(json, params);
    app.rgbwwctrl.continueAnimation(params.channels);
    return true;
}

bool JsonProcessor::onBlink(const String& json, String& msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return onBlink(root, msg);
}

bool JsonProcessor::onBlink(JsonObject& json, String& msg) {
    RequestParameters params;
    JsonProcessor::parseChannelRequestParams(json, params);
    app.rgbwwctrl.blink(params.channels, params.time);
    return true;
}

bool JsonProcessor::onSingleColorCommand(JsonObject& root, String& errorMsg) {
    RequestParameters params;
    parseColorRequestParams(root, params);
    {
        if (params.checkParams(errorMsg) != 0) {
            return false;
        }
    }

    bool queueOk = false;
    if (params.mode == RequestParameters::Mode::Kelvin) {
        //TODO: hand to rgbctrl
    } else if (params.mode == RequestParameters::Mode::Hsv) {
        if(!params.hasHsvFrom) {
            debugapp("ApplicationWebserver::onColor hsv CMD:%s t:%d Q:%d  h:%d s:%d v:%d ct:%d ", params.cmd.c_str(), params.time, params.queue, params.hsv.h.getValue().getValue(), params.hsv.s.getValue().getValue(), params.hsv.v.getValue().getValue(), params.hsv.ct.getValue().getValue());
            if (params.cmd == "fade") {
                queueOk = app.rgbwwctrl.fadeHSV(params.hsv, params.time, params.direction, params.queue, params.requeue, params.name);
            } else {
                queueOk = app.rgbwwctrl.setHSV(params.hsv, params.time, params.queue, params.requeue, params.name);
            }
        } else {
            app.rgbwwctrl.fadeHSV(params.hsvFrom, params.hsv, params.time, params.direction, params.queue);
        }
    } else if (params.mode == RequestParameters::Mode::Raw) {
        if(!params.hasRawFrom) {
            debugapp("ApplicationWebserver::onColor raw CMD:%s Q:%d r:%i g:%i b:%i ww:%i cw:%i", params.cmd.c_str(), params.queue, params.raw.r.getValue().getValue(), params.raw.g.getValue().getValue(), params.raw.b.getValue().getValue(), params.raw.ww.getValue().getValue(), params.raw.cw.getValue().getValue());
            if (params.cmd == "fade") {
                queueOk = app.rgbwwctrl.fadeRAW(params.raw, params.time, params.queue);
            } else {
                queueOk = app.rgbwwctrl.setRAW(params.raw, params.time, params.queue);
            }
        } else {
            debugapp("ApplicationWebserver::onColor raw CMD:%s Q:%d FROM r:%i g:%i b:%i ww:%i cw:%i  TO r:%i g:%i b:%i ww:%i cw:%i",
                    params.raw.r.getValue().getValue(), params.raw.g.getValue().getValue(), params.raw.b.getValue().getValue(), params.raw.ww.getValue().getValue(), params.raw.cw.getValue().getValue(),
                    params.rawFrom.r.getValue().getValue(), params.rawFrom.g.getValue().getValue(), params.rawFrom.b.getValue().getValue(), params.rawFrom.ww.getValue().getValue(), params.rawFrom.cw.getValue().getValue());
            app.rgbwwctrl.fadeRAW(params.rawFrom, params.raw, params.time, params.queue);
        }
    } else {
        errorMsg = "No color object!";
        return false;
    }

    if (!queueOk)
        errorMsg = "Queue full";
    return queueOk;
}


void JsonProcessor::parseChannelRequestParams(JsonObject& root, RequestParameters& params) {
    params.time = 100;

    RGBWWLed::ChannelList channels;
    if (root["channels"].success()) {
        const JsonArray& arr = root["channels"].asArray();
        for(size_t i=0; i < arr.size(); ++i) {
            const String& str = arr[i].asString();
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
        }
    }

    if (root["t"].success()) {
        params.time = root["t"].as<int>();
    }
}

void JsonProcessor::parseColorRequestParams(JsonObject& root, RequestParameters& params) {
    if (root["hsv"].success()) {
        params.mode = RequestParameters::Mode::Hsv;
        if (root["hsv"]["h"].success())
            params.hsv.h = AbsOrRelValue(root["hsv"]["h"].asString(), AbsOrRelValue::Type::Hue);
        if (root["hsv"]["s"].success())
            params.hsv.s = AbsOrRelValue(root["hsv"]["s"].asString());
        if (root["hsv"]["v"].success())
            params.hsv.v = AbsOrRelValue(root["hsv"]["v"].asString());
        if (root["hsv"]["ct"].success())
            params.hsv.ct = AbsOrRelValue(root["hsv"]["ct"].asString(), AbsOrRelValue::Type::Ct);

        if (root["hsv"]["from"].success()) {
            params.hasHsvFrom = true;
            if (root["hsv"]["from"]["h"].success())
                params.hsv.h = AbsOrRelValue(root["hsv"]["from"]["h"].asString(), AbsOrRelValue::Type::Hue);
            if (root["hsv"]["from"]["s"].success())
                params.hsv.s = AbsOrRelValue(root["hsv"]["from"]["s"].asString());
            if (root["hsv"]["from"]["v"].success())
                params.hsv.v = AbsOrRelValue(root["hsv"]["from"]["v"].asString());
            if (root["hsv"]["from"]["ct"].success())
                params.hsv.ct = AbsOrRelValue(root["hsv"]["from"]["ct"].asString(), AbsOrRelValue::Type::Ct);
        }
    }
    else if (root["raw"].success()) {
        params.mode = RequestParameters::Mode::Raw;
        if (root["raw"]["r"].success())
            params.raw.r = AbsOrRelValue(root["raw"]["r"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["g"].success())
            params.raw.g = AbsOrRelValue(root["raw"]["g"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["b"].success())
            params.raw.b = AbsOrRelValue(root["raw"]["b"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["ww"].success())
            params.raw.ww = AbsOrRelValue(root["raw"]["ww"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["cw"].success())
            params.raw.cw = AbsOrRelValue(root["raw"]["cw"].asString(), AbsOrRelValue::Type::Raw);

        if (root["raw"]["from"].success()) {
            params.hasRawFrom = true;
            if (root["raw"]["from"]["r"].success())
                params.rawFrom.r = AbsOrRelValue(root["raw"]["from"]["r"].asString(), AbsOrRelValue::Type::Raw);
            if (root["raw"]["from"]["g"].success())
                params.rawFrom.g = AbsOrRelValue(root["raw"]["from"]["g"].asString(), AbsOrRelValue::Type::Raw);
            if (root["raw"]["from"]["b"].success())
                params.rawFrom.b = AbsOrRelValue(root["raw"]["from"]["b"].asString(), AbsOrRelValue::Type::Raw);
            if (root["raw"]["from"]["ww"].success())
                params.rawFrom.ww = AbsOrRelValue(root["raw"]["from"]["ww"].asString(), AbsOrRelValue::Type::Raw);
            if (root["raw"]["from"]["cw"].success())
                params.rawFrom.cw = AbsOrRelValue(root["raw"]["from"]["cw"].asString(), AbsOrRelValue::Type::Raw);
        }
    }

    if (root["kelvin"].success()) {
        params.mode = RequestParameters::Mode::Kelvin;
    }

    if (root["t"].success()) {
        params.time = root["t"].as<int>();
    }

    if (root["r"].success()) {
        params.requeue = root["r"].as<int>() == 1;
    }

    if (root["kelvin"].success()) {
        params.kelvin = root["kelvin"].as<int>();
    }

    if (root["d"].success()) {
        params.direction = root["d"].as<int>();
    }

    if (root["name"].success()) {
        params.name = root["name"].asString();
    }

    if (root["cmd"].success()) {
        params.cmd = root["cmd"].asString();
    }

    if (root["q"].success()) {
        const String& q = root["q"].asString();
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

    return 0;
}

bool JsonProcessor::onJsonRpc(const String& json) {
	JsonRpcMessageIn rpc(json);

	String msg;
	if (rpc.getMethod() == "color") {
		return onColor(rpc.getRoot(), msg);
	}
}
