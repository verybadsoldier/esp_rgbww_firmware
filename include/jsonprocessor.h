#pragma once

#include <RGBWWLed/RGBWWLedColor.h>


class JsonProcessor {
public:
    bool onColor(const String& json, String& msg, bool relay = true);
    bool onColor(JsonObject root, String& msg, bool relay = true);

    bool onStop(const String& json, String& msg, bool relay = true);
    bool onStop(JsonObject root, String& msg, bool relay = true);

    bool onSkip(const String& json, String& msg, bool relay = true);
    bool onSkip(JsonObject root, String& msg, bool relay = true);

    bool onPause(const String& json, String& msg, bool relay = true);
    bool onPause(JsonObject json, String& msg, bool relay = true);

    bool onContinue(const String& json, String& msg, bool relay = true);
    bool onContinue(JsonObject root, String& msg, bool relay = true);

    bool onBlink(const String& json, String& msg, bool relay = true);
    bool onBlink(JsonObject root, String& msg, bool relay = true);

    bool onToggle(const String& json, String& msg, bool relay = true);
    bool onToggle(JsonObject root, String& msg, bool relay = true);

    bool onDirect(const String& json, String& msg, bool relay);
    bool onDirect(JsonObject root, String& msg, bool relay);

    bool onJsonRpc(const String& json);

private:

    struct RequestParameters {
        String target;

        enum class Mode {
            Undefined,
            Hsv,
            Raw,
            Kelvin,
        };

        Mode mode = Mode::Undefined;

        bool hasHsvFrom = false;
        bool hasRawFrom = false;

        RequestHSVCT hsv;
        RequestHSVCT hsvFrom;

        RequestChannelOutput raw;
        RequestChannelOutput rawFrom;

        int direction = 1;
        bool requeue = false;
        RampTimeOrSpeed ramp = 0;
        String name;

        String cmd = "solid";

        RGBWWLed::ChannelList channels;

        QueuePolicy queue = QueuePolicy::Single;

        int checkParams(String& errorMsg) const;
    };

    void parseRequestParams(JsonObject root, RequestParameters& params);
    void addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels);

    bool onSingleColorCommand(JsonObject root, String& errorMsg);
};
