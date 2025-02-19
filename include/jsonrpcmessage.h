#pragma once

#include <RGBWWLed/RGBWWLed.h>
#include <JsonObjectStream.h>

#define MAX_JSON_MESSAGE_LENGTH 1024
class JsonRpcMessage {
public:
    JsonRpcMessage(const String& name);
    JsonObjectStream& getStream();
    void setId(int id);
    JsonObject getParams();
    JsonObject getRoot();
    //void setParams(String params);
    void setPrarams(JsonVariant params);

private:
    JsonObjectStream _stream;
    JsonObject _pParams;
};

class JsonRpcMessageIn {
public:
    JsonRpcMessageIn(const String& json);
    JsonObject getParams();

    JsonObject getRoot();
    String getMethod();

private:
    DynamicJsonDocument _doc;
};

