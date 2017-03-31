/*
 * eventserver.cpp
 *
 *  Created on: 26.03.2017
 *      Author: Robin
 */
#include "eventserver.h"
#include <SmingCore/Debug.h>


EventServer::~EventServer() {
	stop();
}

void EventServer::start() {
	if (_running)
		return;

	Serial.printf("Starting event server\n");

	if (not listen(_tcpPort)) {
	    Serial.printf("EventServer failed to open listening port!");
	}

	_keepAliveTimer.initializeMs(_keepAliveInterval * 1000, TimerDelegate(&EventServer::publishKeepAlive, this)).start();

	_running = true;
}

void EventServer::stop() {
	if (not _running)
		return;

	close();
	_running = false;
}

void EventServer::onClient(TcpClient *client) {
	Serial.printf("Client connected from: %s\n", client->getRemoteIp().toString().c_str());
	_clients.addElement(client);
}

void EventServer::onClientComplete(TcpClient& client, bool succesfull) {
	Serial.printf("Client removed: %x\n", &client);
	_clients.removeElement(&client);
}

void EventServer::publishCurrentColor(const HSVCT& color) {
	float h, s, v;
	int ct;
	color.asRadian(h, s, v, ct);

    JsonRpcMessage msg("color_event");
    JsonObject& root = msg.getParams();
    root["h"] = h;
    root["s"] = s;
    root["v"] = v;
    root["ct"] = ct;

	Serial.printf("EventServer::publishCurrentColor\n");

	sendToClients(msg);
}

void EventServer::publishKeepAlive() {
    Serial.printf("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg("keep_alive");
    sendToClients(msg);
}

void EventServer::publishTransitionComplete() {
	JsonRpcMessage msg("transition_finished");
	sendToClients(msg);
}

void EventServer::sendToClients(JsonRpcMessage& rpcMsg) {
	//Serial.printf("EventServer: sendToClient: %x, Vector: %x Tests: %d\n", _client, _clients.elementAt(0), _tests[0]);
    rpcMsg.setId(_nextId++);

	for(int i=0; i < _clients.size(); ++i) {
		TcpClient* pClient = (TcpClient*)_clients[i];
		Serial.printf("\tsendToClients: %x\n", pClient);
		pClient->write(&rpcMsg.getStream());
	}
}

/*
 * JsonRpcMessage
 */
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

/**
 * ColorEventPublisher
 */
ColorEventPublisher::~ColorEventPublisher() {
	stop();
}

void ColorEventPublisher::init(EventServer& evServer, RGBWWLed& rgbctrl) {
	_eventServer = &evServer;
	_rgbCtrl = &rgbctrl;
}

void ColorEventPublisher::start() {
	if (_running)
		return;

	Serial.printf("Starting ColorEventPublisher\n");

	_running = true;

	Serial.printf("Starting event timer\n");
	_ledTimer.initializeMs(2000, TimerDelegate(&ColorEventPublisher::publishCurrentColor, this)).start();
}

void ColorEventPublisher::stop() {
	if (not _running)
		return;

	_ledTimer.stop();

	_running = false;
}

void ColorEventPublisher::publishCurrentColor() {
	Serial.printf("ColorEventPublisher::publishCurrentColor\n");

	const HSVCT& curColor = _rgbCtrl->getCurrentColor();

	//if (curColor == _lastColor)
		//return;

	_eventServer->publishCurrentColor(curColor);
	_lastColor = curColor;
}
