#pragma once

#include <stddef.h>
#include <stdint.h>

#include "airport_package_format.h"

namespace airport_store {

enum class State : uint8_t {
  UNAVAILABLE = 0,
  EMPTY = 1,
  INVALID = 2,
  READY = 3
};

bool initialize();
State state();
bool ready();
uint32_t recordCount();
uint16_t radiusMiles();
const char* databaseDate();
const char* databaseCoverage();
bool readRecord(uint32_t index, airport_package_format::Record& out);
const char* stateName();

// Maximum accepted .radarapt package size for the currently installed
// airport-data partition. Returns zero when the partition is unavailable.
size_t maxPackageSize();

// Installs one complete, already-buffered .radarapt package. The caller is
// expected to hold the bounded upload in PSRAM. This function fully validates
// package metadata, SHA-256, and every record before erasing the current
// persistent airport partition. After writing, the partition is re-read and
// revalidated before success is reported.
//
// On any validation failure the existing partition is left untouched.
// A flash erase/write failure can invalidate persistent data, but the runtime
// will then fall back to the compiled database on the next initialize().
bool installPackage(const uint8_t* package, size_t packageSize,
                    char* errorMessage, size_t errorMessageCapacity);

}  // namespace airport_store
