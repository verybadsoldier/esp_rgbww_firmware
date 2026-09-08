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
#include <Network/WebHelpers/base64.h>
#include <RGBWWCtrl.h>

ApplicationWebserver::ApplicationWebserver() {
    _running = false;

    // keep some heap space free
    // value is a good guess and tested to not crash when issuing multiple parallel requests
    HttpServerSettings settings;
    settings.minHeapSize = _minimumHeapAccept;
    settings.keepAliveSeconds =
        5; // do not close instantly when no transmission occurs. some clients are a bit slow (like FHEM)
    configure(settings);

    // workaround for bug in Sming 3.5.0
    // https://github.com/SmingHub/Sming/issues/1236
    setBodyParser(F("*"), bodyToStringParser);
}

void ApplicationWebserver::init() {
    paths.setDefault(HttpPathDelegate(&ApplicationWebserver::onFile, this));
    paths.set(F("/"), HttpPathDelegate(&ApplicationWebserver::onIndex, this));
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

    // animation controls
    paths.set(F("/stop"), HttpPathDelegate(&ApplicationWebserver::onStop, this));
    paths.set(F("/skip"), HttpPathDelegate(&ApplicationWebserver::onSkip, this));
    paths.set(F("/pause"), HttpPathDelegate(&ApplicationWebserver::onPause, this));
    paths.set(F("/continue"), HttpPathDelegate(&ApplicationWebserver::onContinue, this));
    paths.set(F("/blink"), HttpPathDelegate(&ApplicationWebserver::onBlink, this));

    paths.set(F("/toggle"), HttpPathDelegate(&ApplicationWebserver::onToggle, this));
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

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticateExec(HttpRequest& request, HttpResponse& response) {
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
    userPass = userPass.substring(6); // cut "Basic " from start
    if (userPass.length() > 50) {
        return false;
    }

    userPass = base64_decode(userPass);
    debug_d("ApplicationWebserver::authenticated Password: '%s' - Expected password: '%s'", userPass.c_str(),
            app.cfg.general.api_password.c_str());
    if (userPass.endsWith(app.cfg.general.api_password)) {
        return true;
    }

    return false;
}

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticated(HttpRequest& request, HttpResponse& response) {
    bool authenticated = authenticateExec(request, response);

    if (!authenticated) {
        response.code = 401;
        response.setHeader(F("WWW-Authenticate"), F("Basic realm=\"RGBWW Server\""));
        response.setHeader(F("401 wrong credentials"), F("wrong credentials"));
        response.setHeader(F("Connection"), F("close"));
    }

    return authenticated;
}

String ApplicationWebserver::getApiCodeMsg(API_CODES code) {
    switch (code) {
    case API_CODES::API_MISSING_PARAM:
        return F("missing param");
    case API_CODES::API_UNAUTHORIZED:
        return F("authorization required");
    case API_CODES::API_UPDATE_IN_PROGRESS:
        return F("update in progress");
    default:
        return F("bad request");
    }
}

void ApplicationWebserver::sendApiResponse(HttpResponse& response, JsonObjectStream* stream, int code /* = 200 */) {
    if (!checkHeap(response)) {
        delete stream;
        return;
    }

    response.setAllowCrossDomainOrigin("*");
    if (code != 200) {
        response.code = 400;
    }
    response.sendDataStream(stream, MIME_JSON);
}

void ApplicationWebserver::sendApiCode(HttpResponse& response, API_CODES code, String msg /* = "" */) {
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    if (msg == "") {
        msg = getApiCodeMsg(code);
    }
    if (code == API_CODES::API_SUCCESS) {
        json[F("success")] = true;
        sendApiResponse(response, stream, 200);
    } else {
        json[F("error")] = msg;
        sendApiResponse(response, stream, 400);
    }
}

void ApplicationWebserver::onFile(HttpRequest& request, HttpResponse& response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        response.setContentType("text/plain");
        response.code = 503;
        response.sendString(F("OTA in progress"));
        return;
    }
#endif

    if (!app.isFilesystemMounted()) {
        response.setContentType(F("text/plain"));
        response.code = 500;
        response.sendString(F("No filesystem mounted"));
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
        // if accesspoint is active and we couldn`t find the file - redirect to index
        debug_d("ApplicationWebserver::onFile redirecting");
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
    } else {
        response.setCache(86400, true); // It's important to use cache for better performance.
        response.sendFile(file);
    }
}

void ApplicationWebserver::onIndex(HttpRequest& request, HttpResponse& response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        response.setContentType(F("text/plain"));
        response.code = 503;
        response.sendString(F("OTA in progress"));
        return;
    }
#endif

    if (WifiAccessPoint.isEnabled()) {
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
    } else {
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiStation.getIP().toString() + "/webapp";
    }
    response.code = 308;
}

void ApplicationWebserver::onWebapp(HttpRequest& request, HttpResponse& response) {

    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_ESP8266
    if (app.ota.isProccessing()) {
        response.setContentType("text/plain");
        response.code = 503;
        response.sendString(F("OTA in progress"));
        return;
    }
#endif

    if (request.method != HTTP_GET) {
        response.code = 400;
        return;
    }

    if (!app.isFilesystemMounted()) {
        response.setContentType(F("text/plain"));
        response.code = 500;
        response.sendString(F("No filesystem mounted"));
        return;
    }
    if (!WifiStation.isConnected()) {
        // not yet connected - serve initial settings page
        response.sendFile(F("init.html"));
    } else {
        // we are connected to ap - serve normal settings page
        response.sendFile(F("index.html"));
    }
}

bool ApplicationWebserver::checkHeap(HttpResponse& response) {
    unsigned fh = system_get_free_heap_size();
    if (fh < _minimumHeap) {
        response.code = 429;
        response.setHeader(F("Retry-After"), F("2"));
        return false;
    }
    return true;
}

void ApplicationWebserver::onConfig(HttpRequest& request, HttpResponse& response) {
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

    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not POST or GET request"));
        return;
    }

    if (request.method == HTTP_POST) {
        String body = request.getBody();
        if (body == NULL) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse HTTP body"));
            return;
        }

        bool error = false;
        String error_msg = getApiCodeMsg(API_CODES::API_BAD_REQUEST);
        DynamicJsonDocument doc(CONFIG_MAX_LENGTH);
        if (!Json::deserialize(doc, body)) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("JSON deserialization error"));
            return;
        }

        // remove comment for debugging
        // Json::serialize(doc, Serial, Json::Pretty);

        bool ip_updated = false;
        bool color_updated = false;
        bool ap_updated = false;
        JsonObject root = doc.as<JsonObject>();
        if (root.isNull()) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("no root object"));
            return;
        }

        JsonObject jnet = root[F("network")];
        if (!jnet.isNull()) {

            JsonObject con = jnet[F("connection")];
            if (!con.isNull()) {
                ip_updated |= Json::getBoolTolerantChanged(con[F("dhcp")], app.cfg.network.connection.dhcp);

                if (!app.cfg.network.connection.dhcp) {
                    // only change if dhcp is off - otherwise ignore
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
                        error_msg = "missing netmask";
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
                            error_msg = F("missing password for securing ap");
                        }
                    } else if (secured != app.cfg.network.ap.secured) {
                        app.cfg.network.ap.secured = secured;
                        ap_updated = true;
                    }
                }

                Json::getValue(jnet[F("ap")][F("fallback_delay")], app.cfg.network.ap.fallback_delay);
            }

            JsonObject jmqtt = jnet[F("mqtt")];
            if (!jmqtt.isNull()) {
                // TODO: what to do if changed?
                Json::getBoolTolerant(jmqtt[F("enabled")], app.cfg.network.mqtt.enabled);
                Json::getValue(jmqtt[F("server")], app.cfg.network.mqtt.server);
                Json::getValue(jmqtt[F("port")], app.cfg.network.mqtt.port);
                Json::getValue(jmqtt[F("username")], app.cfg.network.mqtt.username);
                Json::getValue(jmqtt[F("password")], app.cfg.network.mqtt.password);
                Json::getValue(jmqtt[F("topic_base")], app.cfg.network.mqtt.topic_base);
                Json::getBoolTolerant(jmqtt[F("homeassistant_discovery_enabled")],
                                      app.cfg.network.mqtt.homeassistant_discovery_enabled);
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
            Json::getValue(jgen[F("device_name")], app.cfg.general.device_name);
            Json::getValue(jgen[F("pin_config")], app.cfg.general.pin_config);
            Json::getValue(jgen[F("buttons_config")], app.cfg.general.buttons_config);
            Json::getValue(jgen[F("buttons_debounce_ms")], app.cfg.general.buttons_debounce_ms);
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
                                                        // json["data"] = "restart";
                }
            }
            if (ap_updated) {
                if (restart && WifiAccessPoint.isEnabled()) {
                    debug_i("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
                    app.delayedCMD(F("restart"), 3000); // wait 3s to first send response
                                                        // json["data"] = "restart";
                }
            }
            if (color_updated) {
                debug_d("ApplicationWebserver::onConfig color settings changed - refreshing");

                // refresh settings
                app.rgbwwctrl.setup();

                // refresh current output
                app.rgbwwctrl.refresh();
            }

            app.cfg.getConfig(root);
            app.mqttclient.publishConfigEvent(root);
            app.eventserver.publishConfigEvent(root);

            app.cfg.save();

            sendApiCode(response, API_CODES::API_SUCCESS);
        } else {
            sendApiCode(response, API_CODES::API_MISSING_PARAM, error_msg);
        }
    } else {
        JsonObjectStream* stream = new JsonObjectStream(CONFIG_MAX_LENGTH);
        JsonObject json = stream->getRoot();

        app.cfg.getConfig(json);

        sendApiResponse(response, stream);
    }
}

void ApplicationWebserver::onInfo(HttpRequest& request, HttpResponse& response) {
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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not GET"));
        return;
    }

    sendApiResponse(response, app.getInfo());
}

void ApplicationWebserver::onColorGet(HttpRequest& request, HttpResponse& response) {
    if (!checkHeap(response))
        return;

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();

    JsonObject raw = json.createNestedObject(F("raw"));
    ChannelOutput output = app.rgbwwctrl.getCurrentOutput();
    raw[F("r")] = output.r;
    raw[F("g")] = output.g;
    raw[F("b")] = output.b;
    raw[F("ww")] = output.ww;
    raw[F("cw")] = output.cw;

    JsonObject hsv = json.createNestedObject(F("hsv"));
    float h, s, v;
    int ct;
    HSVCT c = app.rgbwwctrl.getCurrentColor();
    c.asRadian(h, s, v, ct);
    hsv[F("h")] = h;
    hsv[F("s")] = s;
    hsv[F("v")] = v;
    hsv[F("ct")] = ct;

    sendApiResponse(response, stream);
}

void ApplicationWebserver::onColorPost(HttpRequest& request, HttpResponse& response) {
    String body = request.getBody();
    if (body == NULL) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("no body"));
        return;
    }

    String erroMsg;
    if (!app.jsonproc.onColor(body, erroMsg)) {
        debug_w("ApplicationWebserver::onColorPost error processing json: %s", erroMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, erroMsg);
    } else {

        sendApiCode(response, API_CODES::API_SUCCESS);
    }
}

void ApplicationWebserver::onColor(HttpRequest& request, HttpResponse& response) {
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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not POST or GET"));
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
    for (unsigned int i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c < 0x20)
            return false;
    }
    return true;
}

void ApplicationWebserver::onNetworks(HttpRequest& request, HttpResponse& response) {

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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP GET"));
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream(3000);
    JsonObject json = stream->getRoot();

    bool error = false;

    if (app.network.isScanning()) {
        json["scanning"] = true;
    } else {
        json["scanning"] = false;
        JsonArray netlist = json.createNestedArray(F("available"));
        BssList networks = app.network.getAvailableNetworks();
        for (int i = 0; i < networks.count(); i++) {
            if (networks[i].hidden)
                continue;

            // SSIDs may contain any byte values. Some are not printable and will cause the javascript client to fail
            // on parsing the message. Try to filter those here
            if (!ApplicationWebserver::isPrintable(networks[i].ssid)) {
                debug_w("Filtered SSID due to unprintable characters: %s", networks[i].ssid.c_str());
                continue;
            }

            JsonObject item = netlist.createNestedObject();
            item[F("id")] = (int)networks[i].getHashId();
            item[F("ssid")] = networks[i].ssid;
            item[F("signal")] = networks[i].rssi;
            item[F("encryption")] = networks[i].getAuthorizationMethodName();
            // limit to max 25 networks
            if (i >= 25)
                break;
        }
    }
    sendApiResponse(response, stream);
}

void ApplicationWebserver::onScanNetworks(HttpRequest& request, HttpResponse& response) {

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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }
    if (!app.network.isScanning()) {
        app.network.scan(false);
    }

    sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onConnect(HttpRequest& request, HttpResponse& response) {

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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST or GET"));
        return;
    }

    if (request.method == HTTP_POST) {

        String body = request.getBody();
        if (body == NULL) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not get HTTP body"));
            return;
        }
        DynamicJsonDocument doc(_apiJsonBufferSize);
        if (!Json::deserialize(doc, body)) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("JSON deserialization error"));
            return;
        }
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

void ApplicationWebserver::onSystemReq(HttpRequest& request, HttpResponse& response) {

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
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    bool error = false;
    String body = request.getBody();
    if (body == NULL) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not get HTTP body"));
        return;
    } else {
        debug_i("ApplicationWebserver::onSystemReq: %s", body.c_str());
        DynamicJsonDocument doc(_apiJsonBufferSize);
        if (!Json::deserialize(doc, body)) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("JSON deserialization error"));
            return;
        }

        String cmd = doc[F("cmd")].as<const char*>();
        if (cmd) {
            if (cmd.equals(F("debug"))) {
                bool enable;
                if (Json::getValue(doc[F("enable")], enable)) {
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

void ApplicationWebserver::onUpdate(HttpRequest& request, HttpResponse& response) {
    if (!authenticated(request, response)) {
        return;
    }

#ifdef ARCH_HOST
    sendApiCode(response, API_CODES::API_BAD_REQUEST, "not supported on Host");
    return;
#else
    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST or GET"));
        return;
    }

    if (request.method == HTTP_POST) {
        if (app.ota.isProccessing()) {
            sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
            return;
        }

        String body = request.getBody();
        if (body == NULL) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("could not parse HTTP body"));
            return;
        }
        DynamicJsonDocument doc(_apiJsonBufferSize);
        if (!Json::deserialize(doc, body)) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, F("JSON deserialization error"));
            return;
        }

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

// simple call-response to check if we can reach server
void ApplicationWebserver::onPing(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP GET"));
        return;
    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject json = stream->getRoot();
    json[F("ping")] = "pong";
    sendApiResponse(response, stream);
}

void ApplicationWebserver::onStop(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onStop(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onStop error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}

void ApplicationWebserver::onSkip(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onSkip(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onSkip error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}

void ApplicationWebserver::onPause(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onPause(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onPause error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}

void ApplicationWebserver::onContinue(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onContinue(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onContinue error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}

void ApplicationWebserver::onBlink(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onBlink(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onBlink error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}

void ApplicationWebserver::onToggle(HttpRequest& request, HttpResponse& response) {
    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, F("not HTTP POST"));
        return;
    }

    String errorMsg;
    if (app.jsonproc.onToggle(request.getBody(), errorMsg)) {
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        debug_w("ApplicationWebserver::onToggle error processing json: %s", errorMsg.c_str());
        sendApiCode(response, API_CODES::API_BAD_REQUEST, errorMsg);
    }
}
