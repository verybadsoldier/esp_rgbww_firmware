#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>
#include "application.h"
#include "app-data.h"

//#define DEBUG_MDNS 1

// Global pointer for leader service updates from other components
static LEDControllerAPIService* g_ledControllerAPIService = nullptr;

mdnsHandler::mdnsHandler() {
    // Initialize with default values
}

mdnsHandler::~mdnsHandler() {
    // Clean up global leader responder
    if (leaderResponder) {
        mDNS::server.removeHandler(*leaderResponder);
    }
    
    // Clean up all group responders
    for (auto it = _groupResponders.begin(); it != _groupResponders.end(); ++it) {
        mDNS::server.removeHandler(*it->second);
    }
}

void mdnsHandler::start()
{
    using namespace mDNS;

    debug_i("########################################################");
    debug_i("# mdns Handler initialized, Port: %d", MDNS_SOURCE_PORT);
    debug_i("########################################################");
    

    checkGroupLeadership();

    // Get device hostname from configuration
    String hostName;
    {
        AppConfig::Network network(*app.cfg);
        hostName = network.mdns.getName();
        if (hostName.length() == 0) {
            // Generate hostname from MAC if not set
            hostName = "lightinator-" + String(system_get_chip_id());
        }
        hostName = Util::sanitizeHostname(hostName);
        
        // Set up device web service with this hostname
        deviceWebService.setInstance(hostName);
        
        // Initialize primary responder with device hostname
        primaryResponder.begin(hostName.c_str());
        debug_i("Registered hostname: %s", hostName.c_str());
        
        
        // Add services to the primary responder
        primaryResponder.addService(ledControllerAPIService);
        primaryResponder.addService(deviceWebService);
    }
    
    // Store global reference for API service
    g_ledControllerAPIService = &ledControllerAPIService;

    // Set up leadership election with delay
    _leaderElectionTimer.setCallback(mdnsHandler::checkForLeadershipCb, this);
    _leaderElectionTimer.setIntervalMs(_mdnsTimerInterval * LEADER_ELECTION_DELAY);
    _leaderElectionTimer.startOnce();
    
    // Set search name for discovering other controllers
    setSearchName(F("esprgbwwAPI._http._tcp.local"));
    
    // Set up timer for periodic mDNS searches
    _mdnsSearchTimer.setCallback(mdnsHandler::sendSearchCb, this);
    _mdnsSearchTimer.setIntervalMs(_mdnsTimerInterval);
    _mdnsSearchTimer.startOnce();
    
    // Register handlers with mDNS server
    mDNS::server.addHandler(*this);
    mDNS::server.addHandler(primaryResponder);
    debug_i("mDNS server started");
}

bool mdnsHandler::onMessage(mDNS::Message& message)
{
    bool msgHasA = false, msgHasTXT = false;
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

    auto answer = message[mDNS::ResourceType::SRV];
    if(answer == nullptr) {
#ifdef DEBUG_MDNS
        debug_i("Ignoring message: no SRV record");
#endif
        return false;
    }
    
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

    // Extract required information from the message
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
        msgHasA = true;
    }
    
    answer = message[mDNS::ResourceType::TXT];
    if(answer != nullptr) {
        mDNS::Resource::TXT txt(*answer);
        info.ID = txt["id"].toInt();
        msgHasTXT = true;
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
    } else {
        return false;
    }
}

void mdnsHandler::sendSearch()
{
    static unsigned long lastLeaderCheck = 0;
    unsigned long now = millis();

    // Search for the service
    bool ok = mDNS::server.search(service);
#ifdef DEBUG_MDNS
    debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");
#endif

    // Periodically check if there is still a leader in the network
    if (now - lastLeaderCheck > (_mdnsTimerInterval * LEADER_ELECTION_DELAY)) {
        lastLeaderCheck = now;
        
        // Reset leader detection status
        _leaderDetected = false;
        
        // If we're currently not a leader, schedule a check after the next search cycle
        if (!_isLeader) {
            _leaderElectionTimer.startOnce();
        }
    }
    
    static unsigned long lastGroupLeaderCheck = 0;
    if (now - lastGroupLeaderCheck > (2 * 60 * 1000)) { // 2 minutes
        lastGroupLeaderCheck = now;
        
        // Check group leadership again
        checkGroupLeadership();
    }
    // Restart the timer
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
    if(id == 1) return; // Invalid ID

    // Check if this is a group hostname or the global leader hostname
    bool isGroupOrLeaderHostname = (hostname == "lightinator");
    
    // Also check if hostname matches any of our currently led groups
    for (const auto& groupId : _leadingGroups) {
        AppData::Root::Groups groups(*app.data);
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            if ((*it).getId() == groupId) {
                String sanitizedGroupName = Util::sanitizeHostname((*it).getName());
                if (hostname == sanitizedGroupName) {
                    isGroupOrLeaderHostname = true;
                    break;
                }
            }
        }
        if (isGroupOrLeaderHostname) break;
    }
    
    // If this is a group or leader hostname update, only update IP address, not the hostname
    if (isGroupOrLeaderHostname) {
        debug_i("Group/leader hostname detected: %s (ID: %u) - only updating IP address", 
                hostname.c_str(), id);
    }

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
                
                // Always update IP address
                if (controllerItem.getIpAddress() != ip_address) {
                    debug_i("IP address changed from %s to %s", 
                           controllerItem.getIpAddress().c_str(), ip_address.c_str());
                    controllerItem.setIpAddress(ip_address);
                }
                
                // Only update hostname if this is NOT a group or leader hostname
                if (!isGroupOrLeaderHostname && controllerItem.getName() != hostname) {
                    debug_i("Hostname changed from %s to %s", 
                           controllerItem.getName().c_str(), hostname.c_str());
                    controllerItem.setName(hostname);
                }
                break;
            }
        }
    } else {
        debug_e("error: failed to open hosts db for update");
    }

    if(!found) {
        debug_i("Hostname %s not in list", hostname.c_str());
        
        if(auto controllersUpdate = controllers.update()) {
            auto newController = controllersUpdate.addItem();
            newController.setName(hostname);
            newController.setIpAddress(ip_address);
            newController.setId(String(id));
        } else {
            debug_e("error: failed to add host");
        }
    }

    if (!app.isVisibleController(id)) {
        app.addOrUpdateVisibleController(id, ttl);

        debug_i("Controller %s with ID %i is now visible", hostname.c_str(), id);
        // Controller has just become visible, update the clients
        StaticJsonDocument<256> doc;
        JsonObject newHost = doc.to<JsonObject>();
        newHost[F("id")] = id;
        newHost[F("ttl")] = ttl;
        newHost[F("ip_address")] = ip_address;
        newHost[F("hostname")] = hostname;
        app.wsBroadcast(F("new_host"), newHost);
    }
}

void mdnsHandler::sendWsUpdate(const String& type, JsonObject host) {
    String hostString;
    
    if (serializeJsonPretty(host, hostString)) {
        app.wsBroadcast(type, hostString);
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
    
    // Become leader if we have highest ID OR we've checked max times with no leader
    if (hasHighestId || _leaderCheckCounter >= LEADERSHIP_MAX_FAIL_COUNT) {
        if (hasHighestId) {
            debug_i("No leader detected and we have highest ID, becoming leader");
        } else {
            debug_i("No leader detected after %d checks, becoming leader as a failsafe", LEADERSHIP_MAX_FAIL_COUNT);
        }
        
        becomeLeader();
        _leaderCheckCounter = 0;  // Reset counter
    } else {
        debug_i("Not becoming leader, another controller has higher ID (check %d/%d)", 
                _leaderCheckCounter, LEADERSHIP_MAX_FAIL_COUNT);
        
        // Start another check after a delay if we haven't reached the limit
        if (_leaderCheckCounter < LEADERSHIP_MAX_FAIL_COUNT) {
            _leaderElectionTimer.startOnce();
        }
    }
}

void mdnsHandler::checkForLeadershipCb(void* pTimerArg) {
    mdnsHandler* pThis = static_cast<mdnsHandler*>(pTimerArg);
    pThis->checkForLeadership();
}

void mdnsHandler::becomeLeader() {
    if (_isLeader) return;
    
    _isLeader = true;
    if (g_ledControllerAPIService != nullptr) {
        g_ledControllerAPIService->setLeader(true);
        
        // Create new web service for the leader
        leaderWebService = std::make_unique<LEDControllerWebService>("lightinator");
        
        // Create new responder for "lightinator.local"
        leaderResponder = std::make_unique<mDNS::Responder>();
        leaderResponder->begin("lightinator");
        
        // Add services to the leader responder
        leaderResponder->addService(ledControllerAPIService);
        leaderResponder->addService(*leaderWebService);
        
        // Register the leader responder with mDNS server
        mDNS::server.addHandler(*leaderResponder);
        
        debug_i("This controller is now the global leader (lightinator.local)");
    }
}

void mdnsHandler::relinquishLeadership() {
    if (!_isLeader) return;
    
    _isLeader = false;
    if (g_ledControllerAPIService != nullptr) {
        g_ledControllerAPIService->setLeader(false);
        
        // Remove leader responder from mDNS server
        if (leaderResponder) {
            mDNS::server.removeHandler(*leaderResponder);
            leaderResponder.reset();
        }
        
        // Clean up leader web service
        leaderWebService.reset();
        
        debug_i("This controller is no longer the global leader");
    }
}


void mdnsHandler::checkGroupLeadership() {
    // Step 1: Identify which groups we belong to (already implemented)
    String myId = String(system_get_chip_id()); 
    Vector<String> memberGroups;
    Vector<String> groupsToLead;
    
    // Get access to all groups and track our memberships
    AppData::Root::Groups groups(*app.data);
    
    // Build map of group ID -> group name for easier reference
    std::map<String, String> groupNames;
    
    // Scan for our group memberships
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto& currentGroup = *it;
        String groupId = currentGroup.getId();
        String groupName = currentGroup.getName();
        groupNames[groupId] = groupName;
        
        // Check if we're a member of this group
        bool isMember = false;
        
        for (auto controllerIt = currentGroup.controllerIds.begin(); 
             controllerIt != currentGroup.controllerIds.end(); ++controllerIt) {
            if (*controllerIt == myId) {
                isMember = true;
                memberGroups.add(groupId);
                break;
            }
        }
        
        if (isMember) {
            // Step 2: For each group we're a member of, check if we should be leader
            
            // Determine if we have the highest ID in the group
            bool hasHighestId = true;
            
            for (auto controllerIt = currentGroup.controllerIds.begin(); 
                 controllerIt != currentGroup.controllerIds.end(); ++controllerIt) {
                String controllerId = *controllerIt;
                
                // Skip ourselves
                if (controllerId == myId) continue;
                
                // Convert string IDs to integers for comparison
                unsigned int theirId = controllerId.toInt();
                unsigned int ourId = myId.toInt();
                
                if (theirId > ourId) {
                    hasHighestId = false;
                    break;
                }
            }
            
            // If we have the highest ID in this group, we should be the leader
            if (hasHighestId) {
                groupsToLead.add(groupId);
                debug_i("This device should be the leader for group: %s", groupName.c_str());
            }
        }
    }
    
    // Step 3: Set up leadership for groups where we should be leader
    for (size_t i = 0; i < groupsToLead.size(); i++) {
        String groupId = groupsToLead[i];
        String groupName = groupNames[groupId];
        
        // Only set up leadership if we're not already leader for this group
        if (_leadingGroups.indexOf(groupId) < 0) {
            becomeGroupLeader(groupId, groupName);
        }
    }
    
    // Step 4: Relinquish leadership for groups where we no longer should be leader
    Vector<String> groupsToRelinquish;
    
    for (size_t i = 0; i < _leadingGroups.size(); i++) {
        String groupId = _leadingGroups[i];
        
        // If we're no longer a member or shouldn't be leader, relinquish
        if (memberGroups.indexOf(groupId) < 0 || groupsToLead.indexOf(groupId) < 0) {
            groupsToRelinquish.add(groupId);
        }
    }
    
    for (size_t i = 0; i < groupsToRelinquish.size(); i++) {
        relinquishGroupLeadership(groupsToRelinquish[i]);
    }
    
    // Step 5: Update our service TXT records with current group info
    updateServiceTxtRecords();
}

void mdnsHandler::updateServiceTxtRecords() {
    // Get our current group memberships
    Vector<String> memberGroups;
    String myId = String(system_get_chip_id());
    AppData::Root::Groups groups(*app.data);
    
    // Build list of groups we're members of
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto& currentGroup = *it;
        
        // Check if we're a member
        for (auto controllerIt = currentGroup.controllerIds.begin(); 
             controllerIt != currentGroup.controllerIds.end(); 
             ++controllerIt) {
            
            if (*controllerIt == myId) {
                memberGroups.add(currentGroup.getId());
                break;
            }
        }
    }
    
    // Update our service with group membership
    ledControllerAPIService.setLeader(_isLeader);
    ledControllerAPIService.setGroups(memberGroups);
    ledControllerAPIService.setLeadingGroups(_leadingGroups);
    
    debug_i("Updated service TXT records with %d group memberships and %d leading groups", 
            memberGroups.size(), _leadingGroups.size());
}

void mdnsHandler::becomeGroupLeader(const String& groupId, const String& groupName) {
    debug_i("Becoming leader for group: %s (ID: %s)", groupName.c_str(), groupId.c_str());
    
    // Sanitize the group name for use as a hostname
    String sanitizedName = Util::sanitizeHostname(groupName);
    debug_i("Sanitized group name: %s", sanitizedName.c_str());
    
    // Create responder for this group's hostname
    auto responder = std::make_unique<mDNS::Responder>();
    responder->begin(sanitizedName.c_str());
    
    // Create and set up web service for this group
    auto webService = std::make_unique<LEDControllerWebService>(sanitizedName);
    
    // Add services to the responder
    responder->addService(ledControllerAPIService);
    responder->addService(*webService);
    
    // Register with mDNS server
    mDNS::server.addHandler(*responder);
    
    // Store in our maps
    _groupResponders[groupId] = std::move(responder);
    _groupWebServices[groupId] = std::move(webService);
    
    // Track that we're now leading this group
    _leadingGroups.add(groupId);
    
    debug_i("This controller is now leader for group: %s (%s.local)", 
           groupName.c_str(), sanitizedName.c_str());
}

void mdnsHandler::relinquishGroupLeadership(const String& groupId) {
    // Find the group name for logging
    AppData::Root::Groups groups(*app.data);
    String groupName = "unknown";
    
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        if ((*it).getId() == groupId) {
            groupName = (*it).getName();
            break;
        }
    }
    
    debug_i("Relinquishing leadership for group: %s (ID: %s)", 
           groupName.c_str(), groupId.c_str());
    
    // Remove the responder from mDNS server
    if (_groupResponders.find(groupId) != _groupResponders.end()) {
        mDNS::server.removeHandler(*_groupResponders[groupId]);
        _groupResponders.erase(groupId);
    }
    
    // Clean up the web service
    if (_groupWebServices.find(groupId) != _groupWebServices.end()) {
        _groupWebServices.erase(groupId);
    }
    
    // Remove from our list of led groups
    int idx = _leadingGroups.indexOf(groupId);
    if (idx >= 0) {
        _leadingGroups.remove(idx);
    }
    
    debug_i("This controller is no longer leader for group: %s", groupName.c_str());
}