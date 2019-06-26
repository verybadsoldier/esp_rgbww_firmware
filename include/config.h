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
#pragma once

#include <SmingCore/SmingCore.h>
#include <RGBWWCtrl.h>

#define APP_SETTINGS_FILE ".cfg"
#define APP_SETTINGS_VERSION 1

struct ApplicationSettings {
    struct network {
        struct connection {
            String mdnshostname;
            bool dhcp = true;
            IPAddress ip;
            IPAddress netmask;
            IPAddress gateway;
        };

        struct mqtt {
            bool enabled = false;
            String server = "mqtt.local";
            int port = 1883;
            String username;
            String password;
            String topic_base = "home/";
        };

        struct ap {
            bool secured = DEFAULT_AP_SECURED;
            String ssid;
            String password = DEFAULT_AP_PASSWORD;
        };

        connection connection;
        mqtt mqtt;
        ap ap;
    };

    struct sync {
        bool clock_master_enabled = false;
        int clock_master_interval = 30;

        bool clock_slave_enabled = false;
        String clock_slave_topic= "home/led1/clock";

        bool cmd_master_enabled = false;
        bool cmd_slave_enabled = false;
        String cmd_slave_topic = "home/led1/command";

        bool color_master_enabled = false;
        int color_master_interval_ms = 0;
        bool color_slave_enabled = false;
        String color_slave_topic = "home/led1/color";
    };

    struct events {
        bool server_enabled = true;
        int color_interval_ms = 500;
        int color_mininterval_ms = 500;
        int transfin_interval_ms = 1000;
    };

    struct color {
        struct hsv {
            int model = 0;
            float red = 0;
            float yellow = 0;
            float green = 0;
            float cyan = 0;
            float blue = 0;
            float magenta = 0;
        };

        struct brightness {
            int red = 100;
            int green = 100;
            int blue = 100;
            int ww = 100;
            int cw = 100;
        };

        struct colortemp {
            int ww = DEFAULT_COLORTEMP_WW;
            int cw = DEFAULT_COLORTEMP_CW;
        };

        hsv hsv;
        brightness brightness;
        colortemp colortemp;
        int outputmode = 0;
        String startup_color = "last";
    };

    struct general {
        bool api_secured = DEFAULT_API_SECURED;
        String api_password = DEFAULT_API_PASSWORD;
        String otaurl = DEFAULT_OTA_URL;
        String device_name;
        String pin_config = "13,12,14,5,4";
        String buttons_config;
        int buttons_debounce_ms = 50;
    };

    general general;
    network network;
    color color;
    sync sync;
    events events;

    void load(bool print = false) {
        DynamicJsonBuffer jsonBuffer;
        if (exist()) {
            int size = fileGetSize(APP_SETTINGS_FILE);
            char* jsonString = new char[size + 1];
            fileGetContent(APP_SETTINGS_FILE, jsonString, size + 1);
            JsonObject& root = jsonBuffer.parseObject(jsonString);

            // connection
            network.connection.mdnshostname = root["network"]["connection"]["hostname"].asString();
            network.connection.dhcp = root["network"]["connection"]["dhcp"];
            network.connection.ip = root["network"]["connection"]["ip"].asString();
            network.connection.netmask = root["network"]["connection"]["netmask"].asString();
            network.connection.gateway = root["network"]["connection"]["gateway"].asString();

            // accesspoint
            network.ap.secured = root["network"]["ap"]["secured"];
            network.ap.ssid = root["network"]["ap"]["ssid"].asString();
            network.ap.password = root["network"]["ap"]["password"].asString();

            // mqtt
            if (root["network"]["mqtt"].success()) {
                if (root["network"]["mqtt"]["enabled"].success())
                    network.mqtt.enabled = root["network"]["mqtt"]["enabled"];
                if (root["network"]["mqtt"]["server"].success())
                    network.mqtt.server = root["network"]["mqtt"]["server"].asString();
                if (root["network"]["mqtt"]["port"].success())
                    network.mqtt.port = root["network"]["mqtt"]["port"];
                if (root["network"]["mqtt"]["username"].success())
                    network.mqtt.username = root["network"]["mqtt"]["username"].asString();
                if (root["network"]["mqtt"]["password"].success())
                    network.mqtt.password = root["network"]["mqtt"]["password"].asString();
                if (root["network"]["mqtt"]["topic_base"].success())
                    network.mqtt.topic_base = root["network"]["mqtt"]["topic_base"].asString();
            }

            // color
            color.outputmode = root["color"]["outputmode"];
            if (root["color"]["startup_color"].success())
                color.startup_color = root["color"]["startup_color"].asString();

            // hsv
            color.hsv.model = root["color"]["hsv"]["model"];
            color.hsv.red = root["color"]["hsv"]["red"];
            color.hsv.yellow = root["color"]["hsv"]["yellow"];
            color.hsv.green = root["color"]["hsv"]["green"];
            color.hsv.cyan = root["color"]["hsv"]["cyan"];
            color.hsv.blue = root["color"]["hsv"]["blue"];
            color.hsv.magenta = root["color"]["hsv"]["magenta"];

            // brightness
            color.brightness.red = root["color"]["brightness"]["red"];
            color.brightness.green = root["color"]["brightness"]["green"];
            color.brightness.blue = root["color"]["brightness"]["blue"];
            color.brightness.ww = root["color"]["brightness"]["ww"];
            color.brightness.cw = root["color"]["brightness"]["cw"];

            // general
            if (root["general"].success()) {
                if (root["general"]["api_password"].success())
                    general.api_password = root["general"]["api_password"].asString();
                if (root["general"]["api_secured"].success())
                    general.api_secured = root["general"]["api_secured"];
                if (root["general"]["otaurl"].success())
                    general.otaurl = root["general"]["otaurl"].asString();
                if (root["general"]["device_name"].success())
                    general.device_name = root["general"]["device_name"].asString();
                if (root["general"]["pin_config"].success())
                    general.pin_config = root["general"]["pin_config"].asString();
                if (root["general"]["buttons_config"].success())
                    general.buttons_config = root["general"]["buttons_config"].asString();
                if (root["general"]["buttons_debounce_ms"].success())
                    general.buttons_debounce_ms = root["general"]["buttons_debounce_ms"];
            }

            // sync
            if (root["sync"].success()) {
                if (root["sync"]["clock_master_enabled"].success())
                    sync.clock_master_enabled = root["sync"]["clock_master_enabled"];
                if (root["sync"]["clock_master_interval"].success())
                    sync.clock_master_interval = root["sync"]["clock_master_interval"];
                if (root["sync"]["clock_slave_topic"].success())
                    sync.clock_slave_topic = root["sync"]["clock_slave_topic"].asString();
                if (root["sync"]["clock_slave_enabled"].success())
                    sync.clock_slave_enabled = root["sync"]["clock_slave_enabled"];

                if (root["sync"]["cmd_master_enabled"].success())
                    sync.cmd_master_enabled = root["sync"]["cmd_master_enabled"];
                if (root["sync"]["cmd_slave_enabled"].success())
                    sync.cmd_slave_enabled = root["sync"]["cmd_slave_enabled"];
                if (root["sync"]["cmd_slave_topic"].success())
                    sync.cmd_slave_topic = root["sync"]["cmd_slave_topic"].asString();

                if (root["sync"]["color_master_enabled"].success())
                    sync.color_master_enabled = root["sync"]["color_master_enabled"];
                if (root["sync"]["color_master_interval_ms"].success())
                    sync.color_master_interval_ms = root["sync"]["color_master_interval_ms"];
                if (root["sync"]["color_slave_enabled"].success())
                    sync.color_slave_enabled = root["sync"]["color_slave_enabled"];
                if (root["sync"]["color_slave_topic"].success())
                    sync.color_slave_topic = root["sync"]["color_slave_topic"].asString();
            }


            // events
            if (root["events"].success()) {
                if (root["events"]["server_enabled"].success())
                    events.server_enabled = root["events"]["server_enabled"];
                if (root["events"]["color_interval_ms"].success())
                    events.color_interval_ms = root["events"]["color_interval_ms"];
                if (root["events"]["transfin_interval_ms"].success())
                    events.transfin_interval_ms = root["events"]["transfin_interval_ms"];
            }

            if (print) {
                root.prettyPrintTo(Serial);
            }

            delete[] jsonString;
        }

        sanitizeValues();
    }

    void save(bool print = false) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();

        JsonObject& net = root.createNestedObject("network");
        JsonObject& con = net.createNestedObject("connection");
        con["dhcp"] = network.connection.dhcp;
        con["ip"] = network.connection.ip.toString();
        con["netmask"] = network.connection.netmask.toString();
        con["gateway"] = network.connection.gateway.toString();
        con["mdnhostname"] = network.connection.mdnshostname.c_str();

        JsonObject& jap = net.createNestedObject("ap");
        jap["secured"] = network.ap.secured;
        jap["ssid"] = network.ap.ssid.c_str();
        jap["password"] = network.ap.password.c_str();

        JsonObject& jmqtt = net.createNestedObject("mqtt");
        jmqtt["enabled"] = network.mqtt.enabled;
        jmqtt["server"] = network.mqtt.server.c_str();
        jmqtt["port"] = network.mqtt.port;
        jmqtt["username"] = network.mqtt.username.c_str();
        jmqtt["password"] = network.mqtt.password.c_str();
        jmqtt["topic_base"] = network.mqtt.topic_base.c_str();

        JsonObject& c = root.createNestedObject("color");
        c["outputmode"] = color.outputmode;
        c["startup_color"] = color.startup_color.c_str();

        JsonObject& h = c.createNestedObject("hsv");
        h["model"] = color.hsv.model;
        h["red"] = color.hsv.red;
        h["yellow"] = color.hsv.yellow;
        h["green"] = color.hsv.green;
        h["cyan"] = color.hsv.cyan;
        h["blue"] = color.hsv.blue;
        h["magenta"] = color.hsv.magenta;

        JsonObject& b = c.createNestedObject("brightness");
        b["red"] = color.brightness.red;
        b["green"] = color.brightness.green;
        b["blue"] = color.brightness.blue;
        b["ww"] = color.brightness.ww;
        b["cw"] = color.brightness.cw;

        JsonObject& t = c.createNestedObject("colortemp");
        t["ww"] = color.colortemp.ww;
        t["cw"] = color.colortemp.cw;

        JsonObject& s = jsonBuffer.createObject();
        root["sync"] = s;
        s["clock_master_enabled"] = sync.clock_master_enabled;
        s["clock_master_interval"] = sync.clock_master_interval;
        s["clock_slave_enabled"] = sync.clock_slave_enabled;
        s["clock_slave_topic"] = sync.clock_slave_topic.c_str();

        s["cmd_master_enabled"] = sync.cmd_master_enabled;
        s["cmd_slave_enabled"] = sync.cmd_slave_enabled;
        s["cmd_slave_topic"] = sync.cmd_slave_topic.c_str();

        s["color_master_enabled"] = sync.color_master_enabled;
        s["color_master_interval_ms"] = sync.color_master_interval_ms;
        s["color_slave_enabled"] = sync.color_slave_enabled;
        s["color_slave_topic"] = sync.color_slave_topic.c_str();

        JsonObject& e = jsonBuffer.createObject();
        root["events"] = e;
        e["color_interval_ms"] = events.color_interval_ms;
        e["server_enabled"] = events.server_enabled;
        e["transfin_interval_ms"] = events.transfin_interval_ms;

        JsonObject& g = jsonBuffer.createObject();
        root["general"] = g;
        g["api_secured"] = general.api_secured;
        g["api_password"] = general.api_password.c_str();
        g["otaurl"] = general.otaurl.c_str();
        g["device_name"] = general.device_name.c_str();
        g["pin_config"] = general.pin_config.c_str();
        g["buttons_config"] = general.buttons_config.c_str();
        g["buttons_debounce_ms"] = general.buttons_debounce_ms;
        g["settings_ver"] = APP_SETTINGS_VERSION;

        String rootString;
        if (print) {
            root.prettyPrintTo(Serial);
        }
        root.printTo(rootString);
        fileSetContent(APP_SETTINGS_FILE, rootString);
    }

    bool exist() {
        return fileExist(APP_SETTINGS_FILE);
    }

    void reset() {
        if (exist()) {
            fileDelete(APP_SETTINGS_FILE);
        }
    }

    void sanitizeValues() {
        sync.clock_master_interval = max(sync.clock_master_interval, 1);
    }
};
