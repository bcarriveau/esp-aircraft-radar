#include "airport_store.h"

#include <esp_partition.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace airport_store {
namespace {

constexpr const char* PARTITION_LABEL = "airports";
constexpr esp_partition_subtype_t PARTITION_SUBTYPE =
    static_cast<esp_partition_subtype_t>(0x40);
constexpr size_t HASH_CHUNK_SIZE = 512;

const esp_partition_t* partitionHandle = nullptr;
State storeState = State::UNAVAILABLE;
airport_package_format::Header packageHeader{};

bool partitionRead(size_t offset, void* out, size_t length) {
  if (!partitionHandle || !out || length == 0) return false;
  if (offset > partitionHandle->size || length > partitionHandle->size - offset) {
    return false;
  }
  return esp_partition_read(partitionHandle, offset, out, length) == ESP_OK;
}

bool headerLooksErased(const uint8_t* raw) {
  if (!raw) return false;
  for (size_t i = 0; i < airport_package_format::PACKAGE_HEADER_SIZE; ++i) {
    if (raw[i] != 0xffU) return false;
  }
  return true;
}

bool validatePayloadDigest() {
  uint8_t chunk[HASH_CHUNK_SIZE];
  uint8_t digest[32];
  size_t remaining = packageHeader.payloadSize;
  size_t offset = airport_package_format::PACKAGE_HEADER_SIZE;

  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  mbedtls_sha256_starts(&context, 0);

  while (remaining > 0) {
    const size_t length = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    if (!partitionRead(offset, chunk, length)) {
      mbedtls_sha256_free(&context);
      return false;
    }
    mbedtls_sha256_update(&context, chunk, length);
    offset += length;
    remaining -= length;
  }

  mbedtls_sha256_finish(&context, digest);
  mbedtls_sha256_free(&context);
  return memcmp(digest, packageHeader.payloadSha256, sizeof(digest)) == 0;
}

bool validateRecords() {
  uint8_t raw[airport_package_format::RECORD_SIZE];
  airport_package_format::Record record{};
  for (uint32_t index = 0; index < packageHeader.recordCount; ++index) {
    const size_t offset = airport_package_format::PACKAGE_HEADER_SIZE +
        static_cast<size_t>(index) * airport_package_format::RECORD_SIZE;
    if (!partitionRead(offset, raw, sizeof(raw)) ||
        !airport_package_format::parseRecord(raw, sizeof(raw), record)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool initialize() {
  partitionHandle = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, PARTITION_SUBTYPE, PARTITION_LABEL);
  packageHeader = airport_package_format::Header{};

  if (!partitionHandle) {
    storeState = State::UNAVAILABLE;
    return false;
  }

  uint8_t rawHeader[airport_package_format::PACKAGE_HEADER_SIZE];
  if (!partitionRead(0, rawHeader, sizeof(rawHeader))) {
    storeState = State::INVALID;
    return false;
  }
  if (headerLooksErased(rawHeader)) {
    storeState = State::EMPTY;
    return false;
  }
  if (!airport_package_format::parseHeader(
          rawHeader, sizeof(rawHeader), packageHeader)) {
    storeState = State::INVALID;
    return false;
  }

  const uint64_t packageSize =
      static_cast<uint64_t>(airport_package_format::PACKAGE_HEADER_SIZE) +
      packageHeader.payloadSize;
  if (packageSize > partitionHandle->size ||
      !validatePayloadDigest() ||
      !validateRecords()) {
    packageHeader = airport_package_format::Header{};
    storeState = State::INVALID;
    return false;
  }

  storeState = State::READY;
  return true;
}

State state() { return storeState; }

bool ready() { return storeState == State::READY && partitionHandle; }

uint32_t recordCount() { return ready() ? packageHeader.recordCount : 0; }

uint16_t radiusMiles() { return ready() ? packageHeader.radiusMiles : 0; }

const char* databaseDate() {
  return ready() ? packageHeader.databaseDate : "";
}

const char* databaseCoverage() {
  return ready() ? packageHeader.coverage : "";
}

bool readRecord(uint32_t index, airport_package_format::Record& out) {
  out = airport_package_format::Record{};
  if (!ready() || index >= packageHeader.recordCount) return false;
  uint8_t raw[airport_package_format::RECORD_SIZE];
  const size_t offset = airport_package_format::PACKAGE_HEADER_SIZE +
      static_cast<size_t>(index) * airport_package_format::RECORD_SIZE;
  return partitionRead(offset, raw, sizeof(raw)) &&
      airport_package_format::parseRecord(raw, sizeof(raw), out);
}

const char* stateName() {
  switch (storeState) {
    case State::UNAVAILABLE: return "UNAVAILABLE";
    case State::EMPTY: return "EMPTY";
    case State::INVALID: return "INVALID";
    case State::READY: return "READY";
    default: return "UNKNOWN";
  }
}

}  // namespace airport_store
