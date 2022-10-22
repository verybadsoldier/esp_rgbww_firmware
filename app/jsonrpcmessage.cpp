#include "jsonrpcmessage.h"


JsonRpcMessage::JsonRpcMessage(const String& name) {
    JsonObject json = _stream.getRoot();
    json["jsonrpc"] = "2.0";
    json["method"] = name;
}

JsonObjectStream& JsonRpcMessage::getStream() {
    return _stream;
}

JsonObject JsonRpcMessage::getParams() {
    if (_pParams.isNull()) {
        _pParams = _stream.getRoot().createNestedObject("params");
    }
    return _pParams;
}

JsonObject JsonRpcMessage::getRoot() {
    return _stream.getRoot();
}

void JsonRpcMessage::setId(int id) {
    JsonObject json = _stream.getRoot();
    json["id"] = id;
}

////////////////////////////////////////

JsonRpcMessageIn::JsonRpcMessageIn(const String& json) : _doc(1024) {
	Json::deserialize(_doc, json);
}

JsonObject JsonRpcMessageIn::getParams() {
    return _doc["params"];
}

JsonObject JsonRpcMessageIn::getRoot() {
    return _doc.as<JsonObject>();
}

String JsonRpcMessageIn::getMethod() {
    return getRoot()["method"];
}
