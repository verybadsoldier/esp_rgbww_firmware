#include <Network/Mdns/Handler.h>
#include <Network/Mdns/Message.h>
#include <SimpleTimer.h>
#include <Network/Mdns/Resource.h>
#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>
#include <app-config.h>

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
            txt.add(F("mo=esp32c3"));
            #endif
            txt.add(F("fn=LED Controller API"));
            txt.add(F("id=") + String(system_get_chip_id()));
        }
    private:
};
class LEDControllerWebService : public mDNS::Service{
    public:

        String getInstance() override{
		    return F("lightinator") ;
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
            txt.add(F("fn=LED Controller"));
        }
    private:
};
class LightinatorService : public mDNS::Service {
    public:
        LightinatorService(const String& hostName) : _hostName(sanitizeHostname(hostName)) {}
        
        // Standard HTTP service type
        String getName() override {
            return F("http");  // This becomes _http._tcp
        }
        
        Protocol getProtocol() override {
            return Protocol::Tcp;
        }
        
        // Identify as lightinator in the instance name
        String getInstance() override {
            return _hostName;  // Friendly name with hostname
        }
        
        uint16_t getPort() override {
            return 80;
        }
        
        void addText(mDNS::Resource::TXT& txt) override {
            txt.add(F("path=/"));  
            txt.add(F("txtvers=1")); 
            txt.add(F("id=")+ _hostName);
            txt.add(F("type=lightinator"));
            txt.add(F("service=lightinator"));
            txt.add(F("version=")+ F(GITVERSION));
        }
        
    private:
        String _hostName;
        
        // Sanitize hostname for DNS compatibility
        String sanitizeHostname(const String& input) {
            String result = input;
            
            // Convert to lowercase
            result.toLowerCase();
            
            // Replace invalid characters with dashes
            for (size_t i = 0; i < result.length(); i++) {
                char c = result[i];
                // Allow alphanumeric and dash
                if (!isalnum(c) && c != '-') {
                    result[i] = '-';
                }
            }
            
            // Remove consecutive dashes
            while (result.indexOf("--") >= 0) {
                result.replace("--", "-");
            }
            
            // Remove leading/trailing dashes
            while (result.length() > 0 && result[0] == '-') {
                result = result.substring(1);
            }
            while (result.length() > 0 && result[result.length() - 1] == '-') {
                result = result.substring(0, result.length() - 1);
            }
            
            // If empty after processing, use a default
            if (result.length() == 0) {
                result = F("lightinator");
            }
            
            // Ensure it's not too long (max 63 chars for DNS label)
            if (result.length() > 63) {
                result = result.substring(0, 63);
            }
            
            return result;
        }
    };