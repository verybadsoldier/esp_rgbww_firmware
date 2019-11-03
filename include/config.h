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

#include <RGBWWCtrl.h>
#include <JsonObjectStream.h>

#define APP_SETTINGS_FILE ".cfg"
#define APP_SETTINGS_VERSION 1

#define CONFIG_MAX_LENGTH 2048


struct ApplicationSettings {
    struct network {
        struct connection {
            String mdnshostname;
            bool dhcp = true;
            IpAddress ip;
            IpAddress netmask;
            IpAddress gateway;
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

    struct ntp {
        bool enabled = false;
        String server;
        int interval;
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
    ntp ntp;

    void load(bool print = false) {
        // 1024 is too small and leads to load error
        DynamicJsonDocument doc(CONFIG_MAX_LENGTH);
        if (Json::loadFromFile(doc, APP_SETTINGS_FILE)) {
        	auto root = doc.as<JsonObject>();
        	auto net = root["network"];

            // connection
        	JsonObject con = net["connection"];
            network.connection.mdnshostname = con["hostname"].as<const char*>();
            network.connection.dhcp = con["dhcp"];
            network.connection.ip = con["ip"].as<String>();
            network.connection.netmask = con["netmask"].as<String>();
            network.connection.gateway = con["gateway"].as<String>();

            // accesspoint
            JsonObject jap = net["ap"];
            network.ap.secured = jap["secured"];
            network.ap.ssid = jap["ssid"].as<const char*>();
            network.ap.password = jap["password"].as<String>();

            // mqtt
            JsonObject jmqtt = net["mqtt"];
            if (!jmqtt.isNull()) {
                Json::getValue(jmqtt["enabled"], network.mqtt.enabled);
                Json::getValue(jmqtt["server"], network.mqtt.server);
                Json::getValue(jmqtt["port"], network.mqtt.port);
                Json::getValue(jmqtt["username"], network.mqtt.username);
                Json::getValue(jmqtt["password"], network.mqtt.password);
                Json::getValue(jmqtt["topic_base"], network.mqtt.topic_base);
            }

            // color
            JsonObject jcol = root["color"];
            color.outputmode = jcol["outputmode"];
            Json::getValue(jcol["startup_color"], color.startup_color);

            // hsv
            JsonObject jhsv = jcol["hsv"];
            color.hsv.model = jhsv["model"];
            color.hsv.red = jhsv["red"];
            color.hsv.yellow = jhsv["yellow"];
            color.hsv.green = jhsv["green"];
            color.hsv.cyan = jhsv["cyan"];
            color.hsv.blue = jhsv["blue"];
            color.hsv.magenta = jhsv["magenta"];

            // brightness
            JsonObject jbri = jcol["brightness"];
            color.brightness.red = jbri["red"];
            color.brightness.green = jbri["green"];
            color.brightness.blue = jbri["blue"];
            color.brightness.ww = jbri["ww"];
            color.brightness.cw = jbri["cw"];

            // general
            auto jgen = root["general"];
            if (!jgen.isNull()) {
                Json::getValue(jgen["api_password"], general.api_password);
                Json::getValue(jgen["api_secured"], general.api_secured);
                Json::getValue(jgen["otaurl"], general.otaurl);
                Json::getValue(jgen["device_name"], general.device_name);
                Json::getValue(jgen["pin_config"], general.pin_config);
                Json::getValue(jgen["buttons_config"], general.buttons_config);
                Json::getValue(jgen["buttons_debounce_ms"], general.buttons_debounce_ms);
            }

            // ntp
            auto jntp = root["ntp"];
            if (!jntp.isNull()) {
                Json::getValue(jgen["enabled"], ntp.enabled);
                Json::getValue(jgen["server"], ntp.server);
                Json::getValue(jgen["interval"], ntp.interval);
            }

            // sync
            auto jsync = root["sync"];
            if (!jsync.isNull()) {
                Json::getValue(jsync["clock_master_enabled"], sync.clock_master_enabled);
                Json::getValue(jsync["clock_master_interval"], sync.clock_master_interval);
                Json::getValue(jsync["clock_slave_topic"], sync.clock_slave_topic);
                Json::getValue(jsync["clock_slave_enabled"], sync.clock_slave_enabled);

                Json::getValue(jsync["cmd_master_enabled"], sync.cmd_master_enabled);
                Json::getValue(jsync["cmd_slave_enabled"], sync.cmd_slave_enabled);
                Json::getValue(jsync["cmd_slave_topic"], sync.cmd_slave_topic);

                Json::getValue(jsync["color_master_enabled"], sync.color_master_enabled);
                Json::getValue(jsync["color_master_interval_ms"], sync.color_master_interval_ms);
                Json::getValue(jsync["color_slave_enabled"], sync.color_slave_enabled);
                Json::getValue(jsync["color_slave_topic"], sync.color_slave_topic);
            }


            // events
            auto jevents = root["events"];
            if (!jevents.isNull()) {
                Json::getValue(jevents["server_enabled"], events.server_enabled);
                Json::getValue(jevents["color_interval_ms"], events.color_interval_ms);
                Json::getValue(jevents["transfin_interval_ms"], events.transfin_interval_ms);
            }

            if (print) {
                debug_i("Loaded config file with following contents:");
            	Json::serialize(doc, Serial, Json::Pretty);
            }
        }
        else {
            debug_e("Could not load config file: %s", APP_SETTINGS_FILE);
        }

        sanitizeValues();
    }

    void save(bool print = false) {
        DynamicJsonDocument doc(CONFIG_MAX_LENGTH);
        JsonObject root = doc.to<JsonObject>();

        JsonObject net = root.createNestedObject("network");
        JsonObject con = net.createNestedObject("connection");
        con["dhcp"] = network.connection.dhcp;
        con["ip"] = network.connection.ip.toString();
        con["netmask"] = network.connection.netmask.toString();
        con["gateway"] = network.connection.gateway.toString();
        con["mdnhostname"] = network.connection.mdnshostname;

        JsonObject jap = net.createNestedObject("ap");
        jap["secured"] = network.ap.secured;
        jap["ssid"] = network.ap.ssid;
        jap["password"] = network.ap.password;

        JsonObject jmqtt = net.createNestedObject("mqtt");
        jmqtt["enabled"] = network.mqtt.enabled;
        jmqtt["server"] = network.mqtt.server;
        jmqtt["port"] = network.mqtt.port;
        jmqtt["username"] = network.mqtt.username;
        jmqtt["password"] = network.mqtt.password;
        jmqtt["topic_base"] = network.mqtt.topic_base;

        JsonObject c = root.createNestedObject("color");
        c["outputmode"] = color.outputmode;
        c["startup_color"] = color.startup_color.c_str();

        JsonObject h = c.createNestedObject("hsv");
        h["model"] = color.hsv.model;
        h["red"] = color.hsv.red;
        h["yellow"] = color.hsv.yellow;
        h["green"] = color.hsv.green;
        h["cyan"] = color.hsv.cyan;
        h["blue"] = color.hsv.blue;
        h["magenta"] = color.hsv.magenta;

        JsonObject b = c.createNestedObject("brightness");
        b["red"] = color.brightness.red;
        b["green"] = color.brightness.green;
        b["blue"] = color.brightness.blue;
        b["ww"] = color.brightness.ww;
        b["cw"] = color.brightness.cw;

        JsonObject t = c.createNestedObject("colortemp");
        t["ww"] = color.colortemp.ww;
        t["cw"] = color.colortemp.cw;

        JsonObject n = root.createNestedObject("ntp");
        n["enabled"] = ntp.enabled;
        n["server"] = ntp.server;
        n["interval"] = ntp.interval;

        JsonObject s = root.createNestedObject("sync");
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

        JsonObject e = root.createNestedObject("events");
        e["color_interval_ms"] = events.color_interval_ms;
        e["server_enabled"] = events.server_enabled;
        e["transfin_interval_ms"] = events.transfin_interval_ms;

        JsonObject g = root.createNestedObject("general");
        g["api_secured"] = general.api_secured;
        g["api_password"] = general.api_password.c_str();
        g["otaurl"] = general.otaurl.c_str();
        g["device_name"] = general.device_name.c_str();
        g["pin_config"] = general.pin_config.c_str();
        g["buttons_config"] = general.buttons_config.c_str();
        g["buttons_debounce_ms"] = general.buttons_debounce_ms;
        g["settings_ver"] = APP_SETTINGS_VERSION;

        if (print) {
        	Json::serialize(root, Serial, Json::Pretty);
        }

        debug_i("Saving config to file: %s", APP_SETTINGS_FILE);
        if (!Json::saveToFile(root, APP_SETTINGS_FILE))
            debug_e("Saving config to file failed!");
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
