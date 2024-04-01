/**
 * @file
 * @author  Patrick Jahns http://github.com/patrickjahns
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of<
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 *
 */

#ifdef __riscv
//#if SMING_SOC==esp32c3
//#warning "redefining INT32 to be int, not long int for riscv based esp32c3"
#undef __INT32_TYPE__
#define __INT32_TYPE__      int

#undef __UINT32_TYPE__
#define __UINT32_TYPE__     unsigned int

#endif // __riscv

#include <RGBWWCtrl.h>
#include <Ota/Upgrader.h>
#include <SmingCore.h>
#include <Storage/SysMem.h>
#include <Storage/ProgMem.h>
#include <Storage/Debug.h>

#ifdef ARCH_ESP8266
#include <Platform/OsMessageInterceptor.h>

static OsMessageInterceptor osMessageInterceptor;

/**
 * @brief See if the OS debug message is something we're interested in.
 * @param msg
 * @retval bool true if we want to report this
 */
static bool __noinline parseOsMessage(OsMessage& msg)
{
	m_printf(_F("[OS] %s\r\n"), msg.getBuffer());
	if(msg.startsWith(_F("E:M "))) {
		Serial.println(_F("** OS Memory Error **"));
		return true;
	}
	if(msg.contains(_F(" assert "))) {
		Serial.println(_F("** OS Assert **"));
		return true;
	}
	if(msg.contains(_F("vPortFree"))) {
		Serial.println(_F("** vPortFree **"));
		return true;
	}
	return false;
}

/**
 * @brief Called when the OS outputs a debug message using os_printf, etc.
 * @param msg The message
 */
static void onOsMessage(OsMessage& msg)
{
	// Note: We do the check in a separate function to avoid messing up the stack pointer
	if(parseOsMessage(msg)) {
		if(gdb_present() == eGDB_Attached) {
			gdb_do_break();
		} else {
//#ifdef ARCH_ESP8266
			register uint32_t sp __asm__("a1");
			debug_print_stack(sp + 0x10, 0x3fffffb0);
//#endif
		}
	}
}
#endif

#ifdef ARCH_ESP8266

// include partition file  and rboot for initial OTA
namespace
{
// Note: This file won't exist on initial build!
IMPORT_FSTR(partitionTableData, PROJECT_DIR "/out/Esp8266/debug/firmware/partitions.bin")
//IMPORT_FSTR(rbootData, PROJECT_DIR "/out/Esp8266/debug/firmware/rboot.bin")
} // namespace

extern "C" void __wrap_user_pre_init(void)
{
	static_assert(PARTITION_TABLE_OFFSET == 0x3fa000, "Bad PTO");
	Storage::initialize();
	auto& flash = *Storage::spiFlash;
	if(!flash.partitions()) {
		{
			LOAD_FSTR(data, partitionTableData)
			flash.erase_range(PARTITION_TABLE_OFFSET, flash.getBlockSize());
			flash.write(PARTITION_TABLE_OFFSET, data, partitionTableData.size());
			flash.loadPartitions(PARTITION_TABLE_OFFSET);
		}
       	
	}

	extern void __real_user_pre_init(void);
	__real_user_pre_init();
}

#endif

Application app;

// Sming Framework INIT method - called during boot
void init() {
    Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
    Serial.systemDebugOutput(true); // Debug output to serial
    //System.setCpuFrequencye(CF_160MHz);

#ifdef ARCH_ESP8266
	osMessageInterceptor.begin(onOsMessage);
    debug_i("starting os message interceptor");
#endif

    // set CLR pin to input
    pinMode(CLEAR_PIN, INPUT);

    // seperated application init
    app.init();

    // Run Services on system ready
    System.onReady(SystemReadyDelegate(&Application::startServices, &app));
}

Application::~Application() {
    if (pNtpclient != nullptr) {
        delete pNtpclient;
        pNtpclient = nullptr;
    }
}

void Application::uptimeCounter() {
    ++_uptimeMinutes;
}

void Application::init() {
    app.ota.checkAtBoot();
    for(int i=0;i<10;i++){
        Serial.print(_F("="));
        delay(200);
    }
    Serial.print("\r\n");
    
    debug_i("RGBWW Controller v %s\r\n", fw_git_version);
    #ifdef ARCH_ESP8266
        debug_i("Platform: Esp8266\r\n");
    #endif
    debug_i("Application::init - partition scheme: %s\r\n", fw_part_layout);

    //load settings
    _uptimetimer.initializeMs(60000, TimerDelegate(&Application::uptimeCounter, this)).start();

#ifdef ARCH_ESP8266
    // load boot information
    uint8 bootmode, bootslot;
    debug_i("Application::init - loading boot info");
    if (rboot_get_last_boot_mode(&bootmode)) {
        if (bootmode == MODE_TEMP_ROM) {
            debug_i("Application::init - temp boot, rebooting after OTA");
            System.restart();
        } else {
            debug_i("Application::init - normal boot");
        }
        _bootmode = bootmode;
    }
#endif

    debug_i("going to initialize config");
    config::initializeConfig(cfg); // initialize the config structure if necessary
    // list spiffs partitions 
    listSpiffsPartitions();

    // mount filesystem
    auto romPartition=app.ota.getRomPartition();

    debug_i("Application::init - got rom partition %s @0x%#08x", romPartition.name(),romPartition.address());
    auto spiffsPartition=app.ota.findSpiffsPartition(romPartition);
    debug_i("Application::init - mounting filesystem  %s @0x%#08x",spiffsPartition.name(),spiffsPartition.address());
    _fs_mounted=spiffs_mount(spiffsPartition);
    /*
    if(_fs_mounted) {
        Directory dir;
        if(dir.open()) {
            while(dir.next()) {
                Serial.print("  ");
                Serial.println(dir.stat().name);
            }
        }
        Serial << dir.count() << _F(" files found") << endl << endl;
    }
    */
   
    // check if we need to reset settings
    if (digitalRead(CLEAR_PIN) < 1) {
        debug_i("CLR button low - resetting settings");
        cfg.reset();
        network.forgetWifi();
    }

    // check ota
#ifdef ARCH_ESP8266
    ota.checkAtBoot();
#endif

    // load config
    if (cfg.exist()) {
        cfg.load();
    } else {
        debug_i("Application::init - first run");
        _first_run = true;
        cfg.save();
    }

    mqttclient.init();

    // initialize led ctrl
    rgbwwctrl.init();

    initButtons();

    // initialize networking
    network.init();

    // initialize webserver

    app.webserver.init();

    if (cfg.ntp.enabled) {
        String server = cfg.ntp.server.length() > 0 ? cfg.ntp.server : NTP_DEFAULT_SERVER;
        unsigned interval = cfg.ntp.interval > 0 ? cfg.ntp.interval : NTP_DEFAULT_AUTOQUERY_SECONDS;
        debug_i("Enabling NTP server '%s' with interval %d s", server.c_str(), interval);
        pNtpclient = new NtpClient(server, interval);
    }
    else {
        debug_i("Disabling NTP server");
    }
}

void Application::initButtons() {
    if (cfg.general.buttons_config.length() <= 0)
        return;

    debug_i("Configuring buttons using string: '%s'", cfg.general.buttons_config.c_str());

    Vector<String> buttons;
    splitString(cfg.general.buttons_config, ',', buttons);

    for(uint32_t i=0; i < buttons.count(); ++i) {
        if (buttons[i].length() == 0)
            continue;

        uint32_t pin = buttons[i].toInt();
        if (pin >= _lastToggles.size()) {
            debug_i("Pin %d is invalid. Max is %d", pin, _lastToggles.size() - 1);
            continue;
        }
        debug_i("Configuring button: '%s'", buttons[i].c_str());

        _lastToggles[pin] = 0ul;

        attachInterrupt(pin,  std::bind(&Application::onButtonTogglePressed, this, pin), FALLING);
        pinMode(pin, INPUT_PULLUP);
    }
}

// Will be called when system initialization was completed
void Application::startServices() {
    debug_i("Application::startServices");
    rgbwwctrl.start();
    webserver.start();

    if (cfg.events.server_enabled)
        eventserver.start(app.webserver);
}

void Application::restart() {
    debug_i("Application::restart");
    if (network.isApActive()) {
        network.stopAp();
        _systimer.initializeMs(500, TimerDelegate(&Application::restart, this)).startOnce();
    }
    System.restart();
}

void Application::reset() {
    debug_i("Application::reset");
    cfg.reset();
    rgbwwctrl.colorReset();
    network.forgetWifi();
    delay(500);
    restart();
}

void Application::forget_wifi_and_restart() {
    debug_i("Application::forget_wifi_and_restart");
    network.forgetWifi();
    _systimer.initializeMs(500, TimerDelegate(&Application::restart, this)).startOnce();
}

bool Application::delayedCMD(String cmd, int delay) {
    debug_i("Application::delayedCMD cmd: %s - delay: %i", cmd.c_str(), delay);
    if (cmd.equals(F("reset"))) {
        _systimer.initializeMs(delay, TimerDelegate(&Application::reset, this)).startOnce();
    } else if (cmd.equals(F("restart"))) {
        _systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
    } else if (cmd.equals("stopap")) {
        network.stopAp(2000);
    } else if (cmd.equals("forget_wifi")) {
        _systimer.initializeMs(delay, TimerDelegate(&AppWIFI::forgetWifi, &network)).startOnce();
    } else if (cmd.equals("forget_wifi_and_restart")) {
        network.forgetWifi();
        _systimer.initializeMs(delay, TimerDelegate(&Application::forget_wifi_and_restart, this)).startOnce();
    } else if (cmd.equals("umountfs")) {
        //umountfs();
    } else if (cmd.equals("mountfs")) {
        //
    } else if (cmd.equals("switch_rom")) {
        switchRom();
        //_systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
    } else {
        return false;
    }
    return true;
}


void Application::listSpiffsPartitions()
{
	Serial.println(_F("** Enumerate registered SPIFFS partitions"));
	for(auto part : Storage::findPartition(Storage::Partition::SubType::Data::spiffs)) {
        debug_i("checking filesystem  %s @0x%#08x",part.name(),part.address());

		bool ok = spiffs_mount(part);
		Serial.println(ok ? "OK, listing files:" : "Mount failed!");
		if(ok) {
			Directory dir;
			if(dir.open()) {
				while(dir.next()) {
					Serial.print("  ");
					Serial.println(dir.stat().name);
				}
			}
			Serial << dir.count() << _F(" files found") << endl << endl;
		}
	}
}

void Application::mountfs(int slot) {
    debug_i("Application::mountfs rom slot: %i", slot);
    // auto part = OtaUpgrader::getPartitionForSlot(slot);
    auto part=Storage::findPartition(F("spiffs")+String(slot));
    //auto part = Storage::findPartition(F("spiffs0"));
    debug_i("Application::mountfs trying to mount spiffs at %x, length %d",
            part.address(), part.size());
    _fs_mounted = spiffs_mount(part);
    _fs_mounted ? debug_i("OK, listing files:") : debug_i("Mount failed!");
    
    if(_fs_mounted) {
        if(FileHandle file = fileOpen(F("VERSION"), IFS::OpenFlag::Read)) {
            debug_i("found VERSION file");
            char buffer[64];
            int bytesRead=fileRead(file,buffer,sizeof(buffer));
            buffer[bytesRead]='\0';
            debug_i("\nweb app version String: %s", buffer);
            fileClose(file);
        } else {
            debug_i("Partition has no version file\n");
        }

        Directory dir;
        if(dir.open()) {
            while(dir.next()) {
                Serial.print("  ");
                Serial.println(dir.stat().name);
            }
        }
        debug_i("%i files found", dir.count());
    }       
}

void Application::umountfs() {
    debug_i("Application::umountfs");
    //spiffs_unmount();
}

void Application::switchRom() {
    //ToDo - rewrite to use ota.getRunningPartition() and ota.getNextBootPartition()
    debug_i("Application::switchRom");

    /* old

    int slot = getRomSlot();
    debug_i("    current ROM: %i", slot);
    if (slot == 0) {
        slot = 1;
    } else {
        slot = 0;
    }
#ifdef ARCH_ESP8266
    debug_i("    switching to ROM %i\r\n",slot);
    rboot_set_current_rom(slot);

#endif
    */
   app.ota.doSwitch();
}

int Application::getRomSlot(){
            auto partition = app.ota.getRomPartition();
            uint8_t slot;
            if(partition.name()=="rom1"){
                slot=1;
            }else{
                slot=0;
            }
            return slot;
}

void Application::wsBroadcast(String message) {
    debug_i("Application::wsBroadcast");
    app.webserver.wsBroadcast(message);
}

void Application::wsBroadcast(String cmd, String message){
     JsonRpcMessage msg(cmd);
            JsonObject root = msg.getParams();   
            root["message"] = message;
            String jsonStr = Json::serialize(msg.getRoot());
            wsBroadcast(jsonStr);
}

void Application::onCommandRelay(const String& method, const JsonObject& params) {
    if (!cfg.sync.cmd_master_enabled)
        return;

    mqttclient.publishCommand(method, params);
}

void Application::onButtonTogglePressed(int pin) {
    uint32_t now = millis();
    uint32_t diff = now - _lastToggles[pin];
    if (diff > (uint32_t) cfg.general.buttons_debounce_ms) {  // debounce
        debug_i("Button %d pressed - toggle", pin);
        rgbwwctrl.toggle();
        _lastToggles[pin] = now;
    }
    else {
        debug_d("Button press ignored by debounce. Diff: %d Debounce: %d", diff, cfg.general.buttons_debounce_ms);
    }
}

uint32_t Application::getUptime() {
    return _uptimeMinutes * 60u;
}
