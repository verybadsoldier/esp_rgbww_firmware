#pragma once

#include <SmingCore/SmingCore.h>
#include <RGBWWLed/RGBWWLedColor.h>


class JsonProcessor {
public:
    bool onColor(const String& json, String& msg, bool relay = true);
    bool onColor(JsonObject& root, String& msg, bool relay = true);

    bool onStop(const String& json, String& msg, bool relay = true);
    bool onStop(JsonObject& root, String& msg, bool relay = true);

    bool onSkip(const String& json, String& msg, bool relay = true);
    bool onSkip(JsonObject& root, String& msg, bool relay = true);

    bool onPause(const String& json, String& msg, bool relay = true);
    bool onPause(JsonObject& root, String& msg, bool relay = true);

    bool onContinue(const String& json, String& msg, bool relay = true);
    bool onContinue(JsonObject& root, String& msg, bool relay = true);

    bool onBlink(const String& json, String& msg, bool relay = true);
    bool onBlink(JsonObject& root, String& msg, bool relay = true);

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

        int kelvin;

        int direction = 1;
        bool requeue = false;
        int time = 0;
        String name;

        String cmd = "solid";

        RGBWWLed::ChannelList channels;

        QueuePolicy queue = QueuePolicy::Single;

        int checkParams(String& errorMsg) const;
    };

    void parseColorRequestParams(JsonObject& root, RequestParameters& params);
    void parseChannelRequestParams(JsonObject& root, RequestParameters& params);

    bool onSingleColorCommand(JsonObject& root, String& errorMsg);
};
