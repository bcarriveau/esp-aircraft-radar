#include "airport_package_format.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace {

void putU16(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value & 0xffU);
  p[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
}

void putU32(uint8_t* p, uint32_t value) {
  p[0] = static_cast<uint8_t>(value & 0xffU);
  p[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
  p[2] = static_cast<uint8_t>((value >> 16U) & 0xffU);
  p[3] = static_cast<uint8_t>((value >> 24U) & 0xffU);
}

}  // namespace

int main() {
  uint8_t header[airport_package_format::PACKAGE_HEADER_SIZE]{};
  memcpy(header, airport_package_format::PACKAGE_MAGIC, 16);
  putU16(header + 16, 1);
  putU16(header + 18, 256);
  putU16(header + 20, 55);
  putU16(header + 22, 2);
  putU32(header + 24, 4);
  putU16(header + 28, 120);
  memcpy(header + 32, "2026-08-13", 10);
  memcpy(header + 48, "TEST REGION", 11);
  putU32(header + 112, 220);

  airport_package_format::Header parsed{};
  assert(airport_package_format::parseHeader(header, sizeof(header), parsed));
  assert(parsed.recordCount == 4);
  assert(parsed.payloadSize == 220);
  assert(parsed.radiusMiles == 120);

  uint8_t record[airport_package_format::RECORD_SIZE]{};
  memcpy(record, "KMSN", 4);
  memcpy(record + 8, "DANE COUNTY REGIONAL", 20);
  putU32(record + 40, static_cast<uint32_t>(43139900));
  putU32(record + 44, static_cast<uint32_t>(-89337502));
  putU16(record + 48, 887);
  putU16(record + 50, 9006);
  putU16(record + 52, 2);
  record[54] = 0;

  airport_package_format::Record decoded{};
  assert(airport_package_format::parseRecord(record, sizeof(record), decoded));
  assert(strcmp(decoded.ident, "KMSN") == 0);
  assert(decoded.latitudeE6 == 43139900);
  assert(decoded.longitudeE6 == -89337502);
  assert(decoded.runwayLengthFt == 9006);

  header[148] = 1;
  assert(!airport_package_format::parseHeader(header, sizeof(header), parsed));
  header[148] = 0;
  record[54] = 4;
  assert(!airport_package_format::parseRecord(record, sizeof(record), decoded));
  return 0;
}
