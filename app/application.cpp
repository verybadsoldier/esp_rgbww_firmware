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

#include <RGBWWCtrl.h>

Application app;

// Sming Framework INIT method - called during boot
void GDB_IRAM_ATTR init() {

    Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
    Serial.systemDebugOutput(true); // don`t show system debug messages
    //System.setCpuFrequencye(CF_160MHz);

    // set CLR pin to input
    pinMode(CLEAR_PIN, INPUT);

    // seperated application init
    app.init();

    // Run Services on system ready
    System.onReady(SystemReadyDelegate(&Application::startServices, &app));
}

void Application::printFreeHeap() {
    static uint32 prevheap = 0;
    uint32 hs = system_get_free_heap_size();
    if (hs != prevheap)
        Serial.printf("Free Heap: %u\n", hs);
    prevheap = hs;
}

void Application::init() {
    Serial.systemDebugOutput(false);

    debug_i("RGBWW Controller v %s\r\n", fw_git_version);

    // set timestamp for uptime calculation
    startupTimestamp = rtc.getRtcSeconds();

    //load settings

    // load boot information
    uint8 bootmode, bootslot;
    if (rboot_get_last_boot_mode(&bootmode)) {
        if (bootmode == MODE_TEMP_ROM) {
            debug_i("Application::init - booting after OTA");
        } else {
            debug_i("Application::init - normal boot");
        }
        _bootmode = bootmode;
    }

    if (rboot_get_last_boot_rom(&bootslot)) {
        _romslot = bootslot;
    }

    // mount filesystem
    mountfs(getRomSlot());

    // check if we need to reset settings
    if (digitalRead(CLEAR_PIN) < 1) {
        debug_i("CLR button low - resetting settings");
        cfg.reset();
        network.forgetWifi();
    }

    // check ota
    ota.checkAtBoot();

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

    // initialize networking
    network.init();

    // initialize webserver
    app.webserver.init();

    _heaptimer.initializeMs(200, TimerDelegate(&Application::printFreeHeap, this)).start();
    //_tcpcleantimer.initializeMs(20, TimerDelegate(&Application::tcpCleanup, this)).start();
}

// Will be called when system initialization was completed
void Application::startServices() {
    debug_i("Application::startServices");
    rgbwwctrl.start();
    webserver.start();

    if (cfg.events.server_enabled)
        eventserver.start();
}

void Application::restart() {
    debug_i("Restarting");
    if (network.isApActive()) {
        network.stopAp();
        _systimer.initializeMs(500, TimerDelegate(&Application::restart, this)).startOnce();
    }
    System.restart();
}

void Application::reset() {
    debug_i("Application::reset");
    debug_i("resetting controller");
    cfg.reset();
    rgbwwctrl.colorReset();
    network.forgetWifi();
    delay(500);
    restart();
}

bool Application::delayedCMD(String cmd, int delay) {
    debug_i("Application::delayedCMD cmd: %s - delay: %i", cmd.c_str(), delay);
    if (cmd.equals("reset")) {
        _systimer.initializeMs(delay, TimerDelegate(&Application::reset, this)).startOnce();
    } else if (cmd.equals("restart")) {
        _systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
    } else if (cmd.equals("stopap")) {
        network.stopAp(2000);
    } else if (cmd.equals("forget_wifi")) {
        _systimer.initializeMs(delay, TimerDelegate(&AppWIFI::forgetWifi, &network)).startOnce();
    } else if (cmd.equals("test_channels")) {
        //rgbwwctrl.testChannels();
    } else if (cmd.equals("switch_rom")) {
        switchRom();
        _systimer.initializeMs(delay, TimerDelegate(&Application::restart, this)).startOnce();
    } else {
        return false;
    }
    return true;
}

void Application::mountfs(int slot) {
    debug_i("Application::mountfs rom slot: %i", slot);
    if (slot == 0) {
        debug_i("Application::mountfs trying to mount spiffs at %x, length %d",
                RBOOT_SPIFFS_0, SPIFF_SIZE);
        spiffs_mount_manual(RBOOT_SPIFFS_0, SPIFF_SIZE);
    } else {
        debug_i("Application::mountfs trying to mount spiffs at %x, length %d",
                RBOOT_SPIFFS_1, SPIFF_SIZE);
        spiffs_mount_manual(RBOOT_SPIFFS_1, SPIFF_SIZE);
    }
    _fs_mounted = true;
}

void Application::umountfs() {
    debug_i("Application::umountfs");
    spiffs_unmount();
    _fs_mounted = false;
}

void Application::switchRom() {
    debug_i("Application::switchRom");
    int slot = getRomSlot();
    if (slot == 0) {
        slot = 1;
    } else {
        slot = 0;
    }
    rboot_set_current_rom(slot);
}

void Application::onWifiConnected(const String& ssid) {
    debug_i("Application::onWifiConnected");
}

void Application::onCommandRelay(const String& method, const JsonObject& params) {
    if (!cfg.sync.cmd_master_enabled)
        return;

    //mqttclient.publishCommand(method, params);
}

uint32_t Application::getUptime() {
    return rtc.getRtcSeconds() - startupTimestamp;
}

#include "lwip/tcp_impl.h"

void Application::tcpCleanup()
{
  while(tcp_tw_pcbs!=NULL)
  {
    Serial.printf("Aborting TW con\n");
    tcp_abort(tcp_tw_pcbs);
  }
}
