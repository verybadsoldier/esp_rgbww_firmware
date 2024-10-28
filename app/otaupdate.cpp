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

/*******************************************************************
 * thoughts about OTA to new version
 * 
 * now, that the webapp is no longer delivered as a file system,
 * the use of the file system can change.
 * - move from spiffs to LittleFS 
 * - use both free rom blocks (0x00100000-0x001f9fff and 0x003000-0x003f9ff
 *   as redundant file systems - known good configuration shall be copied, 
 *   but not with the same write rate so we minimize wear on the secondary
 *   file system
 * - if the primary file system cannot be mounted (maybe because one important
 *   flash cell has failed), the socondary file system shall be used and a 
 *   degraded warning shall be displayed
 * 
 * layout pre partition change
 *
 * 
 * 0x00000000   +-------------------------+
 *              |                         |
 * 0x00002000   +-------------------------+
 *              | rom0                    |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x000FA000   +-------------------------+
 *              |                         |
 * 0x00100000   +-------------------------+
 *              | spiffs0                 |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x001C0000   +-------------------------+
 *              |                         |
 * 0x00202000   +-------------------------+
 *              | rom1                    |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x002FA000   +-------------------------+
*               |                         |
 * 0x00300000   +-------------------------+
 *              | spiffs1                 |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x003C0000   +-------------------------+
 *              |                         |
 *              |                         |
 * 0x003FFFFF   +-------------------------+
 * 
 *
 * layout post partition change
 * 
 * 0x00000000   +-------------------------+
 *              | partition Table         |
 * 0x00002000   +-------------------------+
 *              | rom0                    |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x000FA000   +-------------------------+
 *              |                         |
 * 0x00100000   +-------------------------+
 *              | lfs0                    |
 *              | extended from 768kB     |
 *              | to 1000kB               |
 *              |                         |
 *              |                         |
 * 0x001FA000   +-------------------------+
 *              |                         |
 * 0x00202000   +-------------------------+
 *              | rom1                    |
 *              |                         |
 *              |                         |
 *              |                         |
 *              |                         |
 * 0x002FA000   +-------------------------+
 *              |                         |
 * 0x00300000   +-------------------------+
 *              | lfs1                    |
 *              | extended from 768kB     |
 *              | to 1000kB               |
 *              |                         |
 *              |                         |
 * 0x003FA000   +-------------------------+
 *              |                         |
 *              |                         |
 * 0x003FFFFF   +-------------------------+
 * 
 * Concepts:
 * 
 * - lfs0 will be the active file system, lfs1 will be a "last known good" backup
 * - the application will work with lfs0 for all ongoing operations and update lfs1 on 
 *      - config writes
 *      - reboot (?)
 * - if lfs0 cannot be mounted, lfs1 will be used and a flag will be set to notify the user via the API that the system is in a degraded state
 *
 * Transition:
 * 
 * When an lfs aware firmware is first flashed via OTA, the partition scheme has to be changed and existing data has to be copied.
 * The data to copy is rather limited, primarily, it will have to be the .config and .color files. Both should be in RAM once the 
 * system has initialized and thus a true copy may not even be necessary.
 * 
 * The bigger challenge is that, once the file system has been changed, there will be no way back to the oder firmware other than re-flashing it.
 * This means that the other rom partition, not having been updated, is no longer able to run, it will result in a 404 error because it cannot
 * find the webapp (that used to be in the spiffs partition) and it won't find it's configuration, so it will default to an RGB light.
 * This is the identical behaviour to the old "filesystem not mounted" case.
 * One way around this would be to immedieately start another OTA once the first one has completed.
 * 
 * the first OTA to the new layout thus will have to take care of a few extra
 * things like:
 * 
 *  - find, if there are two spiffs partitions
 *      -  make sure the config in ram is good
 *      -  rewrite the partition table
 *      -  create the two lfs file systems
 *      -  save config and color to the new lfs0
 *      -  save config and color to the new lfs1
 *      -  start OTA for the other rom slot
 * 
 * I don't know if I can read the .config 
 * 
 * 
 * - if two spiffs partitions are present, that's a sign of a v1 layout
 *   - if the current spiffs partition as per OTA is spiffs1 (the 2nd), 
 *     copy the config file from spiffs0 to spiffs1, as spiffs0 will be 
 *     overwritten next
 *   - convert the first of those partitions to LittleFS and expand it 
 *     (the current partitions are only 768kB long, why not use the full 
 *     availbale 1000kB)
 *   - copy the config file from the remaining spiffs partition to the new
 *     LittleFS partition
 *   - convert the 2nd spiffs partition to LittleFS 
 * 
 * this is the code to create a partition table, not sure if I can /have to use it
 * 	Storage::initialize();

	auto& table = Storage::spiFlash->editablePartitions();
	if(!table) {
		using SubType = Storage::Partition::SubType;
		table.add(F("rom0"), SubType::App::ota0, 0x2000, 0xf8000);
		table.add(F("rom1"), SubType::App::ota1, 0x102000, 0xf8000);
		table.add(F("spiffs0"), SubType::Data::spiffs, 0x200000, 0xc0000);
		table.add(F("spiffs1"), SubType::Data::spiffs, 0x2c0000, 0xc0000);
		table.add(F("rf_cal"), SubType::Data::rfCal, 0x3fb000, 0x1000);
		table.add(F("phy_init"), SubType::Data::phy, 0x3fc000, 0x1000);
		table.add(F("sys_param"), SubType::Data::sysParam, 0x3fd000, 0x3000);
	}

    Update: all this needs to be rethought a bit to work for the ESP32 and Host as well.
    For The ESP32, there should never be the case that there is no partitio table or partition, 
    the whole code to change the table is only a transitional requirement for the existing ESP8266 
    based controllers.
    
    Since keeping the config partition is only really required when updating OTA on an ESP8266, the
    hwconfig can be simplified to always provide two roms and two lfs partitions. 
 */
#include <otaupdate.h>
#include <RGBWWCtrl.h>

void ApplicationOTA::start(String romurl)
{
	debug_i("ApplicationOTA::start");
	app.wsBroadcast(F("ota_status"), F("started"));
	otaUpdater.reset(new Ota::Network::HttpUpgrader);
	status = OTASTATUS::OTA_PROCESSING;

	auto part = ota.getNextBootPartition();

#if ESP8266
	debug_i("ApplicationOTA::start nextBootPartition: %s %#06x at slot %i", part.name().c_str(), part.address(),
			rboot_get_current_rom());
#endif
	// flash rom to position indicated in the rBoot config rom table
	otaUpdater->addItem(romurl, part);

	ota.begin(part);

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
	}
	debug_i("Starting OTA ...");

	otaUpdater->start();
}

void ApplicationOTA::doSwitch()
{
	auto before = ota.getRunningPartition();
	auto after = ota.getNextBootPartition();

	debug_i("Swapping from %s @0x%s to %s @0x%s", before.name(), String(before.address(), HEX), after.name(),
			String(after.address(), HEX));
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

void ApplicationOTA::beforeOTA()
{
	debug_i("ApplicationOTA::beforeOTA");
	/*
    * this is being executed before otaUpdater->start
    */

	/*
     * ToDo few differnt situations here:
       - old partition is spiffs (and thus .config based)
         (should be irrelevant because then old firmware is running, nothing we can do)
       - old partition is lfs (and thus ConfigDB based)
         (new partition is lfs based, too - so we can copy the config)
     */
	if(dataPartition.name() != "") {
		debug_i("partition layout v1, saving status to old rom");
		saveStatus(OTASTATUS::OTA_FAILED);
	}
}

void ApplicationOTA::afterOTA()
{
	debug_i("ApplicationOTA::afterOTA");
	/*
    * called by upgradeCallback
    * this is being executed after otaUpdater->start but before System.restart
    * so this is still the old firmware running
    */

	if(status == OTASTATUS::OTA_SUCCESS_REBOOT) {
		debug_i("afterOta, rom Slot=%i", app.getRomSlot());

// ToDo: so the ota has been successful, now what?
#ifdef ARCH_ESP8266
		if(app.getRomSlot() == 1) {
			/* getRomSlot returns the current (OTA pre reboot) slot
            * slot 0 means we will be booting into ROM slot 1 next
            * but the most current information is in spiffs0
            * */
			// unmount old Filesystem - mount new filesystem
			app.umountfs();
			app.mountfs(rom_slot);

			// save settings / color into new rom space
			// app.cfg.save();
			app.rgbwwctrl.colorSave();
		}
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
// app.mountfs(app.getRomSlot());
#endif
	}
}

/*
* OTA callback - after the new rom has been written but before restart
*/
void ApplicationOTA::upgradeCallback(Ota::Network::HttpUpgrader& client, bool result)
{
	debug_i("ApplicationOTA::rBootCallback");
	if(result == true) {
		ota.end();

		auto part = ota.getNextBootPartition();
		debug_i("ApplicationOTA::rBootCallback next boot partition: %s", part.name().c_str());
		ota.setBootPartition(part);
		status = OTASTATUS::OTA_SUCCESS_REBOOT;
	} else {
		status = OTASTATUS::OTA_FAILED;
		ota.abort();
		debug_i("OTA failed");
	}
	afterOTA();
	debug_i("OTA callback done, rebooting");
	System.restart();
}

void ApplicationOTA::checkAtBoot()
{
	debug_i("ApplicationOTA::checkAtBoot");
	status = loadStatus();
	/*
    * after a successful OTA reboot, this should be
    * OTA_SUCCESS_REBOOT, so this could be used to 
    * then change the file systems if necessary.
    */
	int rom = app.getRomSlot();
	Serial.systemDebugOutput(true);
	debug_i("ApplicationOTA::checkAtBoot status: %i", status);
	debug_i("after reboot, checking partition layout");
	debug_i("current active rom is: %i", rom);
#if defined(ARCH_ESP8266) || defined(ARCH_ESP32)
	Storage::Debug::listDevices(Serial);
#endif
	if(app.isTempBoot()) {
		debug_i("ApplicationOTA::checkAtBoot permanently enabling rom %i", app.getRomSlot());
#ifdef ESP8266
		rboot_set_current_rom(app.getRomSlot());
#endif
		(OTASTATUS::OTA_NOT_UPDATING);
	}
	saveStatus(OTASTATUS::OTA_SUCCESS);
}
#ifdef ARCH_ESP8266
bool ApplicationOTA::createLFS(uint8_t slot)
{
	/*
   * Create the target LittleFS filesystem, format then mount
   */
	String fsName = F("littlefs") + String(slot);
	auto targetPart = Storage::findPartition(fsName);
	std::unique_ptr<IFS::FileSystem> dstFilesystem{IFS::createLfsFilesystem(targetPart)};
	if(!dstFilesystem) {
		// Cannot proceed
		return false;
	}
	if(dstFilesystem->format() < 0) {
		return false;
	}
	if(dstFilesystem->mount() < 0) {
		return false;
	}
	return true;
}
#endif
#if defined(ARCH_ESP8266) || defined(ARCH_ESP32)
bool ApplicationOTA::copyContent(std::unique_ptr<IFS::FileSystem> src, std::unique_ptr<IFS::FileSystem> dst)
{
	// Copy files
	IFS::FileCopier copier(*src, *dst);

	copier.onError([&](const IFS::FileCopier::ErrorInfo& info) -> bool {
		IFS::FileSystem::Info fsi{};
		info.fileSys.getinfo(fsi);

		/*
		 * SPIFFS doesn't have a root directory entry
		 */
		if(fsi.type == IFS::FileSystem::Type::SPIFFS && info.operation == IFS::FileCopier::Operation::open &&
		   !info.path) {
			return true;
		}

		Serial << fsi.partition.name() << ": " << info << endl;
		return false;
	});

	if(!copier.copyDir(nullptr, nullptr)) {
		// Copy failed
		return false;
	}
	return true;
}

#endif
#ifdef ARCH_ESP8266
bool ApplicationOTA::switchPartitions()
{
	if(!Storage::findPartition(F("lfs1")) && !Storage::findPartition(F("lfs0"))) {
		std::vector<Storage::esp_partition_info_t> partitionTable = getEditablePartitionTable();
		//if present, delete spiffs1 to make space

		if(Storage::findPartition(F("spiffs1"))) {
			if(!delPartition(partitionTable, F("spiffs1"))) {
				debug_i("ApplicationOTA::checkAtBoot failed to delete spiffs1");
				return false;
			}
		}
		if(Storage::findPartition(F("spiffs0"))) {
			if(!delPartition(partitionTable, F("spiffs0"))) {
				debug_i("ApplicationOTA::checkAtBoot failed to delete spiffs0");
				return false;
			}
		}

		int offset = 0x300000;
		if(!addPartition(partitionTable, F("lfs1"), static_cast<uint8_t>(Storage::Partition::Type::data),
						 static_cast<uint8_t>(Storage::Partition::SubType::Data::littlefs), offset, 0x0f8000, 0x00)) {
			debug_i("ApplicationOTA::checkAtBoot failed to add lfs1");
			return false;
		}
		offset = 0x100000;
		if(!addPartition(partitionTable, F("lfs0"), static_cast<uint8_t>(Storage::Partition::Type::data),
						 static_cast<uint8_t>(Storage::Partition::SubType::Data::littlefs), offset, 0x0f8000, 0x00)) {
			debug_i("ApplicationOTA::checkAtBoot failed to add lfs0");
			return false;
		}

		if(!savePartitionTable(partitionTable)) {
			debug_i("ApplicationOTA::checkAtBoot failed to save partition table");
			return false;
		}

		debug_i("OTA post, switchPartition => reloading partition table");
		Storage::spiFlash->loadPartitions(PARTITION_TABLE_OFFSET); // load partition table from storage

		Storage::Debug::listDevices(Serial);

		debug_i("OTA_post, create new file system");
		createLFS(1);
		createLFS(0);

		//debug_i("OTA_post, saving config");
		//app.cfg.save();

		debug_i("OTA_post, switchPartitions => restart");
		app.restart();
		return true;
	}
}
//#endif

//#ifdef ARCH_ESP8266
bool ApplicationOTA::switchPartition(uint8_t slot)
{
	String spiffsPartName = F("spiffs") + String(slot);
	String lfsPartName = F("lfs") + String(slot);
	if(Storage::findPartition(spiffsPartName)) {
		std::vector<Storage::esp_partition_info_t> partitionTable = getEditablePartitionTable();
		if(!delPartition(partitionTable, spiffsPartName)) {
			debug_i("ApplicationOTA::checkAtBoot failed to delete %s", spiffsPartName.c_str());
		} else {
			int offset;
			slot == 0 ? offset = 0x100000 : offset = 0x300000;
			if(!addPartition(partitionTable, lfsPartName, static_cast<uint8_t>(Storage::Partition::Type::data),
							 static_cast<uint8_t>(Storage::Partition::SubType::Data::littlefs), offset, 0x0f8000,
							 0x00)) {
				debug_i("ApplicationOTA::checkAtBoot failed to add %s", lfsPartName.c_str());
			} else {
				if(!savePartitionTable(partitionTable)) {
					debug_i("ApplicationOTA::checkAtBoot failed to save partition table");
				}
			}
			debug_i("partition update saved");
		}
		Storage::spiFlash->loadPartitions(PARTITION_TABLE_OFFSET); // load partition table from storage
		debug_i("OTA post, switchPartition => reloading partition table");
		Storage::Debug::listDevices(Serial);
		return true;
	} else {
		debug_i("OTA post switchPartition => Partition %s not found", spiffsPartName.c_str());
		return false;
	}
}
#endif

void ApplicationOTA::saveStatus(OTASTATUS status)
{
	debug_i("ApplicationOTA::saveStatus %i to rom partition rom%i\n", status, app.getRomSlot());
	StaticJsonDocument<128> doc;
	JsonObject root = doc.to<JsonObject>();
	root[F("status")] = int(status);
	Json::saveToFile(root, OTA_STATUS_FILE);
}

OTASTATUS ApplicationOTA::loadStatus()
{
	debug_i("ApplicationOTA::loadStatus");
	StaticJsonDocument<128> doc;
	if(Json::loadFromFile(doc, OTA_STATUS_FILE)) {
		OTASTATUS status = (OTASTATUS)doc[F("status")].as<int>();
		return status;
	} else {
		return OTASTATUS::OTA_NOT_UPDATING;
	}
}
#ifdef ARCH_ESP8266
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
#endif

#ifdef ARCH_ESP8266
/*
 * partition manipulation functions, only required for the EPS8266 transitional period
 */
std::vector<Storage::esp_partition_info_t> ApplicationOTA::getEditablePartitionTable()
{
	auto& table = Storage::spiFlash->editablePartitions();
	std::vector<Storage::esp_partition_info_t> partitionTable;
	for(auto partition : table) {
		Storage::esp_partition_info_t entry;
		entry.magic = 0x50AA;
		strncpy(entry.name, partition.name().c_str(), sizeof(entry.name));
		entry.type = partition.type();
		entry.subtype = partition.subType();
		entry.offset = partition.address();
		entry.size = partition.size();
		entry.flags = partition.flags();
		partitionTable.push_back(entry);
	}
	return partitionTable;
}

uint8_t ApplicationOTA::getPartitionIndex(std::vector<Storage::esp_partition_info_t>& partitionTable,
										  String partitionName)
{
	for(uint8_t i = 0; i < partitionTable.size(); i++) {
		if(String(partitionTable[i].name) == partitionName) {
			return i;
		}
	}
	return 0;
}

bool ApplicationOTA::addPartition(std::vector<Storage::esp_partition_info_t>& partitionTable, String name, uint8_t type,
								  uint8_t subType, uint32_t start, uint32_t size, uint8_t flags)
{
	Storage::esp_partition_info_t newPartition;
	newPartition.magic = 0x50AA;
	strncpy(newPartition.name, name.c_str(), sizeof(newPartition.name));
	newPartition.type = static_cast<Storage::Partition::Type>(type);
	//newPartition.subtype=static_cast<Storage::Partition::subtype>(subType);
	newPartition.subtype = subType;
	newPartition.offset = start;
	newPartition.size = size;
	newPartition.flags = flags;
	partitionTable.push_back(newPartition);
	return true;
}

bool ApplicationOTA::delPartition(std::vector<Storage::esp_partition_info_t>& partitionTable, String partitionName)
{
	uint8_t index = getPartitionIndex(partitionTable, partitionName);
	if(index == 0) {
		//first partition is the rom partition, this should not be dropped
		return false;
	}
	partitionTable.erase(partitionTable.begin() + index);
	return true;
}

bool ApplicationOTA::savePartitionTable(std::vector<Storage::esp_partition_info_t>& partitionTable)
{
	std::vector<uint8_t> entries;
	auto& flash = *Storage::spiFlash;
	//partitions have to be sorted by start address
	std::sort(partitionTable.begin(), partitionTable.end(),
			  [](const auto& a, const auto& b) { return a.offset < b.offset; });

	// Check for overlaps
	bool isFirst = true;
	uint32_t partitionEnd;
	for(const auto& partition : partitionTable) {
		Serial << "Partition: " << partition.name << " start: " << partition.offset
			   << " end: " << partition.offset + partition.size << endl;
		if(!isFirst) {
			if((partition.offset + partition.size) < partitionEnd) {
				// Overlap detected, handle error
				Serial << "Error: Overlapping partitions detected " << partition.name << endl;
				return false;
			} else {
				partitionEnd = partition.offset + partition.size;
			}
		} else {
			partitionEnd = partition.offset + partition.size;
			isFirst = false;
		}
	}

	// Write partition table to flash
	Storage::esp_partition_info_t deviceHeader;
	strncpy(deviceHeader.name, "spiFlash", sizeof(deviceHeader.name));
	deviceHeader.magic = 0x50AA;
	deviceHeader.type = Storage::Partition::Type::storage; // Set the type to storage
	deviceHeader.subtype = static_cast<uint8_t>(0x01);	 // Set the subtype to 0x01 (partition table header)
	deviceHeader.offset = 0x00000000;
	deviceHeader.size = 0x00400000; //this is the size of the flash chip hardcoded to 4MB / 32MBit
	deviceHeader.flags = 0x00;		// Adjust this to match your device
	uint8_t* headerData = reinterpret_cast<uint8_t*>(&deviceHeader);
	// m_printHex("header:",headerData,sizeof(Storage::esp_partition_info_t)) ;
	entries.insert(entries.end(), headerData, headerData + sizeof(Storage::esp_partition_info_t));

	for(const auto& partition : partitionTable) {
		// Add the magic number
		Serial << "Adding Partition " << partition.name << endl;

		// Add the partition entry
		Storage::esp_partition_info_t entry;
		entry.magic = 0x50AA;
		strncpy(entry.name, partition.name, sizeof(entry.name));
		entry.type = partition.type;
		entry.subtype = partition.subtype;
		entry.offset = partition.offset;
		entry.size = partition.size;
		entry.flags = partition.flags;
		uint8_t* entryData = reinterpret_cast<uint8_t*>(&entry);
		entries.insert(entries.end(), entryData, entryData + sizeof(Storage::esp_partition_info_t));
		// m_printHex("entry:", entryData,sizeof(Storage::esp_partition_info_t));
		if(sizeof(Storage::esp_partition_info_t) % 16 != 0) {
			printf("\n");
		}
	}
	debug_i("all partitions added, going to compute md5 sum");
	// Compute the MD5 hash of the entries
	crypto_md5_context_t md5Context;
	crypto_md5_init(&md5Context);
	crypto_md5_update(&md5Context, entries.data(), entries.size());
	uint8_t md5sum[MD5_SIZE];
	crypto_md5_final(md5sum, &md5Context);

	// Add the MD5 sum entry
	entries.push_back(0xEB); // Magic number
	entries.push_back(0xEB);
	for(int i = 0; i < 14; i++) {
		entries.push_back(0xFF);
	}
	entries.insert(entries.end(), md5sum, md5sum + MD5_SIZE);

	// which this, the partiton table is complete. Write it to flash:
	debug_i("Writing partition table to flash to %0xi", PARTITION_TABLE_OFFSET);
	flash.erase_range(PARTITION_TABLE_OFFSET, flash.getBlockSize());
	flash.write(PARTITION_TABLE_OFFSET, entries.data(), entries.size());
	debug_i("done updating partition table");
	return true;
	//#endif
}
#endif
