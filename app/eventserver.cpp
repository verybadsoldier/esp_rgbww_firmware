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

void EventServer::publishCurrentHsv(const HSVCT& color) {
    if (color != _lastHsv) {
        float h, s, v;
        int ct;
        color.asRadian(h, s, v, ct);

        JsonRpcMessage msg("hsv_event");
        JsonObject& root = msg.getParams();
        root["h"] = h;
        root["s"] = s;
        root["v"] = v;
        root["ct"] = ct;

        Serial.printf("EventServer::publishCurrentHsv\n");

        sendToClients(msg);
    }
    _lastHsv = color;
}

void EventServer::publishClockSlaveStatus(uint32_t offset, uint32_t interval) {
    Serial.printf("EventServer::publishClockSlaveStatus: offset: %d | interval :%d\n", offset, interval);

    JsonRpcMessage msg("clock_slave_status");
    JsonObject& root = msg.getParams();
    root["offset"] = offset;
    root["current_interval"] = interval;
    sendToClients(msg);
}

void EventServer::publishCurrentRaw(const ChannelOutput& raw) {
    if (raw != _lastRaw) {
        JsonRpcMessage msg("raw_event");
        JsonObject& root = msg.getParams();
        root["r"] = raw.r;
        root["g"] = raw.g;
        root["b"] = raw.b;
        root["ww"] = raw.ww;
        root["cw"] = raw.cw;

        Serial.printf("EventServer::publishCurrentRaw\n");

        sendToClients(msg);
    }
    _lastRaw = raw;
}

void EventServer::publishKeepAlive() {
    Serial.printf("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg("keep_alive");
    sendToClients(msg);
}

void EventServer::publishTransitionComplete(const String& name) {
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
