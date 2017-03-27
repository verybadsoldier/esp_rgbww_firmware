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

	Serial.printf("Starting event server");

	if (not listen(_tcpPort)) {
		//throw std::runtime_error("EventServer failed to open listening port!");
	}

	_running = true;
}

void EventServer::stop() {
	if (not _running)
		return;

	close();
	_running = false;
}

void EventServer::onClient(TcpClient *client) {
	Debug.printf("Client connected from: %s", client->getRemoteIp().toString().c_str());
	_clients.addElement(&client);
//	Serial.printf("Connection accepted");
//	_clients[0]->writeString("{Connection accepted}");
	_client  = client;
}

void EventServer::onClientComplete(TcpClient& client, bool succesfull) {
	Debug.printf("Client removed");
//	_clients.removeElement(&client);
}

void EventServer::publishCurrentColor(const HSVCT& color) {
	JsonObjectStream stream;
	JsonObject& json = stream->getRoot();

	JsonObject cc = json.createNestedObject("color_event");
	cc['h'] = "100.0";
	cc['s'] = "100.0";
	cc['v'] = "100.0";
	cc['ct'] = "100.0";

	sendToClients(&stream);
}

void EventServer::publishTransitionComplete(const HSVCT& finalColor) {
	JsonObjectStream stream;
	JsonObject& json = stream->getRoot();

	JsonObject cc = json.createNestedObject("transition_complete");
//	cc['hsv'] =

	sendToClients(&stream);
}

void EventServer::sendToClients(IDataSourceStream* stream) {
	if (_client == nullptr)
		return;

	Serial.printf("EventServer: sendToClient: %d, Vector: %d", _client, _clients.elementAt(0));
	//_client->write(stream);
	//_clients[0]->writeString(stream);

//	for(int i=0; i < _clients.size(); ++i) {
//		Serial.printf("EventServer: sendToClient");
//		if (_clients[i]->write(stream) == -1) {
//			Debug.printf("Sending to client failed, removing...");
//			_clients.remove(i);
//		}
//		else
//			++i;
//		_clients[0]->writeString("{DATA}");
//	}
}


/**
 * ColorEventPublisher
 */
void ColorEventPublisher::~ColorEventPublisher() {
	stop();
}

void ColorEventPublisher::init(EventServer& evServer, RGBWWLed& rgbctrl) {
	_eventServer = &evServer;
	_rgbCtrl = &rgbctrl;
}

void ColorEventPublisher::start() {
	if (_running)
		return;

	Serial.printf("Starting ColorEventPublisher");

	_running = true;

	Serial.printf("Starting event timer");
	_ledTimer.initializeMs(2000, TimerDelegate(&ColorEventPublisher::publishState, this)).start();
}

void ColorEventPublisher::stop() {
	if (not _running)
		return;

	_ledTimer.stop();

	_running = false;
}

void ColorEventPublisher::publishState() {
	const HSVCT& curColor = _rgbCtrl->getCurrentColor();

	if (curColor == _lastColor)
		return;

	_eventServer->publishCurrentColor(curColor);
}
