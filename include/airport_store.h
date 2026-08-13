#pragma once

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

}  // namespace airport_store
