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

JsonObject& JsonRpcMessage::getRoot() {
	return _stream.getRoot();
}

void JsonRpcMessage::setId(int id) {
    JsonObject& json = _stream.getRoot();
    json["id"] = id;
}

////////////////////////////////////////

JsonRpcMessageIn::JsonRpcMessageIn(const String& json) {
	_root = &_jsonBuffer.parseObject(json);
}

JsonObject& JsonRpcMessageIn::getParams() {
    return getRoot()["params"];
}

JsonObject& JsonRpcMessageIn::getRoot() {
	return *_root;
}

String JsonRpcMessageIn::getMethod() {
	return getRoot()["method"];
}
