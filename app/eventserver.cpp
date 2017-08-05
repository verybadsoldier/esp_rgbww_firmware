/*
 * eventserver.cpp
 *
 *  Created on: 26.03.2017
 *      Author: Robin
 */
#include <RGBWWCtrl.h>


EventServer::~EventServer() {
	stop();
}

void EventServer::start() {
	if (_running)
		return;

	Serial.printf("Starting event server\n");
	setTimeOut(_connectionTimeout);
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

void EventServer::publishCurrentState(const ChannelOutput& raw, const HSVCT* pHsv) {
    if (raw == _lastRaw)
        return;
    _lastRaw = raw;

    JsonRpcMessage msg("color_event");
    JsonObject& root = msg.getParams();

    root["mode"] = pHsv ? "hsv" : "raw";

    JsonObject& rawJson = root.createNestedObject("raw");
    rawJson["r"] = raw.r;
    rawJson["g"] = raw.g;
    rawJson["b"] = raw.b;
    rawJson["ww"] = raw.ww;
    rawJson["cw"] = raw.cw;

    if (pHsv) {
        float h, s, v;
        int ct;
        pHsv->asRadian(h, s, v, ct);

        JsonObject& hsvJson = root.createNestedObject("hsv");
        hsvJson["h"] = h;
        hsvJson["s"] = s;
        hsvJson["v"] = v;
        hsvJson["ct"] = ct;
    }

    Serial.printf("EventServer::publishCurrentHsv\n");

    sendToClients(msg);
}

void EventServer::publishClockSlaveStatus(uint32_t offset, uint32_t interval) {
    Serial.printf("EventServer::publishClockSlaveStatus: offset: %d | interval :%d\n", offset, interval);

    JsonRpcMessage msg("clock_slave_status");
    JsonObject& root = msg.getParams();
    root["offset"] = offset;
    root["current_interval"] = interval;
    sendToClients(msg);
}

void EventServer::publishKeepAlive() {
    Serial.printf("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg("keep_alive");
    sendToClients(msg);
}

void EventServer::publishTransitionFinished(const String& name) {
    Serial.printf("EventServer::publishTransitionComplete: %s\n", name.c_str());

	JsonRpcMessage msg("transition_finished");
	JsonObject& root = msg.getParams();
	root["name"] = name;

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
