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
#define DNS_PORT 53

mdnsHandler mdnsHandler;

DnsServer dnsServer;

/**
 * @brief Constructor for the AppWIFI class.
 * 
 * Initializes the member variables of the AppWIFI class.
 */
AppWIFI::AppWIFI()
{
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
BssList AppWIFI::getAvailableNetworks()
{
	return _networks;
}

/**
 * Scans for available Wi-Fi networks.
 * 
 * @param connectAfterScan Flag indicating whether to connect to a network after the scan is completed.
 */
void AppWIFI::scan(bool connectAfterScan)
{
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
void AppWIFI::scanCompleted(bool succeeded, BssList& list)
{
	if(succeeded) {
		debug_i("AppWIFI::scanCompleted");
		_networks.clear();
		for(size_t i = 0; i < list.count(); i++) {
			if(!list[i].hidden && list[i].ssid.length() > 0) {
				_networks.add(list[i]);
			}
		}
	}else{
		debug_e("wifi scan failed");
	}
	// TODO add wsBroadcast of available networks
	_networks.sort([](const BssInfo& a, const BssInfo& b) { return b.rssi - a.rssi; });
	_scanning = false;

	// make sure to trigger connect again cause otherwise the Wifi reconnect attempts may come to a stop
	if(_keepStaAfterScan)
		WifiStation.connect();
}

/**
 * @brief Clears the stored WiFi credentials and disconnects from the WiFi network.
 * 
 * This function resets the WiFi configuration to empty values and disconnects from the current WiFi network.
 * After calling this function, the connection status is set to IDLE.
 */
void AppWIFI::forgetWifi()
{
	debug_i("AppWIFI::forget_wifi");
	WifiStation.config("", "");
	WifiStation.disconnect();
	if(!WifiAccessPoint.isEnabled()) {
		startAp();
	}
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
void AppWIFI::init()
{
	debug_i("AppWIFI::init");
	// ESP SDK function to disable  sleep
	wifi_set_sleep_type(NONE_SLEEP_T);

	//don`t enable/disable again to save eeprom cycles
	if(!WifiStation.isEnabled()) {
		debug_i("AppWIFI::init enable WifiStation");
		WifiStation.enable(true, true);
	}

	WifiStation.enable(true);
	if(WifiAccessPoint.isEnabled()) {
		debug_i("AppWIFI::init WifiAccessPoint disabled");
		WifiAccessPoint.enable(false, true);
	}

	_con_ctr = 0;
	debug_i("AppWIFI::init\n    station %s\n    AP      %s", WifiStation.isEnabled()? "enabled" : "disabled", WifiAccessPoint.isEnabled()? "enabled" : "disabled");
	// ConfigDB adapt
	if(app.isFirstRun()) {
		debug_i("AppWIFI::init initial run - setting up AP, ssid: ");
		String SSID = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
		printf("%s", SSID);

		AppConfig::Network network(*app.cfg);
		if(auto networkUpdate = network.update()) {
			networkUpdate.mdns.setName(String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id()));
			networkUpdate.ap.setSsid(String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id()));
		} //this should never fail as it is during system startup, there are no asynchronous things here. I think
		WifiAccessPoint.setIP(_ApIP);
	}

	// register callbacks
	debug_i("AppWIFI::init register callbacks");
	WifiEvents.onStationDisconnect(StationDisconnectDelegate(&AppWIFI::_STADisconnect, this));
	WifiEvents.onStationConnect(StationConnectDelegate(&AppWIFI::_STAConnected, this));
	WifiEvents.onStationGotIP(StationGotIPDelegate(&AppWIFI::_STAGotIP, this));

	if(WifiStation.getSSID() == "") {
		debug_i("AppWIFI::init no AP to connect to - start own AP");
		// No wifi to connect to - initialize AP
		startAp();

		// already scan for avaialble networks to speedup things later
		scan(false);

	} else {

		//configure WifiClient
		{
			AppConfig::Network network(*app.cfg);
			if(!network.connection.getDhcp() && !network.connection.getIp().length() == 0) {
				debug_i("AppWIFI::init setting static ip");
				if(WifiStation.isEnabledDHCP()) {
					// dhcp is configured off but currently enabled - disable it
					debug_i("AppWIFI::init disabled dhcp");
					WifiStation.enableDHCP(false);
				}
				if(!(WifiStation.getIP() == network.connection.getIp()) ||
				   !(WifiStation.getNetworkGateway() == network.connection.getGateway()) ||
				   !(WifiStation.getNetworkMask() == network.connection.getNetmask())) {
					debug_i("AppWIFI::init updating ip configuration");
					WifiStation.setIP(network.connection.getIp(), network.connection.getNetmask(),
									  network.connection.getGateway());
				}
			} else {
				debug_i("AppWIFI::init dhcp");
				if(!WifiStation.isEnabledDHCP()) {
					debug_i("AppWIFI::init enabling dhcp");
					WifiStation.enableDHCP(true);
				}
			}
		} // end ConfigDB network context
		debug_i("AppWifi::init - triggering wifi connect");
		WifiStation.connect();
	}
}

/**
 * @brief Connects to a Wi-Fi network.
 * 
 * @param ssid The SSID of the Wi-Fi network.
 * @param new_con Flag indicating whether it is a new connection or not.
 */
void AppWIFI::connect(String ssid, bool new_con /* = false */)
{
	connect(ssid, "", new_con);
}

/**
 * @brief Connects to a Wi-Fi network with the specified SSID and password.
 * 
 * @param ssid The SSID of the Wi-Fi network to connect to.
 * @param pass The password of the Wi-Fi network.
 * @param new_con Flag indicating whether it is a new connection or not. Default is false.
 */
void AppWIFI::connect(String ssid, String pass, bool new_con /* = false */)
{
	debug_i("AppWIFI::connect ssid %s newcon %d", ssid.c_str(), new_con);
	_con_ctr = 0;
	_new_connection = new_con;
	_client_status = CONNECTION_STATUS::CONNECTING;

	debug_i("connecting to %s using %s", ssid.c_str(), pass.c_str());
	WifiStation.config(ssid, pass);
	WifiStation.connect();
	broadcastWifiStatus(F("Connecting to WiFi"));
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
void AppWIFI::_STADisconnect(const String& ssid, MacAddress bssid, WifiDisconnectReason reason)
{
	debug_i("AppWIFI::_STADisconnect reason - %i - counter %i", reason, _con_ctr);

	if(_con_ctr == DEFAULT_CONNECTION_RETRIES || WifiStation.getConnectionStatus() == eSCS_WrongPassword) {
		_client_status = CONNECTION_STATUS::ERROR;
		_client_err_msg = WifiStation.getConnectionStatusName();
		debug_i("AppWIFI::_STADisconnect err %s - new connection: %i", _client_err_msg.c_str(), _new_connection);
		if(_new_connection) {
			debug_i("AppWIFI::_STADisconnect - disconnecting station");
			WifiStation.disconnect();
			WifiStation.config("", "");
		} else {
			scan(true);
			startAp();
		}
	}
	debug_i("AppWIFI::_STADisconnect - _client_err_msg: %s", _client_err_msg.c_str());
	broadcastWifiStatus(_client_err_msg);
	_con_ctr++;
}

/**
 * Callback function called when a WiFi connection is established.
 * 
 * primarily sets the hostname, if it's configured in ConfigDB, that is used
 * otherwise, the default hostname is set to "RGBWW-<chipid>"
 * the hostname is used for mDNS and other network services
 * 
 * the function also broadcasts the wifi status to the clients
 * 
 * @param ssid The SSID of the Wi-Fi network.
 * @param bssid The MAC address of the access point.
 * @param channel The Wi-Fi channel used for the connection.
 */
void AppWIFI::_STAConnected(const String& ssid, MacAddress bssid, uint8_t channel)
{
	debug_i("AppWIFI::_STAConnected SSID - %s", ssid.c_str());
	{
		AppConfig::General general(*app.cfg);
		String device_name = general.getDeviceName();
		if(device_name == "") {
			device_name = String(DEFAULT_AP_SSIDPREFIX) + String(system_get_chip_id());
			debug_i("no device name configured, building default name");
		}
		debug_i("AppWIFI::connect setting hostname to %s", device_name.c_str());
		WifiStation.setHostname(device_name);
		{
			AppConfig::Network::OuterUpdater network(*app.cfg);
			network.mdns.setName(device_name);
		} // end ConfigDB network updater context
	}	 // end ConfigDB general context
	broadcastWifiStatus(F("Connected to WiFi"));
	_con_ctr = 0;
	// wifi cstation connected
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
void AppWIFI::_STAGotIP(IpAddress ip, IpAddress mask, IpAddress gateway)
{
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

	{
		AppConfig::General general(*app.cfg);
		AppConfig::Network network(*app.cfg);
		debug_i("AppWIFI::_STAGotIP - device_name %s mdnshostname %s", general.getDeviceName().c_str(),
				network.mdns.getName().c_str());
		if(network.mdns.getName().length() > 0) {
			debug_i("AppWIFI::_STAGotIP - setting mdns hostname to %s", network.mdns.getName().c_str());
		}
	} //end ConfigDB general and network context

	mdnsHandler.start();
	String ipAddress = ip.toString();

	{
		AppConfig::Network network(*app.cfg);
		if(network.connection.getDhcp()) {
			ipAddress = "dhcp";
		}
		mdnsHandler.addHost(network.mdns.getName(), ipAddress, -1);

		broadcastWifiStatus();

		if(network.mqtt.getEnabled()) {
			app.mqttclient.start();
		}
	} // end ConfigDB network context
}

/**
 * Stops the access point (AP) if it is enabled.
 * 
 * @param delay The delay in milliseconds before stopping the AP. If delay is greater than 0, the AP will be stopped after the specified delay.
 */
void AppWIFI::stopAp(int delay)
{
	if(!WifiAccessPoint.isEnabled()) {
		return;
	}

	if(delay > 0) {
		debug_i("AppWIFI::stopAp delay %i", delay);
		_timer.initializeMs(delay, std::bind(&AppWIFI::stopAp, this, 0)).startOnce();
		return;
	}

	debug_i("AppWIFI::stopAp");
	debug_i("Disabling AP");
	_timer.stop();
	if(WifiAccessPoint.isEnabled()) {
		debug_i("AppWIFI::stopAp WifiAP disable");
		WifiAccessPoint.enable(false, false);
	}
	broadcastWifiStatus(F("AP stopping"));
}

/**
 * @brief Starts the Access Point (AP) for the AppWIFI class.
 * 
 * This function enables the AP and configures it with the provided SSID and password.
 * If the AP is already enabled, it does nothing.
 * 
 * @note This function assumes that the necessary configurations are already set in the `app.cfg.network.ap` structure.
 */
void AppWIFI::startAp()
{
	//String ssid="rgbww test";
	debug_i("AppWIFI::startAp");
	debug_i("Enabling AP");
	if(!WifiAccessPoint.isEnabled()) {
		debug_i("AppWIFI:: WifiAP enable");
		WifiAccessPoint.enable(true, false);
		debug_i("AP enabled");
		//debug_i("AP SSID: %s", app.cfg.network.ap.ssid);
		{
			AppConfig::Network network(*app.cfg);
			if(network.ap.getSecured()) {
				WifiAccessPoint.config(network.ap.getSsid(), network.ap.getPassword(), AUTH_WPA2_PSK);
			} else {
				WifiAccessPoint.config(network.ap.getSsid(), "", AUTH_OPEN);
			}
		} // end AppConfig network context
	}
	//start dns server for captive portal
	dnsServer.start(DNS_PORT, "*", WifiAccessPoint.getIP());
	broadcastWifiStatus(F("AP started"));
}

String AppWIFI::getMdnsHosts()
{
	return mdnsHandler.getHosts();
}

/**
 * @brief Broadcasts the WiFi status to all connected clients.
 * 
 * This function broadcasts the WiFi status to all connected clients.
 * It creates a JSON-RPC message with the WiFi status and broadcasts it to all connected clients.
 */
void AppWIFI::broadcastWifiStatus(String message)
{
	if(WifiStation.isConnected() || WifiAccessPoint.isEnabled()) {
		JsonRpcMessage msg(F("wifi_status"));
		JsonObject root = msg.getParams();

		if(message != "") {
			root[F("message")] = message;
		}

		JsonObject station = root.createNestedObject(F("station"));

		station[F("connected")] = WifiStation.isConnected();
		station[F("ssid")] = WifiStation.getSSID();
		station[F("dhcp")] = WifiStation.isEnabledDHCP();
		station[F("ip")] = WifiStation.getIP().toString();
		station[F("netmask")] = WifiStation.getNetworkMask().toString();
		station[F("gateway")] = WifiStation.getNetworkGateway().toString();
		station[F("mac")] = WifiStation.getMAC();

		JsonObject ap = root.createNestedObject("ap");

		ap[F("enabled")] = WifiAccessPoint.isEnabled();
		ap[F("ssid")] = WifiAccessPoint.getSSID();
		ap[F("ip")] = WifiAccessPoint.getIP().toString();

		debug_i("rpc: root =%s", Json::serialize(root).c_str());
		debug_i("rpc: msg =%s", Json::serialize(msg.getRoot()).c_str());

		String jsonStr = Json::serialize(msg.getRoot());

		app.wsBroadcast(jsonStr);
	}
}

void AppWIFI::broadcastWifiStatus()
{
	broadcastWifiStatus("");
}