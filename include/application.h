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
    bool delayedCMD(String cmd, int delay);

    //void listSpiffsPartitions();
    
    void mountfs(int slot);
    void umountfs();

    inline bool isFilesystemMounted() { return _fs_mounted; };
    inline bool isFirstRun() { return _first_run; };
#ifdef ARCH_ESP8266
    inline bool isTempBoot() { return _bootmode == MODE_TEMP_ROM; };
#else
    bool isTempBoot() { return false; };
#endif
    inline int getRomSlot() { return _romslot; };
    inline int getBootMode() { return _bootmode; };
    void switchRom();

    void onCommandRelay(const String& method, const JsonObject& json);
    void onWifiConnected(const String& ssid);
    void onButtonTogglePressed(int pin);

    uint32_t getUptime();
    void uptimeCounter();

public:
    AppWIFI network;
    ApplicationWebserver webserver;
    APPLedCtrl rgbwwctrl;
#ifdef ARCH_ESP8266
    ApplicationOTA ota;
#endif
    ApplicationSettings cfg;
    EventServer eventserver;
    AppMqttClient mqttclient;
    JsonProcessor jsonproc;
    NtpClient* pNtpclient = nullptr;

private:
    void loadbootinfo();

    Timer _systimer;
    int _bootmode = 0;
    int _romslot = 0;
    bool _first_run = false;
    bool _fs_mounted = false;
    bool _run_after_ota = false;

    Timer _uptimetimer;
    uint32_t _uptimeMinutes;
    std::array<int, 17> _lastToggles;
};
// forward declaration for global vars
extern Application app;
