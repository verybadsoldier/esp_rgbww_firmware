/**
 * @file
 * @author  Patrick Jahns http://github.com/patrickjahns
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 */
#include <RGBWWCtrl.h>
#include <Data/WebHelpers/base64.h>

#include <Network/Http/Websocket/WebsocketResource.h>
#include <Storage.h>
#include <config.h>

#define NOCACHE
#define DEBUG_OBJECT_API

ApplicationWebserver::ApplicationWebserver()
{
	_running = false;
	// keep some heap space free
	// value is a good guess and tested to not crash when issuing multiple parallel requests
	HttpServerSettings settings;
	settings.maxActiveConnections = 40;
	settings.minHeapSize = _minimumHeapAccept;
	settings.keepAliveSeconds =
		10; // do not close instantly when no transmission occurs. some clients are a bit slow (like FHEM)
	configure(settings);

	// workaround for bug in Sming 3.5.0
	// https://github.com/SmingHub/Sming/issues/1236
	setBodyParser("*", bodyToStringParser);
}

void ApplicationWebserver::init()
{
	paths.setDefault(HttpPathDelegate(&ApplicationWebserver::onFile, this));
	paths.set("/", HttpPathDelegate(&ApplicationWebserver::onIndex, this));
	paths.set(F("/webapp"), HttpPathDelegate(&ApplicationWebserver::onWebapp, this));
	paths.set(F("/config"), HttpPathDelegate(&ApplicationWebserver::onConfig, this));
	paths.set(F("/info"), HttpPathDelegate(&ApplicationWebserver::onInfo, this));
	paths.set(F("/color"), HttpPathDelegate(&ApplicationWebserver::onColor, this));
	paths.set(F("/networks"), HttpPathDelegate(&ApplicationWebserver::onNetworks, this));
	paths.set(F("/scan_networks"), HttpPathDelegate(&ApplicationWebserver::onScanNetworks, this));
	paths.set(F("/system"), HttpPathDelegate(&ApplicationWebserver::onSystemReq, this));
	paths.set(F("/update"), HttpPathDelegate(&ApplicationWebserver::onUpdate, this));
	paths.set(F("/connect"), HttpPathDelegate(&ApplicationWebserver::onConnect, this));
	paths.set(F("/ping"), HttpPathDelegate(&ApplicationWebserver::onPing, this));
	paths.set(F("/hosts"), HttpPathDelegate(&ApplicationWebserver::onHosts, this));
	paths.set(F("/presets"), HttpPathDelegate(&ApplicationWebserver::onPresets, this));
	paths.set(F("/scenes"), HttpPathDelegate(&ApplicationWebserver::onScenes, this));
	paths.set(F("/object"), HttpPathDelegate(&ApplicationWebserver::onObject, this));
	paths.set(F("/canonical.html"), HttpPathDelegate(&ApplicationWebserver::onIndex, this));
	paths.set(F("/generate_204"), HttpPathDelegate(&ApplicationWebserver::onIndex, this));
	paths.set(F("/static/hotspot.txt"), HttpPathDelegate(&ApplicationWebserver::onIndex, this));

	// animation controls
	paths.set(F("/stop"), HttpPathDelegate(&ApplicationWebserver::onStop, this));
	paths.set(F("/skip"), HttpPathDelegate(&ApplicationWebserver::onSkip, this));
	paths.set(F("/pause"), HttpPathDelegate(&ApplicationWebserver::onPause, this));
	paths.set(F("/continue"), HttpPathDelegate(&ApplicationWebserver::onContinue, this));
	paths.set(F("/blink"), HttpPathDelegate(&ApplicationWebserver::onBlink, this));
	paths.set(F("/toggle"), HttpPathDelegate(&ApplicationWebserver::onToggle, this));

	// websocket api
	wsResource = new WebsocketResource();
	wsResource->setConnectionHandler([this](WebsocketConnection& socket) { this->wsConnected(socket); });
	wsResource->setDisconnectionHandler([this](WebsocketConnection& socket) { this->wsDisconnected(socket); });
	paths.set("/ws", wsResource);

	_init = true;
}

void ApplicationWebserver::wsConnected(WebsocketConnection& socket)
{
	debug_i("===>wsConnected");
	webSockets.addElement(&socket);
	debug_i("===>nr of websockets: %i", webSockets.size());
}

void ApplicationWebserver::wsDisconnected(WebsocketConnection& socket)
{
	debug_i("<===wsDisconnected");
	webSockets.removeElement(&socket);
	debug_i("===>nr of websockets: %i", webSockets.size());
}

void ApplicationWebserver::wsBroadcast(String message)
{
	HttpConnection* connection = nullptr;
	String remoteIP;
	auto tcpConnections = getConnections();
	debug_i("=== Websocket Broadcast === -> %s", message.c_str());
	debug_i("===>nr of tcpConnections: %i", tcpConnections.size());
	for(auto& connection : tcpConnections) { // Iterate over all active sockets
		remoteIP = String(connection->getRemoteIp().toString());
		debug_i("====> remote: %s", remoteIP.c_str());
	}
	debug_i("=========================================");
	debug_i("===>nr of websockets: %i", webSockets.size());
	for(auto& socket : webSockets) { // Iterate over all active sockets
		connection = socket->getConnection();
		remoteIP = String(connection->getRemoteIp().toString());
		debug_i("====> sending to socket %s", remoteIP.c_str());
		socket->send(message, WS_FRAME_TEXT); // Send the message to each socket
	}
}

void ApplicationWebserver::start()
{
	if(_init == false) {
		init();
	}
	listen(80);
	_running = true;
}

void ApplicationWebserver::stop()
{
	close();
	_running = false;
}

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticateExec(HttpRequest& request, HttpResponse& response)
{
	{
		debug_i("ApplicationWebserver::authenticated - checking general context");
		AppConfig::Root config(*app.cfg);
		if(!config.security.getApiSecured())
			return true;
	} // end AppConfig general context

	debug_d("ApplicationWebserver::authenticated - checking...");

	String userPass = request.getHeader(F("Authorization"));
	if(userPass == String::nullstr) {
		debug_d("ApplicationWebserver::authenticated - No auth header");
		return false; // header missing
	}

	debug_d("ApplicationWebserver::authenticated Auth header: %s", userPass.c_str());

	// header in form of: "Basic MTIzNDU2OmFiY2RlZmc="so the 6 is to get to beginning of 64 encoded string
	userPass = userPass.substring(6); //cut "Basic " from start
	if(userPass.length() > 50) {
		return false;
	}
	{
		debug_i("ApplicationWebserver::authenticated - getting password");
		AppConfig::Root config(*app.cfg);
		userPass = base64_decode(userPass);
		//debug_d("ApplicationWebserver::authenticated Password: '%s' - Expected password: '%s'", userPass.c_str(), config.security.getApiPassword.c_str());

		if(userPass.endsWith(config.security.getApiPassword())) {
			return true;
		}
		return false;

	} //end AppConfig general context
}

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticated(HttpRequest& request, HttpResponse& response)
{
	bool authenticated = authenticateExec(request, response);

	if(!authenticated) {
		response.code = HTTP_STATUS_UNAUTHORIZED;
		response.setHeader(F("WWW-Authenticate"), F("Basic realm=\"RGBWW Server\""));
		response.setHeader(F("401 wrong credentials"), F("wrong credentials"));
		response.setHeader(F("Connection"), F("close"));
	}
	return authenticated;
}

String ApplicationWebserver::getApiCodeMsg(API_CODES code)
{
	switch(code) {
	case API_CODES::API_MISSING_PARAM:
		return String(F("missing param"));
	case API_CODES::API_UNAUTHORIZED:
		return String(F("authorization required"));
	case API_CODES::API_UPDATE_IN_PROGRESS:
		return String(F("update in progress"));
	default:
		return String(F("bad request"));
	}
}

void ApplicationWebserver::sendApiResponse(HttpResponse& response, JsonObjectStream* stream, HttpStatus code)
{
	if(!checkHeap(response)) {
		delete stream;
		return;
	}

	setCorsHeaders(response);
	response.setHeader(F("accept"), F("GET, POST, OPTIONS"));

	if(code != HTTP_STATUS_OK) {
		response.code = HTTP_STATUS_BAD_REQUEST;
	}
	response.sendDataStream(stream, MIME_JSON);
}

void ApplicationWebserver::sendApiCode(HttpResponse& response, API_CODES code, String msg /* = "" */)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject json = stream->getRoot();

	setCorsHeaders(response);
	response.setHeader(F("accept"), F("GET, POST, OPTIONS"));

	if(msg == "") {
		msg = getApiCodeMsg(code);
	}
	if(code == API_CODES::API_SUCCESS) {
		json[F("success")] = true;
		sendApiResponse(response, stream, HTTP_STATUS_OK);
	} else {
		json[F("error")] = msg;
		sendApiResponse(response, stream, HTTP_STATUS_BAD_REQUEST);
	}
}

void ApplicationWebserver::onFile(HttpRequest& request, HttpResponse& response)
{
	debug_i("http onFile");
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		response.setContentType(MIME_TEXT);
		response.code = HTTP_STATUS_SERVICE_UNAVAILABLE;
		response.sendString("OTA in progress");
		return;
	}
#endif
// Use client caching for better performance.
#ifndef NOCACHE
	response->setCache(86400, true);
#endif

	String fileName = request.uri.Path;
	debug_i("ApplicationWebserver::onFile with uri path=%s",fileName.c_str());
	if(fileName[0] == '/')
		fileName = fileName.substring(1);
	if(fileName[0] == '.') {
		response.code = HTTP_STATUS_FORBIDDEN;
		return;
	}

	String compressed = fileName + ".gz";
	debug_i("searching file name %s", compressed.c_str());
	auto v = fileMap[compressed];
	if(v) {
		debug_i("found");
		response.headers[HTTP_HEADER_CONTENT_ENCODING] = _F("gzip");
	} else {
		debug_i("searching file name %s", fileName.c_str());
		v = fileMap[fileName];
		if(!v) {
			debug_i("file %s not found in filemap", fileName.c_str());
			if(!app.isFilesystemMounted()) {
				response.setContentType(MIME_TEXT);
				response.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				response.sendString(F("No filesystem mounted"));
				return;
			}
			if(!fileExist(fileName) && !fileExist(fileName + ".gz") && WifiAccessPoint.isEnabled()) {
				//if accesspoint is active and we couldn`t find the file - redirect to index
				debug_d("ApplicationWebserver::onFile redirecting");
				response.headers[HTTP_HEADER_LOCATION] = F("http://") + WifiAccessPoint.getIP().toString() + "/";
			} else {
#ifndef NOCACHE
				response.setCache(86400, true); // It's important to use cache for better performance.
#endif
				response.code = HTTP_STATUS_OK;
				response.sendFile(fileName);
			}
			return;
		}
	}

	debug_i("found %s in fileMap", String(v.key()).c_str());
	auto stream = new FSTR::Stream(v.content());
	response.sendDataStream(stream, ContentType::fromFullFileName(fileName));
}
void ApplicationWebserver::onWebapp(HttpRequest& request, HttpResponse& response)
{
	debug_i("http onWebapp");
	if(!authenticated(request, response)) {
		return;
	}

	response.headers[HTTP_HEADER_LOCATION] = F("/index.html");
	setCorsHeaders(response);

	response.code = HTTP_STATUS_PERMANENT_REDIRECT;
	response.sendString(F("Redirecting to /index.html"));
}

void ApplicationWebserver::onIndex(HttpRequest& request, HttpResponse& response)
{
	debug_i("http onIndex");
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		response.setContentType(MIME_TEXT);
		response.code = HTTP_STATUS_SERVICE_UNAVAILABLE;
		response.sendString(F("OTA in progress"));
		return;
		void publishTransitionFinished(const String& name, bool requeued = false);
	}
#endif

	response.headers[HTTP_HEADER_LOCATION] = F("/index.html");
	setCorsHeaders(response);

	response.code = HTTP_STATUS_PERMANENT_REDIRECT;
	response.sendString(F("Redirecting to /index.html"));
}

bool ApplicationWebserver::checkHeap(HttpResponse& response)
{
	unsigned fh = system_get_free_heap_size();
	if(fh < _minimumHeap) {
		setCorsHeaders(response);
		response.code = HTTP_STATUS_TOO_MANY_REQUESTS;
		response.setHeader(F("Retry-After"), "2");
		return false;
	}
	return true;
}

void ApplicationWebserver::onConfig(HttpRequest& request, HttpResponse& response)
{
	debug_i("onConfig");
	if(!checkHeap(response))
		return;

	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif

	if(request.method != HTTP_POST && request.method != HTTP_GET && request.method != HTTP_OPTIONS) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not POST, GET or OPTIONS request"));
		return;
	}

	/*
    / handle HTTP_OPTIONS request to check if server is CORS permissive (which this firmware 
    / has been for years) this is just to reply to that request in order to pass the CORS test
    */
	if(request.method == HTTP_OPTIONS) {
		// probably a CORS preflight request
		setCorsHeaders(response);
		response.setHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}

	if(request.method == HTTP_POST) {
		debug_i("======================\nHTTP POST request received, ");

		/* ConfigDB importFomStream */
		String oldIP, oldSSID;
		bool mqttEnabled, dhcpEnabled;
		{
			debug_i("ApplicationWebserver::onConfig storing old settings");
			AppConfig::Network network(*app.cfg);
			oldIP = network.connection.getIp();
			oldSSID = network.ap.getSsid();
			mqttEnabled = network.mqtt.getEnabled();
			dhcpEnabled=network.connection.getDhcp();
		}

		auto bodyStream = request.getBodyStream();
		if(bodyStream) {
			ConfigDB::Status status = app.cfg->importFromStream(ConfigDB::Json::format, *bodyStream);

			/*********************************
             * TODO
             * - if network settings changed (ip config, default gateway, netmask, ssid, hostname(?) ) -> reboot 
             * - if mqtt settings changed to enabled -> start mqtt
             *   - if mqtt broker changed -> reconnect
             *   - if mqtt topic changed -> resubscribe
             *   - if mqtt master/secondary changed -> resubscribe to master/secondary topics where necessary
             * - if mqtt settings changed to disabled -> stop mqtt if possile, otherwise reboot
             * - if color setttings changed - reconfigure controller (see below)
             **********************************/

			// update and save settings if we haven`t received any error until now

			// bool restart = root[F("restart")] | false;

			String newIP, newSSID;
			bool newMqttEnabled,newDhcpEnabled;
			{
				debug_i("ApplicationWebserver::onConfig geting new ip settings");
				AppConfig::Network network(*app.cfg);
				newIP = network.connection.getIp();
				newSSID = network.ap.getSsid();
				newMqttEnabled = network.mqtt.getEnabled();
				newDhcpEnabled=network.connection.getDhcp();
			}
			
			if(oldIP != newIP) {
				//if (restart) {
				debug_i("ApplicationWebserver::onConfig ip settings changed - rebooting");
				app.delayedCMD(F("restart"), 3000); // wait 3s to first send response

				//json[F("data")] = "restart";
				//}
			}
			if(oldSSID != newSSID) {
				//
				if(WifiAccessPoint.isEnabled()) {
					debug_i("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
					app.delayedCMD(F("restart"), 3000); // wait 3s to first send response
					// report the fact that the system will restart to the frontend
					//json[F("data")] = "restart";
				}
			}
			if(mqttEnabled != newMqttEnabled) {
				if(newMqttEnabled) {
					if(!app.mqttclient.isRunning()) {
						debug_i("ApplicationWebserver::onConfig mqtt settings changed - starting mqtt");
						app.mqttclient.start();
					}
				} else {
					if(app.mqttclient.isRunning()) {
						debug_i("ApplicationWebserver::onConfig mqtt settings changed - stopping mqtt");
						app.mqttclient.stop();
					} else {
						debug_i("mqttclient was not running, no need to stop");
					}
				}
			
			}
			if(newDhcpEnabled!=dhcpEnabled){
				if(newDhcpEnabled){
					WifiStation.enableDHCP(true);
				}else{
					debug_i("ApplicationWebserver::onConfig ip settings changed - rebooting");
					app.delayedCMD(F("restart"), 3000); // wait 3s to first send response
				}
			}


			//bodyStream->seekOrigin(0,SeekOrigin::Start);
			//app.wsBroadcast(F("config"),bodyStream->moveString());
			/* ConfigDB ToDo
            if (color_updated) {
                debug_d("ApplicationWebserver::onConfig color settings changed - refreshing");

                //refresh settings
                app.rgbwwctrl.setup();

                //refresh current output
                app.rgbwwctrl.refresh();

            }
            */

			setCorsHeaders(response);
			sendApiCode(response, API_CODES::API_SUCCESS);
		} else {
			//CofigDB provide correct error message

			//debug_i("config api error %s",error_msg.c_str());
			//JsonObject root = doc.as<JsonObject>();
			//sendApiCode(response, API_CODES::API_MISSING_PARAM, error_msg);
			setCorsHeaders(response);
			sendApiCode(response, API_CODES::API_MISSING_PARAM);
		}

	} else {
		/*
         * /config GET
         */
		setCorsHeaders(response);
		auto configStream = app.cfg->createExportStream(ConfigDB::Json::format);
		response.sendDataStream(configStream.release(), MIME_JSON);
	}
}

void ApplicationWebserver::onInfo(HttpRequest& request, HttpResponse& response)
{
	debug_i("onInfo");
	if(!checkHeap(response))
		return;

	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif

	if(request.method != HTTP_GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET");
		return;
	}

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject data = stream->getRoot();
	data[F("deviceid")] = String(system_get_chip_id());
#if defined(ARCH_ESP8266) || defined(ARCH_ESP32)
	data[F("current_rom")] = String(app.ota.getRomPartition().name());
#endif
	data[F("git_version")] = fw_git_version;
	data[F("git_date")] = fw_git_date;
	data[F("webapp_version")] = WEBAPP_VERSION;
	data[F("sming")] = SMING_VERSION;
	data[F("event_num_clients")] = app.eventserver.activeClients;
	data[F("uptime")] = app.getUptime();
	data[F("heap_free")] = system_get_free_heap_size();
	data[F("soc")]=SOC;
	
	/*
    FileSystem::Info fsInfo;
    app.getinfo(fsInfo);
    JsonObject FS=data.createNestedObject("fs");
    FS[F("mounted")] = fsInfo.partition?"true":"false";
    FS[F("size")] = fsInfo.total;
    FS[F("used")] = fsInfo.used;
    FS[F("available")] = fsInfo.freeSpace;
*/
	JsonObject rgbww = data.createNestedObject("rgbww");
	rgbww[F("version")] = RGBWW_VERSION;
	rgbww[F("queuesize")] = RGBWW_ANIMATIONQSIZE;

	JsonObject con = data.createNestedObject("connection");
	con[F("connected")] = WifiStation.isConnected();
	con[F("ssid")] = WifiStation.getSSID();
	con[F("dhcp")] = WifiStation.isEnabledDHCP();
	con[F("ip")] = WifiStation.getIP().toString();
	con[F("netmask")] = WifiStation.getNetworkMask().toString();
	con[F("gateway")] = WifiStation.getNetworkGateway().toString();
	con[F("mac")] = WifiStation.getMAC();
	//con[F("mdnshostname")] = app.cfg.network.connection.mdnshostname.c_str();

	sendApiResponse(response, stream);
}

/**
 * @brief Handles the HTTP GET request for retrieving the current color information.
 *
 * This function is responsible for handling the HTTP GET request and returning the current color information
 * in JSON format. The color information includes the raw RGBWW values and the corresponding HSV values.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onColorGet(HttpRequest& request, HttpResponse& response)
{
	debug_i("onColorGet");
	if(!checkHeap(response))
		return;

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject json = stream->getRoot();

	JsonObject raw = json.createNestedObject("raw");
	ChannelOutput output = app.rgbwwctrl.getCurrentOutput();
	raw[F("r")] = output.r;
	raw[F("g")] = output.g;
	raw[F("b")] = output.b;
	raw[F("ww")] = output.ww;
	raw[F("cw")] = output.cw;

	JsonObject hsv = json.createNestedObject("hsv");
	float h, s, v;
	int ct;
	HSVCT c = app.rgbwwctrl.getCurrentColor();
	c.asRadian(h, s, v, ct);
	hsv[F("h")] = h;
	hsv[F("s")] = s;
	hsv[F("v")] = v;
	hsv[F("ct")] = ct;

	setCorsHeaders(response);

	sendApiResponse(response, stream);
}

/**
 * @brief Handles the HTTP POST request for updating the color.
 * 
 * This function is responsible for processing the HTTP POST request and updating the color based on the received body.
 * If the body is empty, it sends a bad request response with the message "no body".
 * If the color update is successful, it sends a success response.
 * If the color update fails, it sends a bad request response with the corresponding error message.
 * 
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onColorPost(HttpRequest& request, HttpResponse& response)
{
	String body = request.getBody();

	setCorsHeaders(response);
	response.setHeader(F("Access-Control-Allow-Methods"), F("GET, PUT, POST, OPTIONS"));
	response.setHeader(F("Access-Control-Allow-Credentials"), F("true"));

	if(body == NULL) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "no body");
		return;
	}

	String msg;

	if(!app.jsonproc.onColor(body, msg)) {
		debug_i("received color update with message %s", msg.c_str());
		sendApiCode(response, API_CODES::API_BAD_REQUEST, msg);
	} else {
		debug_i("received color update with message %s", msg.c_str());
		sendApiCode(response, API_CODES::API_SUCCESS);
	}
}

/**
 * @brief Handles the color request from the client.
 *
 * This function is responsible for handling the color request from the client.
 * It checks for authentication, handles OTA update in progress, sets the necessary headers,
 * and delegates the request to the appropriate handler based on the HTTP method.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onColor(HttpRequest& request, HttpResponse& response)
{
	debug_i("onColor");
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif
	debug_i("received /color request");
	setCorsHeaders(response);
	response.setHeader(F("Access-Control-Allow-Origin"), F("*"));

	if(request.method != HTTP_POST && request.method != HTTP_GET && request.method != HTTP_OPTIONS) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not POST, GET or OPTIONS"));
		debug_i("not POST, GET or OPTIONS");
		return;
	}

	if(request.method == HTTP_OPTIONS) {
		response.setHeader(F("Access-Control-Allow-Methods"), F("GET, PUT, POST, OPTIONS"));

		debug_i("OPTIONS");
		sendApiCode(response, API_CODES::API_SUCCESS);
		return;
	}

	bool error = false;
	if(request.method == HTTP_POST) {
		debug_i("POST");
		ApplicationWebserver::onColorPost(request, response);
	} else if(request.method == HTTP_GET) {
		debug_i("GET");
		ApplicationWebserver::onColorGet(request, response);
	} else {
		debug_i("found unimplementd http_method %i", (int)request.method);
	}
}

/**
 * @brief Checks if a string is printable.
 *
 * This function checks if a given string is printable, i.e., if all characters in the string
 * have ASCII values greater than or equal to 0x20 (space character).
 *
 * @param str The string to be checked.
 * @return True if the string is printable, false otherwise.
 */
bool ApplicationWebserver::isPrintable(String& str)
{
	for(unsigned int i = 0; i < str.length(); ++i) {
		char c = str[i];
		if(c < 0x20)
			return false;
	}
	return true;
}

/**
 * @brief Handles the HTTP request for retrieving network information.
 *
 * This function is responsible for handling the HTTP GET request for retrieving network information.
 * It checks if the request is authenticated and if OTA update is in progress. If not, it returns the
 * available networks along with their details such as SSID, signal strength, and encryption method.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onNetworks(HttpRequest& request, HttpResponse& response)
{
	debug_i("onNetworks");
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif
	if(request.method == HTTP_OPTIONS) {
		setCorsHeaders(response);

		sendApiCode(response, API_CODES::API_SUCCESS);
		return;
	}
	if(request.method != HTTP_GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
		return;
	}

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject json = stream->getRoot();

	bool error = false;

	if(app.network.isScanning()) {
		json[F("scanning")] = true;
	} else {
		json[F("scanning")] = false;
		JsonArray netlist = json.createNestedArray("available");
		BssList networks = app.network.getAvailableNetworks();
		for(unsigned int i = 0; i < networks.count(); i++) {
			if(networks[i].hidden)
				continue;

			// SSIDs may contain any byte values. Some are not printable and will cause the javascript client to fail
			// on parsing the message. Try to filter those here
			if(!ApplicationWebserver::isPrintable(networks[i].ssid)) {
				debug_w("Filtered SSID due to unprintable characters: %s", networks[i].ssid.c_str());
				continue;
			}

			JsonObject item = netlist.createNestedObject();
			item[F("id")] = (int)networks[i].getHashId();
			item[F("ssid")] = networks[i].ssid;
			item[F("signal")] = networks[i].rssi;
			item[F("encryption")] = networks[i].getAuthorizationMethodName();
			//limit to max 25 networks
			if(i >= 25)
				break;
		}
	}
	setCorsHeaders(response);
	sendApiResponse(response, stream);
}

/**
 * @brief Handles the "onScanNetworks" request from the webserver.
 *
 * This function is responsible for handling the "onScanNetworks" request from the webserver.
 * It checks if the request is authenticated, if OTA update is in progress, and if the request method is HTTP POST.
 * If all conditions are met, it initiates a network scan and sends a success response.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onScanNetworks(HttpRequest& request, HttpResponse& response)
{
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif

	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}
	if(!app.network.isScanning()) {
		app.network.scan(false);
	}

	sendApiCode(response, API_CODES::API_SUCCESS);
}

/**
 * @brief Handles the HTTP connection event.
 *
 * This function is called when a client connects to the web server.
 * It performs authentication, checks for ongoing OTA updates, and handles HTTP requests.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onConnect(HttpRequest& request, HttpResponse& response)
{
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif

	if(request.method != HTTP_POST && request.method != HTTP_GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST or GET");
		return;
	}

	if(request.method == HTTP_POST) {
		String body = request.getBody();
		if(body == NULL) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not get HTTP body");
			return;
		}
		// ConfigDB - CONFIG_MAX_LENGTH was no longer defined, what's the right size here?
		StaticJsonDocument<512> doc;
		Json::deserialize(doc, body);
		String ssid;
		String password;
		if(Json::getValue(doc[F("ssid")], ssid)) {
			password = doc[F("password")].as<const char*>();
			debug_d("ssid %s - pass %s", ssid.c_str(), password.c_str());
			app.network.connect(ssid, password, true);
			sendApiCode(response, API_CODES::API_SUCCESS);
			return;

		} else {
			sendApiCode(response, API_CODES::API_MISSING_PARAM);
			return;
		}
	} else {
		JsonObjectStream* stream = new JsonObjectStream();
		JsonObject json = stream->getRoot();

		CONNECTION_STATUS status = app.network.get_con_status();
		json[F("status")] = int(status);
		if(status == CONNECTION_STATUS::ERROR) {
			json[F("error")] = app.network.get_con_err_msg();
		} else if(status == CONNECTION_STATUS::CONNECTED) {
			// return connected
			debug_i("wifi connected, checking if dhcp enabled");
			AppConfig::Network network(*app.cfg);

			if(network.connection.getDhcp()) {
				json[F("ip")] = WifiStation.getIP().toString();
			} else {
				String ip = network.connection.getIp();
				json[F("ip")] = ip;
			}
			json[F("dhcp")] = network.connection.getDhcp() ? F("True") : F("False");
			json[F("ssid")] = WifiStation.getSSID();
		}
		sendApiResponse(response, stream);
	}
}

/**
 * @brief Handles the system request from the client.
 *
 * This function is responsible for handling the system request from the client. It performs the following tasks:
 * - Checks if the client is authenticated.
 * - Checks if an OTA update is in progress (only for ESP8266 architecture).
 * - Handles HTTP OPTIONS request by setting the cross-domain origin header and sending a success API code.
 * - Handles HTTP POST request by processing the request body and executing the specified command.
 * - Sends the appropriate API code response based on the success or failure of the request.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onSystemReq(HttpRequest& request, HttpResponse& response)
{
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_ESP8266
	if(app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}
#endif

	if(request.method == HTTP_OPTIONS) {
		setCorsHeaders(response);

		sendApiCode(response, API_CODES::API_SUCCESS);
		return;
	}
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	bool error = false;
	String body = request.getBody();
	if(body == NULL) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not get HTTP body");
		return;
	} else {
		debug_i("ApplicationWebserver::onSystemReq: %s", body.c_str());
		// ConfigDB - CONFIG_MAX_LENGTH was no longer defined, what's the right size here?
		StaticJsonDocument<512> doc;
		Json::deserialize(doc, body);

		String cmd = doc[F("cmd")].as<const char*>();
		if(cmd) {
			if(cmd.equals("debug")) {
				bool enable;
				if(Json::getValue(doc[F("enable")], enable)) {
					Serial.systemDebugOutput(enable);
				} else {
					error = true;
				}

			} else if(!app.delayedCMD(cmd, 1500)) {
				error = true;
			}

		} else {
			error = true;
		}
	}
	setCorsHeaders(response);

	if(!error) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_MISSING_PARAM);
	}
}

/**
 * @brief Handles the update request from the client.
 *
 * This function is responsible for handling the update request from the client.
 * It performs authentication, checks the request method, and processes the update request.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onUpdate(HttpRequest& request, HttpResponse& response)
{
	if(!authenticated(request, response)) {
		return;
	}

#ifdef ARCH_HOST
	sendApiCode(response, API_CODES::API_BAD_REQUEST, "not supported on Host");
	return;
#else
	if(request.method == HTTP_OPTIONS) {
		// probably a CORS request
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("/update HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}
	if(request.method != HTTP_POST && request.method != HTTP_GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST or GET");
		return;
	}

	if(request.method == HTTP_POST) {
		if(app.ota.isProccessing()) {
			sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
			return;
		}

		String body = request.getBody();
		if(body == NULL) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
			return;
		}

		debug_i("body: %s", body.c_str());
		// ConfigDB - CONFIG_MAX_LENGTH was no longer defined, what's the right size here?
		StaticJsonDocument<512> doc;
		Json::deserialize(doc, body);
		String romurl;
		Json::getValue(doc[F("rom")][F("url")], romurl);

		//String spiffsurl;
		//Json::getValue(doc[F("spiffs")][F("url")],spiffsurl);

		debug_i("starting update process with \n    romurl: %s", romurl.c_str());
		if(romurl == "") {
			debug_i("missing rom url");
			sendApiCode(response, API_CODES::API_MISSING_PARAM);
		} else {
			app.ota.start(romurl);
			setCorsHeaders(response);
			sendApiCode(response, API_CODES::API_SUCCESS);
		}
		return;
	}
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject json = stream->getRoot();
	json[F("status")] = int(app.ota.getStatus());
	sendApiResponse(response, stream);
#endif
}

/**
 * @brief Handles the HTTP GET request for the ping endpoint.
 *
 * simple call-response to check if we can reach server
 * 
 * This function is responsible for handling the HTTP GET request for the ping endpoint.
 * It checks if the request method is GET, and if not, it sends a bad request response.
 * If the request method is GET, it creates a JSON object with the key "ping" and value "pong",
 * and sends the JSON response using the sendApiResponse function.
 *
 * @param request The HTTP request object.
 * @param response The HTTP response object.
 */
void ApplicationWebserver::onPing(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
		return;
	}
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject json = stream->getRoot();
	json[F("ping")] = "pong";
	sendApiResponse(response, stream);
}

void ApplicationWebserver::onStop(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onStop(request.getBody(), msg, true)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onSkip(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onSkip(request.getBody(), msg)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onPause(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onPause(request.getBody(), msg, true)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onContinue(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onContinue(request.getBody(), msg)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onBlink(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onBlink(request.getBody(), msg)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onToggle(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
		return;
	}

	String msg;
	if(app.jsonproc.onToggle(request.getBody(), msg)) {
		sendApiCode(response, API_CODES::API_SUCCESS);
	} else {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
	}
}

void ApplicationWebserver::onStorage(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_POST && request.method != HTTP_GET && request.method != HTTP_OPTIONS) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST, GET or OPTIONS request");
		return;
	}

	/*
    / axios sends a HTTP_OPTIONS request to check if server is CORS permissive (which this firmware 
    / has been for years) this is just to reply to that request in order to pass the CORS test
    */
	if(request.method == HTTP_OPTIONS) {
		// probably a CORS request
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}

	if(request.method == HTTP_POST) {
		debug_i("======================\nHTTP POST request received, ");
		String header = request.getHeader("Content-type");
		if(header != "application/json") {
			sendApiCode(response, API_BAD_REQUEST, "only json content allowed");
		}
		debug_i("got post with content type %s", header.c_str());
		String body = request.getBody();
		if(body == NULL || body.length() > FILE_MAX_SIZE) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
			return;
		}

		bool error = false;

		debug_i("body length: %i", body.length());
		// ConfigDB - CONFIG_MAX_LENGTH was no longer defined, what's the right size here?
		StaticJsonDocument<512> doc;
		Json::deserialize(doc, body);
		String fileName = doc[F("filename")];

		//DynamicJsonDocument data(body.length()+32);
		//Json::deserialize(data, Json::serialize(doc[F("data")]));
		//doc.clear(); //clearing the original document to save RAM
		debug_i("will save to file %s", fileName.c_str());
		debug_i("original document uses %i bytes", doc.memoryUsage());
		String data = doc[F("data")];
		debug_i("data: %s", data.c_str());

		FileHandle file =
			fileOpen(fileName.c_str(), IFS::OpenFlag::Write | IFS::OpenFlag::Create | IFS::OpenFlag::Truncate);
		if(!fileWrite(file, data.c_str(), data.length())) {
			debug_e("Saving config to file %s failed!", fileName.c_str());
		}
		fileClose(file);
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_SUCCESS);
		return;
	}
}

void ApplicationWebserver::onHosts(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_GET && request.method != HTTP_OPTIONS) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET or OPTIONS request");
		return;
	}

	if(request.method == HTTP_OPTIONS) {
		// probably a CORS request
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}

	String myHosts;
	// Set the response body with the JSON
	setCorsHeaders(response);
	response.setContentType(F("application/json"));
	response.sendString(app.network.getMdnsHosts());

	return;
}

void ApplicationWebserver::onPresets(HttpRequest& request, HttpResponse& response)
{
	if(request.method != HTTP_GET && request.method != HTTP_OPTIONS && request.method != HTTP_POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET, POST or OPTIONS request");
		return;
	}

	if(request.method == HTTP_OPTIONS) {
		// probably a CORS request
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}

	if(request.method == HTTP_GET) {
		// Set the response body with the JSON
		debug_i("/presets GET request received, ");
		setCorsHeaders(response);
		response.setContentType(F("application/json"));
		auto presetStream = app.data->createExportStream(ConfigDB::Json::format, F("presets"));
		response.sendDataStream(presetStream.release(), MIME_JSON);
		return;
	}

	if(request.method == HTTP_POST) {
		debug_i("/presets POST request received, ");
		auto bodyStream = request.getBodyStream();
		if(bodyStream) {
			ConfigDB::Status status = app.data->importFromStream(ConfigDB::Json::format, *bodyStream);
		}
		return;
	}
}
void ApplicationWebserver::onScenes(HttpRequest& request, HttpResponse& response)
{
}
void ApplicationWebserver::onObject(HttpRequest& request, HttpResponse& response)
{
	if(request.method == HTTP_OPTIONS) {
		// probably a CORS request
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_SUCCESS, "");
		debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
		return;
	}
	/******************************************************************************************************
    *  valid object types are:
    *
    * g: group
    * {id: <id>, name: <string>, hosts:[hostid, hostid, hostid, hostid, ...]}
    * 
    * p: preset
    * {id: <id>, name: <string>, hsv:{h: <float>, s: <float>, v: <float>}}
    * 
    * p: preset
    * {id: <id>, name: <string>, raw:{r: <float>, g: <float>, b: <float>, ww: <float>, cw: <float>}}
    * 
    * h: host
    * {id: <id>, name: <string>, ip: <string>, active: <bool>}
    * remarkt: the active field shall be added upon sending the file by checking, if the host is in the current mDNS hosts list
    * 
    * s: scene
    * {id: <id>, name: <string>, hosts: [{id: <hostid>,hsv:{h: <float>, s: <float>, v: <float>},...]}
    * 
    * enumerating all objects of a type is done by first sending a GET request to /object?type=<type> which the 
    * controller will reply to with a json array of all objects of the requested type in the following format:
    * {"<type>":[F("2234585-1233362","2234585-0408750","2234585-9433038","2234585-7332130","2234585-7389644")]}
    * it is then the job of the front end to request each object individually by sending a GET request to 
    * /object?type=<type>&id=<id>
    * 
    * creating a new object is done by sending a POST request to /object?type=<type> with the json object as 
    * described above as the body.
    * The id field (both in the url as well as in the json object) should be omitted, in which case the 
    * controller will generate a new id for the object.
    * 
    * updating an existing object is done by sending a POST request to /object?type=<type>&id=<id> with the fully populated
    * json object as the body. In this case the id field in the json object must match the id in the url.
    * 
    * deleting an object is done by sending a DELETE request to /object?type=<type>&id=<id>. No checks are performed.
    * 
    * it's important to understand that the controller only stores the objects, the frontend is fully responsible
    * for the cohesion of the data. If a non-existant host is added to a scene, the controller will not complain.
    * 
    * Since the id for the hosts is the actual ESP8266 it is possible to track controllers through ip address changes
    * and keep their ids constant. This is not implemented yet.
    ******************************************************************************************************/
	String objectType = request.getQueryParameter("type");
	String objectId = request.getQueryParameter("id");
#ifdef DEBUG_OBJECT_API
	debug_i("got request with uri %s for object type %s with id %s.", String(request.uri).c_str(), objectType.c_str(),
			objectId.c_str());
#endif
	auto tcpConnections = getConnections();
	debug_i("===> (objEntry) nr of tcpConnections: %i", tcpConnections.size());

	if(objectType == "") {
#ifdef DEBUG_OBJECT_API
		debug_i("missing object type");
#endif
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_BAD_REQUEST, "missing object type");
		return;
	}
	String types = F("gphs");
	if(types.indexOf(objectType) == -1 || objectType.length() > 1) {
#ifdef DEBUG_OBJECT_API
		debug_i("unsupported object type");
#endif
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_BAD_REQUEST, F("unsupported object type"));
		return;
	}

	if(request.method == HTTP_GET) {
		if(objectId == "") {
			//requested object type but no object id, list all objects of type
			Directory dir;
			if(!dir.open()) {
				debug_i("could not open dir");
				sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not open dir"));
				return;
			} else {
				// ConfigDB - CONFIG_MAX_LENGTH was no longer defined, what's the right size here?
				JsonObjectStream* stream = new JsonObjectStream(512);
				JsonObject doc = stream->getRoot();

				JsonArray objectsList;

				switch(objectType.c_str()[0]) {
				case 'g':
					objectsList = doc.createNestedArray(F("groups"));
					break;
					;
					;
				case 'p':
					objectsList = doc.createNestedArray(F("presets"));
					break;
					;
					;
				case 'h':
					objectsList = doc.createNestedArray(F("hosts"));
					break;
					;
					;
				case 's':
					objectsList = doc.createNestedArray(F("scenes"));
					break;
					;
					;
				}

				while(dir.next()) {
					String fileName = String(dir.stat().name);
					if(fileName.substring(0, 1) == "_") {
#ifdef DEBUG_OBJECT_API
						debug_i("found file: %s", fileName.c_str());
						debug_i("file has object type %s", fileName.substring(1, 2).c_str());
#endif
						if(fileName.substring(1, 2) == objectType) {
#ifdef DEBUG_OBJECT_API
							debug_i("adding file %s to list", fileName);
							debug_i("filename %s, extension starts at %i", fileName.c_str(), fileName.indexOf(F(".")));
#endif
							objectId = fileName.substring(2, fileName.indexOf(F(".")));
							objectsList.add(objectId);
						}
					}
				}
				response.setContentType(F("application/json"));
				setCorsHeaders(response);
				response.sendString(Json::serialize(doc));
				delete stream;
			}
		} else {
			//got GET with object type and id, return object, if available
			debug_i("HTTP GET request received, ");
			String fileName = "_" + objectType + objectId + ".json";
			if(!fileName) {
#ifdef DEBUG_OBJECT_API
				debug_i("file not found");
#endif
				setCorsHeaders(response);
				sendApiCode(response, API_CODES::API_BAD_REQUEST, F("file not found"));
				return;
			}
			response.setContentType(F("application/json"));
			setCorsHeaders(response);
#ifdef DEBUG_OBJECT_API
			debug_i("sending file %s", fileName.c_str());
#endif
			setCorsHeaders(response);
			response.sendFile(fileName);
			return;
		}
	}
	if(request.method == HTTP_POST) {
		debug_i("HTTP PUT request received, ");
		String body = request.getBody();
#ifdef DEBUG_OBJECT_API
		debug_i("request body: %s", body.c_str());
#endif
		if(body == NULL || body.length() > FILE_MAX_SIZE) {
			setCorsHeaders(response);
			sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse HTTP body"));
#ifdef DEBUG_OBJECT_API
			debug_i("body is null or too long");
#endif
			return;
		}
		StaticJsonDocument<FILE_MAX_SIZE> doc;
		DeserializationError error = deserializeJson(doc, body);
		if(error) {
			setCorsHeaders(response);
			sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse json from HTTP body"));
#ifdef DEBUG_OBJECT_API
			debug_i("could not parse json");
#endif
			return;
		}
#ifdef DEBUG_OBJECT_API
		debug_i("parsed json, found name %s", String(doc[F("name")]).c_str());
#endif
		if(objectId == "") {
			//no object id, create new object
			if(doc[F("id")] != "") {
				objectId = String(doc[F("id")]);
			} else {
				debug_i("no object id, creating new object");
				objectId = makeId();
				doc[F("id")] = objectId;
			}
		}
		String fileName = "_" + objectType + objectId + ".json";
#ifdef DEBUG_OBJECT_API
		debug_i("will save to file %s", fileName.c_str());
#endif
		FileHandle file =
			fileOpen(fileName.c_str(), IFS::OpenFlag::Write | IFS::OpenFlag::Create | IFS::OpenFlag::Truncate);
		if(!file) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, F("file not found"));
#ifdef DEBUG_OBJECT_API
			debug_i("couldn not open file for write");
#endif
			return;
		}
		String bodyData;
		serializeJson(doc, bodyData);
#ifdef DEBUG_OBJECT_API
		debug_i("body length: %i", bodyData.length());
		debug_i("data: %s", bodyData.c_str());
#endif
		if(!fileWrite(file, bodyData.c_str(), bodyData.length())) {
#ifdef DEBUG_OBJECT_API
			debug_e("Saving config to file %s failed!", fileName.c_str());
//should probably also send some error message to the client
#endif
		}
		fileClose(file);

		setCorsHeaders(response);
		response.setContentType(F("application/json"));
		//doc.clear();
		doc[F("id")] = objectId;
		bodyData = "";
		serializeJson(doc, bodyData);
		response.sendString(bodyData.c_str());

		// send websocket message to all connected clients to
		// update them about the new object
		JsonRpcMessage msg("preset");
		JsonObject root = msg.getParams();
		root.set(doc.as<JsonObject>());
		debug_i("rpc: root =%s", Json::serialize(root).c_str());
		debug_i("rpc: msg =%s", Json::serialize(msg.getRoot()).c_str());

		String jsonStr = Json::serialize(msg.getRoot());

		wsBroadcast(jsonStr);
		//sendApiCode(response, API_CODES::API_SUCCESS);

		return;
	}
	if(request.method == HTTP_DELETE) {
		String fileName = "_" + objectType + objectId + F(".json");
		FileHandle file = fileDelete(fileName.c_str());
		if(!file) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, F("file not found"));
			return;
		}
		fileClose(file);
		setCorsHeaders(response);
		sendApiCode(response, API_CODES::API_SUCCESS);
		return;
	}
}

String ApplicationWebserver::makeId()
{
	/*
     * generate ID for an object. The id is comprised of a letter, denoting the 
     * class of the current object (preset, group, host or scene) the 7 digit  
     * controller id, a dash and the seven lowest digits of the current microsecond 
     * timestamp. There is a very small chance of collision, and in this case, an  
     * existing preset with the colliding id will just be overwritten as if it had
     * been updated. But as said, I recon the chance that a 2nd id will be generatd
     * on the same controller with the exact same microsecond timestamp is very small
     * names, on the other hand, are not relevant for the system, so they can be pickd
     * freely and technically, objects can even be renamed.
     */
	char ___id[8];
	sprintf(___id, "%07u", (uint32_t)micros() % 10000000);
	String __id = String(___id);
	String chipId = String(system_get_chip_id());
	String objectId = chipId + "-" + __id;
#ifdef DEBUG_OBJECT_API
	debug_i("generated id %s ", objectId.c_str());
#endif
	return objectId;
}

void ApplicationWebserver::setCorsHeaders(HttpResponse& response)
{
	response.setAllowCrossDomainOrigin("*");
	response.setHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
}
