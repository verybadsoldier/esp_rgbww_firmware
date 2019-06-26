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
#include <Services/WebHelpers/base64.h>

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
    paths.set("/animation", HttpPathDelegate(&ApplicationWebserver::onAnimation, this));
    paths.set("/networks", HttpPathDelegate(&ApplicationWebserver::onNetworks, this));
    paths.set("/scan_networks", HttpPathDelegate(&ApplicationWebserver::onScanNetworks, this));
    paths.set("/system", HttpPathDelegate(&ApplicationWebserver::onSystemReq, this));
    paths.set("/update", HttpPathDelegate(&ApplicationWebserver::onUpdate, this));
    paths.set("/connect", HttpPathDelegate(&ApplicationWebserver::onConnect, this));
    paths.set("/generate_204", HttpPathDelegate(&ApplicationWebserver::generate204, this));
    paths.set("/ping", HttpPathDelegate(&ApplicationWebserver::onPing, this));
    paths.set("/stop", HttpPathDelegate(&ApplicationWebserver::onStop, this));
    paths.set("/skip", HttpPathDelegate(&ApplicationWebserver::onSkip, this));
    paths.set("/pause", HttpPathDelegate(&ApplicationWebserver::onPause, this));
    paths.set("/continue", HttpPathDelegate(&ApplicationWebserver::onContinue, this));
    paths.set("/blink", HttpPathDelegate(&ApplicationWebserver::onBlink, this));
    paths.set("/toggle", HttpPathDelegate(&ApplicationWebserver::onToggle, this));
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

    // workaround for this: https://github.com/SmingHub/Sming/issues/1725
    while(userPass.endsWith("="))
        userPass = userPass.substring(0, userPass.length() - 2);

    // test code for the issue. Should not spawn 2 extra bytes at the end
    String s = base64_decode("YWRtaW46dGVzdA==");
    m_printHex("base64_decode", s.c_str(), s.length());

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
        response.code = 401;
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

void ApplicationWebserver::sendApiResponse(HttpResponse &response, JsonObjectStream* stream, int code /* = 200 */) {
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

void ApplicationWebserver::sendApiCode(HttpResponse &response, API_CODES code, String msg /* = "" */) {
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject& json = stream->getRoot();
    if (msg == "") {
        msg = getApiCodeMsg(code);
    }
    if (code == API_CODES::API_SUCCESS) {
        json["success"] = true;
        sendApiResponse(response, stream, 200);
    } else {
        json["error"] = msg;
        sendApiResponse(response, stream, 400);
    }
}

void ApplicationWebserver::onFile(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        response.setContentType("text/plain");
        response.code = 503;
        response.sendString("OTA in progress");
        return;
    }

    if (!app.isFilesystemMounted()) {
        response.setContentType("text/plain");
        response.code = 500;
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

}

void ApplicationWebserver::onIndex(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        response.setContentType("text/plain");
        response.code = 503;
        response.sendString("OTA in progress");
        return;
    }

    if (WifiAccessPoint.isEnabled()) {
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiAccessPoint.getIP().toString() + "/webapp";
    } else {
        response.headers[HTTP_HEADER_LOCATION] = "http://" + WifiStation.getIP().toString() + "/webapp";
    }
    response.code = 308;
}

void ApplicationWebserver::onWebapp(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        response.setContentType("text/plain");
        response.code = 503;
        response.sendString("OTA in progress");
        return;
    }

    if (request.method != HTTP_GET) {
        response.code = 400;
        return;
    }

    if (!app.isFilesystemMounted()) {
        response.setContentType("text/plain");
        response.code = 500;
        response.sendString("No filesystem mounted");
        return;
    }
    if (!WifiStation.isConnected()) {
        // not yet connected - serve initial settings page
        response.sendFile("init.html");
    } else {
        // we are connected to ap - serve normal settings page
        response.sendFile("index.html");
    }
}

bool ApplicationWebserver::checkHeap(HttpResponse &response) {
    uint fh = system_get_free_heap_size();
    if (fh < _minimumHeap) {
        response.code = 429;
        response.setHeader("Retry-After", "2");
        return false;
    }
    return true;
}

void ApplicationWebserver::onConfig(HttpRequest &request, HttpResponse &response) {
    if (!checkHeap(response))
        return;

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST or GET request");
        return;
    }

    if (request.method == HTTP_POST) {
        String body = request.getBody();
        if (body == NULL) {

            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not parse HTTP body");
            return;

        }

        bool error = false;
        String error_msg = getApiCodeMsg(API_CODES::API_BAD_REQUEST);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);

        // remove comment for debugging
        //root.prettyPrintTo(Serial);

        bool ip_updated = false;
        bool color_updated = false;
        bool ap_updated = false;
        if (!root.success()) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, "no root object");
            return;
        }
        if (root["network"].success()) {

            if (root["network"]["connection"].success()) {

                if (root["network"]["connection"]["dhcp"].success()) {

                    if (root["network"]["connection"]["dhcp"] != app.cfg.network.connection.dhcp) {
                        app.cfg.network.connection.dhcp = root["network"]["connection"]["dhcp"];
                        ip_updated = true;
                    }
                }
                if (!app.cfg.network.connection.dhcp) {
                    //only change if dhcp is off - otherwise ignore
                    IPAddress ip, netmask, gateway;
                    if (root["network"]["connection"]["ip"].success()) {
                        ip = root["network"]["connection"]["ip"].asString();
                        if (!(ip == app.cfg.network.connection.ip)) {
                            app.cfg.network.connection.ip = ip;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing ip";
                    }
                    if (root["network"]["connection"]["netmask"].success()) {
                        netmask = root["network"]["connection"]["netmask"].asString();
                        if (!(netmask == app.cfg.network.connection.netmask)) {
                            app.cfg.network.connection.netmask = netmask;
                            ip_updated = true;
                        }
                    } else {
                        error = true;
                        error_msg = "missing netmask";
                    }
                    if (root["network"]["connection"]["gateway"].success()) {
                        gateway = root["network"]["connection"]["gateway"].asString();
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
            if (root["network"]["ap"].success()) {

                if (root["network"]["ap"]["ssid"].success()) {
                    if (root["network"]["ap"]["ssid"] != app.cfg.network.ap.ssid) {
                        app.cfg.network.ap.ssid = root["network"]["ap"]["ssid"].asString();
                        ap_updated = true;
                    }
                }
                if (root["network"]["ap"]["secured"].success()) {
                    if (root["network"]["ap"]["secured"].as<bool>()) {
                        if (root["network"]["ap"]["password"].success()) {
                            if (root["network"]["ap"]["password"] != app.cfg.network.ap.password) {
                                app.cfg.network.ap.secured = root["network"]["ap"]["secured"];
                                app.cfg.network.ap.password = root["network"]["ap"]["password"].asString();
                                ap_updated = true;
                            }
                        } else {
                            error = true;
                            error_msg = "missing password for securing ap";
                        }
                    } else if (root["network"]["ap"]["secured"] != app.cfg.network.ap.secured) {
                        app.cfg.network.ap.secured = root["network"]["ap"]["secured"];
                        ap_updated = true;
                    }
                }

            }
            if (root["network"]["mqtt"].success()) {
                //TODO: what to do if changed?
                if (root["network"]["mqtt"]["enabled"].success()) {
                    if (root["network"]["mqtt"]["enabled"] != app.cfg.network.mqtt.enabled) {
                        app.cfg.network.mqtt.enabled = root["network"]["mqtt"]["enabled"];
                    }
                }
                if (root["network"]["mqtt"]["server"].success()) {
                    if (root["network"]["mqtt"]["server"] != app.cfg.network.mqtt.server) {
                        app.cfg.network.mqtt.server = root["network"]["mqtt"]["server"].asString();
                    }
                }
                if (root["network"]["mqtt"]["port"].success()) {
                    if (root["network"]["mqtt"]["port"] != app.cfg.network.mqtt.port) {
                        app.cfg.network.mqtt.port = root["network"]["mqtt"]["port"];
                    }
                }
                if (root["network"]["mqtt"]["username"].success()) {
                    if (root["network"]["mqtt"]["username"] != app.cfg.network.mqtt.username) {
                        app.cfg.network.mqtt.username = root["network"]["mqtt"]["username"].asString();
                    }
                }
                if (root["network"]["mqtt"]["password"].success()) {
                    if (root["network"]["mqtt"]["password"] != app.cfg.network.mqtt.password) {
                        app.cfg.network.mqtt.password = root["network"]["mqtt"]["password"].asString();
                    }
                }
                if (root["network"]["mqtt"]["topic_base"].success()) {
                    if (root["network"]["mqtt"]["topic_base"] != app.cfg.network.mqtt.topic_base) {
                        app.cfg.network.mqtt.topic_base = root["network"]["mqtt"]["topic_base"].asString();
                    }
                }
            }
        }

        if (root["color"].success()) {

            if (root["color"]["hsv"].success()) {
                if (root["color"]["hsv"]["model"].success()) {
                    if (root["color"]["hsv"]["model"] != app.cfg.color.hsv.model) {
                        app.cfg.color.hsv.model = root["color"]["hsv"]["model"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["red"].success()) {
                    if (root["color"]["hsv"]["red"].as<float>() != app.cfg.color.hsv.red) {
                        app.cfg.color.hsv.red = root["color"]["hsv"]["red"].as<float>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["yellow"].success()) {
                    if (root["color"]["hsv"]["yellow"].as<float>() != app.cfg.color.hsv.yellow) {
                        app.cfg.color.hsv.yellow = root["color"]["hsv"]["yellow"].as<float>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["green"].success()) {
                    if (root["color"]["hsv"]["green"].as<float>() != app.cfg.color.hsv.green) {
                        app.cfg.color.hsv.green = root["color"]["hsv"]["green"].as<float>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["cyan"].success()) {
                    if (root["color"]["hsv"]["cyan"].as<float>() != app.cfg.color.hsv.cyan) {
                        app.cfg.color.hsv.cyan = root["color"]["hsv"]["cyan"].as<float>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["blue"].success()) {
                    if (root["color"]["hsv"]["blue"].as<float>() != app.cfg.color.hsv.blue) {
                        app.cfg.color.hsv.blue = root["color"]["hsv"]["blue"].as<float>();
                        color_updated = true;
                    }
                }
                if (root["color"]["hsv"]["magenta"].success()) {
                    if (root["color"]["hsv"]["magenta"].as<float>() != app.cfg.color.hsv.magenta) {
                        app.cfg.color.hsv.magenta = root["color"]["hsv"]["magenta"].as<float>();
                        color_updated = true;
                    }
                }
            }
            if (root["color"]["outputmode"].success()) {
                if (root["color"]["outputmode"] != app.cfg.color.outputmode) {
                    app.cfg.color.outputmode = root["color"]["outputmode"].as<int>();
                    color_updated = true;
                }
            }
            if (root["color"]["startup_color"].success()) {
                app.cfg.color.startup_color = root["color"]["startup_color"].asString();
            }
            if (root["color"]["brightness"].success()) {

                if (root["color"]["brightness"]["red"].success()) {
                    if (root["color"]["brightness"]["red"].as<int>() != app.cfg.color.brightness.red) {
                        app.cfg.color.brightness.red = root["color"]["brightness"]["red"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["brightness"]["green"].success()) {
                    if (root["color"]["brightness"]["green"].as<int>() != app.cfg.color.brightness.green) {
                        app.cfg.color.brightness.green = root["color"]["brightness"]["green"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["brightness"]["blue"].success()) {
                    if (root["color"]["brightness"]["blue"].as<int>() != app.cfg.color.brightness.blue) {
                        app.cfg.color.brightness.blue = root["color"]["brightness"]["blue"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["brightness"]["ww"].success()) {
                    if (root["color"]["brightness"]["ww"].as<int>() != app.cfg.color.brightness.ww) {
                        app.cfg.color.brightness.ww = root["color"]["brightness"]["ww"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["brightness"]["cw"].success()) {
                    if (root["color"]["brightness"]["cw"].as<int>() != app.cfg.color.brightness.cw) {
                        app.cfg.color.brightness.cw = root["color"]["brightness"]["cw"].as<int>();
                        color_updated = true;
                    }
                }
            }
            if (root["color"]["colortemp"].success()) {

                if (root["color"]["colortemp"]["ww"].success()) {
                    if (root["color"]["colortemp"]["cw"].as<int>() != app.cfg.color.colortemp.ww) {
                        app.cfg.color.colortemp.ww = root["color"]["colortemp"]["ww"].as<int>();
                        color_updated = true;
                    }
                }
                if (root["color"]["colortemp"]["cw"].success()) {
                    if (root["color"]["colortemp"]["cw"].as<int>() != app.cfg.color.colortemp.cw) {
                        app.cfg.color.colortemp.cw = root["color"]["colortemp"]["cw"].as<int>();
                        color_updated = true;
                    }
                }
            }
        }

        if (root["security"].success()) {
            if (root["security"]["api_secured"].success()) {
                if (root["security"]["api_secured"].as<bool>()) {
                    if (root["security"]["api_password"].success()) {
                        if (root["security"]["api_password"] != app.cfg.general.api_password) {
                            app.cfg.general.api_secured = root["security"]["api_secured"];
                            app.cfg.general.api_password = root["security"]["api_password"].asString();
                        }

                    } else {
                        error = true;
                        error_msg = "missing password to secure settings";
                    }
                } else {
                    app.cfg.general.api_secured = root["security"]["api_secured"];
                    app.cfg.general.api_password = "";
                }

            }
        }

        if (root["ota"].success()) {
            if (root["ota"]["url"].success()) {
                app.cfg.general.otaurl = root["ota"]["url"].asString();
            }

        }

        if (root["general"].success()) {
            if (root["general"]["device_name"].success()) {
                app.cfg.general.device_name = root["general"]["device_name"].asString();
            }
            if (root["general"]["pin_config"].success()) {
                app.cfg.general.pin_config = root["general"]["pin_config"].asString();
            }
            if (root["general"]["buttons_config"].success()) {
                app.cfg.general.buttons_config = root["general"]["buttons_config"].asString();
            }
            if (root["general"]["buttons_debounce_ms"].success()) {
                app.cfg.general.buttons_debounce_ms = root["general"]["buttons_debounce_ms"];
            }
        }

        if (root["sync"].success()) {
            if (root["sync"]["clock_master_enabled"].success()) {
                app.cfg.sync.clock_master_enabled = root["sync"]["clock_master_enabled"];
            }
            if (root["sync"]["clock_master_interval"].success()) {
                app.cfg.sync.clock_master_interval = root["sync"]["clock_master_interval"];
            }
            if (root["sync"]["clock_slave_enabled"].success()) {
                app.cfg.sync.clock_slave_enabled = root["sync"]["clock_slave_enabled"];
            }
            if (root["sync"]["clock_slave_topic"].success()) {
                app.cfg.sync.clock_slave_topic = root["sync"]["clock_slave_topic"].asString();
            }
            if (root["sync"]["cmd_master_enabled"].success()) {
                app.cfg.sync.cmd_master_enabled = root["sync"]["cmd_master_enabled"];
            }
            if (root["sync"]["cmd_slave_enabled"].success()) {
                app.cfg.sync.cmd_slave_enabled = root["sync"]["cmd_slave_enabled"];
            }
            if (root["sync"]["cmd_slave_topic"].success()) {
                app.cfg.sync.cmd_slave_topic = root["sync"]["cmd_slave_topic"].asString();
            }

            if (root["sync"]["color_master_enabled"].success()) {
                app.cfg.sync.color_master_enabled = root["sync"]["color_master_enabled"];
            }
            if (root["sync"]["color_master_interval_ms"].success()) {
                app.cfg.sync.color_master_interval_ms = root["sync"]["color_master_interval_ms"];
            }
            if (root["sync"]["color_slave_enabled"].success()) {
                app.cfg.sync.color_slave_enabled = root["sync"]["color_slave_enabled"];
            }
            if (root["sync"]["color_slave_topic"].success()) {
                app.cfg.sync.color_slave_topic = root["sync"]["color_slave_topic"].asString();
            }
        }

        if (root["events"].success()) {
            if (root["events"]["color_interval_ms"].success()) {
                app.cfg.events.color_interval_ms = root["events"]["color_interval_ms"];
            }
            if (root["events"]["color_mininterval_ms"].success()) {
                app.cfg.events.color_mininterval_ms = root["events"]["color_mininterval_ms"];
            }
            if (root["events"]["server_enabled"].success()) {
                app.cfg.events.server_enabled = root["events"]["server_enabled"];
            }
            if (root["events"]["transfin_interval_ms"].success()) {
                app.cfg.events.transfin_interval_ms = root["events"]["transfin_interval_ms"];
            }
        }

        app.cfg.sanitizeValues();

        // update and save settings if we haven`t received any error until now
        if (!error) {
            if (ip_updated) {
                if (root["restart"].success()) {
                    if (root["restart"] == true) {
                        debug_i("ApplicationWebserver::onConfig ip settings changed - rebooting");
                        app.delayedCMD("restart", 3000); // wait 3s to first send response
                        //json["data"] = "restart";

                    }
                }
            };
            if (ap_updated) {
                if (root["restart"].success()) {
                    if (root["restart"] == true && WifiAccessPoint.isEnabled()) {
                        debug_i("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
                        app.delayedCMD("restart", 3000); // wait 3s to first send response
                        //json["data"] = "restart";

                    }
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
        JsonObjectStream* stream = new JsonObjectStream();
        JsonObject& json = stream->getRoot();
        // returning settings
        JsonObject& net = json.createNestedObject("network");
        JsonObject& con = net.createNestedObject("connection");
        con["dhcp"] = WifiStation.isEnabledDHCP();

        //con["ip"] = WifiStation.getIP().toString();
        //con["netmask"] = WifiStation.getNetworkMask().toString();
        //con["gateway"] = WifiStation.getNetworkGateway().toString();

        con["ip"] = app.cfg.network.connection.ip.toString();
        con["netmask"] = app.cfg.network.connection.netmask.toString();
        con["gateway"] = app.cfg.network.connection.gateway.toString();

        JsonObject& ap = net.createNestedObject("ap");
        ap["secured"] = app.cfg.network.ap.secured;
        ap["password"] = app.cfg.network.ap.password.c_str();
        ap["ssid"] = app.cfg.network.ap.ssid.c_str();

        JsonObject& mqtt = net.createNestedObject("mqtt");
        mqtt["enabled"] = app.cfg.network.mqtt.enabled;
        mqtt["server"] = app.cfg.network.mqtt.server.c_str();
        mqtt["port"] = app.cfg.network.mqtt.port;
        mqtt["username"] = app.cfg.network.mqtt.username.c_str();
        mqtt["password"] = app.cfg.network.mqtt.password.c_str();
        mqtt["topic_base"] = app.cfg.network.mqtt.topic_base.c_str();

        JsonObject& color = json.createNestedObject("color");
        color["outputmode"] = app.cfg.color.outputmode;
        color["startup_color"] = app.cfg.color.startup_color;

        JsonObject& hsv = color.createNestedObject("hsv");
        hsv["model"] = app.cfg.color.hsv.model;

        hsv["red"] = app.cfg.color.hsv.red;
        hsv["yellow"] = app.cfg.color.hsv.yellow;
        hsv["green"] = app.cfg.color.hsv.green;
        hsv["cyan"] = app.cfg.color.hsv.cyan;
        hsv["blue"] = app.cfg.color.hsv.blue;
        hsv["magenta"] = app.cfg.color.hsv.magenta;

        JsonObject& brighntess = color.createNestedObject("brightness");
        brighntess["red"] = app.cfg.color.brightness.red;
        brighntess["green"] = app.cfg.color.brightness.green;
        brighntess["blue"] = app.cfg.color.brightness.blue;
        brighntess["ww"] = app.cfg.color.brightness.ww;
        brighntess["cw"] = app.cfg.color.brightness.cw;

        JsonObject& ctmp = color.createNestedObject("colortemp");
        ctmp["ww"] = app.cfg.color.colortemp.ww;
        ctmp["cw"] = app.cfg.color.colortemp.cw;

        JsonObject& s = json.createNestedObject("security");
        s["api_secured"] = app.cfg.general.api_secured;

        JsonObject& ota = json.createNestedObject("ota");
        ota["url"] = app.cfg.general.otaurl;

        JsonObject& sync = json.createNestedObject("sync");
        sync["clock_master_enabled"] = app.cfg.sync.clock_master_enabled;
        sync["clock_master_interval"] = app.cfg.sync.clock_master_interval;
        sync["clock_slave_enabled"] = app.cfg.sync.clock_slave_enabled;
        sync["clock_slave_topic"] = app.cfg.sync.clock_slave_topic.c_str();
        sync["cmd_master_enabled"] = app.cfg.sync.cmd_master_enabled;
        sync["cmd_slave_enabled"] = app.cfg.sync.cmd_slave_enabled;
        sync["cmd_slave_topic"] = app.cfg.sync.cmd_slave_topic.c_str();

        sync["color_master_enabled"] = app.cfg.sync.color_master_enabled;
        sync["color_master_interval_ms"] = app.cfg.sync.color_master_interval_ms;
        sync["color_slave_enabled"] = app.cfg.sync.color_slave_enabled;
        sync["color_slave_topic"] = app.cfg.sync.color_slave_topic.c_str();

        JsonObject& events = json.createNestedObject("events");
        events["color_interval_ms"] = app.cfg.events.color_interval_ms;
        events["color_mininterval_ms"] = app.cfg.events.color_mininterval_ms;
        events["server_enabled"] = app.cfg.events.server_enabled;
        events["transfin_interval_ms"] = app.cfg.events.transfin_interval_ms;

        JsonObject& general = json.createNestedObject("general");
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

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not GET");
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject& data = stream->getRoot();
    data["deviceid"] = String(system_get_chip_id());
    data["current_rom"] = String(app.getRomSlot());
    data["git_version"] = fw_git_version;
    data["git_date"] = fw_git_date;
    data["webapp_version"] = WEBAPP_VERSION;
    data["sming"] = SMING_VERSION;
    data["event_num_clients"] = app.eventserver.activeClients;
    data["uptime"] = app.getUptime();
    data["heap_free"] = system_get_free_heap_size();

    JsonObject& rgbww = data.createNestedObject("rgbww");
    rgbww["version"] = RGBWW_VERSION;
    rgbww["queuesize"] = RGBWW_ANIMATIONQSIZE;

    JsonObject& con = data.createNestedObject("connection");
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
    JsonObject& json = stream->getRoot();

    JsonObject& raw = json.createNestedObject("raw");
    ChannelOutput output = app.rgbwwctrl.getCurrentOutput();
    raw["r"] = output.r;
    raw["g"] = output.g;
    raw["b"] = output.b;
    raw["ww"] = output.ww;
    raw["cw"] = output.cw;

    JsonObject& hsv = json.createNestedObject("hsv");
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

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST or GET");
        return;
    }

    bool error = false;
    if (request.method == HTTP_POST) {
        ApplicationWebserver::onColorPost(request, response);
    } else {
        ApplicationWebserver::onColorGet(request, response);
    }

}

void ApplicationWebserver::onAnimation(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_POST && request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not POST or GET");
        return;
    }

    bool error = false;
    if (request.method == HTTP_POST) {
        String body = request.getBody();
        if (body == NULL || body.length() > 128) {
            sendApiCode(response, API_CODES::API_BAD_REQUEST, "could not get HTTP body or body too long");
            return;

        } else {

            DynamicJsonBuffer jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(body);
            //root.prettyPrintTo(Serial);
        }
        sendApiCode(response, API_CODES::API_SUCCESS);
    } else {
        JsonObjectStream* stream = new JsonObjectStream();
        JsonObject& json = stream->getRoot();

        sendApiResponse(response, stream);
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

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }

    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject& json = stream->getRoot();

    bool error = false;

    if (app.network.isScanning()) {
        json["scanning"] = true;
    } else {
        json["scanning"] = false;
        JsonArray& netlist = json.createNestedArray("available");
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

            JsonObject &item = netlist.createNestedObject();
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

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

    if (request.method != HTTP_POST) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP POST");
        return;
    }
    if (!app.network.isScanning()) {
        app.network.scan();
    }

    sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onConnect(HttpRequest &request, HttpResponse &response) {

    if (!authenticated(request, response)) {
        return;
    }

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
        return;
    }

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
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);
        String ssid = "";
        String password = "";
        if (root["ssid"].success()) {
            ssid = root["ssid"].asString();
            if (root["password"].success()) {
                password = root["password"].asString();
            }
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
        JsonObject& json = stream->getRoot();

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

    if (app.ota.isProccessing()) {
        sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
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
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);

        if (root["cmd"].success()) {
            String cmd = root["cmd"].asString();
            if (cmd.equals("debug")) {
                if (root["enable"].success()) {
                    if (root["enable"]) {
                        Serial.systemDebugOutput(true);
                    } else {
                        Serial.systemDebugOutput(false);
                    }
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

    if (request.method != HTTP_POST
            && request.method != HTTP_GET) {
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
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(body);
        String romurl, spiffsurl;
        bool error = false;

        if (root["rom"].success() && root["spiffs"].success()) {

            if (root["rom"]["url"].success() && root["spiffs"]["url"].success()) {
                romurl = root["rom"]["url"].asString();
                spiffsurl = root["spiffs"]["url"].asString();
            } else {
                error = true;
            }

        } else {
            error = true;
        }
        if (error) {
            sendApiCode(response, API_CODES::API_MISSING_PARAM);
            return;
        } else {
            app.ota.start(romurl, spiffsurl);
            sendApiCode(response, API_CODES::API_SUCCESS);
            return;
        }

    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject& json = stream->getRoot();
    json["status"] = int(app.ota.getStatus());
    sendApiResponse(response, stream);
}

//simple call-response to check if we can reach server
void ApplicationWebserver::onPing(HttpRequest &request, HttpResponse &response) {
    if (request.method != HTTP_GET) {
        sendApiCode(response, API_CODES::API_BAD_REQUEST, "not HTTP GET");
        return;
    }
    JsonObjectStream* stream = new JsonObjectStream();
    JsonObject& json = stream->getRoot();
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

void ApplicationWebserver::generate204(HttpRequest &request, HttpResponse &response) {
    response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response.setHeader("Pragma", "no-cache");
    response.setHeader("Expires", "-1");
    response.setHeader("Content-Lenght", "0");
    response.setContentType("text/plain");
    response.code = 204;
}

