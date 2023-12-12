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
 *
 */
#include <RGBWWCtrl.h>
#include <Data/WebHelpers/base64.h> 
#include <FlashString/Map.hpp>
#include <FlashString/Stream.hpp>

#define FILE_LIST(XX)                    \
    XX(app_min_css, "app.min.css.gz")    \
    XX(app_min_js, "app.min.js.gz")      \
    XX(index_html, "index.html.gz")      \
    XX(init_html, "init.html.gz")        \
    XX(favicon_ico, "favicon.ico.gz")       

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
    settings.minHeapSize = _minimumHeapAccept;
    settings.keepAliveSeconds = 5; // do not close instantly when no transmission occurs. some clients are a bit slow (like FHEM)
    configure(settings);

    // workaround for bug in Sming 3.5.0
    // https://github.com/SmingHub/Sming/issues/1236
    setBodyParser("*", bodyToStringParser);
}


void ApplicationWebserver::init() {
    paths.setDefault(HttpPathDelegate(&ApplicationWebserver::onFile, this));
    paths.set("/", HttpPathDelegate(&ApplicationWebserver::onIndex, this));
    paths.set("/webapp", HttpPathDelegate(&ApplicationWebserver::onWebapp, this));
    paths.set("/config", HttpPathDelegate(&ApplicationWebserver::onConfig, this));
    paths.set("/info", HttpPathDelegate(&ApplicationWebserver::onInfo, this));
    paths.set("/color", HttpPathDelegate(&ApplicationWebserver::onColor, this));
    paths.set("/networks", HttpPathDelegate(&ApplicationWebserver::onNetworks, this));
    paths.set("/scan_networks", HttpPathDelegate(&ApplicationWebserver::onScanNetworks, this));
    paths.set("/system", HttpPathDelegate(&ApplicationWebserver::onSystemReq, this));
    paths.set("/update", HttpPathDelegate(&ApplicationWebserver::onUpdate, this));
    paths.set("/connect", HttpPathDelegate(&ApplicationWebserver::onConnect, this));
    paths.set("/ping", HttpPathDelegate(&ApplicationWebserver::onPing, this));

    // animation controls
    paths.set("/stop", HttpPathDelegate(&ApplicationWebserver::onStop, this));
    paths.set("/skip", HttpPathDelegate(&ApplicationWebserver::onSkip, this));
    paths.set("/pause", HttpPathDelegate(&ApplicationWebserver::onPause, this));
    paths.set("/continue", HttpPathDelegate(&ApplicationWebserver::onContinue, this));
    paths.set("/blink", HttpPathDelegate(&ApplicationWebserver::onBlink, this));

    paths.set("/toggle", HttpPathDelegate(&ApplicationWebserver::onToggle, this));

    // storage api
    paths.set("/storage",HttpPathDelegate(&ApplicationWebserver::onStorage, this));

    _init = true;
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

    String userPass = request.getHeader("Authorization");
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
        response.setHeader("WWW-Authenticate", "Basic realm=\"RGBWW Server\"");
        response.setHeader("401 wrong credentials", "wrong credentials");
        response.setHeader("Connection", "close");
    }

    return authenticated;
}

String ApplicationWebserver::getApiCodeMsg(API_CODES code) {
    switch (code) {
    case API_CODES::API_MISSING_PARAM:
        return String("missing param");
    case API_CODES::API_UNAUTHORIZED:
        return String("authorization required");
    case API_CODES::API_UPDATE_IN_PROGRESS:
        return String("update in progress");
    default:
        return String("bad request");
    }
}

void ApplicationWebserver::sendApiResponse(HttpResponse &response, JsonObjectStream* stream, HttpStatus code) {
    if (!checkHeap(response)) {
        delete stream;
        return;
    }

    response.setAllowCrossDomainOrigin("*");
    response.setHeader("accept","GET, POST, OPTIONS");
    response.setHeader("Access-Control-Allow-Headers","*");
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
        json["success"] = true;
        sendApiResponse(response, stream, HTTP_STATUS_OK);
    } else {
        json["error"] = msg;
        sendApiResponse(response, stream, HTTP_STATUS_BAD_REQUEST);
    }
}

void ApplicationWebserver::onFile(HttpRequest &request, HttpResponse &response) {
    debug_i("gotten http file request");
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

    String fileName = request.uri.getRelativePath();

    //auto response = connection.getResponse();

	String compressed = fileName + ".gz";
	auto v = fileMap[compressed];
    response.setAllowCrossDomainOrigin("*");
	if(v) {
		response.headers[HTTP_HEADER_CONTENT_ENCODING] = _F("gzip");
       	debug_i("found %s in fileMap", String(v.key()).c_str());
    	auto stream = new FSTR::Stream(v.content());
	    response.sendDataStream(stream, ContentType::fromFullFileName(fileName));
	} else {
		v = fileMap[fileName];
		if(!v) {
                if (!fileExist(fileName) && !fileExist(fileName + ".gz")) {
			        debug_w("File '%s' not found in filemap or SPIFFS", fileName.c_str());
                    response.code = HTTP_STATUS_NOT_FOUND;
                    response.sendString("file not found");
                    return;
                }else{
   			        debug_w("File '%s' found in SPIFFS", fileName.c_str());
                    response.code=HTTP_STATUS_OK;
                    response.sendFile(fileName);
                }
			return;
		}else{
            debug_i("found %s in fileMap", String(v.key()).c_str());
            auto stream = new FSTR::Stream(v.content());
            response.sendDataStream(stream, ContentType::fromFullFileName(fileName));
        }
	}

	// Use client caching for better performance.
	//	response->setCache(86400, true);
/*
    if (!app.isFilesystemMounted()) {
        response.setContentType(MIME_TEXT);
        response.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        response.sendString("No filesystem mounted");
        return;
    }

    String file = request.uri.Path;
    if (file[0] == '/')
        file = file.substring(1);
    if (file[0] == '.') {
        response.code = HTTP_STATUS_FORBIDDEN;
        return;
    }

    if (!fileExist(file) && !fileExist(file + ".gz") && WifiAccessPoint.isEnabled()) {
        //if accesspoint is active and we couldn`t find the file - redirect to index
        debug_d("ApplicationWebserver::onFile redirecting");
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
    } else {
        response.setCache(86400, true); // It's important to use cache for better performance.
        response.sendFile(file);
    }
*/
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
        response.sendString("OTA in progress");
        return;
    }
#endif

    if (WifiAccessPoint.isEnabled()) {
        debug_i("adding AP headers");
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
    } else {
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiStation.getIP().toString() + "/webapp";
    }
    response.code = HTTP_STATUS_PERMANENT_REDIRECT;
}

void ApplicationWebserver::onWebapp(HttpRequest &request, HttpResponse &response) {
    debug_i("onWebapp");

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

    if (request.method != HTTP_GET) {
        response.code = HTTP_STATUS_BAD_REQUEST;
        return;
    }

    if (!app.isFilesystemMounted()) {
        debug_i("no filesystem");
        response.setContentType(MIME_TEXT);
        response.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        response.sendString("No filesystem mounted");
        return;
    }
    
    String fileName;
    if (!WifiStation.isConnected()) {
        // not yet connected - serve initial settings page
        debug_i("AP connection, sending init.html");
        fileName=F("init.html");
    } else {
        fileName=F("index.html");
    }
    String compressed = fileName + ".gz";
    auto v = fileMap[compressed];
    if(v) {
        response.headers[HTTP_HEADER_CONTENT_ENCODING] = _F("gzip");
    } else {
        v = fileMap[fileName];
        if(!v) {
            debug_w("File '%s' not found", fileName.c_str());
            response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
            return;
        }
    }

    debug_i("found %s in fileMap", String(v.key()).c_str());
    auto stream = new FSTR::Stream(v.content());
    response.sendDataStream(stream, ContentType::fromFullFileName(fileName));
}

bool ApplicationWebserver::checkHeap(HttpResponse &response) {
    unsigned fh = system_get_free_heap_size();
    if (fh < _minimumHeap) {
        response.code = HTTP_STATUS_TOO_MANY_REQUESTS;
        response.setHeader("Retry-After", "2");
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
        String body = request.getBody();
        debug_i("body: \n", body);
        if (body == NULL) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
            return;
        }

        bool error = false;
        String error_msg = getApiCodeMsg(API_CODES::API_BAD_REQUEST);
        DynamicJsonDocument doc(CONFIG_MAX_LENGTH);
        Json::deserialize(doc, body);

        // remove comment for debugging
        //Json::serialize(doc, Serial, Json::Pretty);

        bool ip_updated = false;
        bool color_updated = false;
        bool ap_updated = false;
        JsonObject root = doc.as<JsonObject>();
        if (root.isNull()) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, "no root object");
            return;
        }

        JsonObject jnet = root["network"];
        if (!jnet.isNull()) {
  
            JsonObject con = jnet["connection"];
            if (!con.isNull()) {
                ip_updated |= Json::getBoolTolerantChanged(con["dhcp"], app.cfg.network.connection.dhcp);

                if (!app.cfg.network.connection.dhcp) {
                    //only change if dhcp is off - otherwise ignore
                    IpAddress ip, netmask, gateway;
                    const char* str;
                    if (Json::getValue(con["ip"], str)) {
                    	ip = str;
                        if (!(ip == app.cfg.network.connection.ip)) {
                            app.cfg.network.connection.ip = ip;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing ip";
                    }
                    if (Json::getValue(con["netmask"], str)) {
                        netmask = str;
                        if (!(netmask == app.cfg.network.connection.netmask)) {
                            app.cfg.network.connection.netmask = netmask;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing netmask";
                    }
                    if (Json::getValue(con["gateway"], str)) {
                        gateway = str;
                        if (!(gateway == app.cfg.network.connection.gateway)) {
                            app.cfg.network.connection.gateway = gateway;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing gateway";
                    }

                }

            }
            if (!jnet["ap"].isNull()) {

            	String ssid;
            	ap_updated |= Json::getValueChanged(jnet["ap"]["ssid"], app.cfg.network.ap.ssid);

            	bool secured;
                if (Json::getBoolTolerant(jnet["ap"]["secured"], secured)) {
                    if (secured) {
                        if (Json::getValueChanged(jnet["ap"]["password"], app.cfg.network.ap.password)) {
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

            JsonObject jmqtt = jnet["mqtt"];
            if (!jmqtt.isNull()) {
                //TODO: what to do if changed?
            	Json::getBoolTolerant(jmqtt["enabled"], app.cfg.network.mqtt.enabled);
            	Json::getValue(jmqtt["server"], app.cfg.network.mqtt.server);
            	Json::getValue(jmqtt["port"], app.cfg.network.mqtt.port);
            	Json::getValue(jmqtt["username"], app.cfg.network.mqtt.username);
            	Json::getValue(jmqtt["password"], app.cfg.network.mqtt.password);
            	Json::getValue(jmqtt["topic_base"], app.cfg.network.mqtt.topic_base);
            }
        }

        JsonObject jcol = root["color"];
        if (!jcol.isNull()) {

        	JsonObject jhsv = jcol["hsv"];
            if (!jhsv.isNull()) {
            	color_updated |= Json::getValueChanged(jhsv["model"], app.cfg.color.hsv.model);
            	color_updated |= Json::getValueChanged(jhsv["red"], app.cfg.color.hsv.red);
            	color_updated |= Json::getValueChanged(jhsv["yellow"], app.cfg.color.hsv.yellow);
            	color_updated |= Json::getValueChanged(jhsv["green"], app.cfg.color.hsv.green);
            	color_updated |= Json::getValueChanged(jhsv["cyan"], app.cfg.color.hsv.cyan);
            	color_updated |= Json::getValueChanged(jhsv["blue"], app.cfg.color.hsv.blue);
            	color_updated |= Json::getValueChanged(jhsv["magenta"], app.cfg.color.hsv.magenta);
            }
        	color_updated |= Json::getValueChanged(jcol["outputmode"], app.cfg.color.outputmode);
        	Json::getValue(jcol["startup_color"], app.cfg.color.startup_color);

        	JsonObject jbri = jcol["brightness"];
        	if (!jbri.isNull()) {
        		color_updated |= Json::getValueChanged(jbri["red"], app.cfg.color.brightness.red);
        		color_updated |= Json::getValueChanged(jbri["green"], app.cfg.color.brightness.green);
        		color_updated |= Json::getValueChanged(jbri["blue"], app.cfg.color.brightness.blue);
        		color_updated |= Json::getValueChanged(jbri["ww"], app.cfg.color.brightness.ww);
        		color_updated |= Json::getValueChanged(jbri["cw"], app.cfg.color.brightness.cw);
            }

        	JsonObject jcoltemp = jcol["colortemp"];
        	if (!jcoltemp.isNull()) {
        		color_updated |= Json::getValueChanged(jcoltemp["ww"], app.cfg.color.colortemp.ww);
        		color_updated |= Json::getValueChanged(jcoltemp["cw"], app.cfg.color.colortemp.cw);
            }
        }

        JsonObject jsec = root["security"];
        if (!jsec.isNull()) {
        	bool secured;
        	if (Json::getBoolTolerant(jsec["api_secured"], secured)) {
                if (secured) {
                    if (Json::getValue(jsec["api_password"], app.cfg.general.api_password)) {
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

        Json::getValue(root["ota"]["url"], app.cfg.general.otaurl);

        JsonObject jgen = root["general"];
        if (!jgen.isNull()) {
        	Json::getValue(jgen["device_name"], app.cfg.general.device_name);
        	Json::getValue(jgen["pin_config"], app.cfg.general.pin_config);
        	Json::getValue(jgen["buttons_config"], app.cfg.general.buttons_config);
        	Json::getValue(jgen["buttons_debounce_ms"], app.cfg.general.buttons_debounce_ms);
        }

        JsonObject jntp = root["ntp"];
        if (!jntp.isNull()) {
            Json::getBoolTolerant(jntp["enabled"], app.cfg.ntp.enabled);
            Json::getValue(jntp["server"], app.cfg.ntp.server);
            Json::getValue(jntp["interval"], app.cfg.ntp.interval);
        }

        JsonObject jsync = root["sync"];
        if (!jsync.isNull()) {
            Json::getBoolTolerant(jsync["clock_master_enabled"], app.cfg.sync.clock_master_enabled);
        	Json::getValue(jsync["clock_master_interval"], app.cfg.sync.clock_master_interval);
        	Json::getBoolTolerant(jsync["clock_slave_enabled"], app.cfg.sync.clock_slave_enabled);
        	Json::getValue(jsync["clock_slave_topic"], app.cfg.sync.clock_slave_topic);
        	Json::getBoolTolerant(jsync["cmd_master_enabled"], app.cfg.sync.cmd_master_enabled);
        	Json::getBoolTolerant(jsync["cmd_slave_enabled"], app.cfg.sync.cmd_slave_enabled);
        	Json::getValue(jsync["cmd_slave_topic"], app.cfg.sync.cmd_slave_topic);

        	Json::getBoolTolerant(jsync["color_master_enabled"], app.cfg.sync.color_master_enabled);
        	Json::getValue(jsync["color_master_interval_ms"], app.cfg.sync.color_master_interval_ms);
        	Json::getBoolTolerant(jsync["color_slave_enabled"], app.cfg.sync.color_slave_enabled);
        	Json::getValue(jsync["color_slave_topic"], app.cfg.sync.color_slave_topic);
        }

        JsonObject jevents = root["events"];
        if (!jevents.isNull()) {
        	Json::getValue(jevents["color_interval_ms"], app.cfg.events.color_interval_ms);
        	Json::getValue(jevents["color_mininterval_ms"], app.cfg.events.color_mininterval_ms);
        	Json::getBoolTolerant(jevents["server_enabled"], app.cfg.events.server_enabled);
        	Json::getValue(jevents["transfin_interval_ms"], app.cfg.events.transfin_interval_ms);
        }

        app.cfg.sanitizeValues();

        // update and save settings if we haven`t received any error until now
        if (!error) {
        	bool restart = root["restart"] | false;
            if (ip_updated) {
            	if (restart) {
					debug_i("ApplicationWebserver::onConfig ip settings changed - rebooting");
					app.delayedCMD("restart", 3000); // wait 3s to first send response
					//json["data"] = "restart";
                }
            }
            if (ap_updated) {
				if (restart && WifiAccessPoint.isEnabled()) {
					debug_i("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
					app.delayedCMD("restart", 3000); // wait 3s to first send response
					//json["data"] = "restart";

				}
            }
            if (color_updated) {
                debug_d("ApplicationWebserver::onConfig color settings changed - refreshing");

                //refresh settings
                app.rgbwwctrl.setup();

                //refresh current output
                app.rgbwwctrl.refresh();

            }
            app.cfg.save();
            sendApiCode(response, API_CODES::API_SUCCESS);
        } else {
            sendApiCode(response, API_CODES::API_MISSING_PARAM, error_msg);
        }

    } else {
        JsonObjectStream* stream = new JsonObjectStream(CONFIG_MAX_LENGTH);
        JsonObject json = stream->getRoot();
        // returning settings
        JsonObject net = json.createNestedObject("network");
        JsonObject con = net.createNestedObject("connection");
        con["dhcp"] = WifiStation.isEnabledDHCP();

        //con["ip"] = WifiStation.getIP().toString();
        //con["netmask"] = WifiStation.getNetworkMask().toString();
        //con["gateway"] = WifiStation.getNetworkGateway().toString();

        con["ip"] = app.cfg.network.connection.ip.toString();
        con["netmask"] = app.cfg.network.connection.netmask.toString();
        con["gateway"] = app.cfg.network.connection.gateway.toString();

        JsonObject ap = net.createNestedObject("ap");
        ap["secured"] = app.cfg.network.ap.secured;
        ap["password"] = app.cfg.network.ap.password;
        ap["ssid"] = app.cfg.network.ap.ssid;

        JsonObject mqtt = net.createNestedObject("mqtt");
        mqtt["enabled"] = app.cfg.network.mqtt.enabled;
        mqtt["server"] = app.cfg.network.mqtt.server;
        mqtt["port"] = app.cfg.network.mqtt.port;
        mqtt["username"] = app.cfg.network.mqtt.username;
        mqtt["password"] = app.cfg.network.mqtt.password;
        mqtt["topic_base"] = app.cfg.network.mqtt.topic_base;

        JsonObject color = json.createNestedObject("color");
        color["outputmode"] = app.cfg.color.outputmode;
        color["startup_color"] = app.cfg.color.startup_color;

        JsonObject hsv = color.createNestedObject("hsv");
        hsv["model"] = app.cfg.color.hsv.model;

        hsv["red"] = app.cfg.color.hsv.red;
        hsv["yellow"] = app.cfg.color.hsv.yellow;
        hsv["green"] = app.cfg.color.hsv.green;
        hsv["cyan"] = app.cfg.color.hsv.cyan;
        hsv["blue"] = app.cfg.color.hsv.blue;
        hsv["magenta"] = app.cfg.color.hsv.magenta;

        JsonObject brighntess = color.createNestedObject("brightness");
        brighntess["red"] = app.cfg.color.brightness.red;
        brighntess["green"] = app.cfg.color.brightness.green;
        brighntess["blue"] = app.cfg.color.brightness.blue;
        brighntess["ww"] = app.cfg.color.brightness.ww;
        brighntess["cw"] = app.cfg.color.brightness.cw;

        JsonObject ctmp = color.createNestedObject("colortemp");
        ctmp["ww"] = app.cfg.color.colortemp.ww;
        ctmp["cw"] = app.cfg.color.colortemp.cw;

        JsonObject s = json.createNestedObject("security");
        s["api_secured"] = app.cfg.general.api_secured;

        JsonObject ota = json.createNestedObject("ota");
        ota["url"] = app.cfg.general.otaurl;

        JsonObject sync = json.createNestedObject("sync");
        sync["clock_master_enabled"] = app.cfg.sync.clock_master_enabled;
        sync["clock_master_interval"] = app.cfg.sync.clock_master_interval;
        sync["clock_slave_enabled"] = app.cfg.sync.clock_slave_enabled;
        sync["clock_slave_topic"] = app.cfg.sync.clock_slave_topic;
        sync["cmd_master_enabled"] = app.cfg.sync.cmd_master_enabled;
        sync["cmd_slave_enabled"] = app.cfg.sync.cmd_slave_enabled;
        sync["cmd_slave_topic"] = app.cfg.sync.cmd_slave_topic;

        sync["color_master_enabled"] = app.cfg.sync.color_master_enabled;
        sync["color_master_interval_ms"] = app.cfg.sync.color_master_interval_ms;
        sync["color_slave_enabled"] = app.cfg.sync.color_slave_enabled;
        sync["color_slave_topic"] = app.cfg.sync.color_slave_topic;

        JsonObject events = json.createNestedObject("events");
        events["color_interval_ms"] = app.cfg.events.color_interval_ms;
        events["color_mininterval_ms"] = app.cfg.events.color_mininterval_ms;
        events["server_enabled"] = app.cfg.events.server_enabled;
        events["transfin_interval_ms"] = app.cfg.events.transfin_interval_ms;

        JsonObject general = json.createNestedObject("general");
        general["device_name"] = app.cfg.general.device_name;
        general["pin_config"] = app.cfg.general.pin_config;
        general["buttons_config"] = app.cfg.general.buttons_config;
        general["buttons_debounce_ms"] = app.cfg.general.buttons_debounce_ms;

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
    data["deviceid"] = String(system_get_chip_id());
    data["current_rom"] = String(app.getRomSlot());
    data["git_version"] = fw_git_version;
    data["git_date"] = fw_git_date;
    data["webapp_version"] = WEBAPP_VERSION;
    data["sming"] = SMING_VERSION;
    data["event_num_clients"] = app.eventserver.activeClients;
    data["uptime"] = app.getUptime();
    data["heap_free"] = system_get_free_heap_size();

    JsonObject rgbww = data.createNestedObject("rgbww");
    rgbww["version"] = RGBWW_VERSION;
    rgbww["queuesize"] = RGBWW_ANIMATIONQSIZE;

    JsonObject con = data.createNestedObject("connection");
    con["connected"] = WifiStation.isConnected();
    con["ssid"] = WifiStation.getSSID();
    con["dhcp"] = WifiStation.isEnabledDHCP();
    con["ip"] = WifiStation.getIP().toString();
    con["netmask"] = WifiStation.getNetworkMask().toString();
    con["gateway"] = WifiStation.getNetworkGateway().toString();
    con["mac"] = WifiStation.getMAC();
    //con["mdnshostname"] = app.cfg.network.connection.mdnshostname.c_str();

    sendApiResponse(response, stream);
}


void ApplicationWebserver::onColorGet(HttpRequest &request, HttpResponse &response) {
    if (!checkHeap(response))
        return;

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();

    JsonObject raw = json.createNestedObject("raw");
    ChannelOutput output = app.rgbwwctrl.getCurrentOutput();
    raw["r"] = output.r;
    raw["g"] = output.g;
    raw["b"] = output.b;
    raw["ww"] = output.ww;
    raw["cw"] = output.cw;

    JsonObject hsv = json.createNestedObject("hsv");
    float h, s, v;
    int ct;
    HSVCT c = app.rgbwwctrl.getCurrentColor();
    c.asRadian(h, s, v, ct);
    hsv["h"] = h;
    hsv["s"] = s;
    hsv["v"] = v;
    hsv["ct"] = ct;

    sendApiResponse(response, stream);
}

void ApplicationWebserver::onColorPost(HttpRequest &request, HttpResponse &response) {
    String body = request.getBody();
    if (body == NULL) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "no body");
        return;

    }

    String msg;
    if (!app.jsonproc.onColor(body, msg)) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, msg);
    }
    else {

        sendApiCode(response, API_CODES::API_SUCCESS);
    }
}

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

    if (request.method != HTTP_POST && request.method != HTTP_GET && request.method!=HTTP_OPTIONS) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST, GET or OPTIONS");
        return;
    }

    if (request.method==HTTP_OPTIONS){
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

bool ApplicationWebserver::isPrintable(String& str) {
    for (unsigned int i=0; i < str.length(); ++i)
    {
        char c = str[i];
        if (c < 0x20)
            return false;
    }
    return true;
}

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

    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();

    bool error = false;

    if (app.network.isScanning()) {
        json["scanning"] = true;
    } else {
        json["scanning"] = false;
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
            item["id"] = (int) networks[i].getHashId();
            item["ssid"] = networks[i].ssid;
            item["signal"] = networks[i].rssi;
            item["encryption"] = networks[i].getAuthorizationMethodName();
            //limit to max 25 networks
            if (i >= 25)
                break;
        }
    }
    sendApiResponse(response, stream);
}

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
        if (Json::getValue(doc["ssid"], ssid)) {
        	password = doc["password"].as<const char*>();
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
        json["status"] = int(status);
        if (status == CONNECTION_STATUS::ERROR) {
            json["error"] = app.network.get_con_err_msg();
        } else if (status == CONNECTION_STATUS::CONNECTED) {
            // return connected
            if (app.cfg.network.connection.dhcp) {
                json["ip"] = WifiStation.getIP().toString();
            } else {
                json["ip"] = app.cfg.network.connection.ip.toString();
            }
            json["dhcp"] = app.cfg.network.connection.dhcp;
            json["ssid"] = WifiStation.getSSID();

        }
        sendApiResponse(response, stream);
    }
}

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

        String cmd = doc["cmd"].as<const char*>();
        if (cmd) {
            if (cmd.equals("debug")) {
            	bool enable;
            	if (Json::getValue(doc["enable"], enable)) {
                    Serial.systemDebugOutput(enable);
                } else {
                    error = true;
                }
            } else if (!app.delayedCMD(cmd, 1500)) {
                error = true;
            }

        } else {
            error = true;
        }

    }
    if (!error) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        sendApiCode(response, API_CODES::API_MISSING_PARAM);
    }

}

void ApplicationWebserver::onUpdate(HttpRequest &request, HttpResponse &response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_HOST
    sendApiCode(response, API_CODES::API_BAD_REQUEST, "not supported on Host");
    return;
#else
    if (request.method != HTTP_POST && request.method != HTTP_GET) {
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
        DynamicJsonDocument doc(1024);
        Json::deserialize(doc, body);

        String romurl, spiffsurl;
        if (!Json::getValue(doc["rom"]["url"], romurl) || !Json::getValue(doc["spiffs"]["url"], spiffsurl)) {
            sendApiCode(response, API_CODES::API_MISSING_PARAM);
        } else {
            app.ota.start(romurl, spiffsurl);
            sendApiCode(response, API_CODES::API_SUCCESS);
        }
        return;
    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    json["status"] = int(app.ota.getStatus());
    sendApiResponse(response, stream);
#endif
}

//simple call-response to check if we can reach server
void ApplicationWebserver::onPing(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    json["ping"] = "pong";
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
        String fileName=doc["filename"];
        
        //DynamicJsonDocument data(body.length()+32);
        //Json::deserialize(data, Json::serialize(doc["data"]));
        //doc.clear(); //clearing the original document to save RAM
        debug_i("will save to file %s", fileName.c_str());
        debug_i("original document uses %i bytes", doc.memoryUsage());
        String data=doc["data"];
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