#pragma once

#include <RGBWWLed/RGBWWLedColor.h>

#include <set>

class JsonProcessor {
  public:
    JsonProcessor(const ApplicationSettings& settings) : _settings(settings) {}

    bool onColor(const String& json, String& msg);
    bool onColor(JsonObject root, String& msg);

    bool onStop(const String& json, String& msg);
    bool onStop(JsonObject root, String& msg);

    bool onSkip(const String& json, String& msg);
    bool onSkip(JsonObject root, String& msg);

    bool onPause(const String& json, String& msg);
    bool onPause(JsonObject json, String& msg);

    bool onContinue(const String& json, String& msg);
    bool onContinue(JsonObject root, String& msg);

    bool onBlink(const String& json, String& msg);
    bool onBlink(JsonObject root, String& msg);

    bool onToggle(const String& json, String& msg);
    bool onToggle(JsonObject root, String& msg);

    bool onJsonRpc(const String& json, String& errorMsg);

  private:
    static bool checkUnsupportedParams(JsonObject obj, const String& rootName, const std::set<String>& allowed,
                                       String& errorMsg);

    const ApplicationSettings& _settings;
    static const int _jsonDocumentMaxSize = 1024;

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
        int stay = 0;
        String name;

        RGBWWLed::ChannelList channels;

        QueuePolicy queue = QueuePolicy::Single;

        bool checkParams(String& errorMsg, const ApplicationSettings& settings) const;
    };

    bool parseRequestParams(JsonObject root, RequestParameters& params, String& errorMsg);
    void addChannelStatesToCmd(JsonObject root, const RGBWWLed::ChannelList& channels);

    bool onSingleColorCommand(JsonObject root, String& errorMsg);
};
