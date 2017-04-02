#include "jsonrpcmessage.h"


JsonRpcMessage::JsonRpcMessage(const String& name) {
    JsonObject& json = _stream.getRoot();
    json["jsonrpc"] = "2.0";
    json["method"] = name;

    _pParams = &json.createNestedObject("params");
}

JsonObjectStream& JsonRpcMessage::getStream() {
    return _stream;
}

JsonObject& JsonRpcMessage::getParams() {
    return *_pParams;
}

void JsonRpcMessage::setId(int id) {
    JsonObject& json = _stream.getRoot();
    json["id"] = id;
}
