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

	//start the mDNS responder with the configured services, using the configured hostname
	{
		AppConfig::Network network(*app.cfg);
		responder.begin(network.mdns.getName().c_str());
	} // end of ConfigDB network context
	  //responder.begin(app.cfg.network.connection.mdnshostname.c_str());
	responder.addService(ledControllerAPIService);
	
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
		unsigned int ttl;
		unsigned int ID;
	} info;

	answer = message[mDNS::ResourceType::A];
	if(answer != nullptr) {
		info.hostName = String(answer->getName());
		info.hostName = info.hostName.substring(0, info.hostName.lastIndexOf(".local"));
		info.ipAddr = String(answer->getRecordString());
		info.ttl = answer->getTtl();
	}
	
	answer = message[mDNS::ResourceType::TXT];
		if(answer!=nullptr){
			mDNS::Resource::TXT txt(*answer);
			//mDNS::printAnswer(Serial, *answer);
			info.ID=txt["id"].toInt();
		}
#ifdef DEBUG_MDNS
	debug_i("found Host %s with ID %i, IP %s and TTL %i", info.hostName.c_str(),info.ID, info.ipAddr.toString().c_str(), info.ttl);
#endif

	addHost(info.hostName, info.ipAddr.toString(), info.ttl, info.ID); //add host to list
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

void mdnsHandler::sendSearchCb(void* pTimerArg) {
#ifdef DEBUG_MDNS
    debug_i("sendSearchCb called");
#endif
    mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
    pThis->sendSearch();
}

void mdnsHandler::addHost(const String& hostname, const String& ip_address, int ttl, unsigned int id)
{
	/*
	* ToDo: to avoid getting faulty data (such as the same host showing up with different IDs), 
	* I may have to implement a CRC mechanism for the mDNS data
	*/
	if(id==1) return; //invalid ID
#ifdef DEBUG_MDNS
	debug_i("Adding host %s with IP %s and ttl %i", hostname.c_str(), ip_address.c_str(), ttl);
#endif
	String _hostname, _ip_address;
	int _ttl;
	unsigned int _id;
	_hostname = hostname;
	_ip_address = ip_address;
	_ttl = ttl;
	_id = id;
	JsonObject newHost;
	String hostString;
	JsonVariant host;

	bool knownHost =false;
	bool updateHost = false;

	String hostsString;
	serializeJsonPretty(hostsDoc, hostString);
	
	for (size_t i = 0; i < hosts.size(); ++i) {
    	host = hosts[i];
		if (host[F("id")] == _id) {
#ifdef DEBUG_MDNS
        debug_i("Hostname %s already in list", _hostname.c_str());
#endif
			if (_ttl != -1) {
				host[F("ttl")] = _ttl; // reset ttl
				updateHost = true;
			}
			if (_ip_address != host[F("ip_address")]) { // update IP address
				host[F("ip_address")] = _ip_address;
				updateHost = true;
			}
			if (_hostname != host[F("hostname")]) { // update hostname
				host[F("hostname")] = _hostname;
				updateHost = true;
			}
			knownHost = true;
			sendWsUpdate(F("updated_host"), host);
			break;
		}
	}
	if(!knownHost) {
		debug_i("updating or adding host %s", _hostname.c_str());
		newHost = hosts.createNestedObject();
		newHost[F("hostname")] = _hostname;
		newHost[F("ip_address")] = _ip_address;
		newHost[F("ttl")] = _ttl;
		newHost[F("id")] = _id;
		sendWsUpdate(F("new_host"), newHost);
	}
}

void mdnsHandler::sendWsUpdate(const String& type, JsonObject host){
	String hostString;
	if (serializeJsonPretty(host, hostString)) {
		app.wsBroadcast(type,hostString);
	}
}

String mdnsHandler::getHosts(){
	/*
     * ToDo: write a version that returns a stream 
     */
	String mdnsHosts;
	serializeJson(hostsDoc, mdnsHosts);
	//Serial.printf("\nnew hosts document:\n %s",mdnsHosts.c_str());
	return mdnsHosts;
}