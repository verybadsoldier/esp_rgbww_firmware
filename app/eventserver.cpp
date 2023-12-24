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

/**
 * @brief Starts the event server with the given web server reference.
 * 
 * This function initializes the event server with the provided web server reference
 * (in order to access the Websocket broadcast function,sets the connection timeout
 * and starts listening on the TCP port. It also starts a timer to periodically 
 * publish keep-alive messages.
 * 
 * @param webServer The reference to the ApplicationWebserver object.
 */
void EventServer::start(ApplicationWebserver& webServer) {
    this->webServer=&webServer;
    debug_i("Starting event server with webserver referal\n");
    setTimeOut(_connectionTimeout);
    if (not listen(_tcpPort)) {
        debug_e("EventServer failed to open listening port!");
    }
  
    auto fnc = TimerDelegate(&EventServer::publishKeepAlive, this);
    _keepAliveTimer.initializeMs(_keepAliveInterval * 1000, fnc).start();
}

/**
 * @brief Stops the EventServer.
 *
 * This function stops the EventServer if it is currently active. If the EventServer is not active, the function does nothing.
 */
void EventServer::stop() {
    if (not active)
        return;

    shutdown();
}

/**
 * @brief Handles the connection of a client to the event server.
 *
 * This function is called when a client connects to the event server. It is responsible for
 * performing any necessary setup or initialization for the client connection.
 *
 * @param client A pointer to the TcpClient object representing the connected client.
 */
void EventServer::onClient(TcpClient *client) {
    TcpServer::onClient(client);
    debug_d("Client connected from: %s\n", client->getRemoteIp().toString().c_str());
}

/**
 * @brief Handles the completion of a client connection.
 *
 * This function is called when a client connection is completed, whether it was successful or not.
 * It is an override of the TcpServer::onClientComplete() function.
 *
 * @param client The TcpClient object representing the client connection.
 * @param successful A boolean indicating whether the client connection was successful or not.
 */
void EventServer::onClientComplete(TcpClient& client, bool succesfull) {
    TcpServer::onClientComplete(client, succesfull);
    debug_d("Client removed: %x\n", &client);
}

/**
 * @brief Publishes the current state of the event server.
 *
 * This function is responsible for publishing the current state of the LED controller
 * by creating a JSON-RPC message with the appropriate parameters and sending it to
 * the connected clients.
 *
 * @param raw The ChannelOutput object representing the raw color values.
 * @param pHsv Pointer to the HSVCT object representing the HSV color values. If null,
 *             the mode will be set to "raw".
 */
void EventServer::publishCurrentState(const ChannelOutput& raw, const HSVCT* pHsv) {
    //debug_i("EventServer::publishCurrentState\n");
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

/**
 * @brief Publishes the clock slave status to the clients.
 *
 * This function creates a JSON-RPC message with the clock slave status and sends it to the clients.
 *
 * @param offset The offset value.
 * @param interval The current interval value.
 */
void EventServer::publishClockSlaveStatus(int offset, uint32_t interval) {
    debug_d("EventServer::publishClockSlaveStatus: offset: %d | interval :%d\n", offset, interval);

    JsonRpcMessage msg("clock_slave_status");
    JsonObject root = msg.getParams();
    root["offset"] = offset;
    root["current_interval"] = interval;
    sendToClients(msg);
}

/**
 * @brief Publishes a keep-alive message to the clients.
 * 
 * This function creates a JSON-RPC message with the method "keep_alive" and sends it to all connected clients.
 */
void EventServer::publishKeepAlive() {
    debug_d("EventServer::publishKeepAlive\n");

    JsonRpcMessage msg("keep_alive");
    sendToClients(msg);
}

/**
 * @brief Publishes a transition finished event.
 *
 * This function creates a JSON-RPC message with the event type "transition_finished" and sends it to the clients.
 *
 * @param name The name of the transition.
 * @param requeued Indicates whether the transition was requeued or not.
 */
void EventServer::publishTransitionFinished(const String& name, bool requeued) {
    debug_d("EventServer::publishTransitionComplete: %s\n", name.c_str());

    JsonRpcMessage msg("transition_finished");
    JsonObject root = msg.getParams();
    root["name"] = name;
    root["requeued"] = requeued;

    sendToClients(msg);
}

/**
 * @brief Sends a JSON-RPC message to all connected clients.
 *
 * This function sends a JSON-RPC message to all clients that are currently connected to the server.
 * The message is serialized into a JSON string and then sent to each client individually using the
 * `sendString` method of the `TcpClient` class. Additionally, the message is broadcasted to all
 * clients using the `wsBroadcast` method of the `webServer` object.
 * 
 * @note this is a bit of a cludge right now. I assume that mid term, I will deprecate the pure tcp 
 *      connection and only use the websocket connection. I'm keeping it for now to maintain compatibility
 *      with the fhem module
 *
 * @param rpcMsg The JSON-RPC message to be sent to the clients.
 */
void EventServer::sendToClients(JsonRpcMessage& rpcMsg) {
    //Serial.printf("EventServer: sendToClient: %x, Vector: %x Tests: %d\n", _client, _clients.elementAt(0), _tests[0]);
    rpcMsg.setId(_nextId++);

    String jsonStr = Json::serialize(rpcMsg.getRoot());
    debug_i("EventServer::sendToClients: %s\n", jsonStr.c_str());

    for(unsigned i=0; i < connections.size(); ++i) {
        auto pClient = reinterpret_cast<TcpClient*>(connections[i]);
        pClient->sendString(jsonStr);
    }

    webServer->wsBroadcast(jsonStr);
}
