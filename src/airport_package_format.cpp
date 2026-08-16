#include "airport_package_format.h"

#include <string.h>

namespace airport_package_format {
namespace {

uint16_t readU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8U);
}

uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8U) |
         (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}

int16_t readI16(const uint8_t* p) {
  return static_cast<int16_t>(readU16(p));
}

int32_t readI32(const uint8_t* p) {
  return static_cast<int32_t>(readU32(p));
}

bool validFixedAscii(const char* text, size_t capacity) {
  if (!text || capacity < 2 || text[0] == '\0') return false;
  bool sawNull = false;
  for (size_t i = 0; i < capacity; ++i) {
    const uint8_t value = static_cast<uint8_t>(text[i]);
    if (sawNull) {
      if (value != 0) return false;
      continue;
    }
    if (value == 0) {
      sawNull = true;
      continue;
    }
    if (value < 0x20U || value > 0x7eU) return false;
  }
  return sawNull;
}

}  // namespace

bool parseHeader(const uint8_t* raw, size_t rawSize, Header& out) {
  out = Header{};
  if (!raw || rawSize < PACKAGE_HEADER_SIZE) return false;
  if (memcmp(raw, PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC)) != 0) return false;

  out.formatVersion = readU16(raw + 16);
  out.headerSize = readU16(raw + 18);
  out.recordSize = readU16(raw + 20);
  out.generatorVersion = readU16(raw + 22);
  out.recordCount = readU32(raw + 24);
  out.radiusMiles = readU16(raw + 28);
  out.flags = readU16(raw + 30);
  memcpy(out.databaseDate, raw + 32, sizeof(out.databaseDate));
  memcpy(out.coverage, raw + 48, sizeof(out.coverage));
  out.payloadSize = readU32(raw + 112);
  memcpy(out.payloadSha256, raw + 116, sizeof(out.payloadSha256));

  if (out.formatVersion != PACKAGE_FORMAT_VERSION ||
      out.headerSize != PACKAGE_HEADER_SIZE ||
      out.recordSize != RECORD_SIZE ||
      out.flags != 0 ||
      out.recordCount == 0 ||
      out.recordCount > MAX_RECORD_COUNT ||
      out.radiusMiles < MIN_RADIUS_MILES ||
      out.radiusMiles > MAX_RADIUS_MILES) {
    return false;
  }

  const uint64_t expectedPayload =
      static_cast<uint64_t>(out.recordCount) * static_cast<uint64_t>(RECORD_SIZE);
  if (expectedPayload != out.payloadSize) return false;
  if (!validFixedAscii(out.databaseDate, sizeof(out.databaseDate)) ||
      !validFixedAscii(out.coverage, sizeof(out.coverage))) {
    return false;
  }

  for (size_t i = 148; i < PACKAGE_HEADER_SIZE; ++i) {
    if (raw[i] != 0) return false;
  }
  return true;
}

bool parseRecord(const uint8_t* raw, size_t rawSize, Record& out) {
  out = Record{};
  if (!raw || rawSize < RECORD_SIZE) return false;
  memcpy(out.ident, raw, sizeof(out.ident));
  memcpy(out.name, raw + 8, sizeof(out.name));
  out.latitudeE6 = readI32(raw + 40);
  out.longitudeE6 = readI32(raw + 44);
  out.elevationFt = readI16(raw + 48);
  out.runwayLengthFt = readU16(raw + 50);
  out.runwayHeadingDegrees = readU16(raw + 52);
  out.category = raw[54];

  if (!validFixedAscii(out.ident, sizeof(out.ident)) ||
      !validFixedAscii(out.name, sizeof(out.name))) {
    return false;
  }
  if (out.latitudeE6 < -90000000 || out.latitudeE6 > 90000000 ||
      out.longitudeE6 < -180000000 || out.longitudeE6 > 180000000 ||
      out.runwayHeadingDegrees > 360 ||
      out.category > 3) {
    return false;
  }
  return true;
}

}  // namespace airport_package_format
