#pragma once

#include <FlashString/Map.hpp>
#include <FlashString/Stream.hpp>

//include auto-generated file List (provided by the front-end build process)
#include <fileList.h>

// Define the names for each file
#define XX(name, file) DEFINE_FSTR_LOCAL(KEY_##name, file)
FILE_LIST(XX)
#undef XX

// Import content for each file
#define XX(name, file) IMPORT_FSTR_LOCAL(CONTENT_##name, PROJECT_DIR "/webapp/" file);
FILE_LIST(XX)
#undef XX

// Define the table structure linking key => content
#define XX(name, file) {&KEY_##name, &CONTENT_##name},
DEFINE_FSTR_MAP_LOCAL(fileMap, FlashString, FlashString, FILE_LIST(XX));
#undef XX