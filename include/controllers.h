#pragma once

#include <app-data.h>
#include <Network/HttpClient.h>
#include <Timer.h>
#include <Data/Stream/DataSourceStream.h>
#include <vector>
#include <algorithm>

class Controllers {
public:
    enum ControllerState {
        NOT_FOUND, INCOMPLETE, OFFLINE, ONLINE
    };

    enum JsonFilter {
        ALL_ENTRIES, VALID_ONLY, VISIBLE_ONLY
    };

    struct ControllerInfo {
        unsigned int id = 0;
        String hostname;
        String ipAddress;
        ControllerState state = NOT_FOUND;
        int ttl = 0;
        bool pingPending = false;
    };

    struct VisibleController {
        unsigned int id;
        int ttl;
        ControllerState state;
        bool pingPending = false;
    };

    class Iterator {
    private:
        Controllers& manager;
        AppData::Root::Controllers configControllers;
        size_t currentIndex;
        size_t totalCount;

    public:
        Iterator(Controllers& mgr, bool atEnd = false);
        ControllerInfo operator*();
        Iterator& operator++();
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
    };

    class JsonPrinter {
    private:
        Print* p;
        Controllers& manager;
        size_t currentIndex;
        size_t totalCount;
        bool pretty;
        bool inObject;
        bool inArray;
        bool done;
        JsonFilter filter;
        size_t printedCount;
        
        size_t printIndent(size_t level);
        size_t printString(const String& str);
        size_t printProperty(const String& name, const String& value, bool isLast = false, size_t indentLevel = 2);
        size_t printProperty(const String& name, int value, bool isLast = false, size_t indentLevel = 2);
        size_t printProperty(const String& name, bool value, bool isLast = false, size_t indentLevel = 2);
        size_t newline();
        bool shouldIncludeController(const Controllers::ControllerInfo& info);
        
    public:
        JsonPrinter(Print& printer, Controllers& mgr, JsonFilter filterType = VALID_ONLY, bool prettyPrint = false);
        size_t operator()();
        bool isDone() const { return done; }
        
        // Add these public methods for JsonStream to use:
        Print* getPrint() const { return p; }
        void setPrint(Print* newPrint) { p = newPrint; }
        
        friend class JsonStream; // Keep friend access
    };

    // JsonStream class nested inside Controllers
    class JsonStream : public IDataSourceStream {
    private:
        JsonPrinter printer;
        String buffer;
        size_t bufferPos;
        bool streamDone;

    public:
        JsonStream(JsonPrinter&& p);
        uint16_t readMemoryBlock(char* data, int bufSize) override;
        bool isFinished() override;
    };

    // Constructor/Destructor
    Controllers();
    ~Controllers();

    // Core methods
    void addOrUpdate(unsigned int id, const String& hostname, const String& ipAddress, int ttl);
    void updateFromPing(unsigned int id, int ttl);
    void removeExpired(int elapsedSeconds);
    
    // Query methods
    ControllerInfo getController(unsigned int id);
    String getIpAddress(unsigned int id);
    String getHostname(unsigned int id);
    unsigned int getIdByHostname(const String& hostname);
    unsigned int getIdByIpAddress(const String& ipAddress);
    uint32_t getHighestId();
    
    // State checks
    bool isVisible(unsigned int id);
    bool isVisibleByHostname(const String& hostname);
    bool isVisibleByIpAddress(const String& ipAddress);
    bool isPingPending(unsigned int id);
    int getTTL(unsigned int id);
    
    // Counts
    size_t getVisibleCount();
    size_t getTotalCount();
    
    // Utility
    void init(int pingInterval = 10000);
    void update();
    void forgetControllers();

    // Iterator support
    Iterator begin();
    Iterator end();
    
    // JSON output methods
    JsonPrinter printJson(Print& printer, JsonFilter filter = VALID_ONLY, bool pretty = false);
    JsonStream* createJsonStream(JsonFilter filter = VALID_ONLY, bool pretty = false);

private:
    static const size_t INVALID_INDEX = SIZE_MAX;
    
    std::vector<VisibleController> visibleControllers;
    
    // Ping management
    bool _pingInProgress;
    size_t _pingIndex;
    int _pingInterval;
    int _pingTimeout;
    std::vector<unsigned int> _controllersToPing;
    Timer _pingTimer;
    HttpClient _pingClient;
    
    // Helper methods
    size_t findVisibleControllerIndex(unsigned int id);
    ControllerInfo findById(unsigned int id);
    ControllerInfo findByIpAddress(const String& ipAddress);
    ControllerInfo findByHostname(const String& hostname);
};