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

#include <Ota/Network/HttpUpgrader.h>
#include <Storage/PartitionStream.h>
#include <Storage/SpiFlash.h>
#include <Ota/Upgrader.h>

#ifndef OTAUPDATE_H_
#define OTAUPDATE_H_
#define OTA_STATUS_FILE ".ota"

enum class OTASTATUS {
    OTA_NOT_UPDATING = 0,
    OTA_PROCESSING = 1,
    OTA_SUCCESS_REBOOT = 2,
    OTA_SUCCESS = 3,
    OTA_FAILED = 4
};

class Application;

class ApplicationOTA {
public:

    void start(String romurl, String spiffsurl); // v1 partition layout needs rom and spiffs 
    void start(String romurl);                   // v2 partition layout only needs rom as the webapp is wrapped into the rom
    void checkAtBoot();
    inline OTASTATUS getStatus() { return status; };
    inline bool isProccessing() { return status == OTASTATUS::OTA_PROCESSING; };

protected:
    std::unique_ptr<Ota::Network::HttpUpgrader> otaUpdater;
    uint8 rom_slot;
    OTASTATUS status = OTASTATUS::OTA_NOT_UPDATING;

protected:
    Storage::Partition spiffsPartition;
    OtaUpgrader ota;
    void upgradeCallback(Ota::Network::HttpUpgrader& client, bool result);
    void reset();
    void beforeOTA();
    void afterOTA();
    void doSwitch();
    void saveStatus(OTASTATUS status);
    OTASTATUS loadStatus();
    Storage::Partition findSpiffsPartition(Storage::Partition appPart);

    friend Application;
};

#endif // OTAUPDATE_H_
