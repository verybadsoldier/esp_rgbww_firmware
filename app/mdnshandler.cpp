#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>
#include "application.h"

// ...

mdnsHandler::mdnsHandler(){};

void mdnsHandler::start()
{
	using namespace mDNS;
	static mDNS::Responder responder;

	static LEDControllerAPIService ledControllerAPIService;
	static LEDControllerWebAppService ledControllerWebAppService;
	static LEDControllerWSService ledControllerWSService;

	//start the mDNS responder with the configured services, using the configured hostname
	{
		AppConfig::Network network(*app.cfg);
		responder.begin(network.mdns.getName().c_str());
	} // end of ConfigDB network context
	  //responder.begin(app.cfg.network.connection.mdnshostname.c_str());
	responder.addService(ledControllerAPIService);
	responder.addService(ledControllerWebAppService);
	responder.addService(ledControllerWSService);

	//create an empty hosts array to store recieved host entries
	hosts = hostsDoc.createNestedArray("hosts");

	//serch for the esprgbwwAIP service. This is used in the onMessage handler to filter out unwanted messages.
	//to fulter for a number of services, this would have to be adapted a bit.
	setSearchName(F("esprgbwwAPI.") + service);

	//query mDNS at regular intervals
	_mdnsSearchTimer.setCallback(mdnsHandler::sendSearchCb, this);
	_mdnsSearchTimer.setIntervalMs(_mdnsTimerInterval);
	_mdnsSearchTimer.startOnce();
	mDNS::server.addHandler(*this);
}

/**
 * @brief Handles an mDNS message.
 *
 * This function is responsible for processing an mDNS message and extracting the required information from it.
 * It checks if the message is a reply and if it contains a PTR record with a matching name.
 * If the message contains TXT and A records, it extracts the instance, service, and IP address information.
 * It then creates a JSON object with the extracted information and serializes it.
 *
 * @param message The mDNS message to be handled.
 * @return True if the message was successfully handled, false otherwise.
 */
bool mdnsHandler::onMessage(mDNS::Message& message)
{
#ifdef DEBUG_MDNS
	debug_i("onMessage handler called");
#endif
	using namespace mDNS;

	// Check if we're interested in this message
	if(!message.isReply()) {
#ifdef DEBUG_MDNS
		debug_i("Ignoring query");
#endif
		return false;
	}

	//mDNS::printMessage(Serial, message);

	auto answer = message[mDNS::ResourceType::SRV];
	if(answer == nullptr) {
#ifdef DEBUG_MDNS
		debug_i("Ignoring message: no SRV record");
#endif
		return false;
	}
	//mDNS::printMessage(Serial, message);
	String answerName = String(answer->getName());
#ifdef DEBUG_MDNS
	debug_i("\nanswerName: %s\nsearchName: %s", answerName.c_str(), searchName.c_str());
#endif
	if(answerName != searchName) {
		//debug_i("Ignoring message: Name doesn't match");
		return false;
	}
#ifdef DEBUG_MDNS
	debug_i("Found matching SRV record");
#endif
	// Extract our required information from the message
	struct {
		String hostName;
		IpAddress ipAddr;
		int ttl;
	} info;

	answer = message[mDNS::ResourceType::A];
	if(answer != nullptr) {
		info.hostName = String(answer->getName());
		info.hostName = info.hostName.substring(0, info.hostName.lastIndexOf(".local"));
		info.ipAddr = String(answer->getRecordString());
		info.ttl = answer->getTtl();
	}
#ifdef DEBUG_MDNS
	debug_i("found Host %s with IP %s and TTL %i", info.hostName.c_str(), info.ipAddr.toString().c_str(), info.ttl);
#endif

	addHost(info.hostName, info.ipAddr.toString(), info.ttl); //add host to list
	return true;
}

/**
 * @brief Sends a search request for the specified services using mDNS.
 *
 * This function sends a search request for the services specified in the `services` array using mDNS.
 * It iterates through each service in the array and calls the `mDNS::server.search()` function to perform the search.
**/
void mdnsHandler::sendSearch()
{
	// Search for the service
	bool ok = mDNS::server.search(service);
#ifdef DEBUG_MDNS
	debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");
#endif

	//restart the timer
	_mdnsSearchTimer.startOnce();
	for(size_t i = 0; i < hosts.size(); ++i) {
		JsonVariant host = hosts[i];
		if((int)host[F("ttl")] == -1) {
			continue;
		}
		host[F("ttl")] = (int)host[F("ttl")] - _mdnsTimerInterval / 1000;
		if(host[F("ttl")].as<int>() < 0) {
			debug_i("Removing host %s from list", host[F("hostname")].as<const char*>());

			// notify websocket clients
			JsonRpcMessage msg(F("removed_host"));
			JsonObject root = msg.getParams();
			root[F("hostname")] = host[F("hostname")];
			String jsonStr = Json::serialize(msg.getRoot());

			app.wsBroadcast(jsonStr);
			hosts.remove(i);
			--i;
		}

		if(host[F("hostname")] == null) {
			hosts.remove(i);
		}
	}
}

void mdnsHandler::sendSearchCb(void* pTimerArg)
{
#ifdef DEBUG_MDNS
	debug_i("sendSearchCb called");
#endif
	mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
	pThis->sendSearch();
}

void mdnsHandler::addHost(const String& hostname, const String& ip_address, int ttl)
{
/*
     * ToDo: this is currently an ugly implementation, as it needs both, the hosts 
     * JsonArray and the app.cfg.network.mdnsHosts string, wasting memory
     * however, it's the most convenient way to pass the list from here to the webserver
     * without having to pass object references around
     */
#ifdef DEBUG_MDNS
	debug_i("Adding host %s with IP %s and ttl %i", hostname.c_str(), ip_address.c_str(), ttl);
#endif
	String _hostname, _ip_address;
	int _ttl;
	_hostname = hostname;
	_ip_address = ip_address;
	_ttl = ttl;

	bool knownHost = false;
	for(JsonVariant host : hosts) {
		if(host[F("hostname")] == _hostname && host[F("ip_address")] == _ip_address) {
#ifdef DEBUG_MDNS
			debug_i("Hostname %s already in list", _hostname.c_str());
#endif
			if(_ttl != -1)
				host[F("ttl")] = _ttl; //reset ttl
			knownHost = true;
			break;
		}
	}

	if(!knownHost) {
		JsonObject newHost = hosts.createNestedObject();

		newHost[F("hostname")] = _hostname;
		newHost[F("ip_address")] = _ip_address;
		newHost[F("ttl")] = _ttl;
		String newHostString;
		serializeJsonPretty(newHost, newHostString);
#ifdef DEBUG_MDNS
		debug_i("new host: %s", newHostString.c_str());
#endif

		JsonRpcMessage msg(F("new_host"));
		JsonObject root = msg.getParams();
		root.set(newHost);
		String jsonStr = Json::serialize(msg.getRoot());

		app.wsBroadcast(jsonStr);
	}
}

String mdnsHandler::getHosts()
{
	String mdnsHosts;
	serializeJson(hostsDoc, mdnsHosts);
	//Serial.printf("\nnew hosts document:\n %s",mdnsHosts.c_str());
	return mdnsHosts;
}