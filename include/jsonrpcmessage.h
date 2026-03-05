#pragma once

#include <JsonObjectStream.h>
#include <RGBWWLed/RGBWWLed.h>

class JsonRpcMessage {
  public:
    JsonRpcMessage(const String& name, size_t capacity = 1024);
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
    JsonRpcMessageIn(const String& json, size_t capacity = 1024);
    JsonObject getParams();

    JsonObject getRoot();
    String getMethod();

  private:
    DynamicJsonDocument _doc;
};