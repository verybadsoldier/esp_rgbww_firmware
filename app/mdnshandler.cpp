#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>
#include "application.h"
#include "app-data.h"

// ...

static LEDControllerAPIService* g_ledControllerAPIService = nullptr;

mdnsHandler::mdnsHandler(){};

void mdnsHandler::start()
{
	using namespace mDNS;

	//start the mDNS responder with the configured services, using the configured hostname
    
	String hostName;
    {
        AppConfig::Network network(*app.cfg);
        hostName = network.mdns.getName();
        responder.begin(hostName.c_str());
    } // end of ConfigDB network context

	
	lightinatorService = std::make_unique<LightinatorService>(hostName);
    g_ledControllerAPIService = &ledControllerAPIService;

	// Set up leadership election with delay
	_leaderElectionTimer.setCallback(mdnsHandler::checkForLeadershipCb, this);
	_leaderElectionTimer.setIntervalMs(_mdnsTimerInterval * LEADER_ELECTION_DELAY);
	_leaderElectionTimer.startOnce();
	
	responder.addService(ledControllerAPIService);
	if(_isLeader)
	{
		responder.addService(*lightinatorService);  
	}
	
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
	bool msgHasA, msgHasTXT=false;
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
		debug_i("Ignoring message: no SRV record");startI
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
		msgHasA=true;
	}
	
	answer = message[mDNS::ResourceType::TXT];
	if(answer!=nullptr){
		mDNS::Resource::TXT txt(*answer);
		//mDNS::printAnswer(Serial, *answer);
		info.ID=txt["id"].toInt();
		msgHasTXT=true;
	}
	if (msgHasA && msgHasTXT) {
        answer = message[mDNS::ResourceType::TXT];
        if (answer != nullptr) {
            mDNS::Resource::TXT txt(*answer);
            String isLeaderTxt = txt["isLeader"];
            if (isLeaderTxt == "1") {
                _leaderDetected = true;
                debug_i("Detected leader: %s", info.hostName.c_str());
            }
        }
        
        addHost(info.hostName, info.ipAddr.toString(), info.ttl, info.ID);
        return true;
    }else{
		return false;
	
	}
}

/**
 * @brief Sends a search request for the specified services using mDNS.
 *
 * This function sends a search request for the services specified in the `services` array using mDNS.
 * It iterates through each service in the array and calls the `mDNS::server.search()` function to perform the search.
**/
void mdnsHandler::sendSearch()
{
    static unsigned long lastLeaderCheck = 0;
    unsigned long now = millis();

	// Search for the service
	bool ok = mDNS::server.search(service);
#ifdef DEBUG_MDNS
	debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");
#endif

	// periodically check if there is still a leader in the network
	if (now - lastLeaderCheck > (_mdnsTimerInterval * LEADER_ELECTION_DELAY)) {
		lastLeaderCheck = now;
		
		// Reset leader detection status
		_leaderDetected = false;
		
		// If we're currently not a leader, schedule a check after the next search cycle
		if (!_isLeader) {
			_leaderElectionTimer.startOnce();
		}
	}
	//restart the timer
	_mdnsSearchTimer.startOnce();
	app.removeExpiredControllers(_mdnsTimerInterval / 1000);
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
	* Rewrite to use a hybrid store model: 
	* for persistent storage of detected hosts, use appData store, it will hold id, name and ip address, the latter two can be updated, the id is fixed
	* an in-Ram structure will only hold if the controller is currently visible 
	*/
	if(id==1) return; //invalid ID

	//JsonObject newHost; //holds the json representation of the new host for the websocket update

#ifdef DEBUG_MDNS
	debug_i("Adding host %s with IP %s and ttl %i", hostname.c_str(), ip_address.c_str(), ttl);
#endif
	AppData::Root::Controllers controllers(*app.data);
	bool found = false;
	if (auto controllersUpdate = controllers.update()) {
		// Find the specific controller to update (must iterate)
		for (auto controllerItem : controllersUpdate) {
			if (controllerItem.getId() == String(id)) {
				found = true;
				debug_i("Hostname %s already in list", hostname.c_str());
				if (controllerItem.getIpAddress() != ip_address) {
					debug_i("IP address changed from %s to %s", controllerItem.getIpAddress().c_str(), ip_address.c_str());
					controllerItem.setIpAddress(ip_address);
				}
				if (controllerItem.getName() != hostname) {
					debug_i("Hostname changed from %s to %s", controllerItem.getName().c_str(), hostname.c_str());
					controllerItem.setName(hostname);
				}
				break;
			}
		}
	}else{
		debug_e("error: failed to open hosts db for update");
	}

	if(!found) {
		debug_i("Hostname %s not in list", hostname.c_str());
		
		if(auto controllersUpdate = controllers.update()) {
			
			auto newController=controllersUpdate.addItem();
			newController.setName(hostname);
			newController.setIpAddress(ip_address);
			newController.setId(String(id));
		}else{
			debug_e("error: failed to add host");
		}
	}

	if (!app.isVisibleController(id)) {
		app.addOrUpdateVisibleController(id, ttl);

		debug_i("Controller %s with ID %i is now visible", hostname.c_str(), id);
		// controller has just become visible, we'll update the clients
		StaticJsonDocument<256> doc;
		JsonObject newHost = doc.to<JsonObject>();
		newHost[F("id")] = id;
		newHost[F("ttl")] = ttl;
		newHost[F("ip_address")] = ip_address;
		newHost[F("hostname")] = hostname;
		app.wsBroadcast(F("new_host"), newHost);
	}

}

void mdnsHandler::sendWsUpdate(const String& type, JsonObject host){
	String hostString;
	
	if (serializeJsonPretty(host, hostString)) {
		app.wsBroadcast(type,hostString);
	}
}

void mdnsHandler::checkForLeadership() {
    if (_leaderDetected) {
        debug_i("Leader already exists in network, not becoming leader");
        _leaderCheckCounter = 0;  // Reset counter when a leader is detected
        return;
    }
    
    debug_i("No leader detected (check round %d)", _leaderCheckCounter + 1);
    
    // Increment leader check counter
    _leaderCheckCounter++;
    
    // Check if this controller has highest ID
    unsigned int myId = system_get_chip_id();
    bool hasHighestId = true;
    
    // Check all visible controllers
    for (const auto& controller : app.visibleControllers) {
        if (controller.id > myId) {
            hasHighestId = false;
            break;
        }
    }
    
    // Become leader if we have highest ID OR we've checked 5 times with no leader
    if (hasHighestId || _leaderCheckCounter >= 5) {
        if (hasHighestId) {
            debug_i("No leader detected and we have highest ID, becoming leader");
        } else {
            debug_i("No leader detected after %d checks, becoming leader as a failsafe", LEADERSHIP_MAX_FAIL_COUNT);
		}
        
        becomeLeader();
        _leaderCheckCounter = 0;  // Reset counter
    }
    else {
        debug_i("Not becoming leader, another controller has higher ID (check %d/%d)", _leaderCheckCounter,LEADERSHIP_MAX_FAIL_COUNT);
        
        // Start another check after a delay if we haven't reached the limit
        if (_leaderCheckCounter < LEADERSHIP_MAX_FAIL_COUNT) {
            _leaderElectionTimer.startOnce();
        }
    }
}
// Dereference the unique_ptr
void mdnsHandler::checkForLeadershipCb(void* pTimerArg) {
    mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
    pThis->checkForLeadership();
}

void mdnsHandler::becomeLeader() {
    if (_isLeader) return;
    
    _isLeader = true;
    if (g_ledControllerAPIService != nullptr) {
        g_ledControllerAPIService->setLeader(true);
		if (lightinatorService) {
            responder.addService(*lightinatorService);  
        } 
        debug_i("This controller is now the leader");
    }
}

void mdnsHandler::relinquishLeadership() {
    if (!_isLeader) return;
    
    _isLeader = false;
    if (g_ledControllerAPIService != nullptr) {
        g_ledControllerAPIService->setLeader(false);
		if (lightinatorService) {
            responder.removeService(*lightinatorService);  
        }
        debug_i("This controller is no longer the leader");
    }
}