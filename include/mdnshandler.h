#include <Network/Mdns/Handler.h>
#include <Network/Mdns/Message.h>
#include <SimpleTimer.h>
#include <Network/Mdns/Resource.h>
#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>
#include <app-config.h>
#include <memory>  // For std::unique_ptr

#define JSON_SIZE 2048
#define LEADERSHIP_MAX_FAIL_COUNT 4
#define LEADER_ELECTION_DELAY 2

// Define service classes FIRST before using them
class LEDControllerAPIService : public mDNS::Service {
public:
    void setLeader(bool isLeader) {
        _isLeader = isLeader;
    }
    
    String getInstance() override { return F("esprgbwwAPI"); }
    String getName() override { return F("http"); }
    Protocol getProtocol() override { return Protocol::Tcp; }
    uint16_t getPort() override { return 80; }
    
    void addText(mDNS::Resource::TXT& txt) override {
        #ifdef ESP8266
        txt.add(F("mo=esp8266"));
        #elif defined(ESP32)
        txt.add(F("mo=esp32"));
        #elif defined(ESP32C3)
        txt.add(F("mo=esp32c3"));
        #endif
        txt.add(F("fn=LED Controller API"));
        txt.add(F("id=") + String(system_get_chip_id()));
        
        // Add leader status if this is the leader
        if (_isLeader) {
            txt.add(F("isLeader=1"));
        }
    }
    
private:
    bool _isLeader = false;
};

class LEDControllerWebService : public mDNS::Service {
public:
    String getInstance() override { return F("lightinator"); }
    String getName() override { return F("http"); }
    Protocol getProtocol() override { return Protocol::Tcp; }
    uint16_t getPort() override { return 80; }
    
    void addText(mDNS::Resource::TXT& txt) override {
        txt.add(F("fn=LED Controller"));
    }
};

class LightinatorService : public mDNS::Service {
public:
    LightinatorService(const String& hostName) : _hostName(sanitizeHostname(hostName)) {}
    
    // Add these required methods:
    String getInstance() override { 
        // Return "lightinator" regardless of the hostname passed to the constructor
        return F("lightinator"); 
    }
    
    String getName() override { return F("http"); }
    Protocol getProtocol() override { return Protocol::Tcp; }
    uint16_t getPort() override { return 80; }
    
    void addText(mDNS::Resource::TXT& txt) override {
        txt.add(F("path=/"));
        txt.add(F("txtvers=1"));
        txt.add(F("isMainLeader=1"));
    }
    
private:
    String _hostName;
    
    String sanitizeHostname(const String& input) {
        String result = input;
        
        // Convert to lowercase
        result.toLowerCase();
        
        // Replace spaces and underscores with hyphens
        result.replace(' ', '-');
        result.replace('_', '-');
        
        // Remove invalid characters (keep a-z, 0-9, and hyphen)
        for (int i = result.length() - 1; i >= 0; i--) {
            char c = result[i];
            if (!((c >= 'a' && c <= 'z') || 
                  (c >= '0' && c <= '9') || 
                  c == '-')) {
                result.remove(i, 1);
            }
        }
        
        // Remove leading and trailing hyphens
        while (result.length() > 0 && result[0] == '-') {
            result.remove(0, 1);
        }
        
        while (result.length() > 0 && result[result.length() - 1] == '-') {
            result.remove(result.length() - 1, 1);
        }
        
        // Ensure we have a valid hostname
        if (result.length() == 0) {
            result = "lightinator"; // Default name if everything was filtered out
        }
        
        // Trim to valid DNS hostname length (63 characters max)
        if (result.length() > 63) {
            result = result.substring(0, 63);
        }
        
        return result;
    }
};

/**
 * @class mdnsHandler
 * @brief A class that handles mDNS (Multicast DNS) functionality.
 */
class mdnsHandler: public mDNS::Responder {
public:
    mdnsHandler();
    virtual ~mdnsHandler() {};
    void start();
    void setSearchName(const String& name) {
        debug_i("setting searchName to %s", name.c_str());
        searchName = name;
    }
    bool onMessage(mDNS::Message& message);
    void addHost(const String& hostname, const String& ip_address, int ttl, unsigned int id);
    void sendWsUpdate(const String& type, JsonObject host);

private:
    // Add responder as member variable
    mDNS::Responder responder;
    
    SimpleTimer _mdnsSearchTimer;        
    String searchName;
    String service = "_http._tcp.local";
    int _mdnsTimerInterval = 60000;

    bool _isLeader = false;
    bool _leaderDetected = false;
    SimpleTimer _leaderElectionTimer;
    uint8_t _leaderCheckCounter = 0;

    void checkForLeadership();
    static void checkForLeadershipCb(void* pTimerArg);
    void becomeLeader();
    void relinquishLeadership();

    static void sendSearchCb(void* pTimerArg);
    void sendSearch();

    LEDControllerAPIService ledControllerAPIService;
    LEDControllerWebService ledControllerWebService;
    std::unique_ptr<LightinatorService> lightinatorService;
};