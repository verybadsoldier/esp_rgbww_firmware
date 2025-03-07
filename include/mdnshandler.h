#include <Network/Mdns/Handler.h>
#include <Network/Mdns/Message.h>
#include <SimpleTimer.h>
#include <Network/Mdns/Resource.h>
#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>

#define JSON_SIZE 2048

// activate debug output
// #define DEBUG_MDNS 

/**
 * @class mdnsHandler
 * @brief A class that handles mDNS (Multicast DNS) functionality.
 * 
 * This class is responsible for starting and managing mDNS services, 
 * adding hosts to the mDNS responder, and retrieving the list of hosts.
 */
class mdnsHandler: public mDNS::Responder {
    public:
        mdnsHandler();
        virtual ~mdnsHandler(){};
        void start();
        void setSearchName(const String& name){
            debug_i("setting searchName to %s", name.c_str());
            searchName=name;
        }
        bool onMessage(mDNS::Message& message);
        void addHost(const String& hostname, const String& ip_address, int ttl, unsigned int id);
        void sendWsUpdate(const String& type, JsonObject host);
        String getHosts();

    private:
        SimpleTimer _mdnsSearchTimer;        
        String searchName;
        String service = "_http._tcp.local";
        int _mdnsTimerInterval = 60000; //TTL for the records is 120s, so we need to update the list every 60s to be sure.

        StaticJsonDocument<JSON_SIZE> hostsDoc;
        JsonArray hosts;

        static void sendSearchCb(void* pTimerArg);
        void sendSearch();
        
};

class LEDControllerAPIService : public mDNS::Service{
    public:

        String getInstance() override{
		    return F("esprgbwwAPI") ;
        }
        String getName() override{
		    return F("http");
        }
        Protocol getProtocol() override{
		    return Protocol::Tcp;
	    }
        uint16_t getPort() override{
		    return 80;
    	};
	    void addText(mDNS::Resource::TXT& txt) override{
            #ifdef ESP8266
            txt.add(F("mo=esp8266"));
            #elif defined(ESP32)
            txt.add(F("mo=esp32"));
            #elif defined(ESP32C3)
            txt.add(F()"mo=esp32c3"));
            #endif
            txt.add(F("fn=LED Controller API"));
            txt.add(F("id=") + String(system_get_chip_id()));
        }
    private:
};

