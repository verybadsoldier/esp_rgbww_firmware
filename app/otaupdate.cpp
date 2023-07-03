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

#ifdef ARCH_ESP8266

#include <RGBWWCtrl.h>

void ApplicationOTA::start(String romurl, String spiffsurl) {
    debug_i("ApplicationOTA::start");
    reset();
    status = OTASTATUS::OTA_PROCESSING;
    if (otaUpdater) {
        delete otaUpdater;
    }
    otaUpdater = new Ota::Network::HttpUpgrader();

    rboot_config bootconf = rboot_get_config();
    rom_slot = app.getRomSlot();

    if (rom_slot == 0) {
        rom_slot = 1;
    } else {
        rom_slot = 0;
    }

    auto part = OtaUpgrader::getPartitionForSlot(rom_slot);
    otaUpdater->addItem(romurl, part);

    part = Storage::findPartition(F("spiffs") + rom_slot);
    otaUpdater->addItem(spiffsurl, part);

    otaUpdater->setCallback(Ota::Network::HttpUpgrader::CompletedDelegate(&ApplicationOTA::upgradeCallback, this));
    beforeOTA();
    debug_i("Starting OTA ...");
    otaUpdater->start();
}

void ApplicationOTA::reset() {
    debug_i("ApplicationOTA::reset");
    status = OTASTATUS::OTA_NOT_UPDATING;
    if (otaUpdater)
        delete otaUpdater;
}

void ApplicationOTA::beforeOTA() {
    debug_i("ApplicationOTA::beforeOTA");

    // save failed to old rom
    saveStatus(OTASTATUS::OTA_FAILED);
}

void ApplicationOTA::afterOTA() {
    debug_i("ApplicationOTA::afterOTA");
    if (status == OTASTATUS::OTA_SUCCESS_REBOOT) {

        // unmount old Filesystem - mount new filesystem
        app.umountfs();
        app.mountfs(rom_slot);

        // save settings / color into new rom space
        app.cfg.save();
        app.rgbwwctrl.colorSave();

        // save success to new rom
        saveStatus(OTASTATUS::OTA_SUCCESS);

        // remount old filesystem
        app.umountfs();
        app.mountfs(app.getRomSlot());

    }
}

void ApplicationOTA::upgradeCallback(Ota::Network::HttpUpgrader& client, bool result) {
    debug_i("ApplicationOTA::rBootCallback");
    if (result == true) {

        // set new temporary boot rom
        debug_i("ApplicationOTA::rBootCallback temp boot %i", rom_slot);
        if (rboot_set_temp_rom(rom_slot)) {
            status = OTASTATUS::OTA_SUCCESS_REBOOT;
            debug_i("OTA successful");
        } else {
            status = OTASTATUS::OTA_FAILED;
            debug_i("OTA failed - could not change the rom");
        }
        // restart after 10s - gives clients enough time
        // to fetch status and init restart themselves
        // don`t automatically restart
        // app.delayedCMD("restart", 10000);
    } else {
        status = OTASTATUS::OTA_FAILED;
        debug_i("OTA failed");
    }
    afterOTA();

}

void ApplicationOTA::checkAtBoot() {
    debug_i("ApplicationOTA::checkAtBoot");
    status = loadStatus();
    if (app.isTempBoot()) {
        debug_i("ApplicationOTA::checkAtBoot permanently enabling rom %i", app.getRomSlot());
        rboot_set_current_rom(app.getRomSlot());
        saveStatus(OTASTATUS::OTA_NOT_UPDATING);
    }
}

void ApplicationOTA::saveStatus(OTASTATUS status) {
    debug_i("ApplicationOTA::saveStatus");
    StaticJsonDocument<128> doc;
    JsonObject root = doc.to<JsonObject>();
    root["status"] = int(status);
    Json::saveToFile(root, OTA_STATUS_FILE);
}

OTASTATUS ApplicationOTA::loadStatus() {
    debug_i("ApplicationOTA::loadStatus");
    StaticJsonDocument<128> doc;
    if (Json::loadFromFile(doc, OTA_STATUS_FILE)) {
        OTASTATUS status = (OTASTATUS) doc["status"].as<int>();
        return status;
    } else {
        return OTASTATUS::OTA_NOT_UPDATING;
    }
}

#endif
