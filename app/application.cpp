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


#include <RGBWWCtrl.h>
#include <Ota/Upgrader.h>
#include <SmingCore.h>
#include <Storage/SysMem.h>
#include <Storage/ProgMem.h>
#include <Storage/Debug.h>
#include <JSON/StreamingParser.h>
#include <VersionListener.h>
#include <FlashString/Stream.hpp>


#if ARCH_ESP8266
#define PART0 "lfs0"
#elif ARCH_ESP32
#define PART0 "factory"
#endif

//IMPORT_FSTR_LOCAL(default_config, PROJECT_DIR "/default_config.json");

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
void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial
	//System.setCpuFrequencye(CF_160MHz);

#ifdef ARCH_ESP8266
	osMessageInterceptor.begin(onOsMessage);
	debug_i("starting os message interceptor");
#endif

	// set CLR pin to input
	// pinMode(CLEAR_PIN, INPUT);

	// seperated application init
	app.init();

	// Run Services on system ready
	System.onReady(SystemReadyDelegate(&Application::startServices, &app));
}

uint32_t getVersion(IDataSourceStream& input)
{
	JSON::VersionListener listener;
	JSON::StaticStreamingParser<128> parser(&listener);
	auto status = parser.parse(input);
	if (listener.hasVersion())
	{
		input.seekFrom(0, SeekOrigin::Start); // rewind to leave the stream in the same state
		return listener.getVersion();
	}
	return -1;
}

Application::~Application()
{
	if(pNtpclient != nullptr) {
		delete pNtpclient;
		pNtpclient = nullptr;
	}
}

void Application::uptimeCounter()
{
	++_uptimeMinutes;
}

void Application::checkRam()
{
	debug_i("Free heap: %d", system_get_free_heap_size());
	String _client_status = WifiStation.getConnectionStatusName();
	debug_i("wifi conection Status: %s", _client_status.c_str());
}

void Application::init()
{
	for(int i = 0; i < 10; i++) {
		Serial.print(_F("="));
		delay(200);
	}
	Serial.print("\r\n");
	// ConfigDB obsoleted debug_i("going to initialize config");
	// ConfigDB obsoleted config::initializeConfig(cfg); // initialize the config structure if necessary
	debug_i("ESP RGBWW Controller Version %s\r\n", fw_git_version);
	debug_i("Sming Version: %s\r\n", sming_git_version);

debug_i("Platform: %s\r\n", SOC);

#if defined(ARCH_ESP8266) || defined(ESP32)
	app.ota.checkAtBoot();
#endif

#if defined(ARCH_ESP8266) //|| defined(ESP32)
	/*
    * verify for new partition layout
    */
	debug_i("application init, \nspiffs0 found: %s\nspiffs1 found: %s\nlfs1 found: %s\nlfs1 found: %s ",
			Storage::findPartition(F("spiffs0")) ? F("true") : F("false"),
			Storage::findPartition(F("spiffs1")) ? F("true") : F("false"),
			Storage::findPartition(PART0) ? F("true") : F("false"),
			Storage::findPartition(F("lfs1")) ? F("true") : F("false"));
	if(!Storage::findPartition(PART0) && !Storage::findPartition(F("lfs1"))) {
		// mount existing data partition
		debug_i("application init (with spiffs) => Mounting file system");
		/* ConfigDB - may need rework
        if(mountfs(app.getRomSlot())){
            if (cfg.exist()) {
                debug_i("application init (with spiffs) => reading config");
                cfg.load();
                debug_i("application init (with spiffs) => config loaded, pin config %s",cfg.general.pin_config_name.c_str());
            }
        }else{
            debug_i("application init (with spiffs) => failed to find config file");
        }
        */

		/*
        * now, app.cfg is the valid full configuration
        * next step is to change the partition layout
        */
		debug_i("application init => switching file systems - partition 1");
		ota.switchPartitions();
		debug_i("application init => saving config");
	}

#endif

	//load settings
	_uptimetimer.initializeMs(60000, TimerDelegate(&Application::uptimeCounter, this)).start();
	_checkRamTimer.initializeMs(10000, TimerDelegate(&Application::checkRam, this)).start();
#ifdef ARCH_ESP8266
	// load boot information
	uint8 bootmode, bootslot;
	debug_i("Application::init - loading boot info");
	if(rboot_get_last_boot_mode(&bootmode)) {
		if(bootmode == MODE_TEMP_ROM) {
			debug_i("Application::init - temp boot, rebooting after OTA");
			System.restart();
		} else {
			debug_i("Application::init - normal boot");
		}
		_bootmode = bootmode;
	}
#endif

// list spiffs partitions
//listSpiffsPartitions();

// mount filesystem
//auto romPartition=app.ota.getRomPartition();

//debug_i("Application::init - got rom partition %s @0x%#08x", romPartition.name(),romPartition.address());
//auto spiffsPartition=app.ota.findSpiffsPartition(romPartition);
#if defined(ARCH_ESP8266) || defined(ARCH_ESP32)
	mountfs(getRomSlot());
	// ToDo - rework mounting filesystem
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
#endif
#ifdef ARCH_HOST
	debug_i("mounting host file system");
	fileSetFileSystem(&IFS::Host::getFileSystem());
#endif

	// initialize config and data
	cfg = std::make_unique<AppConfig>(configDB_PATH);
	data = std::make_unique<AppData>(dataDB_PATH);

	// verify if there is a new version of the hardware config

/*	this should be moved to the ConfigDB intitialization 

	AppConfig::Hardware hardware(*cfg);
	debug_i("Application::init - hardware config loaded");
	
	uint32_t currentVersion=hardware.getVersion();
	{
		debug_i("make pinconfig stream");
		FSTR::Stream fs(fileMap["config/pinconfig.json"]);	
		//Serial.println(fileMap["config/pinconfig.json"]);
		debug_i("get file Version");
		uint32_t fileVersion=getVersion(fs);
		debug_i("fileVersion %i", fileVersion);	
		if(fileVersion == -1){
			debug_i("Application::init - no version found in pinconfig");
		}
		debug_i("Application::init - hardware version: %d, file version: %d", currentVersion, fileVersion);
		if(fileVersion>currentVersion){
			if(auto hardwareUpdate = hardware.update()){
				hardwareUpdate.importFromStream(ConfigDB::Json::format, fs);
			}
		}
	}
*/

// check if we need to reset settings
#if !defined(ARCH_HOST)

/*	if(digitalRead(CLEAR_PIN) < 1) {
		debug_i("CLR button low - resetting settings");
		// ConfigDB - decide if to reload defaults or load a specific saved version
		// perhaps by holding the clear pin low for a certain time along with blink codes?
		// cfg.reset();
		network.forgetWifi();
	}
*/
#endif

	// check ota
#ifdef ARCH_ESP8266
	ota.checkAtBoot();
#endif
	Serial << endl << _F("** Stream **") << endl;
	Serial << "#########################################################################################"<<endl;
	cfg->exportToStream(ConfigDB::Json::format, Serial);
	Serial <<endl;
	Serial << "#########################################################################################"<<endl;
	
	{
		debug_i("application init => checking ConfigDB");
		AppConfig::General general(*cfg);
		debug_i("application init => config is %s", general.getIsInitialized()?"initialized":"not initialized");
		if(!general.getIsInitialized()) {
			debug_i("application init => reading config");

			FSTR::Stream fs(fileMap["config/pinconfig.json"]);	
			debug_i("Application::init - importing hardware configuration from file");
			
			AppConfig::Hardware hardware(*cfg);
			if(auto hardwareUpdate = hardware.update()){
				hardwareUpdate.importFromStream(ConfigDB::Json::format, fs);
			}
			
			debug_i("Application::init - first run");
			_first_run = true;

			if(auto generalUpdate = general.update()) {
				generalUpdate.setIsInitialized(true);
			}

		} else {
			debug_i("ConfigDB already initialized. starting");
		}
	}

	Serial << endl << _F("** Stream **") << endl;
	Serial << "#########################################################################################"<<endl;
	cfg->exportToStream(ConfigDB::Json::format, Serial);
	Serial <<endl;
	Serial << "#########################################################################################"<<endl;
	
	// initialize networking
	network.init();
	debug_i("network initizalized, ssid: %s", WifiStation.getSSID().c_str());
	
	/// initialize led ctrl
	rgbwwctrl.init();
	debug_i("ledctrl initialized");

	initButtons();
	debug_i("buttons initialized");

	// initialize webserver
	app.webserver.init();
	debug_i("webserver initialized");

	debug_i("pin config string %s", fileMap["pin_config"]);

	// ConfigDB: temp only: create an example preset
	/*
	{
		AppData::Presets::OuterUpdater presets(*data);

		presets.clear();

		auto preset = presets.addItem();
		preset.setName("example-hsv");
		preset.setFavorite(true);
		auto hsvUpdater = preset.color.toHsv();
		hsvUpdater.setH(0);
		hsvUpdater.setS(100);
		hsvUpdater.setV(100);
	}
	{
		AppData::Presets::OuterUpdater presets(*data);
		auto preset = presets.addItem();
		preset.setName("example-raw");
		auto rawUpdater = preset.color.toRaw();
		rawUpdater.setR(255);
		rawUpdater.setG(255);
		rawUpdater.setB(255);
		rawUpdater.setWw(255);
		rawUpdater.setCw(255);
	}
	*/
}
void Application::initButtons()
{
	Vector<String> buttons;
	{
		debug_i("Application::initButtons");
		AppConfig::General general(*cfg);

		if(general.getButtonsConfig().length() <= 0)
			return;

		String buttonsConfig = general.getButtonsConfig();

		debug_i("Configuring buttons using string: '%s'", buttonsConfig.c_str());

		splitString(buttonsConfig, ',', buttons);
	} // end of ConfigDB general context

	for(uint32_t i = 0; i < buttons.count(); ++i) {
		if(buttons[i].length() == 0)
			continue;

		uint32_t pin = buttons[i].toInt();
		if(pin >= _lastToggles.size()) {
			debug_i("Pin %d is invalid. Max is %d", pin, _lastToggles.size() - 1);
			continue;
		}
		debug_i("Configuring button: '%s'", buttons[i].c_str());

		_lastToggles[pin] = 0ul;

		attachInterrupt(pin, std::bind(&Application::onButtonTogglePressed, this, pin), FALLING);
		pinMode(pin, INPUT_PULLUP);
	}
}

// Will be called when system initialization was completed
void Application::startServices()
{
	debug_i("Application::startServices");
	rgbwwctrl.start();
	webserver.start();

	{
		debug_i("Application::startServices - starting NTP");
		AppConfig::Root appcfg(*cfg);
		if(appcfg.events.getServerEnabled()) {
			eventserver.setEnabled(true);
			eventserver.start(app.webserver);
		}
	} // end of ConfigDB root context

	{
		debug_i("Application::startServices - starting mqtt");
		AppConfig::Network network(*cfg);
		mqttclient.init(); // initialize mqtt client with node name
		if(network.mqtt.getEnabled()) {
			mqttclient.start();
		}
	}
}

void Application::restart()
{
	debug_i("Application::restart");
	if(network.isApActive()) {
		network.stopAp();
		_systimer.initializeMs(500, TimerDelegate(&Application::restart, this)).startOnce();
	}
	System.restart();
}

void Application::reset()
{
	debug_i("Application::reset");
	//cfg.reset();
	rgbwwctrl.colorReset();
	network.forgetWifi();
	delay(500);
	restart();
}

void Application::forget_wifi_and_restart()
{
	debug_i("Application::forget_wifi_and_restart");
	network.forgetWifi();
	_systimer.initializeMs(500, TimerDelegate(&Application::restart, this)).startOnce();
}

bool Application::delayedCMD(String cmd, int delay)
{
	debug_i("Application::delayedCMD cmd: %s - delay: %i", cmd.c_str(), delay);
	if(cmd.equals(F("reset"))) {
		_systimer.initializeMs(delay, TimerDelegate(&Application::reset, this)).startOnce();
	} else if(cmd.equals(F("restart"))) {
		_systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
	} else if(cmd.equals(F("stopap"))) {
		network.stopAp(2000);
	} else if(cmd.equals(F("forget_wifi"))) {
		_systimer.initializeMs(delay, TimerDelegate(&AppWIFI::forgetWifi, &network)).startOnce();
	} else if(cmd.equals(F("forget_wifi_and_restart"))) {
		network.forgetWifi();
		_systimer.initializeMs(delay, TimerDelegate(&Application::forget_wifi_and_restart, this)).startOnce();
	} else if(cmd.equals(F("umountfs"))) {
		//umountfs();
	} else if(cmd.equals(F("mountfs"))) {
		//
	} else if(cmd.equals(F("switch_rom"))) {
#if ARCH_ESP8266
		switchRom();
//_systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
#endif
	} else {
		return false;
	}
	return true;
}

void Application::listSpiffsPartitions()
{
	Serial.println(_F("** Enumerate registered partitions"));
	mountfs(1);
	listFiles();
	mountfs(0);
	listFiles();
}

bool Application::mountfs(int slot)
{
	/*
    *
    * need a new mount method for the transitional time when the data file
    * system could be spiffs or LitleFS
    *
    */

#ifdef ARCH_HOST
	/*
     * host file system
     */
	debug_i("mounting host file system");
	fileSetFileSystem(&IFS::Host::getFileSystem());
#else
	/*
     * on device file system
     */

	auto part = Storage::findPartition("spiffs" + String(slot));
	if(part) {
		debug_i("mouting spiffs partition %i at %x, length %d", slot, part.address(), part.size());
		return spiffs_mount(part);
	} else {
		part = Storage::findPartition(F("lfs0"));
		if(part) {
			debug_i("mouting primary littlefs partition at %x, length %d", part.address(), part.size());
			if(lfs_mount(part)) {
				return true;
			} else {
				part = Storage::findPartition(F("lfs1"));
				debug_e("primary partition mount failed, mounting secondary lfs partition  at %x, length %d",
						part.address(), part.size());
				return lfs_mount(part);
			};
		}
		debug_i("partition is neither spiffs nor lfs");
		return false;
	}
#endif
}

void Application::listFiles()
{
	if(FileHandle file = fileOpen(F("VERSION"), IFS::OpenFlag::Read)) {
		debug_i("found VERSION file");
		char buffer[64];
		int bytesRead = fileRead(file, buffer, sizeof(buffer));
		buffer[bytesRead] = '\0';
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

void Application::umountfs()
{
	/*
    debug_i("Application::umountfs");
    auto part = Storage::findPartition(F("spiffs")+String(slot));
    if (part){
        debug_i("unmouting spiffs partition %i at %x, length %d", slot,part.address(), part.size());
        return spiffs_unmount(part);
    }else{
        part = Storage::findPartition(F("littlefs")+String(slot));
        if(part){
            debug_i("mouting littlefs partition %i at %x, length %d", slot,part.address(), part.size());
            return true;
            //lfs doesn't seem to define umount
            //return lfs_umount(part);
        }
        debug_i("partition is neither spiffs nor lfs");
    }
    */
}
#if defined(ARCH_ESP8266) || defined(ESP32)
void Application::switchRom()
{
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
#endif

#if defined(ARCH_ESP8266) || defined(ESP32)
int Application::getRomSlot()
{
	auto partition = app.ota.getRomPartition();
	uint8_t slot;
	if(partition.name() == "rom1") {
		slot = 1;
	} else {
		slot = 0;
	}
	return slot;
}
#endif

void Application::wsBroadcast(String message)
{
	debug_i("Application::wsBroadcast");
	app.webserver.wsBroadcast(message);
}

void Application::wsBroadcast(String cmd, String message)
{
	JsonRpcMessage msg(cmd);
	JsonObject root = msg.getParams();
	root[F("message")] = message;
	String jsonStr = Json::serialize(msg.getRoot());
	wsBroadcast(jsonStr);
}

void Application::onCommandRelay(const String& method, const JsonObject& params)
{
	debug_i("Application::onCommandRelay");
	AppConfig::Sync sync(*cfg);
	if(sync.getCmdMasterEnabled())
		mqttclient.publishCommand(method, params);
}

void Application::onButtonTogglePressed(int pin)
{
	uint32_t now = millis();
	uint32_t diff = now - _lastToggles[pin];
	debug_i("Application::onButtonTogglePressed");
	AppConfig::General general(*cfg);
	if(diff > (uint32_t)general.getButtonsDebounceMs()) { // debounce
		debug_i("Button %d pressed - toggle", pin);
		rgbwwctrl.toggle();
		_lastToggles[pin] = now;
	} else {
		debug_d("Button press ignored by debounce. Diff: %d Debounce: %d", diff, general.getButtonsDebounceMs());
	}
}

uint32_t Application::getUptime()
{
	return _uptimeMinutes * 60u;
}
