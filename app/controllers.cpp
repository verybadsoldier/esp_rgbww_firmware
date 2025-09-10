#include <RGBWWCtrl.h>
#include <controllers.h>
#include <application.h>

extern Application app;

// Constructor
Controllers::Controllers() : _pingInProgress(false), _pingIndex(0), _pingInterval(10000), _pingTimeout(5000) {
    debug_i("Controllers constructor called");
    
    if (!app.data) {
        debug_e("app.data is NULL in Controllers constructor!");
        return;
    }
    
    debug_i("Controllers constructor: accessing ConfigDB...");
    AppData::Root::Controllers controllers(*app.data);
    size_t count = 0;
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        count++;
        debug_i("Found controller ID: %s", (*it).getId().c_str());
    }
    debug_i("Controllers constructor: found %d controllers in DB", count);
    
    visibleControllers.reserve(std::max(count, static_cast<size_t>(10)));
    debug_i("Controllers constructor completed");
}

// Destructor
Controllers::~Controllers() {
    _pingTimer.stop();
}

// Core methods
void Controllers::addOrUpdate(unsigned int id, const String& hostname, const String& ipAddress, int ttl) {
    debug_i("Controllers::addOrUpdate id=%u, hostname=%s, ip=%s, ttl=%d", id, hostname.c_str(), ipAddress.c_str(), ttl);
    
    // Find existing visible controller
    size_t index = findVisibleControllerIndex(id);

    if (index != INVALID_INDEX) {
        // Update existing
        visibleControllers[index].ttl = ttl;
        visibleControllers[index].state = (ttl > 0) ? ONLINE : OFFLINE;
        visibleControllers[index].pingPending = false;
    } else {
        // Add new visible controller
        VisibleController newController;
        newController.id = id;
        newController.ttl = ttl;
        newController.state = (ttl > 0) ? ONLINE : OFFLINE;
        newController.pingPending = false;
        visibleControllers.push_back(newController);
    }

    AppData::Root::Controllers controllers(*app.data);
    bool foundInConfig = false;
    
    if (auto controllersUpdate = controllers.update()) {
        // Find the specific controller to update (must iterate)
        for (auto controllerItem : controllersUpdate) {
            if (controllerItem.getId() == String(id)) {
                foundInConfig = true;
                #ifdef DEBUG_MDNS
                debug_i("Hostname %s already in list", hostname.c_str());
                #endif

                // Always update IP address
                if (controllerItem.getIpAddress() != ipAddress) {
                    debug_i("IP address changed from %s to %s", 
                           controllerItem.getIpAddress().c_str(), ipAddress.c_str());
                    controllerItem.setIpAddress(ipAddress);
                }
                
                // Only update hostname if this is NOT a group or leader hostname
                if ( controllerItem.getName() != hostname) {
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

    if(!foundInConfig) {
        debug_i("Hostname %s not in list adding to hostname db", hostname.c_str());

        if(auto controllersUpdate = controllers.update()) {
            auto newController = controllersUpdate.addItem();
            newController.setName(hostname);
            newController.setIpAddress(ipAddress);
            newController.setId(String(id));
        } else {
            debug_e("error: failed to add host");
        }
    }

}

void Controllers::updateFromPing(unsigned int id, int ttl) {
    addOrUpdate(id, "", "", ttl);
}

void Controllers::removeExpired(int elapsedSeconds) {
    for (auto& controller : visibleControllers) {
        controller.ttl = std::max(0, controller.ttl - elapsedSeconds);
        if (controller.ttl <= 0) {
            controller.state = OFFLINE;
        }
    }
    
    // Remove controllers that have been offline for too long
    visibleControllers.erase(
        std::remove_if(visibleControllers.begin(), visibleControllers.end(),
            [](const VisibleController& c) { return c.ttl <= -300; }), // Remove after 5 minutes offline
        visibleControllers.end()
    );
}

// Query methods
Controllers::ControllerInfo Controllers::getController(unsigned int id) {
    return findById(id);
}

String Controllers::getIpAddress(unsigned int id) {
    auto info = findById(id);
    return (info.state != NOT_FOUND) ? info.ipAddress : String();
}

String Controllers::getHostname(unsigned int id) {
    auto info = findById(id);
    return (info.state != NOT_FOUND) ? info.hostname : String();
}

unsigned int Controllers::getIdByHostname(const String& hostname) {
    auto info = findByHostname(hostname);
    return (info.state != NOT_FOUND) ? info.id : 0;
}

unsigned int Controllers::getIdByIpAddress(const String& ipAddress) {
    auto info = findByIpAddress(ipAddress);
    return (info.state != NOT_FOUND) ? info.id : 0;
}

uint32_t Controllers::getHighestId() {
    uint32_t highest = 0;
    AppData::Root::Controllers controllers(*app.data);
    for (auto& controller : controllers) {
        uint32_t id = controller.getId().toInt();
        if (id > highest) {
            highest = id;
        }
    }
    return highest;
}

// State checks
bool Controllers::isVisible(unsigned int id) {
    size_t index = findVisibleControllerIndex(id);
    return index != INVALID_INDEX && visibleControllers[index].state == ONLINE;
}

bool Controllers::isVisibleByHostname(const String& hostname) {
    unsigned int id = getIdByHostname(hostname);
    return id != 0 && isVisible(id);
}

bool Controllers::isVisibleByIpAddress(const String& ipAddress) {
    unsigned int id = getIdByIpAddress(ipAddress);
    return id != 0 && isVisible(id);
}

bool Controllers::isPingPending(unsigned int id) {
    size_t index = findVisibleControllerIndex(id);
    return index != INVALID_INDEX && visibleControllers[index].pingPending;
}

int Controllers::getTTL(unsigned int id) {
    size_t index = findVisibleControllerIndex(id);
    return (index != INVALID_INDEX) ? visibleControllers[index].ttl : 0;
}

// Counts
size_t Controllers::getVisibleCount() {
    size_t count = 0;
    for (const auto& controller : visibleControllers) {
        if (controller.state == ONLINE) {
            count++;
        }
    }
    return count;
}

size_t Controllers::getTotalCount() {
    AppData::Root::Controllers controllers(*app.data);
    size_t count = 0;
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        count++;
    }
    return count;
}

// Utility
void Controllers::init(int pingInterval) {
    _pingInterval = pingInterval;
    // Additional initialization if needed
}

void Controllers::update() {
    // Update logic if needed
}

// Iterator implementation
Controllers::Iterator::Iterator(Controllers& mgr, bool atEnd) 
    : manager(mgr), configControllers(*app.data), currentIndex(0), totalCount(0) {
    
    // Count total controllers
    for (auto it = configControllers.begin(); it != configControllers.end(); ++it) {
        totalCount++;
    }
    
    if (atEnd) {
        currentIndex = totalCount;
    }
}

Controllers::ControllerInfo Controllers::Iterator::operator*() {
    AppData::Root::Controllers controllers(*app.data);
    size_t index = 0;
    for (auto it = controllers.begin(); it != controllers.end(); ++it, ++index) {
        if (index == currentIndex) {
            auto& configItem = *it;
            Controllers::ControllerInfo info;
            info.id = configItem.getId().toInt();
            info.hostname = configItem.getName();
            info.ipAddress = configItem.getIpAddress();
            info.state = OFFLINE;
            info.ttl = 0;
            info.pingPending = false;
            
            // Check if controller is visible
            size_t visibleIndex = manager.findVisibleControllerIndex(info.id);
            if (visibleIndex != Controllers::INVALID_INDEX) {
                info.ttl = manager.visibleControllers[visibleIndex].ttl;
                info.state = (info.ttl > 0) ? ONLINE : OFFLINE;
                info.pingPending = manager.visibleControllers[visibleIndex].pingPending;
            } else if (info.hostname.length() == 0 || info.ipAddress.length() == 0) {
                info.state = INCOMPLETE;
            }
            
            return info;
        }
    }
    
    // Return empty info if not found
    return ControllerInfo();
}

Controllers::Iterator& Controllers::Iterator::operator++() {
    currentIndex++;
    return *this;
}

bool Controllers::Iterator::operator==(const Iterator& other) const {
    return currentIndex == other.currentIndex;
}

bool Controllers::Iterator::operator!=(const Iterator& other) const {
    return !(*this == other);
}

Controllers::Iterator Controllers::begin() {
    return Iterator(*this, false);
}

Controllers::Iterator Controllers::end() {
    return Iterator(*this, true);
}

// Helper methods
size_t Controllers::findVisibleControllerIndex(unsigned int id) {
    for (size_t i = 0; i < visibleControllers.size(); i++) {
        if (visibleControllers[i].id == id) {
            return i;
        }
    }
    return INVALID_INDEX;
}

Controllers::ControllerInfo Controllers::findById(unsigned int id) {
    AppData::Root::Controllers controllers(*app.data);
    for (auto& controller : controllers) {
        if (controller.getId().toInt() == id) {
            ControllerInfo info;
            info.id = id;
            info.hostname = controller.getName();
            info.ipAddress = controller.getIpAddress();
            info.state = OFFLINE;
            info.ttl = 0;
            info.pingPending = false;
            
            // Check if visible
            size_t visibleIndex = findVisibleControllerIndex(id);
            if (visibleIndex != INVALID_INDEX) {
                info.ttl = visibleControllers[visibleIndex].ttl;
                info.state = (info.ttl > 0) ? ONLINE : OFFLINE;
                info.pingPending = visibleControllers[visibleIndex].pingPending;
            } else if (info.hostname.length() == 0 || info.ipAddress.length() == 0) {
                info.state = INCOMPLETE;
            }
            
            return info;
        }
    }
    
    return ControllerInfo(); // NOT_FOUND
}

Controllers::ControllerInfo Controllers::findByIpAddress(const String& ipAddress) {
    AppData::Root::Controllers controllers(*app.data);
    for (auto& controller : controllers) {
        if (controller.getIpAddress() == ipAddress) {
            return findById(controller.getId().toInt());
        }
    }
    return ControllerInfo(); // NOT_FOUND
}

Controllers::ControllerInfo Controllers::findByHostname(const String& hostname) {
    AppData::Root::Controllers controllers(*app.data);
    for (auto& controller : controllers) {
        if (controller.getName() == hostname) {
            return findById(controller.getId().toInt());
        }
    }
    return ControllerInfo(); // NOT_FOUND
}

// JSON output methods
Controllers::JsonPrinter Controllers::printJson(Print& printer, JsonFilter filter, bool pretty) {
    return JsonPrinter(printer, *this, filter, pretty);
}

// JsonPrinter implementation
Controllers::JsonPrinter::JsonPrinter(Print& printer, Controllers& mgr, JsonFilter filterType, bool prettyPrint)
    : p(&printer), manager(mgr), currentIndex(0), totalCount(0), pretty(prettyPrint), 
      inObject(false), inArray(false), done(false), filter(filterType), printedCount(0) {
    
    // Count total controllers
    AppData::Root::Controllers controllers(*app.data);
    for (auto it = controllers.begin(); it != controllers.end(); ++it) {
        totalCount++;
    }
}

bool Controllers::JsonPrinter::shouldIncludeController(const Controllers::ControllerInfo& info) {
    bool result;
    switch (filter) {
        case ALL_ENTRIES:
            result= true;
            break;

        case VALID_ONLY:
            result= info.id != 0 && 
                   info.hostname.length() > 0 && 
                   info.ipAddress.length() > 0;
            break;

        case VISIBLE_ONLY:
            result = info.state == ONLINE;
            break;

        default:
            result = false;
    }
    debug_i("%s controller: %u, hostname: %s, ip: %s, state: %d, ttl: %d", result?"return": "skip", info.id, info.hostname.c_str(), info.ipAddress.c_str(), info.state, info.ttl);

    return result;
}

size_t Controllers::JsonPrinter::operator()() {
    if (!p || done) {
        return 0;
    }
    
    size_t n = 0;
    
    // Start of JSON object
    if (currentIndex == 0 && !inObject) {
        n += p->print('{');
        if (pretty) n += p->print('\n');
        n += printIndent(1);
        n += printString("hosts");
        n += p->print(pretty ? ": [" : ":[");
        if (pretty) n += p->print('\n');
        inObject = true;
        inArray = true;
        return n;
    }
    
    // Process controllers
    while (currentIndex < totalCount) {
        // Get controller data from ConfigDB
        AppData::Root::Controllers controllers(*app.data);
        size_t index = 0;
        Controllers::ControllerInfo info;
        bool found = false;
        
        for (auto it = controllers.begin(); it != controllers.end(); ++it, ++index) {
            if (index == currentIndex) {
                auto& configItem = *it;
                info.id = configItem.getId().toInt();
                info.hostname = configItem.getName();
                info.ipAddress = configItem.getIpAddress();
                info.state = OFFLINE;
                info.ttl = 0;
                info.pingPending = false;
                
                // Check if controller is visible (online)
                size_t visibleIndex = manager.findVisibleControllerIndex(info.id);
                if (visibleIndex != INVALID_INDEX) {
                    info.ttl = manager.visibleControllers[visibleIndex].ttl;
                    info.state = (info.ttl > 0) ? ONLINE : OFFLINE;
                    info.pingPending = manager.visibleControllers[visibleIndex].pingPending;
                } else if (info.hostname.length() == 0 || info.ipAddress.length() == 0) {
                    info.state = INCOMPLETE;
                } else {
                    info.state = OFFLINE;
                }
                
                found = true;
                break;
            }
        }
        
        currentIndex++;
        
        if (!found || !shouldIncludeController(info)) {
            continue; // Skip this controller, try next
        }
        
        // Add comma separator if needed
        if (printedCount > 0) {
            n += p->print(',');
            if (pretty) n += p->print('\n');
        }
        
        // Print controller object
        n += printIndent(2);
        n += p->print('{');
        
        n += printProperty("id", String(info.id), false, 3);
        n += printProperty("hostname", info.hostname, false, 3);
        n += printProperty("ip_address", info.ipAddress, false, 3);
        n += printProperty("visible", (info.state == ONLINE), true, 3); // Last property
        
        n += p->print('}');
        
        printedCount++;
        return n; // Return after printing one controller
    }
    
    // End of JSON structure
    if (inArray && !done) {
        if (pretty) {
            n += p->print('\n');
            n += printIndent(1);
        }
        n += p->print("]}");
        done = true;
        return n;
    }
    
    return 0;
}

size_t Controllers::JsonPrinter::printIndent(size_t level) {
    if (!pretty) return 0;
    size_t n = 0;
    for (size_t i = 0; i < level * 2; i++) {
        n += p->print(' ');
    }
    return n;
}

size_t Controllers::JsonPrinter::printString(const String& str) {
    size_t n = 0;
    n += p->print('"');
    // Escape special characters
    for (unsigned i = 0; i < str.length(); i++) {
        char c = str[i];
        switch (c) {
            case '"': n += p->print("\\\""); break;
            case '\\': n += p->print("\\\\"); break;
            case '\n': n += p->print("\\n"); break;
            case '\r': n += p->print("\\r"); break;
            case '\t': n += p->print("\\t"); break;
            default: n += p->print(c); break;
        }
    }
    n += p->print('"');
    return n;
}

size_t Controllers::JsonPrinter::printProperty(const String& name, const String& value, bool isLast, size_t indentLevel) {
    size_t n = 0;
    if (pretty) {
        n += p->print('\n');
        n += printIndent(indentLevel);
    }
    n += printString(name);
    n += p->print(pretty ? ": " : ":");
    n += printString(value);
    if (!isLast) {
        n += p->print(',');
    }
    return n;
}

size_t Controllers::JsonPrinter::printProperty(const String& name, int value, bool isLast, size_t indentLevel) {
    size_t n = 0;
    if (pretty) {
        n += p->print('\n');
        n += printIndent(indentLevel);
    }
    n += printString(name);
    n += p->print(pretty ? ": " : ":");
    n += p->print(value);
    if (!isLast) {
        n += p->print(',');
    }
    return n;
}

size_t Controllers::JsonPrinter::printProperty(const String& name, bool value, bool isLast, size_t indentLevel) {
    size_t n = 0;
    if (pretty) {
        n += p->print('\n');
        n += printIndent(indentLevel);
    }
    n += printString(name);
    n += p->print(pretty ? ": " : ":");
    n += p->print(value ? "true" : "false");
    if (!isLast) {
        n += p->print(',');
    }
    return n;
}

size_t Controllers::JsonPrinter::newline() {
    if (pretty) {
        return p->print('\n');
    }
    return 0;
}

Controllers::JsonStream::JsonStream(Controllers::JsonPrinter&& p) 
    : printer(std::move(p)), bufferPos(0), streamDone(false) {
}

uint16_t Controllers::JsonStream::readMemoryBlock(char* data, int bufSize) {
    if (streamDone) {
        return 0;
    }

    // Fill buffer if needed
    while (bufferPos >= buffer.length() && !printer.isDone()) {
        buffer = "";  // Clear the buffer
        bufferPos = 0;
        
        // Capture next chunk from printer
        class StringCapture : public Print {
        private:
            String& str;
        public:
            StringCapture(String& s) : str(s) {}
            size_t write(uint8_t c) override { str += (char)c; return 1; }
            size_t write(const uint8_t* buf, size_t size) override {
                for (size_t i = 0; i < size; i++) str += (char)buf[i];
                return size;
            }
        };
        
        StringCapture capture(buffer);
        Print* oldPrint = printer.getPrint();  // Use the public getter
        printer.setPrint(&capture);            // Use the public setter
        printer(); // Single call generates next chunk
        printer.setPrint(oldPrint);            // Restore original print target
    }

    if (bufferPos >= buffer.length() && printer.isDone()) {
        streamDone = true;
        return 0;
    }

    // Copy from buffer to output
    size_t available = buffer.length() - bufferPos;
    size_t copySize = std::min((size_t)bufSize, available);
    
    // Ensure we don't exceed uint16_t range
    if (copySize > UINT16_MAX) {
        copySize = UINT16_MAX;
    }
    
    if (copySize > 0) {
        memcpy(data, buffer.c_str() + bufferPos, copySize);
        bufferPos += copySize;
    }

    return (uint16_t)copySize;
}

bool Controllers::JsonStream::isFinished() {
    return streamDone;
}

Controllers::JsonStream* Controllers::createJsonStream(JsonFilter filter, bool pretty) {
    class DummyPrint : public Print {
    public:
        size_t write(uint8_t) override { return 1; }
    };
    
    static DummyPrint dummyPrint;
    auto printer = printJson(dummyPrint, filter, pretty);
    return new JsonStream(std::move(printer));
}