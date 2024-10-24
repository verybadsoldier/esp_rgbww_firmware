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
 * @section PLAN
 * holding the config struct in RAM seems wasteful. Also, having two entirely different
 * sets of code to serialize/deserialize the config struct for the API and load store 
 * is less than ideal. 
 * Also, the json representation of the base struct can outgrow the 1370Bytes allowed 
 * in a single http request and there may not be enough RAM to assemble multiple fragents 
 * or tcp packets.
 * 
 * ConfigDB (https://github.com/mikee47/ConfigDB) is a library that can store and retrieve
 * json based configuration data. It requires a filesystem to store the data, ideally LittleFS 
 * to leveage the copy on write feature.
 * 
 * this will be a major change as the config struct will be replaced by ConfigDB 
 * 
 */
#pragma once

#include <RGBWWCtrl.h>
#include <JsonObjectStream.h>
#include <app-config.h>
#include <app-data.h>
#include <ConfigDB/Json/Format.h>
#include <ConfigDB/Network/HttpImportResource.h>
#include <Data/CStringArray.h>
#include <Data/Format/Json.h>

#define configDB_PATH "app-config"
#define dataDB_PATH "app-data"

/*
void save(bool print = true) {
    //save config to storage 
}

bool exist() {
    // does a valid config exist
    return true;
}

void reset() {
    // reset config to default values
    // to be extended to "factory settings" or a specific known good version
}

void sanitizeValues() {
    // this should be handled by min/max setings int the db
    //sync.clock_master_interval = max(sync.clock_master_interval, 1);
    
}
*/
/* 
namespace config{
inline void initializeConfig(AppConfig& cfg){
//this may not even be needed, initialization is done by either defaults or by loading from storage
}
}//namespace config
*/  

