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
    debug_i("Starting event server\n");
    setTimeOut(_connectionTimeout);
    if (not listen(_tcpPort)) {
        debug_e("EventServer failed to open listening port!");
    }

    auto fnc = TimerDelegate(&EventServer::publishKeepAlive, this);
    _keepAliveTimer.initializeMs(_keepAliveInterval * 1000, fnc).start();
}

void EventServer::stop() {
    if (not active)
        return;

    shutdown();
}

void EventServer::onClient(TcpClient *client) {
    TcpServer::onClient(client);
    debug_d("Client connected from: %s\n", client->getRemoteIp().toString().c_str());
}

void EventServer::onClientComplete(TcpClient& client, bool succesfull) {
    TcpServer::onClientComplete(client, succesfull);
    debug_d("Client removed: %x\n", &client);
}

void EventServer::publishCurrentState(const ChannelOutput& raw, const HSVCT* pHsv) {
    if (raw == _lastRaw)
        return;
    _lastRaw = raw;

    JsonRpcMessage msg("color_event");
    JsonObject root = msg.getParams();

    root["mode"] = pHsv ? "hsv" : "raw";

    JsonObject rawJson = root.createNestedObject("raw");
    rawJson["r"] = raw.r;
    rawJson["g"] = raw.g;
    rawJson["b"] = raw.b;
    rawJson["ww"] = raw.ww;
    rawJson["cw"] = raw.cw;

    if (pHsv) {
        float h, s, v;
        int ct;
        pHsv->asRadian(h, s, v, ct);

        JsonObject hsvJson = root.createNestedObject("hsv");
        hsvJson["h"] = h;
        hsvJson["s"] = s;
        hsvJson["v"] = v;
        hsvJson["ct"] = ct;
    }

    debug_d("EventServer::publishCurrentHsv\n");

    sendToClients(msg);
}

void EventServer::publishClockSlaveStatus(int offset, uint32_t interval) {
    debug_d("EventServer::publishClockSlaveStatus: offset: %d | interval :%d\n", offset, interval);

    JsonRpcMessage msg("clock_slave_status");
    JsonObject root = msg.getParams();
    root["offset"] = offset;
    root["current_interval"] = interval;
    sendToClients(msg);
}

void EventServer::publishKeepAlive() {
    debug_d("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg("keep_alive");
    sendToClients(msg);
}

void EventServer::publishTransitionFinished(const String& name, bool requeued) {
    debug_d("EventServer::publishTransitionComplete: %s\n", name.c_str());

    JsonRpcMessage msg("transition_finished");
    JsonObject root = msg.getParams();
    root["name"] = name;
    root["requeued"] = requeued;

    sendToClients(msg);
}

void EventServer::sendToClients(JsonRpcMessage& rpcMsg) {
    //Serial.printf("EventServer: sendToClient: %x, Vector: %x Tests: %d\n", _client, _clients.elementAt(0), _tests[0]);
    rpcMsg.setId(_nextId++);

    String jsonStr = Json::serialize(rpcMsg.getRoot());

    for(unsigned i=0; i < connections.size(); ++i) {
        auto pClient = reinterpret_cast<TcpClient*>(connections[i]);
        pClient->sendString(jsonStr);
    }
}
