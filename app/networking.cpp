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

AppWIFI::AppWIFI() {
    _ApIP = IpAddress(String(DEFAULT_AP_IP));
    _client_err_msg = "";
    _con_ctr = 0;
    _scanning = false;
    _new_connection = false;
    _client_status = CONNECTION_STATUS::IDLE;
}

BssList AppWIFI::getAvailableNetworks() {
    return _networks;
}

void AppWIFI::scan(bool connectAfterScan) {
    _scanning = true;
    _keepStaAfterScan = connectAfterScan;
    WifiStation.startScan(ScanCompletedDelegate(&AppWIFI::scanCompleted, this));
}

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

void AppWIFI::forgetWifi() {
    debug_i("AppWIFI::forget_wifi");
    WifiStation.config("", "");
    WifiStation.disconnect();
    _client_status = CONNECTION_STATUS::IDLE;
}

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

void AppWIFI::connect(String ssid, bool new_con /* = false */) {
    connect(ssid, "", new_con);
}

void AppWIFI::connect(String ssid, String pass, bool new_con /* = false */) {
    debug_i("AppWIFI::connect ssid %s newcon %d", ssid.c_str(), new_con);
    _con_ctr = 0;
    _new_connection = new_con;
    _client_status = CONNECTION_STATUS::CONNECTING;
    WifiStation.config(ssid, pass);
    WifiStation.connect();
}

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

void AppWIFI::_STAConnected(const String& ssid, MacAddress bssid, uint8_t channel) {
    debug_i("AppWIFI::_STAConnected SSID - %s", ssid.c_str());

    _con_ctr = 0;
    app.onWifiConnected(ssid);
}

void AppWIFI::_STAGotIP(IpAddress ip, IpAddress mask, IpAddress gateway) {
    debug_i("AppWIFI::_STAGotIP");
    _con_ctr = 0;
    _client_status = CONNECTION_STATUS::CONNECTED;

    // if we have a new connection, wait 90 seconds oterhwise
    // disable the accesspoint mode directly
    if(_new_connection) {
        stopAp(90000);
    } else {
        stopAp(1000);
    }

    if(app.cfg.network.mqtt.enabled) {
        app.mqttclient.start();
    }
}

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