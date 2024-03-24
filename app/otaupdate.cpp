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

void ApplicationOTA::start(String romurl){
    // we'll say an empty spiffsurl is indicative of a v2 partition layout
    start(romurl, "");
}

// start OTA for v1 partition layout
void ApplicationOTA::start(String romurl, String spiffsurl) {
    debug_i("ApplicationOTA::start");
    app.wsBroadcast("ota_status","started");
	otaUpdater.reset(new Ota::Network::HttpUpgrader);
    status = OTASTATUS::OTA_PROCESSING;

    // debugging
    // spiffsurl="";
    auto part = ota.getNextBootPartition();

    debug_i("ApplicationOTA::start nextBootPartition: %s %#06x at slot %i", part.name().c_str(), part.address(), rboot_get_current_rom());
    // flash rom to position indicated in the rBoot config rom table
    otaUpdater->addItem(romurl, part);

	ota.begin(part);

    if(spiffsurl!=""){
        auto spiffsPart=findSpiffsPartition(part);
        debug_i("ApplicationOTA::start spiffspart: %s", spiffsPart.name().c_str());
        if(spiffsPart){
            otaUpdater->addItem(spiffsurl,spiffsPart, new Storage::PartitionStream(spiffsPart, Storage::Mode::BlockErase));
            debug_i("ApplicationOTA::start added spiffsurl: %s with blockerase=true", spiffsurl.c_str());
        }
        // ToDo: I guess I should do some error handling here - what if the partition can't be found?
    }
    otaUpdater->setCallback(Ota::Network::HttpUpgrader::CompletedDelegate(&ApplicationOTA::upgradeCallback, this));
    
    beforeOTA();
    
    unsigned fh = system_get_free_heap_size();
    debug_i("Free heap before OTA: %i", fh);

    debug_i("Current running partition: %s", ota.getRunningPartition().name());
    debug_i("OTA target partition: %s", part.name().c_str());
    debug_i("configured OTA item list");
    debug_i("========================");
    const auto& items = otaUpdater->getItems();
    for(const auto& item : items) {
        debug_i("  URL: %s", item.url.c_str());
        debug_i("  Partition: %s", item.partition.name().c_str());
        debug_i("  Size: %i", item.size);
        debug_i("  ---------");
        //debug_i("Stream: %p", item.getStream());
    }

    debug_i("Starting OTA ...");

    otaUpdater->start();
}

void ApplicationOTA::doSwitch(){
   	auto before = ota.getRunningPartition();
	auto after = ota.getNextBootPartition();

	debug_i("Swapping from %s @0x%s to %s @0x%s",before.name(),String(before.address(), HEX), after.name(), String(after.address(), HEX));
	if(ota.setBootPartition(after)) {
		debug_i("Restarting...\r\n");
		System.restart();
	} else {
		debug_i("Switch failed.");
	} 
}

/*
void ApplicationOTA::reset() {
    debug_i("ApplicationOTA::reset");
    status = OTASTATUS::OTA_NOT_UPDATING;
    if (otaUpdater)
        delete otaUpdater;
}
*/

void ApplicationOTA::beforeOTA() {
    debug_i("ApplicationOTA::beforeOTA");

    // save files to old rom
    // only in v1 partition layout, 
    // that is: if there is a spiffsPartition
    // which is only assigned if there's a spiffsurl
    if(spiffsPartition.name()!=""){
        debug_i("partition layout v1, saving status to old rom");
        saveStatus(OTASTATUS::OTA_FAILED);
    }
}

void ApplicationOTA::afterOTA() {
    debug_i("ApplicationOTA::afterOTA");
    if (status == OTASTATUS::OTA_SUCCESS_REBOOT) {

        // unmount old Filesystem - mount new filesystem
        // app.umountfs();
        // app.mountfs(rom_slot);

        // save settings / color into new rom space
        // app.cfg.save();
        // app.rgbwwctrl.colorSave();

        // save success to new rom
        saveStatus(OTASTATUS::OTA_SUCCESS);

        // remount old filesystem
        // app.umountfs();
        //app.mountfs(app.getRomSlot());
        
        
    }
}

void ApplicationOTA::upgradeCallback(Ota::Network::HttpUpgrader& client, bool result) {
    debug_i("ApplicationOTA::rBootCallback");
    if (result == true) {
        ota.end();

        auto part=ota.getNextBootPartition();
        debug_i("ApplicationOTA::rBootCallback next boot partition: %s", part.name().c_str());
        ota.setBootPartition(part);
        status = OTASTATUS::OTA_SUCCESS_REBOOT;
    }else{
        status = OTASTATUS::OTA_FAILED;
        ota.abort();
        debug_i("OTA failed");
    }
    afterOTA();
    debug_i("OTA callback done, rebooting");
    System.restart();
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

Storage::Partition ApplicationOTA::findSpiffsPartition(Storage::Partition appPart)
{
	String name = "spiffs";
	name += ota.getSlot(appPart);
	auto part = Storage::findPartition(name);
	if(!part) {
		debug_w("Partition '%s' not found", name.c_str());
	}
	return part;
}
