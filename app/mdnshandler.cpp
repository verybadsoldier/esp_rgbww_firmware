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
}

bool mdnsHandler::onMessage(mDNS::Message& message)
{
    using namespace mDNS;
    mDNS::printMessage(Serial, message);

    // Check if we're interested in this message
    if(!message.isReply()) {
        debug_i("Ignoring query");
        return false;
    }
    auto answer = message[mDNS::ResourceType::PTR];
    if(answer == nullptr) {
        debug_i("Ignoring message: no PTR record");
        return false;
    }
    if(answer->getName() != searchName) {
        debug_i("Ignoring message: Name doesn't match");
        return false;
    }

    // Extract our required information from the message
    struct {
        String instance;
        String service;
        IpAddress ipaddr;
    } info;

    answer = message[mDNS::ResourceType::TXT];
    if(answer != nullptr) {
        mDNS::Resource::TXT txt(*answer);
        info.instance = txt["md"];
        info.service = txt["fn"];
    }

    answer = message[mDNS::ResourceType::A];
    if(answer != nullptr) {
        mDNS::Resource::A a(*answer);
        info.ipaddr = a.getAddress();
    }

    // Create a JSON object
    StaticJsonDocument<200> doc;
    doc["hostname"] = info.instance;
    doc["ip_address"] = info.ipaddr.toString();
    doc["service"] = info.service;

    // Serialize JSON document
    String output;
    serializeJson(doc, output);
    Serial.println(output);

    return true;
}

void mdnsHandler::sendSearch()
{
    // Search for the services
    String services[] = {F("esprgbwwAPI._http._tcp.local"), F("esprgbwwWebApp._http._tcp.local"), F("esprgbwwWS._ws._tcp.local")};
    for (String service : services) {
        bool ok = mDNS::server.search(service);
        debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");
        // Tell our handler what we're looking for
        myMessageHandler.setSearchName(service);
    }
}