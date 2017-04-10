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
}

void ApplicationWebserver::init() {
	setDefaultHandler(HttpPathDelegate(&ApplicationWebserver::onFile, this));
	enableHeaderProcessing("Authorization");
	addPath("/", HttpPathDelegate(&ApplicationWebserver::onIndex, this));
	addPath("/webapp", HttpPathDelegate(&ApplicationWebserver::onWebapp, this));
	addPath("/config", HttpPathDelegate(&ApplicationWebserver::onConfig, this));
	addPath("/info", HttpPathDelegate(&ApplicationWebserver::onInfo, this));
	addPath("/color", HttpPathDelegate(&ApplicationWebserver::onColor, this));
	addPath("/animation", HttpPathDelegate(&ApplicationWebserver::onAnimation, this));
	addPath("/networks", HttpPathDelegate(&ApplicationWebserver::onNetworks, this));
	addPath("/scan_networks", HttpPathDelegate(&ApplicationWebserver::onScanNetworks, this));
	addPath("/system", HttpPathDelegate(&ApplicationWebserver::onSystemReq, this));
	addPath("/update", HttpPathDelegate(&ApplicationWebserver::onUpdate, this));
	addPath("/connect", HttpPathDelegate(&ApplicationWebserver::onConnect, this));
	addPath("/generate_204", HttpPathDelegate(&ApplicationWebserver::generate204, this));
	addPath("/ping", HttpPathDelegate(&ApplicationWebserver::onPing, this));
	addPath("/stop", HttpPathDelegate(&ApplicationWebserver::onStop, this));
	addPath("/skip", HttpPathDelegate(&ApplicationWebserver::onSkip, this));
	addPath("/pause", HttpPathDelegate(&ApplicationWebserver::onPause, this));
	addPath("/continue", HttpPathDelegate(&ApplicationWebserver::onContinue, this));
	addPath("/blink", HttpPathDelegate(&ApplicationWebserver::onBlink, this));
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

bool ICACHE_FLASH_ATTR ApplicationWebserver::authenticated(HttpRequest &request, HttpResponse &response) {
	if (!app.cfg.general.api_secured)
		return true;
	String userPass = request.getHeader("Authorization");
	// header in form of: "Basic MTIzNDU2OmFiY2RlZmc="so the 6 is to get to beginning of 64 encoded string
	int headerLength = userPass.length() - 6;
	if (headerLength > 50) {
		return false;
	}

	unsigned char decbuf[headerLength]; // buffer for the decoded string
	int outlen = base64_decode(headerLength, userPass.c_str() + 6, headerLength, decbuf);
	decbuf[outlen] = 0;
	userPass = String((char*) decbuf);
	if (userPass.endsWith(app.cfg.general.api_password)) {
		return true;
	}

	response.authorizationRequired();
	response.setHeader("WWW-Authenticate", "Basic realm=\"RGBWW Server\"");
	response.setHeader("401 wrong credentials", "wrong credentials");
	response.setHeader("Connection", "close");
	return false;

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
	response.setAllowCrossDomainOrigin("*");
	if (code != 200) {
		response.badRequest();
	}
	response.sendJsonObject(stream);
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
		response.setStatusCode(503, "SERVICE UNAVAILABLE");
		response.sendString("OTA in progress");
		return;
	}

	if (!app.isFilesystemMounted()) {
		response.setContentType("text/plain");
		response.setStatusCode(500, "INTERNAL SERVER ERROR");
		response.sendString("No filesystem mounted");
		return;
	}

	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);
	if (file[0] == '.') {
		response.forbidden();
		return;
	}

	if (!fileExist(file) && !fileExist(file + ".gz") && WifiAccessPoint.isEnabled()) {
		//if accesspoint is active and we couldn`t find the file - redirect to index
		debugapp("ApplicationWebserver::onFile redirecting");
		response.redirect("http://" + WifiAccessPoint.getIP().toString() + "/webapp");
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
		response.setStatusCode(503, "SERVICE UNAVAILABLE");
		response.sendString("OTA in progress");
		return;
	}

	if (WifiAccessPoint.isEnabled()) {
		response.redirect("http://" + WifiAccessPoint.getIP().toString() + "/webapp");
	} else {
		response.redirect("http://" + WifiStation.getIP().toString() + "/webapp");
	}

}

void ApplicationWebserver::onWebapp(HttpRequest &request, HttpResponse &response) {

	if (!authenticated(request, response)) {
		return;
	}

	if (app.ota.isProccessing()) {
		response.setContentType("text/plain");
		response.setStatusCode(503, "SERVICE UNAVAILABLE");
		response.sendString("OTA in progress");
		return;
	}

	if (request.getRequestMethod() != RequestMethod::GET) {
		response.badRequest();
		return;
	}

	if (!app.isFilesystemMounted()) {
		response.setContentType("text/plain");
		response.setStatusCode(500, "INTERNAL SERVER ERROR");
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

void ApplicationWebserver::onConfig(HttpRequest &request, HttpResponse &response) {

	if (!authenticated(request, response)) {
		return;
	}

	if (app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}

	if (request.getRequestMethod() != RequestMethod::POST && request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	if (request.getRequestMethod() == RequestMethod::POST) {
		if (request.getBody() == NULL) {

			sendApiCode(response, API_CODES::API_BAD_REQUEST);
			return;

		}

		bool error = false;
		String error_msg = getApiCodeMsg(API_CODES::API_BAD_REQUEST);
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(request.getBody());

		// remove comment for debugging
		//root.prettyPrintTo(Serial);

		bool ip_updated = false;
		bool color_updated = false;
		bool ap_updated = false;
		if (!root.success()) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST);
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

		// update and save settings if we haven`t received any error until now
		if (!error) {
			if (ip_updated) {
				if (root["restart"].success()) {
					if (root["restart"] == true) {
						debugapp("ApplicationWebserver::onConfig ip settings changed - rebooting");
						app.delayedCMD("restart", 3000); // wait 3s to first send response
						//json["data"] = "restart";

					}
				}
			};
			if (ap_updated) {
				if (root["restart"].success()) {
					if (root["restart"] == true && WifiAccessPoint.isEnabled()) {
						debugapp("ApplicationWebserver::onConfig wifiap settings changed - rebooting");
						app.delayedCMD("restart", 3000); // wait 3s to first send response
						//json["data"] = "restart";

					}
				}
			}
			if (color_updated) {
				debugapp("ApplicationWebserver::onConfig color settings changed - refreshing");

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
		ap["ssid"] = app.cfg.network.ap.ssid.c_str();

		JsonObject& mqtt = net.createNestedObject("mqtt");
		mqtt["enabled"] = app.cfg.network.mqtt.enabled;
		mqtt["server"] = app.cfg.network.mqtt.server.c_str();
		mqtt["port"] = app.cfg.network.mqtt.port;
		mqtt["username"] = app.cfg.network.mqtt.username.c_str();

		//mqtt["password"] = app.cfg.network.mqtt.password.c_str();

		JsonObject& color = json.createNestedObject("color");
		color["outputmode"] = app.cfg.color.outputmode;

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
		sendApiResponse(response, stream);
	}
}

void ApplicationWebserver::onInfo(HttpRequest &request, HttpResponse &response) {

	if (!authenticated(request, response)) {
		return;
	}

	if (app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}

	if (request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& data = stream->getRoot();
	data["deviceid"] = String(system_get_chip_id());
	data["current_rom"] = String(app.getRomSlot());
	data["firmware"] = fw_version;
	data["git_version"] = fw_git_version;
	data["git_date"] = fw_git_date;
	data["config_version"] = app.cfg.configversion;
	data["sming"] = SMING_VERSION;
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

RGBWWLed::ChannelList ApplicationWebserver::parseChannelRequestParams(HttpRequest &request) {
    String body = request.getBody();
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(body);

    RGBWWLed::ChannelList channels;
    if (root["channels"].success()) {
        const JsonArray& arr = root["channels"].asArray();
        for(size_t i=0; i < arr.size(); ++i) {
            const String& str = arr[i].asString();
            if (str == "h") {
                channels.add(CtrlChannel::Hue);
            }
            else if (str == "s") {
                channels.add(CtrlChannel::Sat);
            }
            else if (str == "v") {
                channels.add(CtrlChannel::Val);
            }
            else if (str == "ct") {
                channels.add(CtrlChannel::ColorTemp);
            }
        }
    }

    return channels;
}

void ApplicationWebserver::parseColorRequestParams(JsonObject& root, ColorRequestParameters& params) {
    debugapp("parse3");
	if (root["hsv"].success()) {
	    debugapp("parse4");
		params.mode = ColorRequestParameters::Mode::Hsv;
		if (root["hsv"]["h"].success())
		    params.hsv.h = AbsOrRelValue(root["hsv"]["h"].asString(), AbsOrRelValue::Type::Hue);
        if (root["hsv"]["s"].success())
            params.hsv.s = AbsOrRelValue(root["hsv"]["s"].asString());
        if (root["hsv"]["v"].success())
            params.hsv.v = AbsOrRelValue(root["hsv"]["v"].asString());
		if (root["hsv"]["ct"].success())
		    params.hsv.ct = AbsOrRelValue(root["hsv"]["ct"].asString(), AbsOrRelValue::Type::Ct);
	    debugapp("parse5");

		if (root["hsv"]["from"].success()) {
			params.hasHsvFrom = true;
	        if (root["hsv"]["from"]["h"].success())
	            params.hsv.h = AbsOrRelValue(root["hsv"]["from"]["h"].asString(), AbsOrRelValue::Type::Hue);
            if (root["hsv"]["from"]["s"].success())
                params.hsv.s = AbsOrRelValue(root["hsv"]["from"]["s"].asString());
            if (root["hsv"]["from"]["v"].success())
                params.hsv.v = AbsOrRelValue(root["hsv"]["from"]["v"].asString());
	        if (root["hsv"]["from"]["ct"].success())
	            params.hsv.ct = AbsOrRelValue(root["hsv"]["from"]["ct"].asString(), AbsOrRelValue::Type::Ct);
		}
	}
	else if (root["raw"].success()) {
        params.mode = ColorRequestParameters::Mode::Raw;
		if (root["raw"]["r"].success())
		    params.raw.r = AbsOrRelValue(root["raw"]["r"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["g"].success())
            params.raw.r = AbsOrRelValue(root["raw"]["g"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["b"].success())
            params.raw.r = AbsOrRelValue(root["raw"]["b"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["ww"].success())
            params.raw.r = AbsOrRelValue(root["raw"]["ww"].asString(), AbsOrRelValue::Type::Raw);
        if (root["raw"]["cw"].success())
            params.raw.r = AbsOrRelValue(root["raw"]["cw"].asString(), AbsOrRelValue::Type::Raw);

		if (root["raw"]["from"].success()) {
			params.hasRawFrom = true;
	        if (root["raw"]["from"]["r"].success())
	            params.raw.r = AbsOrRelValue(root["raw"]["from"]["r"].asString(), AbsOrRelValue::Type::Raw);
	        if (root["raw"]["from"]["g"].success())
	            params.raw.r = AbsOrRelValue(root["raw"]["from"]["g"].asString(), AbsOrRelValue::Type::Raw);
	        if (root["raw"]["from"]["b"].success())
	            params.raw.r = AbsOrRelValue(root["raw"]["from"]["b"].asString(), AbsOrRelValue::Type::Raw);
	        if (root["raw"]["from"]["ww"].success())
	            params.raw.r = AbsOrRelValue(root["raw"]["from"]["ww"].asString(), AbsOrRelValue::Type::Raw);
	        if (root["raw"]["from"]["cw"].success())
	            params.raw.r = AbsOrRelValue(root["raw"]["from"]["cw"].asString(), AbsOrRelValue::Type::Raw);
		}
	}

	if (root["kelvin"].success()) {
        params.mode = ColorRequestParameters::Mode::Kelvin;
	}

	if (root["t"].success()) {
		params.time = root["t"].as<int>();
	}

	if (root["r"].success()) {
		params.requeue = root["r"].as<int>() == 1;
	}

	if (root["kelvin"].success()) {
		params.kelvin = root["kelvin"].as<int>();
	}

	if (root["d"].success()) {
		params.direction = root["d"].as<int>();
	}

	if (root["name"].success()) {
		params.name = root["name"].asString();
	}

	if (root["cmd"].success()) {
		params.cmd = root["cmd"].asString();
	}

	if (root["q"].success()) {
		const String& q = root["q"].asString();
		if (q == "back")
			params.queue = QueuePolicy::Back;
		else if (q == "front")
			params.queue = QueuePolicy::Front;
		else if (q == "front_reset")
			params.queue = QueuePolicy::FrontReset;
		else if (q == "single")
			params.queue = QueuePolicy::Single;
		else {
		    params.queue = QueuePolicy::Invalid;
		}
	}
}

int ApplicationWebserver::ColorRequestParameters::checkParams(String& errorMsg) const {
    if (mode == Mode::Hsv) {
        if (hsv.ct.hasValue()) {
            if (hsv.ct != 0 && (hsv.ct < 100 || hsv.ct > 10000 || (hsv.ct > 500 && hsv.ct < 2000))) {
                errorMsg = "bad param for ct";
                return 1;
            }
        }

        if (!hsv.h.hasValue() && !hsv.s.hasValue() && !hsv.v.hasValue() && !hsv.ct.hasValue()) {
            errorMsg = "Need at least one HSVCT component!";
            return 1;
        }
    }
    else if (mode == Mode::Raw) {
        if (!raw.r.hasValue() && !raw.g.hasValue() && !raw.b.hasValue() && !raw.ww.hasValue() && !raw.cw.hasValue()) {
            errorMsg = "Need at least one RAW component!";
            return 1;
        }
    }

    if (queue == QueuePolicy::Invalid) {
        errorMsg = "Invalid queue policy";
        return 1;
    }

    if (cmd != "fade" && cmd != "solid") {
        errorMsg = "Invalid cmd";
        return 1;
    }

    if (direction < 0 || direction > 1) {
        errorMsg = "Invalid direction";
        return 1;
    }

    return 0;
}


void ApplicationWebserver::onColorGet(HttpRequest &request, HttpResponse &response) {
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	String mode = request.getQueryParameter("mode", "hsv");
	if (mode.equals("raw")) {
		JsonObject& raw = json.createNestedObject("raw");
		ChannelOutput output = app.rgbwwctrl.getCurrentOutput();
		raw["r"] = output.r;
		raw["g"] = output.g;
		raw["b"] = output.b;
		raw["ww"] = output.ww;
		raw["cw"] = output.cw;

	} else if (mode.equals("temp")) {
		json["kelvin"] = 0;
		//TODO get kelvin from controller
	} else {
		JsonObject& hsv = json.createNestedObject("hsv");

		float h, s, v;
		int ct;
		HSVCT c = app.rgbwwctrl.getCurrentColor();
		c.asRadian(h, s, v, ct);
		hsv["h"] = h;
		hsv["s"] = s;
		hsv["v"] = v;
		hsv["ct"] = ct;
	}
	sendApiResponse(response, stream);
}

void ApplicationWebserver::onColorPost(HttpRequest &request, HttpResponse &response) {
	String body = request.getBody();
	if (body == NULL || body.length() > 128) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;

	}

	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(body);
	if (root["cmds"].success()) {
		Vector<String> errors;
		// multi command post (needs testing)
		const JsonArray& cmds = root["cmds"].asArray();
		for(int i=0; i < cmds.size(); ++i) {
			String msg;
			if (!onColorPostCmd(cmds[i], msg)) {
				errors.add(msg);
			}
		}

		if (errors.size() == 0)
			sendApiCode(response, API_CODES::API_SUCCESS);
		else {
			String msg;
			for (int i=0; i < errors.size(); ++i) {
				msg += errors[i] + "|";
			}
			sendApiCode(response, API_CODES::API_BAD_REQUEST, msg);
		}
	}
	else {
		String msg;
		if (onColorPostCmd(root, msg)) {
			sendApiCode(response, API_CODES::API_SUCCESS);
		}
		else {
			sendApiCode(response, API_CODES::API_BAD_REQUEST, msg);
		}
	}
}

bool ApplicationWebserver::onColorPostCmd(JsonObject& root, String& errorMsg) {
	debugapp("parse1");

	ColorRequestParameters params;
	parseColorRequestParams(root, params);

    debugapp("parse2");

    {
        if (params.checkParams(errorMsg) != 0) {
            debugapp("paERer");
            return false;
        }
    }

	bool queueOk = false;
	if (params.mode == ColorRequestParameters::Mode::Kelvin) {
		//TODO: hand to rgbctrl
	} else if (params.mode == ColorRequestParameters::Mode::Hsv) {
	    debugapp("parse244");
        debugapp("parse4-1");
		debugapp("Exec HSV");


		if(!params.hasHsvFrom) {
			debugapp("a1");

			debugapp("ApplicationWebserver::onColor hsv CMD:%s t:%d Q:%d  h:%d s:%d v:%d ct:%d ", params.cmd.c_str(), params.time, params.queue, params.hsv.h.getValue().getValue(), params.hsv.s.getValue().getValue(), params.hsv.v.getValue().getValue(), params.hsv.ct.getValue().getValue());
			debugapp("a2");
			if (params.cmd == "fade") {
				queueOk = app.rgbwwctrl.fadeHSV(params.hsv, params.time, params.direction, params.queue, params.requeue, params.name);
			} else {
	            debugapp("setHSV");
	            queueOk = app.rgbwwctrl.setHSV(params.hsv, params.time, params.queue, params.requeue, params.name);
			}
		} else {
			app.rgbwwctrl.fadeHSV(params.hsvFrom, params.hsv, params.time, params.direction, params.queue);
		}
	} else if (params.mode == ColorRequestParameters::Mode::Raw) {
		if(!params.hasRawFrom) {
			debugapp("ApplicationWebserver::onColor raw CMD:%s Q:%d r:%i g:%i b:%i ww:%i cw:%i", params.cmd.c_str(), params.queue, params.raw.r.getValue().getValue(), params.raw.g.getValue().getValue(), params.raw.b.getValue().getValue(), params.raw.ww.getValue().getValue(), params.raw.cw.getValue().getValue());
			if (params.cmd == "fade") {
				queueOk = app.rgbwwctrl.fadeRAW(params.raw, params.time, params.queue);
			} else {
				queueOk = app.rgbwwctrl.setRAW(params.raw, params.time, params.queue);
			}
		} else {
			debugapp("ApplicationWebserver::onColor raw CMD:%s Q:%d FROM r:%i g:%i b:%i ww:%i cw:%i  TO r:%i g:%i b:%i ww:%i cw:%i",
					params.raw.r.getValue().getValue(), params.raw.g.getValue().getValue(), params.raw.b.getValue().getValue(), params.raw.ww.getValue().getValue(), params.raw.cw.getValue().getValue(),
					params.rawFrom.r.getValue().getValue(), params.rawFrom.g.getValue().getValue(), params.rawFrom.b.getValue().getValue(), params.rawFrom.ww.getValue().getValue(), params.rawFrom.cw.getValue().getValue());
			app.rgbwwctrl.fadeRAW(params.rawFrom, params.raw, params.time, params.queue);
		}
	} else {
		errorMsg = "No color object!";
		return false;
	}

	if (!queueOk)
		errorMsg = "Queue full";
	return queueOk;
}

void ApplicationWebserver::onColor(HttpRequest &request, HttpResponse &response) {
	if (!authenticated(request, response)) {
		return;
	}

	if (app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}

	if (request.getRequestMethod() != RequestMethod::POST && request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	bool error = false;
	if (request.getRequestMethod() == RequestMethod::POST) {
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

	if (request.getRequestMethod() != RequestMethod::POST && request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	bool error = false;
	if (request.getRequestMethod() == RequestMethod::POST) {
		String body = request.getBody();
		if (body == NULL || body.length() > 128) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST);
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

void ApplicationWebserver::onNetworks(HttpRequest &request, HttpResponse &response) {

	if (!authenticated(request, response)) {
		return;
	}

	if (app.ota.isProccessing()) {
		sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
		return;
	}

	if (request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
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

	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
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

	if (request.getRequestMethod() != RequestMethod::POST && request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	if (request.getRequestMethod() == RequestMethod::POST) {

		String body = request.getBody();
		if (body == NULL) {

			sendApiCode(response, API_CODES::API_BAD_REQUEST);
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
			debugapp("ssid %s - pass %s", ssid.c_str(), password.c_str());
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

	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	bool error = false;
	String body = request.getBody();
	if (body == NULL) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
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

	if (request.getRequestMethod() != RequestMethod::POST
			&& request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	if (request.getRequestMethod() == RequestMethod::POST) {
		if (app.ota.isProccessing()) {
			sendApiCode(response, API_CODES::API_UPDATE_IN_PROGRESS);
			return;
		}

		if (request.getBody() == NULL) {
			sendApiCode(response, API_CODES::API_BAD_REQUEST);
			return;
		}
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.parseObject(request.getBody());
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
	if (request.getRequestMethod() != RequestMethod::GET) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["ping"] = "pong";
	sendApiResponse(response, stream);
}

void ApplicationWebserver::onStop(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

    RGBWWLed::ChannelList channels = parseChannelRequestParams(request);
	app.rgbwwctrl.clearAnimationQueue(channels);
	app.rgbwwctrl.skipAnimation(channels);

	sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onSkip(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

	RGBWWLed::ChannelList channels = parseChannelRequestParams(request);
	app.rgbwwctrl.skipAnimation(channels);

	sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onPause(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

    RGBWWLed::ChannelList channels = parseChannelRequestParams(request);
	app.rgbwwctrl.pauseAnimation(channels);

	sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onContinue(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

    RGBWWLed::ChannelList channels = parseChannelRequestParams(request);
	app.rgbwwctrl.continueAnimation(channels);

	sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::onBlink(HttpRequest &request, HttpResponse &response) {
	if (request.getRequestMethod() != RequestMethod::POST) {
		sendApiCode(response, API_CODES::API_BAD_REQUEST);
		return;
	}

    RGBWWLed::ChannelList channels = parseChannelRequestParams(request);
	app.rgbwwctrl.blink(channels);

	sendApiCode(response, API_CODES::API_SUCCESS);
}

void ApplicationWebserver::generate204(HttpRequest &request, HttpResponse &response) {
	response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	response.setHeader("Pragma", "no-cache");
	response.setHeader("Expires", "-1");
	response.setHeader("Content-Lenght", "0");
	response.setContentType("text/plain");
	response.setStatusCode(204, "NO CONTENT");
}

