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
    _ApIP = IPAddress(String(DEFAULT_AP_IP));
    _client_err_msg = "";
    _con_ctr = 0;
    _scanning = false;
    _dns_active = false;
    _new_connection = false;
    _client_status = CONNECTION_STATUS::IDLE;
}

BssList AppWIFI::getAvailableNetworks() {
    return _networks;
}

void AppWIFI::scan() {
    _scanning = true;
    WifiStation.startScan(std::bind(&AppWIFI::scanCompleted, this, _1, _2));
}

void AppWIFI::scanCompleted(bool succeeded, BssList list) {
    debug_i("AppWIFI::scanCompleted. Success: %d", succeeded);
    if (succeeded) {
        _networks.clear();
        for (int i = 0; i < list.count(); i++) {
            if (!list[i].hidden && list[i].ssid.length() > 0) {
                _networks.add(list[i]);
            }
        }
    }
    _networks.sort([](const BssInfo& a, const BssInfo& b) {return b.rssi - a.rssi;});
    _scanning = false;
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
        debug_i("AppWIFI::init initial run - setting up AP");
        app.cfg.network.connection.mdnshostname = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
        app.cfg.network.ap.ssid = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
        app.cfg.save();
        WifiAccessPoint.setIP(_ApIP);
    }

    // register callbacks
    WifiEvents.onStationDisconnect(std::bind(&AppWIFI::_STADisconnect, this, _1, _2, _3, _4));
    WifiEvents.onStationConnect(std::bind(&AppWIFI::_STAConnected, this, _1, _2, _3, _4));
    WifiEvents.onStationGotIP(std::bind(&AppWIFI::_STAGotIP, this, _1, _2, _3));


    if (WifiStation.getSSID() == "") {

        debug_i("AppWIFI::init no AP to connect to - start own AP");
        // No wifi to connect to - initialize AP
        startAp();

        // already scan for avaialble networks to speedup things later
        scan();

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

void AppWIFI::_STADisconnect(String ssid, uint8_t ssid_len, uint8_t bssid[6], uint8_t reason) {
    debug_i("AppWIFI::_STADisconnect reason - %i - counter %i", reason, _con_ctr);
    if (_con_ctr >= DEFAULT_CONNECTION_RETRIES || WifiStation.getConnectionStatus() == eSCS_WrongPassword) {
        _client_status = CONNECTION_STATUS::ERROR;
        _client_err_msg = WifiStation.getConnectionStatusName();
        debug_i("AppWIFI::_STADisconnect err %s", _client_err_msg.c_str());
        if (_new_connection) {
            WifiStation.disconnect();
            WifiStation.config("", "");
        } else {
            scan();
            startAp();
        }
        _con_ctr = 0;
        return;
    }
    _con_ctr++;
}

void AppWIFI::_STAConnected(String ssid, uint8_t ssid_len, uint8_t bssid[6], uint8_t reason) {
    debug_i("AppWIFI::_STAConnected reason - %i", reason);

    app.onWifiConnected(ssid);
}

void AppWIFI::_STAGotIP(IPAddress ip, IPAddress mask, IPAddress gateway) {
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
    }

    debug_i("AppWIFI::stopAp");
    debug_i("Disabling AP and DNS server");
    _timer.stop();
    if (WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI::stopAp WifiAP disable");
        WifiAccessPoint.enable(false, false);
    }
    if (_dns_active) {
        debug_i("AppWIFI::stopAp DNS disable");
        _dns.close();
    }
}

void AppWIFI::startAp() {
    byte DNS_PORT = 53;
    debug_i("AppWIFI::startAp");
    debug_i("Enabling AP and DNS server");
    if (!WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI:: WifiAP enable");
        WifiAccessPoint.enable(true, false);
        if (app.cfg.network.ap.secured) {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, app.cfg.network.ap.password, AUTH_WPA2_PSK);
        } else {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, "", AUTH_OPEN);
        }
    }
    if (!_dns_active) {
        debug_i("AppWIFI:: DNS enable");
        _dns_active = true;
        _dns.setErrorReplyCode(DNSReplyCode::NoError);
        _dns.start(DNS_PORT, "*", _ApIP);
    }
}
