#include <Network/Mdns/Responder.h>
#include <Network/Mdns/debug.h>

class mdnsHandler{
    public:
        mdnsHandler();
        virtual ~mdnsHandler(){};
        bool onMessage(mDNS::Message& message);
        void sendSearch();
        void start();

    private:
        

};

class LEDControllerAPIService : public mDNS::Service{
    public:

        String getInstance() override{
		    return F("esprgbwwAPI");
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
		    return F("ws");
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