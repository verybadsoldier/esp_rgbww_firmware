/*
 * eventserver.cpp
 *
 *  Created on: 26.03.2017
 *      Author: Robin
 */
#include <RGBWWCtrl.h>

EventServer::EventServer(std::function<void()> connectCallback) : _connectCallback(connectCallback) {}

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

void EventServer::onClient(TcpClient* client) {
    TcpServer::onClient(client);
    debug_e("Client connected from: %s\n", client->getRemoteIp().toString().c_str());
    this->_connectCallback();
}

void EventServer::onClientComplete(TcpClient& client, bool succesfull) {
    TcpServer::onClientComplete(client, succesfull);
    debug_d("Client removed: %x\n", &client);
}

void EventServer::publishColorEvent(const ChannelOutput& raw, const HSVCT* pHsv, bool force) {
    if (!force && (raw == _lastRaw && (pHsv == nullptr || *pHsv == _lastHsv)))
        return;

    _lastRaw = raw;

    if (pHsv != nullptr) {
        _lastHsv = *pHsv;
    }

    JsonRpcMessage msg(F("color_event"));
    JsonObject root = msg.getParams();

    root[F("mode")] = pHsv ? F("hsv") : F("raw");

    JsonObject rawJson = root.createNestedObject(F("raw"));
    rawJson[F("r")] = raw.r;
    rawJson[F("g")] = raw.g;
    rawJson[F("b")] = raw.b;
    rawJson[F("ww")] = raw.ww;
    rawJson[F("cw")] = raw.cw;

    if (pHsv) {
        float h, s, v;
        int ct;
        pHsv->asRadian(h, s, v, ct);

        JsonObject hsvJson = root.createNestedObject(F("hsv"));
        hsvJson[F("h")] = h;
        hsvJson[F("s")] = s;
        hsvJson[F("v")] = v;
        hsvJson[F("ct")] = ct;
    }

    debug_d("EventServer::publishColorEvent\n");

    sendToClients(msg);
}

void EventServer::publishConfigEvent(const JsonObject& jsonObj) {
    JsonRpcMessage msg(F("config"), CONFIG_MAX_LENGTH);
    JsonObject root = msg.getParams();

    root.set(jsonObj);

    debug_d("EventServer::publishConfigEvent\n");

    sendToClients(msg);
}

void EventServer::publishInfo(std::shared_ptr<JsonObjectStream> pInfo) {
    JsonRpcMessage msg(F("info"));
    JsonObject root = msg.getParams();

    root.set(pInfo->getRoot());

    debug_d("EventServer::publishInfo\n");

    sendToClients(msg);
}

void EventServer::publishStateCompleted() {
    JsonRpcMessage msg(F("state_completed"));

    debug_d("EventServer::publishStateCompleted\n");

    sendToClients(msg);
}

void EventServer::publishClockSlaveStatus(int offset, uint32_t interval) {
    debug_d("EventServer::publishClockSlaveStatus: offset: %d | interval :%d\n", offset, interval);

    JsonRpcMessage msg(F("clock_slave_status"));
    JsonObject root = msg.getParams();
    root[F("offset")] = offset;
    root[F("current_interval")] = interval;
    sendToClients(msg);
}

void EventServer::publishKeepAlive() {
    debug_d("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg(F("keep_alive"));
    sendToClients(msg);
}

void EventServer::publishTransitionFinished(const String& name, bool requeued) {
    debug_d("EventServer::publishTransitionComplete: %s\n", name.c_str());

    JsonRpcMessage msg(F("transition_finished"));
    JsonObject root = msg.getParams();
    root[F("name")] = name;
    root[F("requeued")] = requeued;

    sendToClients(msg);
}

void EventServer::sendToClients(JsonRpcMessage& rpcMsg) {
    // Serial.printf("EventServer: sendToClient: %x, Vector: %x Tests: %d\n", _client, _clients.elementAt(0),
    // _tests[0]);
    rpcMsg.setId(_nextId++);

    String jsonStr = Json::serialize(rpcMsg.getRoot());

    for (unsigned i = 0; i < connections.size(); ++i) {
        auto pClient = reinterpret_cast<TcpClient*>(connections[i]);
        pClient->sendString(jsonStr);
    }
}
