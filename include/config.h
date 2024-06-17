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
 * @section PLAN
 * holding the config struct in RAM seems wasteful. Also, having two entirely different
 * sets of code to serialize/deserialize the config struct for the API and load store 
 * is less than ideal. 
 * Also, the json representation of the base struct can outgrow the 1370Bytes allowed 
 * in a single http request and there may not be enough RAM to assemble multiple fragents 
 * or tcp packets.
 * 
 * I believe a better way to enhance this would be to have a config class that abstracts 
 * the json handling and avoids holding all data in RAM. 
 * I envision a class that can be used like this:
 * auto config.get("json path")
 * config.set("json path", "value") with value being either of type String or JsonObject or JsonArray
 * config.save()
 * config.load()
 * config.init()
 * 
 */
#pragma once

#include <RGBWWCtrl.h>
#include <JsonObjectStream.h>

#define APP_SETTINGS_FILE ".cfg"
#define APP_SETTINGS_VERSION 1

#define CONFIG_MAX_LENGTH 4096


struct channel {
    String name;
    int pin;
};

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
        String mdnsHosts;
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
        #ifdef ESP8266
        // String supported_color_models="[\"RGB\",\"RGBW\",\"RGBWW\",\"RAW\")]";
        // can't just stuff a string in here and hope it'll be interpreted as an array, this has to be a vector, too
        std::vector<String> supported_color_models;
        #endif
        
        String pin_config_name="mrpj";
        String pin_config_url="https://raw.githubusercontent.com/pljakobs/esp_rgb_webapp2/devel/public/config/pinconfig.json";

        std::vector<channel> channels;
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
        	auto net = root[F("network")];

            // connection
        	JsonObject con = net[F("connection")];
            network.connection.mdnshostname = con[F("hostname")].as<const char*>();
            network.connection.dhcp = con[F("dhcp")];
            network.connection.ip = con[F("ip")].as<String>();
            network.connection.netmask = con[F("netmask")].as<String>();
            network.connection.gateway = con[F("gateway")].as<String>();

            // accesspoint
            JsonObject jap = net[F("ap")];
            network.ap.secured = jap[F("secured")];
            network.ap.ssid = jap[F("ssid")].as<const char*>();
            network.ap.password = jap[F("password")].as<String>();

            // mqtt
            JsonObject jmqtt = net[F("mqtt")];
            if (!jmqtt.isNull()) {
                Json::getValue(jmqtt[F("enabled")], network.mqtt.enabled);
                Json::getValue(jmqtt[F("server")], network.mqtt.server);
                Json::getValue(jmqtt[F("port")], network.mqtt.port);
                Json::getValue(jmqtt[F("username")], network.mqtt.username);
                Json::getValue(jmqtt[F("password")], network.mqtt.password);
                Json::getValue(jmqtt[F("topic_base")], network.mqtt.topic_base);
            }

            // color
            JsonObject jcol = root[F("color")];
            color.outputmode = jcol[F("outputmode")];
            Json::getValue(jcol[F("startup_color")], color.startup_color);

            // hsv
            JsonObject jhsv = jcol[F("hsv")];
            color.hsv.model = jhsv[F("model")];
            color.hsv.red = jhsv[F("red")];
            color.hsv.yellow = jhsv[F("yellow")];
            color.hsv.green = jhsv[F("green")];
            color.hsv.cyan = jhsv[F("cyan")];
            color.hsv.blue = jhsv[F("blue")];
            color.hsv.magenta = jhsv[F("magenta")];

            // brightness
            JsonObject jbri = jcol[F("brightness")];
            color.brightness.red = jbri[F("red")];
            color.brightness.green = jbri[F("green")];
            color.brightness.blue = jbri[F("blue")];
            color.brightness.ww = jbri[F("ww")];
            color.brightness.cw = jbri[F("cw")];

            // general
            auto jgen = root[F("general")];
            if (!jgen.isNull()) {
                Json::getValue(jgen[F("api_password")], general.api_password);
                Json::getValue(jgen[F("api_secured")], general.api_secured);
                Json::getValue(jgen[F("otaurl")], general.otaurl);
                Json::getValue(jgen[F("device_name")], general.device_name);
                Json::getValue(jgen[F("pin_config")], general.pin_config);
                Json::getValue(jgen[F("buttons_config")], general.buttons_config);
                Json::getValue(jgen[F("buttons_debounce_ms")], general.buttons_debounce_ms);
                Json::getValue(jgen[F("pin_config_name")], general.pin_config_name);
                Json::getValue(jgen[F("pin_config_url")], general.pin_config_url);
            }
            auto jchan=root[F("general.channels")];
            if(!jchan.isNull()) {
                // update the channels vector, clear it first
                general.channels.clear();
                for (auto channel : jchan.as<JsonArray>()) {
                    general.channels.push_back( { 
                        jchan[F("name")],
                        jchan[F("pin")]
                        } 
                    );
                }
            }

            // ntp
            auto jntp = root[F("ntp")];
            if (!jntp.isNull()) {
                Json::getValue(jgen[F("enabled")], ntp.enabled);
                Json::getValue(jgen[F("server")], ntp.server);
                Json::getValue(jgen[F("interval")], ntp.interval);
            }

            // sync
            auto jsync = root[F("sync")];
            if (!jsync.isNull()) {
                Json::getValue(jsync[F("clock_master_enabled")], sync.clock_master_enabled);
                Json::getValue(jsync[F("clock_master_interval")], sync.clock_master_interval);
                Json::getValue(jsync[F("clock_slave_topic")], sync.clock_slave_topic);
                Json::getValue(jsync[F("clock_slave_enabled")], sync.clock_slave_enabled);

                Json::getValue(jsync[F("cmd_master_enabled")], sync.cmd_master_enabled);
                Json::getValue(jsync[F("cmd_slave_enabled")], sync.cmd_slave_enabled);
                Json::getValue(jsync[F("cmd_slave_topic")], sync.cmd_slave_topic);

                Json::getValue(jsync[F("color_master_enabled")], sync.color_master_enabled);
                Json::getValue(jsync[F("color_master_interval_ms")], sync.color_master_interval_ms);
                Json::getValue(jsync[F("color_slave_enabled")], sync.color_slave_enabled);
                Json::getValue(jsync[F("color_slave_topic")], sync.color_slave_topic);
            }

            // events
            auto jevents = root[F("events")];
            if (!jevents.isNull()) {
                Json::getValue(jevents[F("server_enabled")], events.server_enabled);
                Json::getValue(jevents[F("color_interval_ms")], events.color_interval_ms);
                Json::getValue(jevents[F("transfin_interval_ms")], events.transfin_interval_ms);
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

    void save(bool print = true) {
        DynamicJsonDocument doc(CONFIG_MAX_LENGTH);
        JsonObject root = doc.to<JsonObject>();

        JsonObject net = root.createNestedObject("network");
        JsonObject con = net.createNestedObject("connection");
        con[F("dhcp")] = network.connection.dhcp;
        con[F("ip")] = network.connection.ip.toString();
        con[F("netmask")] = network.connection.netmask.toString();
        con[F("gateway")] = network.connection.gateway.toString();
        con[F("mdnhostname")] = network.connection.mdnshostname;

        JsonObject jap = net.createNestedObject("ap");
        jap[F("secured")] = network.ap.secured;
        jap[F("ssid")] = network.ap.ssid;
        jap[F("password")] = network.ap.password;

        JsonObject jmqtt = net.createNestedObject("mqtt");
        jmqtt[F("enabled")] = network.mqtt.enabled;
        jmqtt[F("server")] = network.mqtt.server;
        jmqtt[F("port")] = network.mqtt.port;
        jmqtt[F("username")] = network.mqtt.username;
        jmqtt[F("password")] = network.mqtt.password;
        jmqtt[F("topic_base")] = network.mqtt.topic_base;

        JsonObject c = root.createNestedObject("color");
        c[F("outputmode")] = color.outputmode;
        c[F("startup_color")] = color.startup_color.c_str();

        JsonObject h = c.createNestedObject("hsv");
        h[F("model")] = color.hsv.model;
        h[F("red")] = color.hsv.red;
        h[F("yellow")] = color.hsv.yellow;
        h[F("green")] = color.hsv.green;
        h[F("cyan")] = color.hsv.cyan;
        h[F("blue")] = color.hsv.blue;
        h[F("magenta")] = color.hsv.magenta;

        JsonObject b = c.createNestedObject("brightness");
        b[F("red")] = color.brightness.red;
        b[F("green")] = color.brightness.green;
        b[F("blue")] = color.brightness.blue;
        b[F("ww")] = color.brightness.ww;
        b[F("cw")] = color.brightness.cw;

        JsonObject t = c.createNestedObject("colortemp");
        t[F("ww")] = color.colortemp.ww;
        t[F("cw")] = color.colortemp.cw;

        JsonObject n = root.createNestedObject("ntp");
        n[F("enabled")] = ntp.enabled;
        n[F("server")] = ntp.server;
        n[F("interval")] = ntp.interval;

        JsonObject s = root.createNestedObject("sync");
        s[F("clock_master_enabled")] = sync.clock_master_enabled;
        s[F("clock_master_interval")] = sync.clock_master_interval;
        s[F("clock_slave_enabled")] = sync.clock_slave_enabled;
        s[F("clock_slave_topic")] = sync.clock_slave_topic.c_str();

        s[F("cmd_master_enabled")] = sync.cmd_master_enabled;
        s[F("cmd_slave_enabled")] = sync.cmd_slave_enabled;
        s[F("cmd_slave_topic")] = sync.cmd_slave_topic.c_str();

        s[F("color_master_enabled")] = sync.color_master_enabled;
        s[F("color_master_interval_ms")] = sync.color_master_interval_ms;
        s[F("color_slave_enabled")] = sync.color_slave_enabled;
        s[F("color_slave_topic")] = sync.color_slave_topic.c_str();

        JsonObject e = root.createNestedObject("events");
        e[F("color_interval_ms")] = events.color_interval_ms;
        e[F("server_enabled")] = events.server_enabled;
        e[F("transfin_interval_ms")] = events.transfin_interval_ms;

        JsonObject g = root.createNestedObject("general");
        g[F("api_secured")] = general.api_secured;
        g[F("api_password")] = general.api_password.c_str();
        g[F("otaurl")] = general.otaurl.c_str();
        g[F("device_name")] = general.device_name.c_str();
        g[F("pin_config")] = general.pin_config.c_str();
        g[F("buttons_config")] = general.buttons_config.c_str();
        g[F("buttons_debounce_ms")] = general.buttons_debounce_ms;
        g[F("pin_config_name")] = general.pin_config_name.c_str();
        g[F("pin_config_url")] = general.pin_config_url.c_str();
        g[F("pin_config_name")]=general.pin_config_name.c_str();
        g[F("settings_ver")] = APP_SETTINGS_VERSION;

        auto j=g.createNestedArray("channels");
        for (uint8_t i=0;i<general.channels.size();i++) {
            j[i][F("name")]=general.channels[i].name.c_str();
            j[i][F("pin")]=general.channels[i].pin;
        }

        if (print) {
        	Json::serialize(root, Serial, Json::Pretty);
        }

        debug_i("Saving config to file: %s", APP_SETTINGS_FILE);
        if (!Json::saveToFile(root, APP_SETTINGS_FILE))
            {
                debug_e("Saving config to file failed!");
            }else{
                debug_i("success");
            }
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
    namespace config{
    inline void initializeConfig(ApplicationSettings& cfg){
        debug_i("initializing vectors in config");
        if(cfg.general.pin_config_name=="")
            cfg.general.pin_config_name="mrpw"; //set a sensible default. Other configs can be read from the pinconfig json source either on github or in spiffs. 
        debug_i("populating channels array");
        if (cfg.general.channels.size() == 0 && cfg.general.pin_config_name == "mrpj") {
            cfg.general.channels.push_back({ "red", 13 });
            cfg.general.channels.push_back({ "green", 12 });
            cfg.general.channels.push_back({ "blue", 14 });
            cfg.general.channels.push_back({ "warmwhite", 5 });
            cfg.general.channels.push_back({ "coldwhite", 4 });
        }
        debug_i("added %i elements to channels array", cfg.general.channels.size());
        for (uint8_t i=0;i<cfg.general.channels.size();i++) {
            debug_i("channel %i: %s, %i", i, cfg.general.channels[i].name.c_str(), cfg.general.channels[i].pin);
        }
        #ifdef ARCH_ESP8266
        if (cfg.general.supported_color_models.size() == 0) {
            cfg.general.supported_color_models.push_back("RGB");
            cfg.general.supported_color_models.push_back("RGBW");
            cfg.general.supported_color_models.push_back("RGBWW");
            cfg.general.supported_color_models.push_back("RAW");
        }
        #endif
    }
}//namespace config