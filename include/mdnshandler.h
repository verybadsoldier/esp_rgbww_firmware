#include <Network/Mdns/Handler.h>
#include <Network/Mdns/Message.h>
#include <SimpleTimer.h>
#include <Network/Mdns/Resource.h>
#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>
#include <app-config.h>
#include <map>
#include <memory>


#define JSON_SIZE 2048
#define LEADERSHIP_MAX_FAIL_COUNT 4
#define LEADER_ELECTION_DELAY 2

#define TTL_MDNS 60
#define TTL_HTTP_VERIFIED 300

namespace Util {
    String sanitizeHostname(const String& input) {
        String result = input;
        
        // Convert to lowercase
        result.toLowerCase();
        
        // Replace spaces and underscores with hyphens
        result.replace(' ', '-');
        result.replace('_', '-');
        

        
        // Remove leading and trailing hyphens
        while (result.length() > 0 && result[0] == '-') {
            result.remove(0, 1);
        }
        
        while (result.length() > 0 && result[result.length() - 1] == '-') {
            result.remove(result.length() - 1, 1);
        }
        
        // Same for spaces
        while (result.length() > 0 && result[0] == ' ') {
            result.remove(0, 1);
        }
        
        while (result.length() > 0 && result[result.length() - 1] == ' ') {
            result.remove(result.length() - 1, 1);
        }


                // Common character mappings by language group
        
        // German
        result.replace("ä", "ae");
        result.replace("ö", "oe");
        result.replace("ü", "ue");
        result.replace("ß", "ss");
        result.replace("Ä", "Ae");
        result.replace("Ö", "Oe");
        result.replace("Ü", "Ue");
        result.replace("ẞ", "SS");
        
        // French/Latin accents
        result.replace("é", "e");
        result.replace("è", "e");
        result.replace("ê", "e");
        result.replace("ë", "e");
        result.replace("á", "a");
        result.replace("à", "a");
        result.replace("â", "a");
        result.replace("ã", "a");
        result.replace("í", "i");
        result.replace("ì", "i");
        result.replace("î", "i");
        result.replace("ó", "o");
        result.replace("ò", "o");
        result.replace("ô", "o");
        result.replace("õ", "o");
        result.replace("ú", "u");
        result.replace("ù", "u");
        result.replace("û", "u");
        
        // Spanish/Portuguese
        result.replace("ñ", "n");
        result.replace("ç", "c");
        
        // Nordic
        result.replace("å", "a");
        result.replace("ø", "o");
        result.replace("æ", "ae");
        
        // Eastern European
        result.replace("ł", "l");
        result.replace("ż", "z");
        result.replace("ź", "z");
        result.replace("ć", "c");

        // Remove remaining invalid characters (keep a-z, 0-9, and hyphen)
        for (int i = result.length() - 1; i >= 0; i--) {
            char c = result[i];
            if (!((c >= 'a' && c <= 'z') || 
                (c >= '0' && c <= '9') || 
                c == '-')) {
                result.remove(i, 1);
            }
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
} // namespace Util

// Define service classes FIRST before using them
class LEDControllerAPIService : public mDNS::Service {
    public:
        void setLeader(bool isLeader) {
            _isLeader = isLeader;
        }
        
        void setGroups(const Vector<String>& groups) {
            _groups = groups;
        }
        
        void setLeadingGroups(const Vector<String>& groups) {
            _leadingGroups = groups;
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
            
            // Add groups information
            if (_groups.size() > 0) {
                String groupList = "";
                for (size_t i = 0; i < _groups.size(); i++) {
                    if (i > 0) groupList += ",";
                    groupList += _groups[i];
                }
                txt.add(F("groups=") + groupList);
            }
            
            // Add leading groups
            for (size_t i = 0; i < _leadingGroups.size(); i++) {
                txt.add(F("leads_") + _leadingGroups[i] + "=1");
            }
        }
        
    private:
        bool _isLeader = false;
        Vector<String> _groups;
        Vector<String> _leadingGroups;
    };

    class LEDControllerWebService : public mDNS::Service {
        public:
            enum class HostType {
                Device,  // Regular device hostname
                Leader,  // Global leader (lightinator)
                Group    // Group hostname
            };
        
            LEDControllerWebService(const String& instance = "lightinator", HostType type = HostType::Device) 
                : _instance(instance), _hostType(type) {}
            
            void setInstance(const String& instance) {
                _instance = instance;
            }
            
            String getInstance() override { return _instance; }
            String getName() override { return F("http"); }
            Protocol getProtocol() override { return Protocol::Tcp; }
            uint16_t getPort() override { return 80; }
            
            void addText(mDNS::Resource::TXT& txt) override {
                txt.add(F("fn=LED Controller"));
                txt.add(F("instance=") + _instance);
                txt.add("id=" + String(system_get_chip_id()));
                
                // Add type indicator
                switch (_hostType) {
                    case HostType::Device:
                        txt.add(F("type=host"));
                        break;
                    case HostType::Leader:
                        txt.add(F("type=leader"));
                        break;
                    case HostType::Group:
                        txt.add(F("type=group"));
                        break;
                }
            }


        private:
            String _instance;
            HostType _hostType;
        };
/**
 * @class mdnsHandler
 * @brief A class that handles mDNS (Multicast DNS) functionality.
 * 
 * This class provides:
 * 1. Controller discovery through LEDControllerAPIService
 * 2. Web access via hostname.local for every controller
 * 3. Web access via lightinator.local for the global leader
 * 4. Web access via groupname.local for group leaders
 */
class mdnsHandler: public mDNS::Handler {  
public:
    mdnsHandler();
    virtual ~mdnsHandler() ;
    
    /**
     * @brief Initialize and start mDNS services
     */
    void start();
    
    /**
     * @brief Set the search name for discovering other controllers
     */
    void setSearchName(const String& name) {
        debug_i("setting searchName to %s", name.c_str());
        searchName = name;
    }
    
    /**
     * @brief Process incoming mDNS messages
     * @param message The incoming message
     * @return true if the message was handled, false otherwise
     */
    bool onMessage(mDNS::Message& message) override;
    
    /**
     * @brief Add a discovered host to the list
     */
    void addHost(const String& hostname, const String& ip_address, int ttl, unsigned int id);
    
    /**
     * @brief Send WebSocket update about discovered hosts
     */
    void sendWsUpdate(const String& type, JsonObject host);
    
    /**
     * @brief Set the group name for group leadership
     * @param groupName The name of the group
     */
        
    /**
     * @brief Set or clear group leadership status
     * @param enable true to become leader, false to relinquish leadership
     */
        void checkGroupLeadership() ;


private:
    // Responders - each handles a different hostname
    mDNS::Responder primaryResponder;                    // hostname.local
    std::unique_ptr<mDNS::Responder> leaderResponder;    // lightinator.local
        
    // Discovery
    SimpleTimer _mdnsSearchTimer;       
    SimpleTimer _pingTimer; 
    String searchName;
    String service = "_http._tcp.local";
    int _mdnsTimerInterval = 60000;
    int _mdnsPingInterval = 60000; // Ping every minute

    // Global leadership
    bool _isLeader = false;
    bool _leaderDetected = false;
    SimpleTimer _leaderElectionTimer;
    uint8_t _leaderCheckCounter = 0;
    
    void updateServiceTxtRecords();
    void becomeGroupLeader(const String& groupId, const String& groupName);
    void relinquishGroupLeadership(const String& groupId);

    // Track group leadership
    Vector<String> _leadingGroups;
    std::map<String, std::unique_ptr<mDNS::Responder>> _groupResponders;
    std::map<String, std::unique_ptr<LEDControllerWebService>> _groupWebServices;

    // Leadership methods
    void checkForLeadership();
    static void checkForLeadershipCb(void* pTimerArg);
    void becomeLeader();
    void relinquishLeadership();
    

    // Discovery methods
    static void sendSearchCb(void* pTimerArg);
    void sendSearch();
    void queryKnownControllers(uint8_t batchIndex);

    // Service instances
    LEDControllerAPIService ledControllerAPIService;     // For API discovery
    std::unique_ptr<LEDControllerWebService> deviceWebService;            // For hostname.local
    std::unique_ptr<LEDControllerWebService> leaderWebService;   // For lightinator.local
    // Hostname resolution handling
    std::map<String, String> _pendingHostnameResolutions;

    // Process different types of mDNS responses
    bool processApiServiceResponse(mDNS::Message& message);
    bool processHostnameARecord(mDNS::Message& message, mDNS::Answer* a_answer);
    bool processHostnameResponse(mDNS::Message& message, const String& hostname);
    bool pingController(const String& ipAddress, unsigned int id);
    void pingAllControllers();
    static void pingAllControllersCb(void* pTimerArg);
    
};