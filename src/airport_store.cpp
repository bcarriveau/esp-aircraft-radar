#include "airport_store.h"

#include <esp_err.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>

namespace airport_store {
namespace {

constexpr const char* PARTITION_LABEL = "airports";
constexpr esp_partition_subtype_t PARTITION_SUBTYPE =
    static_cast<esp_partition_subtype_t>(0x40);
constexpr size_t HASH_CHUNK_SIZE = 512;
constexpr size_t FLASH_ERASE_SECTOR_SIZE = 4096;

const esp_partition_t* partitionHandle = nullptr;
State storeState = State::UNAVAILABLE;
airport_package_format::Header packageHeader{};

void setError(char* output, size_t capacity, const char* message) {
  if (!output || capacity == 0) return;
  snprintf(output, capacity, "%s", message ? message : "");
}

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

bool validatePayloadDigestFromFlash() {
  uint8_t chunk[HASH_CHUNK_SIZE];
  uint8_t digest[32];
  size_t remaining = packageHeader.payloadSize;
  size_t offset = airport_package_format::PACKAGE_HEADER_SIZE;

  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  if (mbedtls_sha256_starts(&context, 0) != 0) {
    mbedtls_sha256_free(&context);
    return false;
  }

  while (remaining > 0) {
    const size_t length = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    if (!partitionRead(offset, chunk, length) ||
        mbedtls_sha256_update(&context, chunk, length) != 0) {
      mbedtls_sha256_free(&context);
      return false;
    }
    offset += length;
    remaining -= length;
  }

  const int finishResult = mbedtls_sha256_finish(&context, digest);
  mbedtls_sha256_free(&context);
  return finishResult == 0 &&
      memcmp(digest, packageHeader.payloadSha256, sizeof(digest)) == 0;
}

bool validateRecordsFromFlash() {
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

bool calculatePayloadDigest(const uint8_t* payload, size_t payloadSize,
                            uint8_t digest[32]) {
  if (!payload || !digest || payloadSize == 0) return false;

  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  if (mbedtls_sha256_starts(&context, 0) != 0) {
    mbedtls_sha256_free(&context);
    return false;
  }

  size_t offset = 0;
  while (offset < payloadSize) {
    const size_t remaining = payloadSize - offset;
    const size_t length = remaining < HASH_CHUNK_SIZE
        ? remaining : HASH_CHUNK_SIZE;
    if (mbedtls_sha256_update(&context, payload + offset, length) != 0) {
      mbedtls_sha256_free(&context);
      return false;
    }
    offset += length;
  }

  const int result = mbedtls_sha256_finish(&context, digest);
  mbedtls_sha256_free(&context);
  return result == 0;
}

bool validatePackageBuffer(const uint8_t* package, size_t packageSize,
                           airport_package_format::Header& header,
                           char* errorMessage,
                           size_t errorMessageCapacity) {
  header = airport_package_format::Header{};
  if (!partitionHandle) {
    setError(errorMessage, errorMessageCapacity,
             "Airport data partition is unavailable");
    return false;
  }
  if (!package ||
      packageSize < airport_package_format::PACKAGE_HEADER_SIZE ||
      packageSize > partitionHandle->size) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package size is invalid");
    return false;
  }
  if (!airport_package_format::parseHeader(
          package, airport_package_format::PACKAGE_HEADER_SIZE, header)) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package header is invalid");
    return false;
  }

  const uint64_t expectedSize =
      static_cast<uint64_t>(airport_package_format::PACKAGE_HEADER_SIZE) +
      header.payloadSize;
  if (expectedSize != packageSize || expectedSize > partitionHandle->size) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package length does not match its header");
    return false;
  }

  uint8_t digest[32]{};
  const uint8_t* payload =
      package + airport_package_format::PACKAGE_HEADER_SIZE;
  if (!calculatePayloadDigest(payload, header.payloadSize, digest) ||
      memcmp(digest, header.payloadSha256, sizeof(digest)) != 0) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package SHA-256 validation failed");
    return false;
  }

  airport_package_format::Record record{};
  for (uint32_t index = 0; index < header.recordCount; ++index) {
    const size_t offset = airport_package_format::PACKAGE_HEADER_SIZE +
        static_cast<size_t>(index) * airport_package_format::RECORD_SIZE;
    if (offset > packageSize ||
        airport_package_format::RECORD_SIZE > packageSize - offset ||
        !airport_package_format::parseRecord(
            package + offset, airport_package_format::RECORD_SIZE, record)) {
      setError(errorMessage, errorMessageCapacity,
               "Airport package contains an invalid record");
      return false;
    }
  }

  setError(errorMessage, errorMessageCapacity, "");
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
      !validatePayloadDigestFromFlash() ||
      !validateRecordsFromFlash()) {
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

size_t maxPackageSize() {
  if (!partitionHandle) {
    partitionHandle = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, PARTITION_SUBTYPE, PARTITION_LABEL);
  }
  return partitionHandle ? partitionHandle->size : 0;
}

bool installPackage(const uint8_t* package, size_t packageSize,
                    char* errorMessage, size_t errorMessageCapacity) {
  if (!partitionHandle) {
    partitionHandle = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, PARTITION_SUBTYPE, PARTITION_LABEL);
  }

  airport_package_format::Header validatedHeader{};
  if (!validatePackageBuffer(package, packageSize, validatedHeader,
                             errorMessage, errorMessageCapacity)) {
    return false;
  }

  // No flash is modified before the complete buffered package has passed
  // metadata, size, SHA-256, and per-record validation.
  const size_t eraseSize =
      (packageSize + FLASH_ERASE_SECTOR_SIZE - 1U) &
      ~(FLASH_ERASE_SECTOR_SIZE - 1U);
  if (eraseSize == 0 || eraseSize > partitionHandle->size) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package erase span is invalid");
    return false;
  }

  const esp_err_t eraseResult =
      esp_partition_erase_range(partitionHandle, 0, eraseSize);
  if (eraseResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Airport partition erase failed: %s",
             esp_err_to_name(eraseResult));
    setError(errorMessage, errorMessageCapacity, message);
    storeState = State::INVALID;
    packageHeader = airport_package_format::Header{};
    return false;
  }

  const esp_err_t writeResult =
      esp_partition_write(partitionHandle, 0, package, packageSize);
  if (writeResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Airport partition write failed: %s",
             esp_err_to_name(writeResult));
    setError(errorMessage, errorMessageCapacity, message);
    storeState = State::INVALID;
    packageHeader = airport_package_format::Header{};
    return false;
  }

  // Re-read the persistent copy and re-run the exact runtime validation path.
  if (!initialize()) {
    setError(errorMessage, errorMessageCapacity,
             "Airport package write verification failed");
    return false;
  }

  if (packageHeader.recordCount != validatedHeader.recordCount ||
      packageHeader.payloadSize != validatedHeader.payloadSize ||
      memcmp(packageHeader.payloadSha256, validatedHeader.payloadSha256,
             sizeof(packageHeader.payloadSha256)) != 0) {
    storeState = State::INVALID;
    packageHeader = airport_package_format::Header{};
    setError(errorMessage, errorMessageCapacity,
             "Airport package readback does not match uploaded package");
    return false;
  }

  setError(errorMessage, errorMessageCapacity, "");
  return true;
}

}  // namespace airport_store
