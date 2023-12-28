#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>

// ...

mdnsHandler::mdnsHandler(){};

void mdnsHandler::start()
{
    static mDNS::Responder responder;

    static LEDControllerAPIService ledControllerAPIService;
    static LEDControllerWebAppService ledControllerWebAppService;  
    static LEDControllerWSService ledControllerWSService;

	responder.begin(app.cfg.network.connection.mdnshostname.c_str());
	responder.addService(ledControllerAPIService);
    responder.addService(ledControllerWebAppService);
    responder.addService(ledControllerWSService);

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
    debug_i("onMessage handler called");
    using namespace mDNS;
    
    // Check if we're interested in this message
    if(!message.isReply()) {
        debug_i("Ignoring query");
        return false;
    }
    
    mDNS::printMessage(Serial, message);
    
    auto answer = message[mDNS::ResourceType::SRV];
    if(answer == nullptr) {
        debug_i("Ignoring message: no SRV record");
        return false;
    }

    String answerName=String(answer->getName());
    debug_i("\nanswer name: %s\n searchName: %s", answerName.c_str(),searchName.c_str());
    if(answerName!= searchName){
        debug_i("length of answerName: %i", answerName.length());
        for(int i=0; i<answerName.length(); i++){
            Serial.printf("%#00x ", answerName[i]);
        }
        Serial.println();
        for(int i=0; i<searchName.length(); i++){
            Serial.printf("%#00x ", searchName[i]);
        }
        debug_i("Ignoring message: Name doesn't match");
        return false;
    }
    
    // Extract our required information from the message
    struct {
        String hostName;
        IpAddress ipAddr;
        int ttl;
    } info;

    answer = message[mDNS::ResourceType::A];
    if(answer != nullptr) {
        info.hostName=String(answer->getName());
        info.ipAddr=String(answer->getRecordString());
        info.ttl=answer->getTtl();
      }
      debug_i("found Host %s with IP %s and TTL %i", info.hostName.c_str(), info.ipAddr.toString().c_str(), info.ttl);
/*
    answer = message[mDNS::ResourceType::A];
    if(answer != nullptr) {
        mDNS::Resource::A a(*answer);
        info.ipaddr = a.getAddress();
    }
*/
    // Create a JSON object
    
    StaticJsonDocument<200> doc;
    doc["hostname"] = info.hostName;
    doc["ip_address"] = info.ipAddr.toString();
    doc["ttl"] = info.ttl;

    // debug_i("Found service: %s at address %s", info.service.c_str(), info.ipaddr.toString().c_str());

    // Serialize JSON document
    String output;
    serializeJson(doc, output);
    Serial.println(output);

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
        debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");

	    setSearchName(service);

        //restart the timer
        _mdnsSearchTimer.startOnce();
}

void mdnsHandler::sendSearchCb(void* pTimerArg) {
    debug_i("sendSearchCb called");
    mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
    pThis->sendSearch();
}