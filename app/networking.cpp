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
#include "mdnshandler.cpp"

mdnsHandler mdnsHandler;

/**
 * @brief Constructor for the AppWIFI class.
 * 
 * Initializes the member variables of the AppWIFI class.
 */
AppWIFI::AppWIFI() {
    _ApIP = IpAddress(String(DEFAULT_AP_IP));
    _client_err_msg = "";
    _con_ctr = 0;
    _scanning = false;
    _new_connection = false;
    _client_status = CONNECTION_STATUS::IDLE;
}

/**
 * @brief Retrieves the available networks.
 * 
 * @return The list of available networks.
 */
BssList AppWIFI::getAvailableNetworks() {
    return _networks;
}

/**
 * Scans for available Wi-Fi networks.
 * 
 * @param connectAfterScan Flag indicating whether to connect to a network after the scan is completed.
 */
void AppWIFI::scan(bool connectAfterScan) {
    _scanning = true;
    _keepStaAfterScan = connectAfterScan;
    WifiStation.startScan(ScanCompletedDelegate(&AppWIFI::scanCompleted, this));
}

/**
 * Callback function called when WiFi scan is completed.
 * 
 * @param succeeded Indicates whether the scan succeeded or not.
 * @param list The list of available WiFi networks.
 */
void AppWIFI::scanCompleted(bool succeeded, BssList& list) {
    debug_i("AppWIFI::scanCompleted. Success: %d", succeeded);
    if (succeeded) {
        _networks.clear();
        for (size_t i = 0; i < list.count(); i++) {
            if (!list[i].hidden && list[i].ssid.length() > 0) {
                _networks.add(list[i]);
            }
        }
    }
    _networks.sort([](const BssInfo& a, const BssInfo& b) {return b.rssi - a.rssi;});
    _scanning = false;

    // make sure to trigger connect again cause otherwise the Wifi reconnect attempts may come to a stop
    if (_keepStaAfterScan)
        WifiStation.connect();
}

/**
 * @brief Clears the stored WiFi credentials and disconnects from the WiFi network.
 * 
 * This function resets the WiFi configuration to empty values and disconnects from the current WiFi network.
 * After calling this function, the connection status is set to IDLE.
 */
void AppWIFI::forgetWifi() {
    debug_i("AppWIFI::forget_wifi");
    WifiStation.config("", "");
    WifiStation.disconnect();
    _client_status = CONNECTION_STATUS::IDLE;
}

/**
 * @brief Initializes the AppWIFI class.
 * 
 * This function disables wifi sleep, enables WifiStation if it is not already enabled,
 * disables WifiAccessPoint if it is enabled, sets up the access point and station configurations,
 * registers callbacks for station disconnect, connect, and IP acquisition events,
 * and configures the WifiClient with static IP or DHCP based on the configuration.
 * 
 * If there is no access point to connect to, it starts its own access point and scans for available networks.
 * 
 * @note This function assumes that the app object is available and isFirstRun() returns the correct value.
 */
void AppWIFI::init() {

    // ESP SDK function to disable wifi sleep
    wifi_set_sleep_type(NONE_SLEEP_T);

    //don`t enable/disable again to save eeprom cycles
    if (!WifiStation.isEnabled()) {
        debug_i("AppWIFI::init enable WifiStation");
        WifiStation.enable(true, true);
    }

    if (WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI::init WifiAccessPoint disabled");
        WifiAccessPoint.enable(false, true);
    }

    _con_ctr = 0;

    if (app.isFirstRun()) {
        debug_i("AppWIFI::init initial run - setting up AP, ssid: ");
        String SSID=String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
        printf("%s",SSID);
        app.cfg.network.connection.mdnshostname = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
        app.cfg.network.ap.ssid = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
        app.cfg.save();
        WifiAccessPoint.setIP(_ApIP);
    }

    // register callbacks
    WifiEvents.onStationDisconnect(StationDisconnectDelegate(&AppWIFI::_STADisconnect, this));
    WifiEvents.onStationConnect(StationConnectDelegate(&AppWIFI::_STAConnected, this));
    WifiEvents.onStationGotIP(StationGotIPDelegate(&AppWIFI::_STAGotIP, this));


    if (WifiStation.getSSID() == "") {

        debug_i("AppWIFI::init no AP to connect to - start own AP");
        // No wifi to connect to - initialize AP
        startAp();

        // already scan for avaialble networks to speedup things later
        scan(false);

    } else {

        //configure WifiClient
        if (!app.cfg.network.connection.dhcp && !app.cfg.network.connection.ip.isNull()) {
            debug_i("AppWIFI::init setting static ip");
            if (WifiStation.isEnabledDHCP()) {
                debug_i("AppWIFI::init disabled dhcp");
                WifiStation.enableDHCP(false);
            }
            if (!(WifiStation.getIP() == app.cfg.network.connection.ip)
                    || !(WifiStation.getNetworkGateway() == app.cfg.network.connection.gateway)
                    || !(WifiStation.getNetworkMask() == app.cfg.network.connection.netmask)) {
                debug_i("AppWIFI::init updating ip configuration");
                WifiStation.setIP(app.cfg.network.connection.ip,app.cfg.network.connection.netmask,app.cfg.network.connection.gateway);
            }
        } else {
            debug_i("AppWIFI::init dhcp");
            if (!WifiStation.isEnabledDHCP()) {
                debug_i("AppWIFI::init enabling dhcp");
                WifiStation.enableDHCP(true);
            }
        }
    }
}

/**
 * @brief Connects to a Wi-Fi network.
 * 
 * @param ssid The SSID of the Wi-Fi network.
 * @param new_con Flag indicating whether it is a new connection or not.
 */
void AppWIFI::connect(String ssid, bool new_con /* = false */) {
    connect(ssid, "", new_con);
}

/**
 * @brief Connects to a Wi-Fi network with the specified SSID and password.
 * 
 * @param ssid The SSID of the Wi-Fi network to connect to.
 * @param pass The password of the Wi-Fi network.
 * @param new_con Flag indicating whether it is a new connection or not. Default is false.
 */
void AppWIFI::connect(String ssid, String pass, bool new_con /* = false */) {
    debug_i("AppWIFI::connect ssid %s newcon %d", ssid.c_str(), new_con);
    _con_ctr = 0;
    _new_connection = new_con;
    _client_status = CONNECTION_STATUS::CONNECTING;
    
    WifiStation.config(ssid, pass);
    WifiStation.connect();
}

/**
 * @brief Handles the disconnection of the station from the Wi-Fi network.
 *
 * This function is called when the station disconnects from the Wi-Fi network.
 * It checks the reason for disconnection and performs necessary actions based on the reason.
 * If the disconnection reason is either reaching the maximum connection retries or wrong password,
 * it sets the client status to ERROR and updates the client error message.
 * If a new connection is requested, it disconnects the station and configures it with empty SSID and password.
 * Otherwise, it scans for available networks and starts the access point.
 *
 * @param ssid The SSID of the Wi-Fi network.
 * @param bssid The MAC address of the Wi-Fi network.
 * @param reason The reason for disconnection.
 */
void AppWIFI::_STADisconnect(const String& ssid, MacAddress bssid, WifiDisconnectReason reason) {
    debug_i("AppWIFI::_STADisconnect reason - %i - counter %i", reason, _con_ctr);

    if (_con_ctr == DEFAULT_CONNECTION_RETRIES || WifiStation.getConnectionStatus() == eSCS_WrongPassword) {
        _client_status = CONNECTION_STATUS::ERROR;
        _client_err_msg = WifiStation.getConnectionStatusName();
        debug_i("AppWIFI::_STADisconnect err %s - new connection: %i", _client_err_msg.c_str(), _new_connection);
        if (_new_connection) {
            debug_i("AppWIFI::_STADisconnect - disconnecting station");
            WifiStation.disconnect();
            WifiStation.config("", "");
        } else {
            scan(true);
            startAp();
        }
    }
    _con_ctr++;
}

/**
 * Callback function called when a WiFi connection is established.
 * 
 * @param ssid The SSID of the Wi-Fi network.
 * @param bssid The MAC address of the access point.
 * @param channel The Wi-Fi channel used for the connection.
 */
void AppWIFI::_STAConnected(const String& ssid, MacAddress bssid, uint8_t channel) {
    debug_i("AppWIFI::_STAConnected SSID - %s", ssid.c_str());

    if(app.cfg.general.device_name!="") {
        debug_i("AppWIFI::connect setting hostname to %s", app.cfg.general.device_name.c_str());
        WifiStation.setHostname(app.cfg.general.device_name);
        app.cfg.network.connection.mdnshostname = app.cfg.general.device_name;
    }else{
        debug_i("no device name configured, setting hostname to default %s",String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id()));
        app.cfg.network.connection.mdnshostname = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
    }
    
    _con_ctr = 0;
    //mdnsHandler.start(); // start mDNS responderF
    app.onWifiConnected(ssid);
}

/**
 * @brief Callback function called when the WiFi station gets an IP address.
 * 
 * This function is called when the WiFi station successfully connects to a WiFi network
 * and obtains an IP address. It performs the necessary initialization steps and starts
 * the required services.
 * 
 * @param ip The IP address assigned to the WiFi station.
 * @param mask The subnet mask assigned to the WiFi station.
 * @param gateway The gateway IP address assigned to the WiFi station.
 */
void AppWIFI::_STAGotIP(IpAddress ip, IpAddress mask, IpAddress gateway) {
    debug_i("AppWIFI::_STAGotIP");
    _con_ctr = 0;
    _client_status = CONNECTION_STATUS::CONNECTED;

    // if we have a new connection, wait 90 seconds otherwise
    // disable the accesspoint mode directly
    if(_new_connection) {
        stopAp(90000);
    } else {
        stopAp(1000);
    }

    debug_i("AppWIFI::_STAGotIP - device_name %s mdnshostname %s", app.cfg.general.device_name.c_str(),app.cfg.network.connection.mdnshostname.c_str());
    if(app.cfg.network.connection.mdnshostname.length() > 0) {
        debug_i("AppWIFI::_STAGotIP - setting mdns hostname to %s", app.cfg.network.connection.mdnshostname.c_str());
    }

    mdnsHandler.start();
    String ipAddress=ip.toString();

    mdnsHandler.addHost(app.cfg.network.connection.mdnshostname, ipAddress, -1);

    if(app.cfg.network.mqtt.enabled) {
        app.mqttclient.start();
    }
}

/**
 * Stops the access point (AP) if it is enabled.
 * 
 * @param delay The delay in milliseconds before stopping the AP. If delay is greater than 0, the AP will be stopped after the specified delay.
 */
void AppWIFI::stopAp(int delay) {
    if (!WifiAccessPoint.isEnabled()) {
    	return;
    }

    if (delay > 0) {
    	debug_i("AppWIFI::stopAp delay %i", delay);
        _timer.initializeMs(delay, std::bind(&AppWIFI::stopAp, this, 0)).startOnce();
        return;
    }

    debug_i("AppWIFI::stopAp");
    debug_i("Disabling AP");
    _timer.stop();
    if (WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI::stopAp WifiAP disable");
        WifiAccessPoint.enable(false, false);
    }
}

/**
 * @brief Starts the Access Point (AP) for the AppWIFI class.
 * 
 * This function enables the AP and configures it with the provided SSID and password.
 * If the AP is already enabled, it does nothing.
 * 
 * @note This function assumes that the necessary configurations are already set in the `app.cfg.network.ap` structure.
 */
void AppWIFI::startAp() {
    //String ssid="rgbww test";
    debug_i("AppWIFI::startAp");
    debug_i("Enabling AP");
    if (!WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI:: WifiAP enable");
        WifiAccessPoint.enable(true, false);
        debug_i("AP enabled");
        //debug_i("AP SSID: %s", app.cfg.network.ap.ssid);
        if (app.cfg.network.ap.secured) {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, app.cfg.network.ap.password, AUTH_WPA2_PSK);
        } else {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, "", AUTH_OPEN);
            //WifiAccessPoint.config(ssid, "", AUTH_OPEN);
        }
    }
}

String AppWIFI::getMdnsHosts() {
    return mdnsHandler.getHosts();
}