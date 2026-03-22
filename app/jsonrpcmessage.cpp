#include "jsonrpcmessage.h"

JsonRpcMessage::JsonRpcMessage(const String& name, size_t capacity) : _stream(capacity) {
    JsonObject json = _stream.getRoot();
    json[F("jsonrpc")] = F("2.0");
    json[F("method")] = name;
}

JsonObjectStream& JsonRpcMessage::getStream() {
    return _stream;
}

JsonObject JsonRpcMessage::getParams() {
    if (_pParams.isNull()) {
        _pParams = _stream.getRoot().createNestedObject(F("params"));
    }
    return _pParams;
}

JsonObject JsonRpcMessage::getRoot() {
    return _stream.getRoot();
}

void JsonRpcMessage::setId(int id) {
    JsonObject json = _stream.getRoot();
    json[F("id")] = id;
}

////////////////////////////////////////

JsonRpcMessageIn::JsonRpcMessageIn(const String& json, size_t capacity) : _doc(capacity) {
    Json::deserialize(_doc, json);
}

JsonObject JsonRpcMessageIn::getParams() {
    return _doc[F("params")];
}

JsonObject JsonRpcMessageIn::getRoot() {
    return _doc.as<JsonObject>();
}

String JsonRpcMessageIn::getMethod() {
    return getRoot()[F("method")];
}
