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
#include <FlashString/Map.hpp>
#include <FlashString/Stream.hpp>
#include <Network/Http/Websocket/WebsocketResource.h>
#include <Storage.h>

#define NOCACHE
#define DEBUG_OBJECT_API

#define FILE_LIST(XX)                                 \
    XX(index_html,      "index.html.gz")              \
    XX(VERSION,         "VERSION")                    \
    XX(index_jss,       "assets/index.css.gz")        \
    XX(index_js,        "assets/index.js.gz")         \
    XX(RgbwwLayout_css, "assets/RgbwwLayout.css.gz")  \
    XX(RgbwwLayout_js,  "assets/RgbwwLayout.js.gz")   \
    XX(i18n_js,         "assets/i18n.js.gz")          \
    XX(pinconfig_json,  "config/pinconfig.json")      \
    XX(favicon_ico,     "icons/favicon.ico")          \
    XX(network_wifi_3_bar_locked_FILL0_wght400_GRAD0_opsz24, "icons/network_wifi_3_bar_locked_FILL0_wght400_GRAD0_opsz24.svg") \
    XX(network_wifi_1_bar_FILL0_wght400_GRAD0_opsz24,        "icons/network_wifi_1_bar_FILL0_wght400_GRAD0_opsz24.svg")        \
    XX(network_wifi_FILL0_wght400_GRAD0_opsz24,              "icons/network_wifi_FILL0_wght400_GRAD0_opsz24.svg")              \
    XX(network_wifi_1_bar_locked_FILL0_wght400_GRAD0_opsz24, "icons/network_wifi_1_bar_locked_FILL0_wght400_GRAD0_opsz24.svg") \
    XX(network_wifi_locked_FILL0_wght400_GRAD0_opsz24,       "icons/network_wifi_locked_FILL0_wght400_GRAD0_opsz24.svg")       \
    XX(network_wifi_2_bar_FILL0_wght400_GRAD0_opsz24,        "icons/network_wifi_2_bar_FILL0_wght400_GRAD0_opsz24.svg")        \
    XX(signal_wifi_statusbar_null_FILL0_wght400_GRAD0_opsz24,"icons/signal_wifi_statusbar_null_FILL0_wght400_GRAD0_opsz24.svg")\
    XX(network_wifi_2_bar_locked_FILL0_wght400_GRAD0_opsz24, "icons/network_wifi_2_bar_locked_FILL0_wght400_GRAD0_opsz24.svg") \
    XX(wifi_lock_FILL0_wght400_GRAD0_opsz24,                 "icons/wifi_lock_FILL0_wght400_GRAD0_opsz24.svg")                 \
    XX(network_wifi_3_bar_FILL0_wght400_GRAD0_opsz24,        "icons/network_wifi_3_bar_FILL0_wght400_GRAD0_opsz24.svg")    

// Define the names for each file
#define XX(name, file) DEFINE_FSTR_LOCAL(KEY_##name, file)
FILE_LIST(XX)
#undef XX

// Import content for each file
#define XX(name, file) IMPORT_FSTR_LOCAL(CONTENT_##name, PROJECT_DIR "/webapp/" file);
FILE_LIST(XX)
#undef XX

// Define the table structure linking key => content
#define XX(name, file) {&KEY_##name, &CONTENT_##name},
DEFINE_FSTR_MAP_LOCAL(fileMap, FlashString, FlashString, FILE_LIST(XX));
#undef XX

ApplicationWebserver::ApplicationWebserver() {
    _running = false;
    // keep some heap space free
    // value is a good guess and tested to not crash when issuing multiple parallel requests
    HttpServerSettings settings;
    settings.maxActiveConnections=40;
    settings.minHeapSize = _minimumHeapAccept;  
    settings.keepAliveSeconds = 10; // do not close instantly when no transmission occurs. some clients are a bit slow (like FHEM)
    configure(settings);

    // workaround for bug in Sming 3.5.0
    // https://github.com/SmingHub/Sming/issues/1236
    setBodyParser("*", bodyToStringParser);
}

void ApplicationWebserver::init() {
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
    paths.set(F("/object"), HttpPathDelegate(&ApplicationWebserver::onObject, this));
    // animation controls
    paths.set(F("/stop"), HttpPathDelegate(&ApplicationWebserver::onStop, this));
    paths.set(F("/skip"), HttpPathDelegate(&ApplicationWebserver::onSkip, this));
    paths.set(F("/pause"), HttpPathDelegate(&ApplicationWebserver::onPause, this));
    paths.set(F("/continue"), HttpPathDelegate(&ApplicationWebserver::onContinue, this));
    paths.set(F("/blink"), HttpPathDelegate(&ApplicationWebserver::onBlink, this));

    paths.set(F("/toggle"), HttpPathDelegate(&ApplicationWebserver::onToggle, this));

    // storage api
    //paths.set("/storage",HttpPathDelegate(&ApplicationWebserver::onStorage, this));

    // websocket api
    wsResource= new WebsocketResource();
	wsResource->setConnectionHandler([this](WebsocketConnection& socket) { this->wsConnected(socket); });
    wsResource->setDisconnectionHandler([this](WebsocketConnection& socket) { this->wsDisconnected(socket); });
	paths.set("/ws", wsResource);

    _init = true;
}

void ApplicationWebserver::wsConnected(WebsocketConnection& socket){
    debug_i("===>wsConnected");
    webSockets.addElement(&socket);
    debug_i("===>nr of websockets: %i", webSockets.size());
}

void ApplicationWebserver::wsDisconnected(WebsocketConnection& socket){
    debug_i("<===wsDisconnected");
    webSockets.removeElement(&socket);
    debug_i("===>nr of websockets: %i", webSockets.size());
}

void ApplicationWebserver::wsBroadcast(String message){
    HttpConnection *connection = nullptr;
    String remoteIP;
    auto tcpConnections=getConnections();
    debug_i("=== Websocket Broadcast ===\n%s",message.c_str());
    debug_i("===>nr of tcpConnections: %i", tcpConnections.size());
    for(auto& connection : tcpConnections) { // Iterate over all active sockets
        remoteIP=String(connection->getRemoteIp().toString());
        debug_i("====> remote: %s",remoteIP.c_str());
    }
    debug_i("=========================================");
    debug_i("===>nr of websockets: %i", webSockets.size());
    for(auto& socket : webSockets) { // Iterate over all active sockets
        connection=socket->getConnection();
        remoteIP=String(connection->getRemoteIp().toString());
        debug_i("====> sending to socket %s",remoteIP.c_str());
        socket->send(message, WS_FRAME_TEXT); // Send the message to each socket
    }
}

void ApplicationWebserver::start() {
    if (_init == false) {
        init();
    }
    listen(80);
    _running = true;
}

void ApplicationWebserver::stop() {
    close();
    _running = false;
}

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticateExec(HttpRequest &request, HttpResponse &response) {
    if (!app.cfg.general.api_secured)
        return true;

    debug_d("ApplicationWebserver::authenticated - checking...");

    String userPass = request.getHeader(F("Authorization"));
    if (userPass == String::nullstr) {
        debug_d("ApplicationWebserver::authenticated - No auth header");
        return false; // header missing
    }

    debug_d("ApplicationWebserver::authenticated Auth header: %s", userPass.c_str());

    // header in form of: "Basic MTIzNDU2OmFiY2RlZmc="so the 6 is to get to beginning of 64 encoded string
    userPass = userPass.substring(6); //cut "Basic " from start
    if (userPass.length() > 50) {
        return false;
    }

    userPass = base64_decode(userPass);
    debug_d("ApplicationWebserver::authenticated Password: '%s' - Expected password: '%s'", userPass.c_str(), app.cfg.general.api_password.c_str());
    if (userPass.endsWith(app.cfg.general.api_password)) {
        return true;
    }
    return false;
}

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticated(HttpRequest &request, HttpResponse &response) {
    bool authenticated = authenticateExec(request, response);

    if (!authenticated) {
        response.code = HTTP_STATUS_UNAUTHORIZED;
        response.setHeader(F("WWW-Authenticate"), F("Basic realm=\"RGBWW Server\""));
        response.setHeader(F("401 wrong credentials"), F("wrong credentials"));
        response.setHeader(F("Connection"), F("close"));
    }
    return authenticated;
}

String ApplicationWebserver::getApiCodeMsg(API_CODES code) {
    switch (code) {
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

void ApplicationWebserver::sendApiResponse(HttpResponse &response, JsonObjectStream* stream, HttpStatus code) {
    if (!checkHeap(response)) {
        delete stream;
        return;
    }

    response.setAllowCrossDomainOrigin("*");
    response.setHeader(F("accept"),F("GET, POST, OPTIONS"));
    response.setAllowCrossDomainOrigin("*");

    if (code != HTTP_STATUS_OK) {
        response.code = HTTP_STATUS_BAD_REQUEST;
    }
    response.sendDataStream(stream, MIME_JSON);
}

void ApplicationWebserver::sendApiCode(HttpResponse &response, API_CODES code, String msg /* = "" */) {
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    if (msg == "") {
        msg = getApiCodeMsg(code);
    }
    if (code == API_CODES::API_SUCCESS) {
        json[F("success")] = true;
        sendApiResponse(response, stream, HTTP_STATUS_OK);
    } else {
        json[F("error")] = msg;
        sendApiResponse(response, stream, HTTP_STATUS_BAD_REQUEST);
    }
}

void ApplicationWebserver::onFile(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
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
    if (fileName[0] == '/')
        fileName = fileName.substring(1);
    if (fileName[0] == '.') {
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
		if(!v){ 
            debug_i("file %s not found in filemap", fileName.c_str());
            if (!app.isFilesystemMounted()) {
                response.setContentType(MIME_TEXT);
                response.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
                response.sendString(F("No filesystem mounted"));
                return;
            }
            if (!fileExist(fileName) && !fileExist(fileName + ".gz") && WifiAccessPoint.isEnabled()) {
                //if accesspoint is active and we couldn`t find the file - redirect to index
                debug_d("ApplicationWebserver::onFile redirecting");
                response.headers[HTTP_HEADER_LOCATION] = F("http://") + WifiAccessPoint.getIP().toString() +"/";
            } else {
                #ifndef NOCACHE
                response.setCache(86400, true); // It's important to use cache for better performance.
                #endif
                response.code=HTTP_STATUS_OK;
                response.sendFile(fileName);
            }
			return;
		}
	}

	debug_i("found %s in fileMap", String(v.key()).c_str());
	auto stream = new FSTR::Stream(v.content());
	response.sendDataStream(stream, ContentType::fromFullFileName(fileName));
    

}
void ApplicationWebserver::onWebapp(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

    response.headers[HTTP_HEADER_LOCATION]=F("/index.html");
    response.setAllowCrossDomainOrigin("*");

    response.code = HTTP_STATUS_PERMANENT_REDIRECT;
    response.sendString(F("Redirecting to /index.html"));
}

void ApplicationWebserver::onIndex(HttpRequest &request, HttpResponse &response) {
    debug_i("http onIndex");
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        response.setContentType(MIME_TEXT);
        response.code = HTTP_STATUS_SERVICE_UNAVAILABLE;
        response.sendString(F("OTA in progress"));
        return;	void publishTransitionFinished(const String& name, bool requeued = false);
    }
#endif

response.headers[HTTP_HEADER_LOCATION]=F("/index.html");
    response.setAllowCrossDomainOrigin("*");

    response.code = HTTP_STATUS_PERMANENT_REDIRECT;
    response.sendString(F("Redirecting to /index.html"));

}

bool ApplicationWebserver::checkHeap(HttpResponse &response) {
    unsigned fh = system_get_free_heap_size();
    if (fh < _minimumHeap) {
        response.setAllowCrossDomainOrigin("*");
        response.code = HTTP_STATUS_TOO_MANY_REQUESTS;
        response.setHeader(F("Retry-After"), "2");
        return false;
    }
    return true;
}

void ApplicationWebserver::onConfig(HttpRequest &request, HttpResponse &response) {
    debug_i("onConfig");
    if (!checkHeap(response))
        return;

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif
    

    if (request.method != HTTP_POST && request.method != HTTP_GET && request.method!=HTTP_OPTIONS) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not POST, GET or OPTIONS request"));
        return;
    }
    
    /*
    / handle HTTP_OPTIONS request to check if server is CORS permissive (which this firmware 
    / has been for years) this is just to reply to that request in order to pass the CORS test
    */
    if (request.method == HTTP_OPTIONS){
        // probably a CORS preflight request
        response.setAllowCrossDomainOrigin("*");
        response.setHeader("Access-Control-Allow-Headers", "Content-Type");
        sendApiCode(response,API_CODES::API_SUCCESS,"");
        debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
        return;
    }
    
    if (request.method == HTTP_POST) {
        debug_i("======================\nHTTP POST request received, ");

        StaticJsonDocument<CONFIG_MAX_LENGTH> doc;
    	
        String error_msg;
        bool error=false;
        
        if(!Json::deserialize(doc, request.getBodyStream())) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse HTTP body"));
            error_msg = getApiCodeMsg(API_CODES::API_BAD_REQUEST);
            return;
        }

        // remove comment for debugging
        debug_i("serialized json object");
        Json::serialize(doc, Serial, Json::Pretty);

        bool ip_updated = false;
        bool color_updated = false;
        bool ap_updated = false;

        JsonObject root = doc.as<JsonObject>();
        if (root.isNull()) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("no root object"));
            return;
        }
        response.setAllowCrossDomainOrigin("*");
        JsonObject jnet = root[F("network")];
        if (!jnet.isNull()) {
  
            JsonObject con = jnet[F("connection")];
            if (!con.isNull()) {
                ip_updated |= Json::getBoolTolerantChanged(con[F("dhcp")], app.cfg.network.connection.dhcp);

                if (!app.cfg.network.connection.dhcp) {
                    //only change if dhcp is off - otherwise ignore
                    IpAddress ip, netmask, gateway;
                    const char* str;
                    if (Json::getValue(con[F("ip")], str)) {
                    	ip = str;
                        if (!(ip == app.cfg.network.connection.ip)) {
                            app.cfg.network.connection.ip = ip;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing ip";
                    }
                    if (Json::getValue(con[F("netmask")], str)) {
                        netmask = str;
                        if (!(netmask == app.cfg.network.connection.netmask)) {
                            app.cfg.network.connection.netmask = netmask;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = F("missing netmask");
                    }
                    if (Json::getValue(con[F("gateway")], str)) {
                        gateway = str;
                        if (!(gateway == app.cfg.network.connection.gateway)) {
                            app.cfg.network.connection.gateway = gateway;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = F("missing gateway");
                    }
                }
            }
            if (!jnet[F("ap")].isNull()) {
            	String ssid;
            	ap_updated |= Json::getValueChanged(jnet[F("ap")][F("ssid")], app.cfg.network.ap.ssid);

            	bool secured;
                if (Json::getBoolTolerant(jnet[F("ap")][F("secured")], secured)) {
                    if (secured) {
                        if (Json::getValueChanged(jnet[F("ap")][F("password")], app.cfg.network.ap.password)) {
							app.cfg.network.ap.secured = true;
							ap_updated = true;
                        } else {
                            error = true;
                            error_msg = "missing password for securing ap";
                        }
                    } else if (secured != app.cfg.network.ap.secured) {
                        app.cfg.network.ap.secured = secured;
                        ap_updated = true;
                    }
                }
            }

            JsonObject jmqtt = jnet[F("mqtt")];
            if (!jmqtt.isNull()) {
                //TODO: what to do if changed?
            	Json::getBoolTolerant(jmqtt[F("enabled")], app.cfg.network.mqtt.enabled);
            	Json::getValue(jmqtt[F("server")], app.cfg.network.mqtt.server);
            	Json::getValue(jmqtt[F("port")], app.cfg.network.mqtt.port);
            	Json::getValue(jmqtt[F("username")], app.cfg.network.mqtt.username);
            	Json::getValue(jmqtt[F("password")], app.cfg.network.mqtt.password);
            	Json::getValue(jmqtt[F("topic_base")], app.cfg.network.mqtt.topic_base);
            }
        }

        JsonObject jcol = root[F("color")];
        if (!jcol.isNull()) {

        	JsonObject jhsv = jcol[F("hsv")];
            if (!jhsv.isNull()) {
            	color_updated |= Json::getValueChanged(jhsv[F("model")], app.cfg.color.hsv.model);
            	color_updated |= Json::getValueChanged(jhsv[F("red")], app.cfg.color.hsv.red);
            	color_updated |= Json::getValueChanged(jhsv[F("yellow")], app.cfg.color.hsv.yellow);
            	color_updated |= Json::getValueChanged(jhsv[F("green")], app.cfg.color.hsv.green);
            	color_updated |= Json::getValueChanged(jhsv[F("cyan")], app.cfg.color.hsv.cyan);
            	color_updated |= Json::getValueChanged(jhsv[F("blue")], app.cfg.color.hsv.blue);
            	color_updated |= Json::getValueChanged(jhsv[F("magenta")], app.cfg.color.hsv.magenta);
            }
        	color_updated |= Json::getValueChanged(jcol[F("outputmode")], app.cfg.color.outputmode);
        	Json::getValue(jcol[F("startup_color")], app.cfg.color.startup_color);

        	JsonObject jbri = jcol[F("brightness")];
        	if (!jbri.isNull()) {
        		color_updated |= Json::getValueChanged(jbri[F("red")], app.cfg.color.brightness.red);
        		color_updated |= Json::getValueChanged(jbri[F("green")], app.cfg.color.brightness.green);
        		color_updated |= Json::getValueChanged(jbri[F("blue")], app.cfg.color.brightness.blue);
        		color_updated |= Json::getValueChanged(jbri[F("ww")], app.cfg.color.brightness.ww);
        		color_updated |= Json::getValueChanged(jbri[F("cw")], app.cfg.color.brightness.cw);
            }

        	JsonObject jcoltemp = jcol[F("colortemp")];
        	if (!jcoltemp.isNull()) {
        		color_updated |= Json::getValueChanged(jcoltemp[F("ww")], app.cfg.color.colortemp.ww);
        		color_updated |= Json::getValueChanged(jcoltemp[F("cw")], app.cfg.color.colortemp.cw);
            }
        }

        JsonObject jsec = root[F("security")];
        if (!jsec.isNull()) {
        	bool secured;
        	if (Json::getBoolTolerant(jsec[F("api_secured")], secured)) {
                if (secured) {
                    if (Json::getValue(jsec[F("api_password")], app.cfg.general.api_password)) {
                        app.cfg.general.api_secured = secured;
                    } else {
                        error = true;
                        error_msg = "missing password to secure settings";
                    }
                } else {
                    app.cfg.general.api_secured = false;
                    app.cfg.general.api_password = nullptr;
                }

            }
        }

        Json::getValue(root[F("ota")][F("url")], app.cfg.general.otaurl);

        JsonObject jgen = root[F("general")];
        if (!jgen.isNull()) {
            debug_i("general settings found");
        	Json::getValue(jgen[F("device_name")], app.cfg.general.device_name);
        	debug_i("device_name: %s", app.cfg.general.device_name.c_str());
            Json::getValue(jgen[F("pin_config")], app.cfg.general.pin_config);
          	Json::getValue(jgen[F("buttons_config")], app.cfg.general.buttons_config);
        	Json::getValue(jgen[F("buttons_debounce_ms")], app.cfg.general.buttons_debounce_ms);
            Json::getValue(jgen[F("current_pin_config_name")],app.cfg.general.pin_config_name);
            debug_i("pin_config_name: %s", app.cfg.general.pin_config_name.c_str());
            Json::getValue(jgen[F("pin_config_url")],app.cfg.general.pin_config_url);
            // read channels array from config and push it to app.cfg.general.channels
            // if there are already channels in the vector, clear it first 
            JsonArray jchannels = jgen[F("channels")];
            debug_i("populating channels");
            Json::serialize(jchannels, Serial, Json::Pretty);

            if (app.cfg.general.channels.size()!=0){
                app.cfg.general.channels.clear();
            }
            uint8_t numChannels=jchannels.size();
            debug_i("populating %i channels", numChannels);
            for (uint8_t i = 0; i < jchannels.size(); i++) {
                JsonObject jchannel = jchannels[i];
                if (!jchannel.isNull()) {
                    channel channel;
                    Json::getValue(jchannel[F("pin")], channel.pin);
                    Json::getValue(jchannel[F("name")], channel.name);
                    debug_i("adding channel %i with name %s",channel.pin, channel.name);
                    app.cfg.general.channels.push_back(channel);
                }
            }
        }

        JsonObject jntp = root[F("ntp")];
        if (!jntp.isNull()) {
            Json::getBoolTolerant(jntp[F("enabled")], app.cfg.ntp.enabled);
            Json::getValue(jntp[F("server")], app.cfg.ntp.server);
            Json::getValue(jntp[F("interval")], app.cfg.ntp.interval);
        }

        JsonObject jsync = root[F("sync")];
        if (!jsync.isNull()) {
            Json::getBoolTolerant(jsync[F("clock_master_enabled")], app.cfg.sync.clock_master_enabled);
        	Json::getValue(jsync[F("clock_master_interval")], app.cfg.sync.clock_master_interval);
        	Json::getBoolTolerant(jsync[F("clock_slave_enabled")], app.cfg.sync.clock_slave_enabled);
        	Json::getValue(jsync[F("clock_slave_topic")], app.cfg.sync.clock_slave_topic);
        	Json::getBoolTolerant(jsync[F("cmd_master_enabled")], app.cfg.sync.cmd_master_enabled);
        	Json::getBoolTolerant(jsync[F("cmd_slave_enabled")], app.cfg.sync.cmd_slave_enabled);
        	Json::getValue(jsync[F("cmd_slave_topic")], app.cfg.sync.cmd_slave_topic);

        	Json::getBoolTolerant(jsync[F("color_master_enabled")], app.cfg.sync.color_master_enabled);
        	Json::getValue(jsync[F("color_master_interval_ms")], app.cfg.sync.color_master_interval_ms);
        	Json::getBoolTolerant(jsync[F("color_slave_enabled")], app.cfg.sync.color_slave_enabled);
        	Json::getValue(jsync[F("color_slave_topic")], app.cfg.sync.color_slave_topic);
        }

        JsonObject jevents = root[F("events")];
        if (!jevents.isNull()) {
        	Json::getValue(jevents[F("color_interval_ms")], app.cfg.events.color_interval_ms);
        	Json::getValue(jevents[F("color_mininterval_ms")], app.cfg.events.color_mininterval_ms);
        	Json::getBoolTolerant(jevents[F("server_enabled")], app.cfg.events.server_enabled);
        	Json::getValue(jevents[F("transfin_interval_ms")], app.cfg.events.transfin_interval_ms);
        }

        app.cfg.sanitizeValues();

        // update and save settings if we haven`t received any error until now
        if (!error) {
        	bool restart = root[F("restart")] | false;
            if (ip_updated) {
            	if (restart) {
					debug_i("ApplicationWebserver::onConfig ip settings changed - rebooting");
					app.delayedCMD(F("restart"), 3000); // wait 3s to first send response
					//json[F("data")] = "restart";
                }
            }
            if (ap_updated) {
				if (restart && WifiAccessPoint.isEnabled()) {
					debug_i("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
					app.delayedCMD(F("restart"), 3000); // wait 3s to first send response
					//json[F("data")] = "restart";

				}
            }
            if (color_updated) {
                debug_d("ApplicationWebserver::onConfig color settings changed - refreshing");

                //refresh settings
                app.rgbwwctrl.setup();

                //refresh current output
                app.rgbwwctrl.refresh();

            }
            debug_i("saving config");
            app.cfg.save();
            JsonObject root = doc.as<JsonObject>();
            sendApiCode(response, API_CODES::API_SUCCESS);
        } else {
            debug_i("config api error %s",error_msg.c_str());
            JsonObject root = doc.as<JsonObject>();
            sendApiCode(response, API_CODES::API_MISSING_PARAM, error_msg);
        }

    } else {
        JsonObjectStream* stream = new JsonObjectStream(CONFIG_MAX_LENGTH);
        JsonObject json = stream->getRoot();
        // returning settings
        JsonObject net = json.createNestedObject(F("network"));
        JsonObject con = net.createNestedObject(F("connection"));
        con[F("dhcp")] = WifiStation.isEnabledDHCP();

        //con[F("ip")] = WifiStation.getIP().toString();
        //con[F("netmask")] = WifiStation.getNetworkMask().toString();
        //con[F("gateway")] = WifiStation.getNetworkGateway().toString();

        con[F("ip")] = app.cfg.network.connection.ip.toString();
        con[F("netmask")] = app.cfg.network.connection.netmask.toString();
        con[F("gateway")] = app.cfg.network.connection.gateway.toString();

        JsonObject ap = net.createNestedObject("ap");
        ap[F("secured")] = app.cfg.network.ap.secured;
        ap[F("password")] = app.cfg.network.ap.password;
        ap[F("ssid")] = app.cfg.network.ap.ssid;

        JsonObject mqtt = net.createNestedObject("mqtt");
        mqtt[F("enabled")] = app.cfg.network.mqtt.enabled;
        mqtt[F("server")] = app.cfg.network.mqtt.server;
        mqtt[F("port")] = app.cfg.network.mqtt.port;
        mqtt[F("username")] = app.cfg.network.mqtt.username;
        mqtt[F("password")] = app.cfg.network.mqtt.password;
        mqtt[F("topic_base")] = app.cfg.network.mqtt.topic_base;

        JsonObject color = json.createNestedObject("color");
        color[F("outputmode")] = app.cfg.color.outputmode;
        color[F("startup_color")] = app.cfg.color.startup_color;

        JsonObject hsv = color.createNestedObject("hsv");
        hsv[F("model")] = app.cfg.color.hsv.model;

        hsv[F("red")] = app.cfg.color.hsv.red;
        hsv[F("yellow")] = app.cfg.color.hsv.yellow;
        hsv[F("green")] = app.cfg.color.hsv.green;
        hsv[F("cyan")] = app.cfg.color.hsv.cyan;
        hsv[F("blue")] = app.cfg.color.hsv.blue;
        hsv[F("magenta")] = app.cfg.color.hsv.magenta;

        JsonObject brighntess = color.createNestedObject("brightness");
        brighntess[F("red")] = app.cfg.color.brightness.red;
        brighntess[F("green")] = app.cfg.color.brightness.green;
        brighntess[F("blue")] = app.cfg.color.brightness.blue;
        brighntess[F("ww")] = app.cfg.color.brightness.ww;
        brighntess[F("cw")] = app.cfg.color.brightness.cw;

        JsonObject ctmp = color.createNestedObject("colortemp");
        ctmp[F("ww")] = app.cfg.color.colortemp.ww;
        ctmp[F("cw")] = app.cfg.color.colortemp.cw;

        JsonObject s = json.createNestedObject("security");
        s[F("api_secured")] = app.cfg.general.api_secured;

        JsonObject ota = json.createNestedObject("ota");
        ota[F("url")] = app.cfg.general.otaurl;

        JsonObject sync = json.createNestedObject("sync");
        sync[F("clock_master_enabled")] = app.cfg.sync.clock_master_enabled;
        sync[F("clock_master_interval")] = app.cfg.sync.clock_master_interval;
        sync[F("clock_slave_enabled")] = app.cfg.sync.clock_slave_enabled;
        sync[F("clock_slave_topic")] = app.cfg.sync.clock_slave_topic;
        sync[F("cmd_master_enabled")] = app.cfg.sync.cmd_master_enabled;
        sync[F("cmd_slave_enabled")] = app.cfg.sync.cmd_slave_enabled;
        sync[F("cmd_slave_topic")] = app.cfg.sync.cmd_slave_topic;

        sync[F("color_master_enabled")] = app.cfg.sync.color_master_enabled;
        sync[F("color_master_interval_ms")] = app.cfg.sync.color_master_interval_ms;
        sync[F("color_slave_enabled")] = app.cfg.sync.color_slave_enabled;
        sync[F("color_slave_topic")] = app.cfg.sync.color_slave_topic;

        JsonObject events = json.createNestedObject("events");
        events[F("color_interval_ms")] = app.cfg.events.color_interval_ms;
        events[F("color_mininterval_ms")] = app.cfg.events.color_mininterval_ms;
        events[F("server_enabled")] = app.cfg.events.server_enabled;
        events[F("transfin_interval_ms")] = app.cfg.events.transfin_interval_ms;

        JsonObject general = json.createNestedObject("general");
        general[F("device_name")] = app.cfg.general.device_name;
        general[F("pin_config")] = app.cfg.general.pin_config;
        general[F("buttons_config")] = app.cfg.general.buttons_config;
        general[F("buttons_debounce_ms")] = app.cfg.general.buttons_debounce_ms;
        general[F("current_pin_config_name")] = app.cfg.general.pin_config_name;
        general[F("pin_config_url")] = app.cfg.general.pin_config_url;

        auto channels = general.createNestedArray("channels");
        debug_i("adding channels to json");
        for(uint8_t channel=0;channel<app.cfg.general.channels.size();channel++){
            StaticJsonDocument<64> channelConfig;
            debug_i("adding channel %i with name %s",app.cfg.general.channels[channel].pin, app.cfg.general.channels[channel].name.c_str());
            channelConfig[F("pin")] = app.cfg.general.channels[channel].pin;
            channelConfig[F("name")] = app.cfg.general.channels[channel].name;
            channels.add(channelConfig);
        }

        auto supported_color_models = general.createNestedArray(F("supported_color_models"));
        debug_i("adding color models");
        for(uint8_t i=0;i<app.cfg.general.supported_color_models.size();i++){
            debug_i("adding color model %s",app.cfg.general.supported_color_models[i].c_str());
            String color_model=app.cfg.general.supported_color_models[i];
            supported_color_models.add(color_model);
        }
        response.setAllowCrossDomainOrigin("*");
        debug_i("sending config json of size %i",sizeof(stream));
        sendApiResponse(response, stream);
    }
}

void ApplicationWebserver::onInfo(HttpRequest &request, HttpResponse &response) {
    if (!checkHeap(response))
        return;

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif

    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET");
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject data = stream->getRoot();
    data[F("deviceid")] = String(system_get_chip_id());
    data[F("current_rom")] = String(app.ota.getRomPartition().name());
    data[F("git_version")] = fw_git_version;
    data[F("git_date")] = fw_git_date;
    data[F("webapp_version")] = WEBAPP_VERSION;
    data[F("api_version")]=F("2");
    data[F("sming")] = SMING_VERSION;
    data[F("event_num_clients")] = app.eventserver.activeClients;
    data[F("uptime")] = app.getUptime();
    data[F("heap_free")] = system_get_free_heap_size();
    data[F("config_size")]=sizeof(app.cfg);
    #ifdef ARCH_ESP8266
        data[F("soc")]=F("Esp8266");
    #elif ARCH_ESP32
        data[F("soc")]=F("Esp32");
    #endif   
    #ifdef PART_LAYOUT
        data[F("part_layout")]=PART_LAYOUT;
    #else
        data[F("part_layout")]=F("v1");
    #endif

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
void ApplicationWebserver::onColorGet(HttpRequest &request, HttpResponse &response) {
    if (!checkHeap(response))
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

    response.setAllowCrossDomainOrigin("*");
    response.setHeader("Access-Control-Allow-Origin", "*");

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
void ApplicationWebserver::onColorPost(HttpRequest &request, HttpResponse &response) {
    String body = request.getBody();
    response.setAllowCrossDomainOrigin("*");
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setHeader("Access-Control-Allow-Methods","GET, PUT, POST, OPTIONS");
    response.setHeader("Access-Control-Allow-Credentials","true");

    if (body == NULL) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "no body");
        return;
    }

    String msg;
    
    if (!app.jsonproc.onColor(body, msg)) {
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
void ApplicationWebserver::onColor(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif
    debug_i("received /color request");
    response.setAllowCrossDomainOrigin("*");
    response.setHeader("Access-Control-Allow-Origin", "*");

    if (request.method != HTTP_POST && request.method != HTTP_GET && request.method!=HTTP_OPTIONS) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST, GET or OPTIONS");
        return;
    }

    if (request.method==HTTP_OPTIONS){
            response.setHeader("Access-Control-Allow-Methods","GET, PUT, POST, OPTIONS");

        sendApiCode(response, API_CODES::API_SUCCESS);
        return;
    }

    bool error = false;
    if (request.method == HTTP_POST) {
        ApplicationWebserver::onColorPost(request, response);
    } else {
        ApplicationWebserver::onColorGet(request, response);
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
bool ApplicationWebserver::isPrintable(String& str) {
    for (unsigned int i=0; i < str.length(); ++i)
    {
        char c = str[i];
        if (c < 0x20)
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
void ApplicationWebserver::onNetworks(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif
    if (request.method == HTTP_OPTIONS){
        response.setAllowCrossDomainOrigin("*");

        sendApiCode(response, API_CODES::API_SUCCESS);
        return;
    }
    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();

    bool error = false;

    if (app.network.isScanning()) {
        json[F("scanning")] = true;
    } else {
        json[F("scanning")] = false;
        JsonArray netlist = json.createNestedArray("available");
        BssList networks = app.network.getAvailableNetworks();
        for (unsigned int i = 0; i < networks.count(); i++) {
            if (networks[i].hidden)
                continue;

            // SSIDs may contain any byte values. Some are not printable and will cause the javascript client to fail
            // on parsing the message. Try to filter those here
            if (!ApplicationWebserver::isPrintable(networks[i].ssid)) {
                debug_w("Filtered SSID due to unprintable characters: %s", networks[i].ssid.c_str());
                continue;
            }

            JsonObject item = netlist.createNestedObject();
            item[F("id")] = (int) networks[i].getHashId();
            item[F("ssid")] = networks[i].ssid;
            item[F("signal")] = networks[i].rssi;
            item[F("encryption")] = networks[i].getAuthorizationMethodName();
            //limit to max 25 networks
            if (i >= 25)
                break;
        }
    }
            response.setAllowCrossDomainOrigin("*");
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
void ApplicationWebserver::onScanNetworks(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif

    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }
    if (!app.network.isScanning()) {
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
void ApplicationWebserver::onConnect(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif

    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST or GET");
        return;
    }

    if (request.method == HTTP_POST) {

        String body = request.getBody();
        if (body == NULL) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not get HTTP body");
            return;

        }
        DynamicJsonDocument doc(1024);
        Json::deserialize(doc, body);
        String ssid;
        String password;
        if (Json::getValue(doc[F("ssid")], ssid)) {
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
        if (status == CONNECTION_STATUS::ERROR) {
            json[F("error")] = app.network.get_con_err_msg();
        } else if (status == CONNECTION_STATUS::CONNECTED) {
            // return connected
            if (app.cfg.network.connection.dhcp) {
                json[F("ip")] = WifiStation.getIP().toString();
            } else {
                json[F("ip")] = app.cfg.network.connection.ip.toString();
            }
            json[F("dhcp")] = app.cfg.network.connection.dhcp;
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
void ApplicationWebserver::onSystemReq(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }
#endif

    if(request.method == HTTP_OPTIONS){
        response.setAllowCrossDomainOrigin("*");

        sendApiCode(response, API_CODES::API_SUCCESS);
        return;
    }
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    bool error = false;
    String body = request.getBody();
    if (body == NULL) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not get HTTP body");
        return;
    } else {
        debug_i("ApplicationWebserver::onSystemReq: %s", body.c_str());
        DynamicJsonDocument doc(1024);
        Json::deserialize(doc, body);

        String cmd = doc[F("cmd")].as<const char*>();
        if (cmd) {
            if (cmd.equals("debug")) {
            	bool enable;
            	if (Json::getValue(doc[F("enable")], enable)) {
                    Serial.systemDebugOutput(enable);
                } else {
                    error = true;
                }

            }else if (!app.delayedCMD(cmd, 1500)) {
                error = true;
            }

        } else {
            error = true;
        }

    }
    response.setAllowCrossDomainOrigin("*");

    
    if (!error) {
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
void ApplicationWebserver::onUpdate(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_HOST
    sendApiCode(response, API_CODES::API_BAD_REQUEST, "not supported on Host");
    return;
#else
    if (request.method == HTTP_OPTIONS){
        // probably a CORS request
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response,API_CODES::API_SUCCESS,"");
        debug_i("/update HTTP_OPTIONS Request, sent API_SUCCSSS");
        return;
    }   if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST or GET");
        return;
    }

    if (request.method == HTTP_POST) {
        if (app.ota.isProccessing()) {
            sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
            return;
        }

        String body = request.getBody();
        if (body == NULL) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
            return;
        }

        debug_i("body: %s", body.c_str());
        DynamicJsonDocument doc(1024);
            Json::deserialize(doc, body);

        String romurl;
        Json::getValue(doc[F("rom")][F("url")],romurl);
        
        //String spiffsurl;
        //Json::getValue(doc[F("spiffs")][F("url")],spiffsurl);
        
        debug_i("starting update process with \n    romurl: %s", romurl.c_str());
        if (! romurl ) {
            sendApiCode(response, API_CODES::API_MISSING_PARAM);
        } else {
            app.ota.start(romurl);
            response.setAllowCrossDomainOrigin("*");
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
void ApplicationWebserver::onPing(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    json[F("ping")] = "pong";
    sendApiResponse(response, stream);
}

void ApplicationWebserver::onStop(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onStop(request.getBody(), msg, true)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onSkip(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onSkip(request.getBody(), msg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onPause(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onPause(request.getBody(), msg, true)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onContinue(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onContinue(request.getBody(), msg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onBlink(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onBlink(request.getBody(), msg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onToggle(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }

    String msg;
    if (app.jsonproc.onToggle(request.getBody(), msg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    }
    else {
        sendApiCode(response, API_CODES::API_BAD_REQUEST);
    }
}

void ApplicationWebserver::onStorage(HttpRequest &request, HttpResponse &response){
    if (request.method != HTTP_POST && request.method != HTTP_GET && request.method!=HTTP_OPTIONS) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST, GET or OPTIONS request");
        return;
    }
    
    /*
    / axios sends a HTTP_OPTIONS request to check if server is CORS permissive (which this firmware 
    / has been for years) this is just to reply to that request in order to pass the CORS test
    */
    if (request.method == HTTP_OPTIONS){
        // probably a CORS request
        sendApiCode(response,API_CODES::API_SUCCESS,"");
        debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
        return;
    }
    
    if (request.method == HTTP_POST) {
        debug_i("======================\nHTTP POST request received, ");
        String header=request.getHeader("Content-type");
        if(header!="application/json"){
            sendApiCode(response,API_BAD_REQUEST,"only json content allowed");
        }
        debug_i("got post with content type %s", header.c_str());
        String body = request.getBody();
        if (body == NULL || body.length()>FILE_MAX_SIZE) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
            return;
        }

        bool error = false;
        
        debug_i("body length: %i", body.length());
        DynamicJsonDocument doc(body.length()+32);
        Json::deserialize(doc, body);
        String fileName=doc[F("filename")];
        
        //DynamicJsonDocument data(body.length()+32);
        //Json::deserialize(data, Json::serialize(doc[F("data")]));
        //doc.clear(); //clearing the original document to save RAM
        debug_i("will save to file %s", fileName.c_str());
        debug_i("original document uses %i bytes", doc.memoryUsage());
        String data=doc[F("data")];
        debug_i("data: %s", data.c_str());
        
        FileHandle file=fileOpen(fileName.c_str(),IFS::OpenFlag::Write|IFS::OpenFlag::Create|IFS::OpenFlag::Truncate);
        if(!fileWrite(file, data.c_str(), data.length())){
            debug_e("Saving config to file %s failed!", fileName.c_str());
        }
        fileClose(file);
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response, API_CODES::API_SUCCESS);
        return;       
    }
}

void ApplicationWebserver::onHosts(HttpRequest &request, HttpResponse &response){
    if (request.method != HTTP_GET && request.method!=HTTP_OPTIONS) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET or OPTIONS request");
        return;
    }
    
    if (request.method == HTTP_OPTIONS){
        // probably a CORS request
        sendApiCode(response,API_CODES::API_SUCCESS,"");
        debug_i("HTTP_OPTIONS Request, sent API_SUCCSSS");
        return;
    }
    
    String myHosts;
    // Set the response body with the JSON
    response.setAllowCrossDomainOrigin("*");
    response.setContentType(F("application/json"));
    response.sendString(app.network.getMdnsHosts());

    return;
}

 void ApplicationWebserver::onObject(HttpRequest &request, HttpResponse &response){
    if (request.method == HTTP_OPTIONS){
        // probably a CORS request
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response,API_CODES::API_SUCCESS,"");
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
    debug_i("got request with uri %s for object type %s with id %s.",String(request.uri).c_str(), objectType.c_str(), objectId.c_str());
    #endif
    auto tcpConnections=getConnections();
    debug_i("===> (objEntry) nr of tcpConnections: %i", tcpConnections.size());

    if (objectType==""){
        #ifdef DEBUG_OBJECT_API
        debug_i("missing object type");        
        #endif
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "missing object type");
        return;
    }
    String types=F("gphs");
    if (types.indexOf(objectType)==-1||objectType.length()>1){
        #ifdef DEBUG_OBJECT_API
        debug_i("unsupported object type");
        #endif
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("unsupported object type"));
        return;
    }
    
    if( request.method == HTTP_GET) {
        if (objectId==""){
        
            //requested object type but no object id, list all objects of type
            Directory dir;
            if(!dir.open()) {
                debug_i("could not open dir");
                sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not open dir"));
                return;
            }else{
                JsonObjectStream* stream = new JsonObjectStream(CONFIG_MAX_LENGTH);
                JsonObject doc = stream->getRoot();

                JsonArray objectsList;

                switch (objectType.c_str()[0]) {
                    case 'g':
                        objectsList = doc.createNestedArray(F("groups")); 
                        break;
                        ;;
                    case 'p':
                        objectsList = doc.createNestedArray(F("presets"));
                        break;
                        ;;
                    case 'h':
                        objectsList = doc.createNestedArray(F("hosts")); 
                        break;
                        ;;
                    case 's':
                        objectsList = doc.createNestedArray(F("scenes"));
                        break;
                        ;;
                }

                while(dir.next()) {
                    String fileName=String(dir.stat().name);
                    if(fileName.substring(0,1)=="_"){
                        #ifdef DEBUG_OBJECT_API
                        debug_i("found file: %s",fileName.c_str());
                        debug_i("file has object type %s",fileName.substring(1,2).c_str()); 
                        #endif
                        if(fileName.substring(1,2)==objectType){
                            #ifdef DEBUG_OBJECT_API
                            debug_i("adding file %s to list",fileName);
                            debug_i("filename %s, extension starts at %i",fileName.c_str(),fileName.indexOf(F(".")));
                            #endif
                            objectId=fileName.substring(2, fileName.indexOf(F(".")));
                            objectsList.add(objectId);
                        }
                    }
                }
                response.setContentType(F("application/json"));
                response.setAllowCrossDomainOrigin("*");
                response.sendString(Json::serialize(doc));
                delete stream;
            }
        }else{
            //got GET with object type and id, return object, if available
            debug_i("HTTP GET request received, ");
            String fileName = "_"+objectType+ objectId + ".json"; 
            if (!fileName) {
                #ifdef DEBUG_OBJECT_API
                debug_i("file not found");
                #endif
                response.setAllowCrossDomainOrigin("*");
                sendApiCode(response, API_CODES::API_BAD_REQUEST, F("file not found"));
                return;
            }
            response.setContentType(F("application/json"));
            response.setAllowCrossDomainOrigin("*");
            #ifdef DEBUG_OBJECT_API
            debug_i("sending file %s", fileName.c_str());
            #endif
            response.setAllowCrossDomainOrigin("*");
            response.sendFile(fileName);
            return;
        }
    }
    if (request.method==HTTP_POST){
        debug_i(   "HTTP PUT request received, ");
        String body = request.getBody();
        #ifdef DEBUG_OBJECT_API
        debug_i("request body: %s", body.c_str());
        #endif
        if (body == NULL || body.length()>FILE_MAX_SIZE) {
            response.setAllowCrossDomainOrigin("*");
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse HTTP body"));
            #ifdef DEBUG_OBJECT_API
            debug_i("body is null or too long");
            #endif
            return;
        }
        StaticJsonDocument<FILE_MAX_SIZE> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            response.setAllowCrossDomainOrigin("*");
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse json from HTTP body"));
            #ifdef DEBUG_OBJECT_API
            debug_i("could not parse json");
            #endif
            return;
        }
        #ifdef DEBUG_OBJECT_API
        debug_i("parsed json, found name %s",String(doc[F("name")]).c_str());
        #endif
        if(objectId==""){
            //no object id, create new object
            if(doc[F("id")]!=""){
                objectId=String(doc[F("id")]);
            }else{
               debug_i("no object id, creating new object");
               objectId=makeId();
               doc[F("id")]=objectId;
            }
        }
        String fileName = "_"+objectType+objectId + ".json"; 
        #ifdef DEBUG_OBJECT_API
        debug_i("will save to file %s", fileName.c_str());
        #endif
        FileHandle file = fileOpen(fileName.c_str(), IFS::OpenFlag::Write|IFS::OpenFlag::Create|IFS::OpenFlag::Truncate);
        if (!file) {
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
        if(!fileWrite(file, bodyData.c_str(), bodyData.length())){
            #ifdef DEBUG_OBJECT_API
            debug_e("Saving config to file %s failed!", fileName.c_str());
            //should probably also send some error message to the client
            #endif
        }
        fileClose(file);

        response.setAllowCrossDomainOrigin("*");
        response.setContentType(F("application/json"));
        //doc.clear();
        doc[F("id")]=objectId;
        bodyData="";
        serializeJson(doc, bodyData);
        response.sendString(bodyData.c_str());

        // send websocket message to all connected clients to 
        // update them about the new object
        JsonRpcMessage msg("preset");
        JsonObject root = msg.getParams();
        root.set(doc.as<JsonObject>());        
        debug_i("rpc: root =%s",Json::serialize(root).c_str());
        debug_i("rpc: msg =%s",Json::serialize(msg.getRoot()).c_str());
        
        String jsonStr = Json::serialize(msg.getRoot());

        wsBroadcast(jsonStr);
        //sendApiCode(response, API_CODES::API_SUCCESS);

        return;       
    }
    if (request.method==HTTP_DELETE){
        String fileName = "_"+objectType+objectId + F(".json"); 
        FileHandle file = fileDelete(fileName.c_str());
        if (!file) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("file not found"));
            return;
        }
        fileClose(file);
        response.setAllowCrossDomainOrigin("*");
        sendApiCode(response, API_CODES::API_SUCCESS);
        return;       
    }

}

String ApplicationWebserver::makeId(){
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
    sprintf(___id, "%07u",(uint32_t)micros()%10000000);
    String __id=String(___id);
    String chipId=String(system_get_chip_id());
    String objectId=chipId+"-"+__id;
    #ifdef DEBUG_OBJECT_API
    debug_i("generated id %s ",objectId.c_str());
    #endif
    return objectId;
}
