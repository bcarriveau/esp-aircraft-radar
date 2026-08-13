#pragma once

#include <stddef.h>
#include <stdint.h>

namespace airport_package_format {

constexpr size_t PACKAGE_HEADER_SIZE = 256;
constexpr size_t RECORD_SIZE = 55;
constexpr uint16_t PACKAGE_FORMAT_VERSION = 1;
constexpr uint16_t MIN_RADIUS_MILES = 90;
constexpr uint16_t MAX_RADIUS_MILES = 500;
constexpr uint32_t MAX_RECORD_COUNT = 65535;
constexpr char PACKAGE_MAGIC[16] = "BILLS-AIRPORTDB";

struct Header {
  uint16_t formatVersion = 0;
  uint16_t headerSize = 0;
  uint16_t recordSize = 0;
  uint16_t generatorVersion = 0;
  uint32_t recordCount = 0;
  uint16_t radiusMiles = 0;
  uint16_t flags = 0;
  char databaseDate[16]{};
  char coverage[64]{};
  uint32_t payloadSize = 0;
  uint8_t payloadSha256[32]{};
};

struct Record {
  char ident[8]{};
  char name[32]{};
  int32_t latitudeE6 = 0;
  int32_t longitudeE6 = 0;
  int16_t elevationFt = 0;
  uint16_t runwayLengthFt = 0;
  uint16_t runwayHeadingDegrees = 0;
  uint8_t category = 0;
};

bool parseHeader(const uint8_t* raw, size_t rawSize, Header& out);
bool parseRecord(const uint8_t* raw, size_t rawSize, Record& out);

}  // namespace airport_package_format
