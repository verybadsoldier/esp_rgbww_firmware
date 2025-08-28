#include <ArduinoJson.h>
#include <mdnshandler.h>
#include <RGBWWCtrl.h>
#include "application.h"
#include "app-data.h"
#include <Network/Http/HttpRequest.h>
#include <Network/Http/HttpClient.h>


#define DEBUG_MDNS 

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
    
    // Get device hostname from configuration first (moved up)
    String hostName;
    {
        AppConfig::Network network(*app.cfg);
        hostName = network.mdns.getName();
        if (hostName.length() == 0) {
            // Generate hostname from MAC if not set
            hostName = "lightinator-" + String(system_get_chip_id());
        }
        hostName = Util::sanitizeHostname(hostName);
    }
    
    // Create device web service with proper hostname
    deviceWebService = std::make_unique<LEDControllerWebService>(hostName, 
        LEDControllerWebService::HostType::Device);

    checkGroupLeadership();
    
    // Initialize primary responder with device hostname
    primaryResponder.begin(hostName.c_str());
    #ifdef DEBUG_MDNS
    debug_i("Registered hostname: %s", hostName.c_str());
    #endif

    // Add services to the primary responder
    primaryResponder.addService(ledControllerAPIService);
    primaryResponder.addService(*deviceWebService);  // Note the * to dereference
    
    // Store global reference for API service
    g_ledControllerAPIService = &ledControllerAPIService;

    // Set up leadership election with delay
    #ifdef DEBUG_MDNS
    debug_i("starting leader election timer timer");
    #endif 
    _leaderElectionTimer.setCallback(mdnsHandler::checkForLeadershipCb, this);
    _leaderElectionTimer.setIntervalMs(_mdnsTimerInterval * LEADER_ELECTION_DELAY);
    _leaderElectionTimer.startOnce();
    
    // Set search name for discovering other controllers
    setSearchName(F("esprgbwwAPI._http._tcp.local"));
    
    // Set up timer for periodic mDNS searches
    #ifdef DEBUG_MDNS
    debug_i("starting mDNS search timer");
    #endif 
    _mdnsSearchTimer.setCallback(mdnsHandler::sendSearchCb, this);
    _mdnsSearchTimer.setIntervalMs(_mdnsTimerInterval);
    _mdnsSearchTimer.startOnce();

    // Register handlers with mDNS server
    mDNS::server.addHandler(primaryResponder);
    mDNS::server.addHandler(*this); 
    #ifdef DEBUG_MDNS
    debug_i("mDNS server started");
    #endif
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

    auto srv_answer = message[mDNS::ResourceType::SRV];
    if(srv_answer == nullptr) {
#ifdef DEBUG_MDNS
        debug_i("No SRV record in this message");
#endif
        // Let's check if this is a direct A record response without SRV
        auto a_answer = message[mDNS::ResourceType::A]; 
        if(a_answer != nullptr) {
            // Process hostname A record response
            return processHostnameARecord(message, a_answer);
        }
        return false;
    }
    
    String answerName = String(srv_answer->getName());
#ifdef DEBUG_MDNS
    debug_i("\nanswerName: %s\nsearchName: %s", answerName.c_str(), searchName.c_str());
#endif
    
    // Check if this is an API service response or a hostname response
    if(answerName == searchName) {
        // This is an API service response - process as before
        return processApiServiceResponse(message);
    } else if(answerName.endsWith("._http._tcp.local")) {
        // This is likely a hostname response
        String hostname = answerName.substring(0, answerName.indexOf("._http._tcp.local"));
        #ifdef DEBUG_MDNS
        debug_i("Processing hostname response for: %s", hostname.c_str());
        #endif

        // Extract hostname data from the message
        return processHostnameResponse(message, hostname);
    }
    
    // Not a response we're interested in
    return false;
}

// Process API service responses (existing logic)
bool mdnsHandler::processApiServiceResponse(mDNS::Message& message) 
{
    using namespace mDNS;
    bool msgHasA = false, msgHasTXT = false;
    
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

    auto answer = message[mDNS::ResourceType::A];
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
            
            // Check for leader
            String isLeaderTxt = txt["isLeader"];
            if (isLeaderTxt == "1") {
                _leaderDetected = true;
                #ifdef DEBUG_MDNS
                debug_i("Detected leader: %s", info.hostName.c_str());
                #endif
            }
            
            // Get hostname type
            String hostnameType = txt["type"];
            if (hostnameType=="") hostnameType="undefined";
            #ifdef DEBUG_MDNS
            debug_i("Hostname %s, type: %s", info.hostName.c_str(), hostnameType.c_str());
            #endif

            // Only add to host table if this is a device hostname
            if (hostnameType == "host") {
                addHost(info.hostName, info.ipAddr.toString(), info.ttl, info.ID);
            } else {
                // For leader/group hostnames, just log them
                #ifdef DEBUG_MDNS
                debug_i("Detected %s hostname: %s (ID: %u)", 
                     hostnameType.c_str(), info.hostName.c_str(), info.ID);
                #endif
            }
        }
        return true;
    } else {
        return false;
    }
}

// Process hostname A record responses
bool mdnsHandler::processHostnameARecord(mDNS::Message& message, mDNS::Answer* a_answer) {    
    using namespace mDNS;
    
    // Extract hostname from A record
    String hostname = String(a_answer->getName());
    
    // Remove .local suffix if present
    if (hostname.endsWith(".local")) {
        hostname = hostname.substring(0, hostname.lastIndexOf(".local"));
    }
    
    // Get IP address from A record
    String ipAddress = String(a_answer->getRecordString());
    unsigned int ttl = a_answer->getTtl();
    #ifdef DEBUG_MDNS
    debug_i("Got A record for hostname: %s, IP: %s", hostname.c_str(), ipAddress.c_str());
    #endif

    // Look up ID by hostname in our persistent controller database
    unsigned int controllerId = 0;
    AppData::Root::Controllers controllers(*app.data);
    
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        String storedName = (*it).getName();
        
        // Case-insensitive comparison
        if (hostname.equalsIgnoreCase(storedName)) {
            controllerId = (*it).getId().toInt();
            #ifdef DEBUG_MDNS
            debug_i("Found matching controller ID: %u", controllerId);
            #endif
            break;
        }
    }
    
    // Only process if we found the controller ID
    if (controllerId > 0) {
        addHost(hostname, ipAddress, ttl, controllerId);
        return true;
    }
    
    // Save this information for later matching with TXT records
    _pendingHostnameResolutions[hostname] = ipAddress;

    #ifdef DEBUG_MDNS
    debug_i("Hostname stored for later ID resolution: %s", hostname.c_str());
    #endif

    return false; // Not fully processed yet
}

// Process hostname responses with potential SRV records
bool mdnsHandler::processHostnameResponse(mDNS::Message& message, const String& hostname) 
{
    using namespace mDNS;
    String controllerType;
    
    String ipAddress;
    unsigned int ttl = 60; // Default TTL
    
    {
        // Get A record if available
        auto a_answer = message[mDNS::ResourceType::A];
 
        if (a_answer != nullptr) {
            ipAddress = String(a_answer->getRecordString());
            ttl = a_answer->getTtl();
            #ifdef DEBUG_MDNS
            debug_i("Hostname IP address: %s (TTL: %u)", ipAddress.c_str(), ttl);
            #endif
        } else {
            // No A record, can't proceed
            return false;
        }
    }
        
    // Try to get TXT record for ID
    {
        auto txt_answer = message[mDNS::ResourceType::TXT];
        unsigned int controllerId = 0;
        
        if (txt_answer != nullptr) {
            mDNS::Resource::TXT txt(*txt_answer);
            controllerId = txt["id"].toInt();
            controllerType = txt["type"];
            #ifdef DEBUG_MDNS
            debug_i("Found controller ID: %u, type: %s", controllerId, controllerType.c_str());
            #endif

            // If we have an ID and it's a host type, add the host
            if (controllerId > 0 && controllerType == "host") {
                addHost(hostname, ipAddress, ttl, controllerId);
                return true;
            } else if (controllerId > 0) {
                // Log but don't add non-host entries
                #ifdef DEBUG_MDNS
                debug_i("Ignoring non-host entry: %s (ID: %u, type: %s)",
                    hostname.c_str(), controllerId, controllerType.c_str());
                #endif
            }
        }
    }
    // No valid TXT record or not a host type - don't fall back to hostname lookup
    #ifdef DEBUG_MDNS
    debug_i("No valid host TXT record found for %s - ignoring", hostname.c_str());
    #endif
    return false;
}

void mdnsHandler::sendSearch()
{
    static unsigned long lastLeaderCheck = 0;
    static uint8_t queryIndex = 0;
    unsigned long now = millis();

    // Search for the service
    bool ok = mDNS::server.search(service);
#ifdef DEBUG_MDNS
    debug_i("search('%s'): %s", service.c_str(), ok ? "OK" : "FAIL");
#endif

    // Query for known controllers directly to improve discovery
    queryKnownControllers(queryIndex);
    
    // Increment index for next batch of controllers
    queryIndex++;
    if (queryIndex >= 5) queryIndex = 0;  // reset after covering all controllers in batches


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

/**
 * @brief Query known controllers directly to improve discovery reliability
 * 
 * This method reads the persistent storage to find controllers that have been
 * discovered before and sends direct mDNS queries for them, rather than just
 * waiting to discover them through service announcements.
 * 
 * @param batchIndex Which subset of controllers to query (to avoid congestion)
 */
void mdnsHandler::queryKnownControllers(uint8_t batchIndex) 
{
    // Access the controllers from persistent storage
    AppData::Root::Controllers controllers(*app.data);
    
    // Get total count of controllers to determine batch size
    size_t totalControllers = 0;
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        totalControllers++;
    }
    
    if (totalControllers == 0) {
        return; // No controllers to query
    }
    
    // Calculate how many controllers to query per batch (5 batches total)
    // Always at least 1, at most 20% of total controllers per batch
    size_t batchSize = max((size_t)1, totalControllers / 5);
    size_t startIdx = batchIndex * batchSize;
    size_t endIdx = min(startIdx + batchSize, totalControllers);
    
    // Skip this batch if it's out of range
    if (startIdx >= totalControllers) {
        return;
    }
    
    // Process controllers in the current batch
    size_t currentIdx = 0;
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        // Skip controllers not in the current batch
        if (currentIdx < startIdx) {
            currentIdx++;
            continue;
        }
        
        if (currentIdx >= endIdx) {
            break;
        }

        String hostname = (*it).getName();
        String hostname_local = hostname + ".local";
        

        #ifdef DEBUG_MDNS
        debug_i("starting mdns query for %s", hostname_local.c_str());
        #endif
        
        // Query for this host using standard search
        mDNS::server.search(hostname_local.c_str(), mDNS::ResourceType::PTR);
        
        // Also search for the API service
        String api_service = "esprgbwwAPI._http._tcp.local";
        mDNS::server.search(api_service, mDNS::ResourceType::PTR);

        
         // Start ping for this controller

        String ipAddress=(*it).getIpAddress();
        #ifdef DEBUG_MDNS
        debug_i("starting ping on %s", ipAddress.c_str());
        #endif
        if (ipAddress.length() > 0 && ipAddress != "0.0.0.0") {
            // Mark pingPending in visibleControllers
            unsigned int id = (*it).getId().toInt();
            for (auto& ctrl : app.visibleControllers) {
                if (ctrl.id == id) {
                    ctrl.pingPending = true;
                    break;
                }
            }
            HttpClient* client = new HttpClient();
            client->downloadString("http://" + ipAddress + "/ping", RequestCompletedDelegate(&mdnsHandler::pingCallback, this));
        }


        #ifdef DEBUG_MDNS
        debug_i("Querying for controller: %s", hostname.c_str());
        #endif

        currentIdx++;
    }
}

void mdnsHandler::addHost(const String& hostname, const String& ip_address, int ttl, unsigned int id)
{
    if(id == 1) return; // Invalid ID

    // Define isGroupOrLeaderHostname before using it (new code)
    bool isGroupOrLeaderHostname = false;
    
    // For group hostnames, we already filter by type in onMessage()
    // This is just a fallback safety check 

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
                #ifdef DEBUG_MDNS
                debug_i("Hostname %s already in list", hostname.c_str());
                #endif

                // Always update IP address
                if (controllerItem.getIpAddress() != ip_address) {
                    #ifdef DEBUG_MDNS
                    debug_i("IP address changed from %s to %s", 
                           controllerItem.getIpAddress().c_str(), ip_address.c_str());
                    #endif
                    controllerItem.setIpAddress(ip_address);
                }
                
                // Only update hostname if this is NOT a group or leader hostname
                if (!isGroupOrLeaderHostname && controllerItem.getName() != hostname) {
                    #ifdef DEBUG_MDNS
                    debug_i("Hostname changed from %s to %s", 
                           controllerItem.getName().c_str(), hostname.c_str());
                    #endif
                    controllerItem.setName(hostname);
                }
                break;
            }
        }
    } else {
        debug_e("error: failed to open hosts db for update");
    }

    if(!found) {
        #ifdef DEBUG_MDNS
        debug_i("Hostname %s not in list adding to hostname db", hostname.c_str());
        #endif

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
        // For mDNS discoveries, use the standard mDNS TTL
        app.addOrUpdateVisibleController(id, TTL_MDNS);

        #ifdef DEBUG_MDNS
        debug_i("Controller %s with ID %i is now visible", hostname.c_str(), id);
        #endif
        // Controller has just become visible, update the clients
        StaticJsonDocument<256> doc;
        JsonObject newHost = doc.to<JsonObject>();
        newHost[F("id")] = id;
        newHost[F("ttl")] = TTL_MDNS;
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
        #ifdef DEBUG_MDNS
        debug_i("Leader already exists in network, not becoming leader");
        #endif
        _leaderCheckCounter = 0;  // Reset counter when a leader is detected
        return;
    }

    #ifdef DEBUG_MDNS
    debug_i("No leader detected (check round %d)", _leaderCheckCounter + 1);
    #endif
    
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
            #ifdef DEBUG_MDNS
            debug_i("No leader detected and we have highest ID, becoming leader");
            #endif
        } else {
            #ifdef DEBUG_MDNS
            debug_i("No leader detected after %d checks, becoming leader as a failsafe", LEADERSHIP_MAX_FAIL_COUNT);
            #endif
        }
        
        becomeLeader();
        _leaderCheckCounter = 0;  // Reset counter
    } else {
        #ifdef DEBUG_MDNS
        debug_i("Not becoming leader, another controller has higher ID (check %d/%d)", 
                _leaderCheckCounter, LEADERSHIP_MAX_FAIL_COUNT);
        #endif

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
        leaderWebService = std::make_unique<LEDControllerWebService>("lightinator", 
            LEDControllerWebService::HostType::Leader);        
        // Create new responder for "lightinator.local"
        leaderResponder = std::make_unique<mDNS::Responder>();
        leaderResponder->begin("lightinator");
        
        // Add services to the leader responder
        leaderResponder->addService(*leaderWebService);  // Note the * to dereference
        
        // Register the leader responder with mDNS server
        mDNS::server.addHandler(*leaderResponder);

        #ifdef DEBUG_MDNS
        debug_i("This controller is now the global leader (lightinator.local)");
        #endif
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
        #ifdef DEBUG_MDNS
        debug_i("This controller is no longer the global leader");
        #endif
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
                #ifdef DEBUG_MDNS
                debug_i("This device should be the leader for group: %s", groupName.c_str());
                #endif
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

    #ifdef DEBUG_MDNS
    debug_i("Updated service TXT records with %d group memberships and %d leading groups", 
            memberGroups.size(), _leadingGroups.size());
    #endif
}

void mdnsHandler::becomeGroupLeader(const String& groupId, const String& groupName) {
    #ifdef DEBUG_MDNS
    debug_i("Becoming leader for group: %s (ID: %s)", groupName.c_str(), groupId.c_str());
    #endif

    // Sanitize the group name for use as a hostname
    String sanitizedName = Util::sanitizeHostname(groupName);
    #ifdef DEBUG_MDNS
    debug_i("Sanitized group name: %s", sanitizedName.c_str());
    #endif

    // Create responder for this group's hostname
    auto responder = std::make_unique<mDNS::Responder>();
    responder->begin(sanitizedName.c_str());
    
    // Create and set up web service for this group
    auto webService = std::make_unique<LEDControllerWebService>(sanitizedName, LEDControllerWebService::HostType::Group);    
    // Add services to the responder
    responder->addService(*webService);
    
    // Register with mDNS server
    mDNS::server.addHandler(*responder);
    
    // Store in our maps
    _groupResponders[groupId] = std::move(responder);
    _groupWebServices[groupId] = std::move(webService);
    
    // Track that we're now leading this group
    _leadingGroups.add(groupId);

    #ifdef DEBUG_MDNS
    debug_i("This controller is now leader for group: %s (%s.local)", 
           groupName.c_str(), sanitizedName.c_str());
    #endif
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

    #ifdef DEBUG_MDNS
    debug_i("Relinquishing leadership for group: %s (ID: %s)", 
           groupName.c_str(), groupId.c_str());
    #endif

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

    #ifdef DEBUG_MDNS
    debug_i("This controller is no longer leader for group: %s", groupName.c_str());
    #endif
}



// The callback function for all pings
int mdnsHandler::pingCallback(HttpConnection& connection, bool successful)
{
    String ip = connection.getRemoteIp().toString();
    unsigned int id = app.getControllerIdforIpAddress(ip);

    // Find the controller in visibleControllers and update status
    for (auto& ctrl : app.visibleControllers) {
        if (ctrl.id == id) {
            ctrl.pingPending = false;
            ctrl.ttl = TTL_HTTP_VERIFIED;
            #ifdef DEBUG_MDNS
            debug_i("Ping successful for controller: %d", id);
            #endif
            break;
        }
    }
    return 0;
}