#include <Network/Mdns/Handler.h>
#include <Network/Mdns/Message.h>
#include <SimpleTimer.h>
#include <Network/Mdns/Resource.h>
#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>

#define JSON_SIZE 2048

// activate debug output
//#define DEBUG_MDNS 

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
        void addHost(const String& hostname, const String& ip_address, int ttl);
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
            txt.add("ty=rgbwwctrl");
            #ifdef ESP8266
            txt.add("mo=esp8266");
            #elif defined(ESP32)
            txt.add("mo=esp32");
            #endif
            txt.add("fn=LED Controller API");
            txt.add("id=" + String(system_get_chip_id()));
            txt.add("path=/");
        }
    private:
};

class LEDControllerWebAppService : public mDNS::Service{
    public:

        String getInstance() override{
		    return F("esprgbwwWebApp");
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
            txt.add("ty=rgbwwctrl");
            #ifdef ESP8266
            txt.add("mo=esp8266");
            #elif defined(ESP32)
            txt.add("mo=esp32");
            #endif
            txt.add("fn=LED Controller WebApp");
            txt.add("id=" + String(system_get_chip_id()));
            txt.add("path=/webapp");
        }
    private:
};

class LEDControllerWSService : public mDNS::Service{
    public:

        String getInstance() override{
		    return F("esprgbwwWS");
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
            txt.add("ty=rgbwwctrl");
            #ifdef ESP8266
            txt.add("mo=esp8266");
            #elif defined(ESP32)
            txt.add("mo=esp32");
            #endif
            txt.add("fn=LED Controller");
            txt.add("id=" + String(system_get_chip_id()));
            txt.add("path=/ws");
        }
    private:
};