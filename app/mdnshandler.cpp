#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>

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
	responder.begin(app.cfg.network.connection.mdnshostname.c_str());
	responder.addService(ledControllerAPIService);
    responder.addService(ledControllerWebAppService);
    responder.addService(ledControllerWSService);
    
    //create an empty hosts array to store recieved host entries
    hosts=hostsDoc.createNestedArray("hosts");
    
    
    //serch for the esprgbwwAIP service. This is used in the onMessage handler to filter out unwanted messages.
    //to fulter for a number of services, this would have to be adapted a bit.
    setSearchName("esprgbwwAPI."+service);

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
    //debug_i("onMessage handler called");
    using namespace mDNS;
   
    // Check if we're interested in this message
    if(!message.isReply()) {
        //debug_i("Ignoring query");
        return false;
    }
    
    //mDNS::printMessage(Serial, message);
    
    auto answer = message[mDNS::ResourceType::SRV];
    if(answer == nullptr) {
        //debug_i("Ignoring message: no SRV record");
        return false;
    }
    //mDNS::printMessage(Serial, message);
    String answerName=String(answer->getName());
    //debug_i("\nanswerName: %s\nsearchName: %s", answerName.c_str(),searchName.c_str());
    if(answerName!= searchName){
        //debug_i("Ignoring message: Name doesn't match");
        return false;
    }
    debug_i("Found matching SRV record");
    // Extract our required information from the message
    struct {
        String hostName;
        IpAddress ipAddr;
        int ttl;
    } info;

    answer = message[mDNS::ResourceType::A];
    if(answer != nullptr) {
        info.hostName=String(answer->getName());
        info.hostName = info.hostName.substring(0, info.hostName.lastIndexOf(".local"));
        info.ipAddr=String(answer->getRecordString());
        info.ttl=answer->getTtl();
      }
    debug_i("found Host %s with IP %s and TTL %i", info.hostName.c_str(), info.ipAddr.toString().c_str(), info.ttl);
    
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
    debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");

    //restart the timer
    _mdnsSearchTimer.startOnce();
    for (size_t i = 0; i < hosts.size(); ++i) {
        JsonVariant host = hosts[i];
        if((int)host["ttl"]==-1){
            continue;
        }
        host["ttl"] = (int)host["ttl"] - _mdnsTimerInterval/1000;
        if (host["ttl"].as<int>() < 0) {
            debug_i("Removing host %s from list", host["hostname"].as<const char*>());
            hosts.remove(i);
            --i;
        }
        
        if (host["hostname"]== null){
            hosts.remove(i);
        }
        
    }
}

void mdnsHandler::sendSearchCb(void* pTimerArg) {
    debug_i("sendSearchCb called");
    mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
    pThis->sendSearch();
}

void mdnsHandler::addHost(const String& hostname, const String& ip_address, int ttl){
    /*
     * ToDo: this is currently an ugly implementation, as it needs both, the hosts 
     * JsonArray and the app.cfg.network.mdnsHosts string, wasting memory
     * however, it's the most convenient way to pass the list from here to the webserver
     * without having to pass object references around
     */
    debug_i("Adding host %s with IP %s and ttl %i", hostname.c_str(), ip_address.c_str(), ttl);
    String _hostname, _ip_address;
    int _ttl;
    _hostname=hostname;
    _ip_address=ip_address;
    _ttl=ttl;   
    
    bool knownHost=false;
    for (JsonVariant host : hosts) {
        if (host["hostname"] ==_hostname && host["ip_address"] == _ip_address) {
            debug_i("Hostname %s already in list", _hostname.c_str());
            if(_ttl!=-1)
                host["ttl"] = _ttl; //reset ttl
            knownHost=true;
            break;
        }
    }

    if (!knownHost) {
        JsonObject newHost = hosts.createNestedObject();

        newHost["hostname"] = _hostname;
        newHost["ip_address"] = _ip_address;
        newHost["ttl"] = _ttl;
        String newHostString;
        serializeJsonPretty(newHost, newHostString);
        debug_i("new host: %s", newHostString.c_str());
    }
}

String mdnsHandler::getHosts(){
    String mdnsHosts;
    serializeJson(hostsDoc,mdnsHosts);
    //Serial.printf("\nnew hosts document:\n %s",mdnsHosts.c_str());
    return mdnsHosts;
}