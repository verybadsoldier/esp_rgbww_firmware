#pragma once

#include <RGBWWLed/RGBWWLed.h>


class JsonRpcMessage {
public:
    JsonRpcMessage(const String& name);
    JsonObjectStream& getStream();
    void setId(int id);
    JsonObject getParams();
    JsonObject getRoot();

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

