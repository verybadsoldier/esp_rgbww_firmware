#pragma once

#include <SmingCore/SmingCore.h>

#include <RGBWWLed/RGBWWLed.h>


class JsonRpcMessage {
public:
    JsonRpcMessage(const String& name);
    JsonObjectStream& getStream();
    void setId(int id);
    JsonObject& getParams();

private:
    const String _data;

    JsonObjectStream _stream;
    JsonObject* _pParams;
};

