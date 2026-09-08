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
    _scanning = false;
    _new_connection = false;
    _client_status = CONNECTION_STATUS::IDLE;
    _disconnectedAt = 0;
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
    debug_i("AppWIFI::scanCompleted: Success: %d, %d network(s) found", succeeded, list.count());
    if (succeeded) {
        _networks.clear();
        for (int i = 0; i < list.count(); i++) {
            if (!list[i].hidden && list[i].ssid.length() > 0) {
                _networks.add(list[i]);
            }
        }
    }
    _networks.sort([](const BssInfo& a, const BssInfo& b) { return b.rssi - a.rssi; });
    _scanning = false;

    // Check if configured target SSID was found in the scan results
    String targetSsid = WifiStation.getSSID();
    if (targetSsid.length() > 0) {
        bool foundTarget = false;
        for (int i = 0; i < _networks.count(); i++) {
            if (_networks[i].ssid.equals(targetSsid)) {
                debug_i("AppWIFI::scanCompleted: Target SSID '%s' found! (BSSID: %s, Ch: %d, RSSI: %d dBm)",
                        targetSsid.c_str(), _networks[i].bssid.toString().c_str(), _networks[i].channel,
                        _networks[i].rssi);
                foundTarget = true;
                break;
            }
        }
        if (!foundTarget && succeeded) {
            debug_i("AppWIFI::scanCompleted: Target SSID '%s' not visible in scan", targetSsid.c_str());
        }
    }

    // make sure to trigger connect again cause otherwise the Wifi reconnect attempts may come to a stop
    if (_keepStaAfterScan)
        WifiStation.connect();
}

void AppWIFI::forgetWifi() {
    debug_i("AppWIFI::forget_wifi");
    _reconnectTimer.stop();
    _dhcpTimer.stop();
    _disconnectedAt = 0;
    WifiStation.config("", "");
    WifiStation.disconnect();
    _client_status = CONNECTION_STATUS::IDLE;
}

void AppWIFI::init() {

    // ESP SDK function to disable wifi sleep
    wifi_set_sleep_type(NONE_SLEEP_T);

    // don`t enable/disable again to save eeprom cycles
    if (!WifiStation.isEnabled()) {
        debug_i("AppWIFI::init enable WifiStation");
        WifiStation.enable(true, true);
    }

    if (WifiAccessPoint.isEnabled()) {
        debug_i("AppWIFI::init WifiAccessPoint disabled");
        WifiAccessPoint.enable(false, true);
    }

    _disconnectedAt = 0;

    if (app.isFirstRun()) {
        debug_i("AppWIFI::init initial run - setting up AP");
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

        // configure WifiClient
        if (!app.cfg.network.connection.dhcp && !app.cfg.network.connection.ip.isNull()) {
            debug_i("AppWIFI::init setting static ip");
            if (WifiStation.isEnabledDHCP()) {
                debug_i("AppWIFI::init disabled dhcp");
                WifiStation.enableDHCP(false);
            }
            if (!(WifiStation.getIP() == app.cfg.network.connection.ip) ||
                !(WifiStation.getNetworkGateway() == app.cfg.network.connection.gateway) ||
                !(WifiStation.getNetworkMask() == app.cfg.network.connection.netmask)) {
                debug_i("AppWIFI::init updating ip configuration");
                WifiStation.setIP(app.cfg.network.connection.ip, app.cfg.network.connection.netmask,
                                  app.cfg.network.connection.gateway);
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
    _disconnectedAt = millis();
    _reconnectTimer.stop();
    _dhcpTimer.stop();
    _new_connection = new_con;
    _client_status = CONNECTION_STATUS::CONNECTING;
    WifiStation.config(ssid, pass);
    WifiStation.connect();
}

void AppWIFI::_STADisconnect(const String& ssid, MacAddress bssid, WifiDisconnectReason reason) {
    debug_i("AppWIFI::_STADisconnect: SSID '%s', BSSID: %s, reason: %d (%s)", ssid.c_str(), bssid.toString().c_str(),
            reason, WifiEvents.getDisconnectReasonDesc(reason).c_str());

    _dhcpTimer.stop();

    if (_client_status != CONNECTION_STATUS::ERROR) {
        _client_status = CONNECTION_STATUS::CONNECTING;
    }

    if (_disconnectedAt == 0) {
        _disconnectedAt = millis();
    }

    // Start recurring reconnect timer (every 30s) if not already running
    if (!_reconnectTimer.isStarted()) {
        debug_i("AppWIFI::_STADisconnect starting reconnect timer (%d ms)", WIFI_RECONNECT_INTERVAL_MS);
        _reconnectTimer.initializeMs(WIFI_RECONNECT_INTERVAL_MS, TimerDelegate(&AppWIFI::onReconnectTimer, this))
            .start();
    }

    // If this was a new connection attempt and the disconnect reason was wrong password, set error state and restart AP
    if (_new_connection && WifiStation.getConnectionStatus() == eSCS_WrongPassword) {
        _client_status = CONNECTION_STATUS::ERROR;
        _client_err_msg = WifiStation.getConnectionStatusName();
        debug_i("AppWIFI::_STADisconnect wrong password on new connection - disconnecting station");
        _reconnectTimer.stop();
        _dhcpTimer.stop();
        WifiStation.disconnect();
        WifiStation.config("", "");
        startAp();
    }
}

void AppWIFI::onReconnectTimer() {
    if (_client_status == CONNECTION_STATUS::CONNECTED) {
        _reconnectTimer.stop();
        _dhcpTimer.stop();
        return;
    }

    // Check if we have been disconnected for longer than fallback delay (10 min)
    if (_disconnectedAt > 0) {
        unsigned long elapsed = millis() - _disconnectedAt;
        if (elapsed >= WIFI_AP_FALLBACK_DELAY_MS && !WifiAccessPoint.isEnabled()) {
            debug_w("AppWIFI::onReconnectTimer: Disconnected for %lu s (threshold: %lu s), activating fallback AP",
                    elapsed / 1000, (unsigned long)(WIFI_AP_FALLBACK_DELAY_MS / 1000));
            startAp();
        } else if (!WifiAccessPoint.isEnabled()) {
            unsigned long remaining = (WIFI_AP_FALLBACK_DELAY_MS - elapsed) / 1000;
            debug_i("AppWIFI::onReconnectTimer: Disconnected for %lu s (AP fallback in %lu s)", elapsed / 1000,
                    remaining);
        } else {
            debug_i("AppWIFI::onReconnectTimer: Disconnected for %lu s (fallback AP active)", elapsed / 1000);
        }
    }

    // If clients are connected to our SoftAP, skip scanning to avoid channel hopping that disconnects clients
    if (WifiAccessPoint.isEnabled()) {
        uint8_t clients = wifi_softap_get_station_num();
        if (clients > 0) {
            debug_i("AppWIFI::onReconnectTimer: Skipping periodic WiFi scan (%u client(s) connected to AP)", clients);
            return;
        }
    }

    // Trigger channel scan across all channels to find AP if not already scanning and not waiting for DHCP
    if (!_scanning && !_dhcpTimer.isStarted()) {
        debug_i("AppWIFI::onReconnectTimer: Triggering periodic WiFi scan");
        scan(true);
    }
}

void AppWIFI::_STAConnected(const String& ssid, MacAddress bssid, uint8_t channel) {
    debug_i("AppWIFI::_STAConnected: Associated with SSID '%s' (BSSID: %s, Ch: %d)", ssid.c_str(),
            bssid.toString().c_str(), channel);

    app.onWifiConnected(ssid);

    // If using DHCP and we don't have an IP yet, start DHCP timeout watchdog
    if (WifiStation.isEnabledDHCP() && _client_status != CONNECTION_STATUS::CONNECTED) {
        debug_i("AppWIFI::_STAConnected: Starting DHCP timeout watchdog (%d ms)", WIFI_DHCP_TIMEOUT_MS);
        _dhcpTimer.initializeMs(WIFI_DHCP_TIMEOUT_MS, TimerDelegate(&AppWIFI::onDhcpTimeout, this)).startOnce();
    }
}

void AppWIFI::onDhcpTimeout() {
    if (_client_status == CONNECTION_STATUS::CONNECTED) {
        return;
    }
    debug_w("AppWIFI::onDhcpTimeout: No IP received within %d ms after association! Resetting station to retry DHCP...",
            WIFI_DHCP_TIMEOUT_MS);
    WifiStation.disconnect();
    WifiStation.connect();
}

void AppWIFI::_STAGotIP(IpAddress ip, IpAddress mask, IpAddress gateway) {
    debug_i("AppWIFI::_STAGotIP: Connected! IP: %s, Mask: %s, GW: %s", ip.toString().c_str(), mask.toString().c_str(),
            gateway.toString().c_str());
    _disconnectedAt = 0;
    _reconnectTimer.stop();
    _dhcpTimer.stop();
    _client_status = CONNECTION_STATUS::CONNECTED;

    // if we have a new connection, wait 90 seconds otherwise
    // disable the accesspoint mode directly
    if (_new_connection) {
        debug_i("AppWIFI::_STAGotIP: Disabling AP in 90 s (new connection)");
        stopAp(90000);
    } else {
        debug_i("AppWIFI::_STAGotIP: Disabling AP in 1 s");
        stopAp(1000);
    }

    if (app.cfg.network.mqtt.enabled) {
        app.mqttclient.start();
    }
}

void AppWIFI::stopAp(int delay) {
    if (!WifiAccessPoint.isEnabled()) {
        return;
    }

    if (delay > 0) {
        debug_i("AppWIFI::stopAp: Scheduling AP disable in %d ms", delay);
        _timer.initializeMs(delay, std::bind(&AppWIFI::stopAp, this, 0)).startOnce();
        return;
    }

    debug_i("AppWIFI::stopAp: Disabling AP now");
    _timer.stop();
    if (WifiAccessPoint.isEnabled()) {
        WifiAccessPoint.enable(false, false);
    }
}

void AppWIFI::startAp() {
    debug_i("AppWIFI::startAp: Enabling fallback AP with SSID '%s'", app.cfg.network.ap.ssid.c_str());
    if (!WifiAccessPoint.isEnabled()) {
        WifiAccessPoint.enable(true, false);
        if (app.cfg.network.ap.secured) {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, app.cfg.network.ap.password, AUTH_WPA2_PSK);
        } else {
            WifiAccessPoint.config(app.cfg.network.ap.ssid, "", AUTH_OPEN);
        }
    }
}
