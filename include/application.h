 /*
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
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 *
 */
#pragma once

static const char* fw_git_version = GITVERSION;
static const char* fw_git_date = GITDATE;
static const char* sming_git_version = SMING_VERSION;

// main forward declarations
class Application {

public:
    ~Application();

    void init();
    void initButtons();

    void startServices();
    void stopServices();

    void reset();
    void restart();
    void forget_wifi_and_restart();
    bool delayedCMD(String cmd, int delay);

    void wsBroadcast(String message);
    void wsBroadcast(String cmd, String message);

    void listSpiffsPartitions();
    
    bool mountfs(int slot);
    void umountfs();

    inline bool isFilesystemMounted() { return _fs_mounted; };
    inline bool isFirstRun() { return _first_run; };

    void checkRam();
#ifdef ARCH_ESP8266
    inline bool isTempBoot() { return _bootmode == MODE_TEMP_ROM; };
#else
    bool isTempBoot() { return false; };
#endif
    int getRomSlot();
    //inline int getBootMode() { return _bootmode; };
    void switchRom();

    void onCommandRelay(const String& method, const JsonObject& json);
    //void onWifiConnected(const String& ssid);
    
    void onButtonTogglePressed(int pin);

    uint32_t getUptime();
    void uptimeCounter();

public:
    AppWIFI network;
    ApplicationWebserver webserver;
    APPLedCtrl rgbwwctrl;
#if defined(ARCH_ESP8266) || defined(ESP32)
    ApplicationOTA ota;
#endif
    std::unique_ptr<AppConfig> cfg;
    //std::unique_ptr<AppConfig> cfg;
    std::unique_ptr<AppData> data;
    EventServer eventserver;
    AppMqttClient mqttclient;
    JsonProcessor jsonproc;
    NtpClient* pNtpclient = nullptr;

private:
    void loadbootinfo();
    void listFiles();

    Timer _systimer;
    int _bootmode = 0;
    int _romslot = 0;
    bool _first_run = false;
    bool _fs_mounted = false;
    bool _run_after_ota = false;

    Timer _uptimetimer;
    Timer _checkRamTimer;
    uint32_t _uptimeMinutes;
    std::array<int, 17> _lastToggles;
};
// forward declaration for global vars
extern Application app;
