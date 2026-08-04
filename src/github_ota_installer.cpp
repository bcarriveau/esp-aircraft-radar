#include "github_ota_installer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_app_format.h>
#include <esp_attr.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <xtensa/xtensa_api.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <new>
#include <strings.h>

#include "ota_update.h"
#include "update_policy.h"

namespace github_ota_installer {
namespace {

constexpr uint16_t PACKAGE_FORMAT_VERSION = 1;
constexpr uint16_t PACKAGE_HEADER_SIZE = 512;
constexpr uint8_t ESP_APPLICATION_MAGIC = 0xE9;
constexpr uint16_t ESP32_S3_IMAGE_CHIP_ID = 9;
constexpr char PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA";
constexpr char PACKAGE_HARDWARE_ID[32] = "WAVESHARE-ESP32-S3-LCD-7";
constexpr char RELEASE_DOWNLOAD_PREFIX[] =
    "https://github.com/bcarriveau/esp-aircraft-radar/releases/download/";
constexpr char USER_AGENT[] = "BILLS-Aircraft-Radar-7in-Installer/1";
constexpr uint8_t MAX_REDIRECTS = 3;
constexpr uint32_t INSTALL_TOTAL_TIMEOUT_MS = 3UL * 60UL * 1000UL;
constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 8000UL;
constexpr uint32_t HTTP_BODY_IDLE_TIMEOUT_MS = 15000UL;
constexpr uint32_t MINIMUM_HTTP_BUDGET_MS = 500UL;
constexpr uint32_t TRANSPORT_RELEASE_DELAY_MS = 75UL;
constexpr size_t DOWNLOAD_BUFFER_BYTES = 4096U;
constexpr size_t INTERNAL_FLASH_WRITE_BUFFER_BYTES = 1024U;
constexpr uint32_t PROGRESS_GRANULARITY_BYTES = 64U * 1024U;

constexpr uint32_t RESTART_DELAY_MS = 1500U;
constexpr uint32_t RESTART_SETTLE_MS = 500U;
constexpr uint32_t RESTART_TASK_STACK_BYTES = 4096U;
constexpr uint32_t RESTART_LOOP_QUIESCE_TIMEOUT_MS = 1000U;
constexpr BaseType_t RESTART_TASK_CORE = 0;
constexpr BaseType_t RESTART_LOOP_CORE = 1;
constexpr UBaseType_t RESTART_TASK_PRIORITY = configMAX_PRIORITIES - 1;
constexpr char RESTART_TASK_NAME[] = "gh_ota_restart";
constexpr uint32_t RESTART_LOOP_WAITING = 0;
constexpr uint32_t RESTART_LOOP_QUIESCED = 1;
constexpr uint32_t RESTART_LOOP_ABORTED = 2;

#pragma pack(push, 1)
struct PackageHeader {
  char magic[16];
  uint16_t formatVersion;
  uint16_t headerSize;
  char hardwareId[32];
  char buildId[96];
  uint32_t firmwareSize;
  uint8_t firmwareSha256[32];
  uint8_t reserved[328];
};
#pragma pack(pop)

static_assert(sizeof(PackageHeader) == PACKAGE_HEADER_SIZE,
              "Radar OTA package header must remain 512 bytes");
static_assert(sizeof(esp_image_header_t) == 24,
              "Unexpected ESP application image header size");

enum class HeaderFailure : uint8_t {
  NONE = 0,
  TOTAL_BYTES,
  CONTENT_LENGTH,
  TRANSFER_ENCODING,
  LOCATION_TOO_LONG,
  CONFLICTING_FRAMING,
};

struct HttpHeaderState {
  size_t totalBytes = 0;
  HeaderFailure failure = HeaderFailure::NONE;
  bool contentLengthSeen = false;
  bool transferEncodingSeen = false;
  bool chunkedOnly = false;
  uint64_t contentLength = 0;
  char location[update_policy::MAX_REDIRECT_URL_LENGTH + 1U]{};
};

struct InstallWorkspace {
  HttpHeaderState headers;
  char currentUrl[update_policy::MAX_REDIRECT_URL_LENGTH + 1U]{};
  char host[96]{};
  char redirectHost[96]{};
  uint8_t downloadBuffer[DOWNLOAD_BUFFER_BYTES]{};
  uint8_t packageHeaderBytes[PACKAGE_HEADER_SIZE]{};
  uint8_t* flashWriteBuffer = nullptr;
  PackageHeader packageHeader{};
  uint8_t imagePrefix[sizeof(esp_image_header_t)]{};
  uint8_t buildMatchFailure[96]{};
  size_t packageHeaderReceived = 0;
  size_t imagePrefixReceived = 0;
  size_t buildPatternLength = 0;
  size_t buildMatchLength = 0;
  bool buildIdSeen = false;
  uint32_t packageReceived = 0;
  uint32_t payloadReceived = 0;
  uint32_t payloadWritten = 0;
  uint32_t lastProgressBytes = 0;
  const esp_partition_t* updatePartition = nullptr;
  esp_ota_handle_t otaHandle = 0;
  bool otaHandleActive = false;
  mbedtls_sha256_context packageSha;
  mbedtls_sha256_context firmwareSha;
  bool packageShaActive = false;
  bool firmwareShaActive = false;
};

static_assert(sizeof(InstallWorkspace) <= 16U * 1024U,
              "Remote OTA workspace exceeded its bounded PSRAM budget");

class WorkspaceGuard {
 public:
  WorkspaceGuard() {
    workspace_ = static_cast<InstallWorkspace*>(heap_caps_malloc(
        sizeof(InstallWorkspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (workspace_) new (workspace_) InstallWorkspace{};
  }
  ~WorkspaceGuard() {
    if (workspace_) {
      cleanup(*workspace_);
      workspace_->~InstallWorkspace();
      heap_caps_free(workspace_);
    }
  }
  InstallWorkspace* get() const { return workspace_; }

 private:
  static void cleanup(InstallWorkspace& workspace) {
    if (workspace.otaHandleActive) {
      esp_ota_abort(workspace.otaHandle);
      workspace.otaHandleActive = false;
    }
    if (workspace.packageShaActive) {
      mbedtls_sha256_free(&workspace.packageSha);
      workspace.packageShaActive = false;
    }
    if (workspace.firmwareShaActive) {
      mbedtls_sha256_free(&workspace.firmwareSha);
      workspace.firmwareShaActive = false;
    }
    if (workspace.flashWriteBuffer) {
      heap_caps_free(workspace.flashWriteBuffer);
      workspace.flashWriteBuffer = nullptr;
    }
  }

  InstallWorkspace* workspace_ = nullptr;
};

portMUX_TYPE restartMux = portMUX_INITIALIZER_UNLOCKED;
RestartState currentRestartState = RestartState::IDLE;
char restartStatusMessage[128]{};
uint32_t restartAtMs = 0;
uint32_t restartExecuteAtMs = 0;
TaskHandle_t restartTaskHandle = nullptr;
DRAM_ATTR uint32_t restartLoopState = RESTART_LOOP_WAITING;
bool restartTaskCreationAttempted = false;

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  snprintf(destination, capacity, "%s", source ? source : "");
}

bool deadlineReached(uint32_t deadlineMs) {
  return static_cast<int32_t>(millis() - deadlineMs) >= 0;
}

uint32_t remainingToDeadline(uint32_t deadlineMs) {
  const uint32_t now = millis();
  return static_cast<int32_t>(deadlineMs - now) > 0 ? deadlineMs - now : 0U;
}

const char* skipWhitespace(const char* text) {
  if (!text) return nullptr;
  while (*text == ' ' || *text == '\t') ++text;
  return text;
}

bool equalsIgnoreCase(const char* left, const char* right) {
  return left && right && strcasecmp(left, right) == 0;
}

bool equalsHeaderToken(const char* text, const char* expected) {
  text = skipWhitespace(text);
  if (!text || !expected) return false;
  const size_t expectedLength = strlen(expected);
  if (strncasecmp(text, expected, expectedLength) != 0) return false;
  text += expectedLength;
  while (*text == ' ' || *text == '\t') ++text;
  return *text == 0;
}

bool parseUnsignedHeader(const char* text, uint64_t& value) {
  text = skipWhitespace(text);
  if (!text || !*text) return false;
  uint64_t parsed = 0;
  while (*text >= '0' && *text <= '9') {
    const uint8_t digit = static_cast<uint8_t>(*text - '0');
    if (parsed > (UINT64_MAX - digit) / 10U) return false;
    parsed = parsed * 10U + digit;
    ++text;
  }
  while (*text == ' ' || *text == '\t') ++text;
  if (*text != 0) return false;
  value = parsed;
  return true;
}

void setHeaderFailure(HttpHeaderState& state, HeaderFailure failure) {
  if (state.failure == HeaderFailure::NONE) state.failure = failure;
}

const char* headerFailureMessage(HeaderFailure failure) {
  switch (failure) {
    case HeaderFailure::TOTAL_BYTES:
      return "Release response headers exceeded the 16384-byte limit";
    case HeaderFailure::CONTENT_LENGTH:
      return "Release Content-Length was invalid or conflicting";
    case HeaderFailure::TRANSFER_ENCODING:
      return "Release Transfer-Encoding was invalid or repeated";
    case HeaderFailure::LOCATION_TOO_LONG:
      return "Release redirect exceeded the 4095-byte limit";
    case HeaderFailure::CONFLICTING_FRAMING:
      return "Release response contained conflicting framing headers";
    case HeaderFailure::NONE:
    default:
      return "Release response headers were invalid";
  }
}

esp_err_t httpEventHandler(esp_http_client_event_t* event) {
  if (!event || !event->user_data) return ESP_OK;
  HttpHeaderState& state = *static_cast<HttpHeaderState*>(event->user_data);
  if (event->event_id != HTTP_EVENT_ON_HEADER || !event->header_key ||
      !event->header_value) {
    return state.failure == HeaderFailure::NONE ? ESP_OK : ESP_FAIL;
  }

  size_t updatedTotal = state.totalBytes;
  if (!update_policy::accumulateHeaderBytes(
          state.totalBytes, strlen(event->header_key),
          strlen(event->header_value), updatedTotal)) {
    setHeaderFailure(state, HeaderFailure::TOTAL_BYTES);
    return ESP_FAIL;
  }
  state.totalBytes = updatedTotal;

  if (equalsIgnoreCase(event->header_key, "Content-Length")) {
    uint64_t parsed = 0;
    if (!parseUnsignedHeader(event->header_value, parsed) ||
        (state.contentLengthSeen && state.contentLength != parsed)) {
      setHeaderFailure(state, HeaderFailure::CONTENT_LENGTH);
      return ESP_FAIL;
    }
    state.contentLengthSeen = true;
    state.contentLength = parsed;
  } else if (equalsIgnoreCase(event->header_key, "Transfer-Encoding")) {
    if (state.transferEncodingSeen ||
        !equalsHeaderToken(event->header_value, "chunked")) {
      setHeaderFailure(state, HeaderFailure::TRANSFER_ENCODING);
      return ESP_FAIL;
    }
    state.transferEncodingSeen = true;
    state.chunkedOnly = true;
  } else if (equalsIgnoreCase(event->header_key, "Location")) {
    const char* value = skipWhitespace(event->header_value);
    if (!update_policy::redirectUrlLengthValid(value)) {
      setHeaderFailure(state, HeaderFailure::LOCATION_TOO_LONG);
      return ESP_FAIL;
    }
    copyText(state.location, sizeof(state.location), value);
  }

  if (state.contentLengthSeen && state.transferEncodingSeen) {
    setHeaderFailure(state, HeaderFailure::CONFLICTING_FRAMING);
    return ESP_FAIL;
  }
  return ESP_OK;
}

void releaseHttpClient(esp_http_client_handle_t client, bool opened) {
  if (!client) return;
  if (opened) esp_http_client_close(client);
  esp_http_client_cleanup(client);
  delay(TRANSPORT_RELEASE_DELAY_MS);
}

enum class StopReason : uint8_t {
  NONE = 0,
  CANCELLED,
  LOCAL_OTA,
  WIFI,
  DEADLINE
};

StopReason stopReason(CancelCallback cancelRequested, uint32_t deadlineMs,
                      char* message, size_t messageCapacity) {
  if (cancelRequested && cancelRequested()) {
    copyText(message, messageCapacity,
             "Remote installation cancelled by the radar or user");
    return StopReason::CANCELLED;
  }
  ota_update::Status localOta;
  ota_update::copyStatus(localOta);
  if (localOta.serverRunning || ota_update::busy()) {
    copyText(message, messageCapacity,
             "Remote installation yielded to local browser OTA");
    return StopReason::LOCAL_OTA;
  }
  if (WiFi.status() != WL_CONNECTED) {
    copyText(message, messageCapacity,
             "Wi-Fi disconnected during remote installation");
    return StopReason::WIFI;
  }
  if (deadlineReached(deadlineMs)) {
    copyText(message, messageCapacity,
             "Remote installation exceeded its three-minute deadline");
    return StopReason::DEADLINE;
  }
  return StopReason::NONE;
}

Result resultForStopReason(StopReason reason) {
  return reason == StopReason::CANCELLED || reason == StopReason::LOCAL_OTA
      ? Result::CANCELLED
      : Result::FAILED;
}

bool makeAssetUrl(const Release& release, char* destination, size_t capacity) {
  if (!destination || capacity == 0 ||
      !update_policy::boundedPrintableAscii(
          release.tag, update_policy::MAX_TAG_LENGTH, false) ||
      !update_policy::assetNameValid(release.asset)) {
    return false;
  }
  const int written = snprintf(destination, capacity, "%s%s/%s",
                               RELEASE_DOWNLOAD_PREFIX, release.tag,
                               release.asset);
  return written > 0 && static_cast<size_t>(written) < capacity;
}

void prepareBuildIdentityMatcher(InstallWorkspace& workspace) {
  workspace.buildPatternLength = strnlen(
      workspace.packageHeader.buildId,
      sizeof(workspace.packageHeader.buildId));
  workspace.buildMatchLength = 0;
  workspace.buildIdSeen = false;
  memset(workspace.buildMatchFailure, 0,
         sizeof(workspace.buildMatchFailure));
  for (size_t index = 1, prefix = 0;
       index < workspace.buildPatternLength; ++index) {
    while (prefix > 0 && workspace.packageHeader.buildId[index] !=
                             workspace.packageHeader.buildId[prefix]) {
      prefix = workspace.buildMatchFailure[prefix - 1U];
    }
    if (workspace.packageHeader.buildId[index] ==
        workspace.packageHeader.buildId[prefix]) {
      ++prefix;
    }
    workspace.buildMatchFailure[index] = static_cast<uint8_t>(prefix);
  }
}

void observeBuildIdentity(InstallWorkspace& workspace,
                          const uint8_t* data, size_t length) {
  if (workspace.buildIdSeen || workspace.buildPatternLength == 0) return;
  for (size_t index = 0; index < length; ++index) {
    const char current = static_cast<char>(data[index]);
    while (workspace.buildMatchLength > 0 &&
           current != workspace.packageHeader
                          .buildId[workspace.buildMatchLength]) {
      workspace.buildMatchLength =
          workspace.buildMatchFailure[workspace.buildMatchLength - 1U];
    }
    if (current == workspace.packageHeader
                       .buildId[workspace.buildMatchLength]) {
      ++workspace.buildMatchLength;
      if (workspace.buildMatchLength == workspace.buildPatternLength) {
        workspace.buildIdSeen = true;
        return;
      }
    }
  }
}

bool validatePackageHeader(InstallWorkspace& workspace,
                           const Release& release,
                           char* message, size_t messageCapacity) {
  memcpy(&workspace.packageHeader, workspace.packageHeaderBytes,
         sizeof(workspace.packageHeader));
  const PackageHeader& header = workspace.packageHeader;
  if (memcmp(header.magic, PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC)) != 0) {
    copyText(message, messageCapacity,
             "Downloaded file is not a Bill's Radar OTA package");
    return false;
  }
  if (header.formatVersion != PACKAGE_FORMAT_VERSION ||
      header.headerSize != PACKAGE_HEADER_SIZE) {
    copyText(message, messageCapacity,
             "Downloaded package format is unsupported");
    return false;
  }
  if (header.hardwareId[sizeof(header.hardwareId) - 1U] != 0 ||
      strcmp(header.hardwareId, PACKAGE_HARDWARE_ID) != 0) {
    copyText(message, messageCapacity,
             "Downloaded package is for different hardware");
    return false;
  }
  if (header.buildId[sizeof(header.buildId) - 1U] != 0 ||
      strcmp(header.buildId, release.buildId) != 0) {
    copyText(message, messageCapacity,
             "Downloaded package build does not match the manifest");
    return false;
  }
  if (header.firmwareSize != release.firmwareSize ||
      memcmp(header.firmwareSha256, release.firmwareSha256,
             sizeof(header.firmwareSha256)) != 0) {
    copyText(message, messageCapacity,
             "Downloaded package firmware identity does not match the manifest");
    return false;
  }
  if (!workspace.updatePartition ||
      header.firmwareSize < update_policy::MINIMUM_FIRMWARE_BYTES ||
      header.firmwareSize > workspace.updatePartition->size) {
    copyText(message, messageCapacity,
             "Firmware does not fit the inactive OTA partition");
    return false;
  }

  esp_ota_handle_t startedHandle = 0;
  const esp_err_t beginResult = esp_ota_begin(
      workspace.updatePartition, header.firmwareSize, &startedHandle);
  if (beginResult != ESP_OK) {
    snprintf(message, messageCapacity, "OTA partition begin failed: %s",
             esp_err_to_name(beginResult));
    return false;
  }
  workspace.otaHandle = startedHandle;
  workspace.otaHandleActive = true;
  mbedtls_sha256_init(&workspace.firmwareSha);
  if (mbedtls_sha256_starts(&workspace.firmwareSha, 0) != 0) {
    copyText(message, messageCapacity,
             "Firmware SHA-256 initialization failed");
    return false;
  }
  workspace.firmwareShaActive = true;
  prepareBuildIdentityMatcher(workspace);
  Serial.printf("Remote OTA package accepted: build=%s firmware=%lu bytes\n",
                header.buildId,
                static_cast<unsigned long>(header.firmwareSize));
  return true;
}

bool validateImagePrefix(InstallWorkspace& workspace,
                         char* message, size_t messageCapacity) {
  esp_image_header_t imageHeader{};
  memcpy(&imageHeader, workspace.imagePrefix, sizeof(imageHeader));
  if (imageHeader.magic != ESP_APPLICATION_MAGIC) {
    copyText(message, messageCapacity,
             "Downloaded firmware has invalid ESP image magic");
    return false;
  }
  if (imageHeader.chip_id != ESP32_S3_IMAGE_CHIP_ID) {
    copyText(message, messageCapacity,
             "Downloaded firmware is not for ESP32-S3");
    return false;
  }
  return true;
}

bool writeFirmwareBytes(InstallWorkspace& workspace,
                        const uint8_t* data, size_t length,
                        char* message, size_t messageCapacity) {
  if (!length) return true;
  if (!workspace.flashWriteBuffer) {
    copyText(message, messageCapacity,
             "Internal OTA flash-write buffer is unavailable");
    return false;
  }
  while (length) {
    const size_t chunk = std::min(length, INTERNAL_FLASH_WRITE_BUFFER_BYTES);
    // The HTTPS receive buffer lives in PSRAM. Copy each bounded write into
    // internal RAM before esp_ota_write(), because flash operations may make
    // external RAM inaccessible while the cache is disabled.
    memcpy(workspace.flashWriteBuffer, data, chunk);
    const esp_err_t result = esp_ota_write(
        workspace.otaHandle, workspace.flashWriteBuffer, chunk);
    if (result != ESP_OK) {
      snprintf(message, messageCapacity, "Firmware write failed: %s",
               esp_err_to_name(result));
      return false;
    }
    workspace.payloadWritten += static_cast<uint32_t>(chunk);
    data += chunk;
    length -= chunk;
  }
  return true;
}

bool processPayload(InstallWorkspace& workspace,
                    const uint8_t* data, size_t length,
                    char* message, size_t messageCapacity) {
  if (!length) return true;
  if (!workspace.firmwareShaActive || !workspace.otaHandleActive) {
    copyText(message, messageCapacity,
             "Remote OTA writer was not initialized");
    return false;
  }
  if (workspace.payloadReceived > workspace.packageHeader.firmwareSize ||
      length > workspace.packageHeader.firmwareSize -
                   workspace.payloadReceived) {
    copyText(message, messageCapacity,
             "Downloaded package contains excess firmware data");
    return false;
  }

  observeBuildIdentity(workspace, data, length);
  if (mbedtls_sha256_update(&workspace.firmwareSha, data, length) != 0) {
    copyText(message, messageCapacity,
             "Firmware SHA-256 update failed");
    return false;
  }
  workspace.payloadReceived += static_cast<uint32_t>(length);

  if (workspace.imagePrefixReceived < sizeof(workspace.imagePrefix)) {
    const size_t needed = sizeof(workspace.imagePrefix) -
                          workspace.imagePrefixReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(workspace.imagePrefix + workspace.imagePrefixReceived,
           data, copyLength);
    workspace.imagePrefixReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (workspace.imagePrefixReceived < sizeof(workspace.imagePrefix)) {
      return true;
    }
    if (!validateImagePrefix(workspace, message, messageCapacity) ||
        !writeFirmwareBytes(workspace, workspace.imagePrefix,
                            sizeof(workspace.imagePrefix),
                            message, messageCapacity)) {
      return false;
    }
  }
  return writeFirmwareBytes(workspace, data, length,
                            message, messageCapacity);
}

bool processPackageBytes(InstallWorkspace& workspace, const Release& release,
                         const uint8_t* data, size_t length,
                         char* message, size_t messageCapacity) {
  if (!workspace.packageShaActive) {
    copyText(message, messageCapacity,
             "Package SHA-256 was not initialized");
    return false;
  }
  if (mbedtls_sha256_update(&workspace.packageSha, data, length) != 0) {
    copyText(message, messageCapacity,
             "Package SHA-256 update failed");
    return false;
  }
  workspace.packageReceived += static_cast<uint32_t>(length);

  if (workspace.packageHeaderReceived < PACKAGE_HEADER_SIZE) {
    const size_t needed = PACKAGE_HEADER_SIZE -
                          workspace.packageHeaderReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(workspace.packageHeaderBytes + workspace.packageHeaderReceived,
           data, copyLength);
    workspace.packageHeaderReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (workspace.packageHeaderReceived == PACKAGE_HEADER_SIZE &&
        !validatePackageHeader(workspace, release,
                               message, messageCapacity)) {
      return false;
    }
  }
  return !length || processPayload(workspace, data, length,
                                   message, messageCapacity);
}

bool finishPackage(InstallWorkspace& workspace, const Release& release,
                   char* message, size_t messageCapacity) {
  if (workspace.packageReceived != release.packageSize ||
      workspace.packageHeaderReceived != PACKAGE_HEADER_SIZE ||
      workspace.payloadReceived != release.firmwareSize ||
      workspace.payloadWritten != release.firmwareSize ||
      !workspace.otaHandleActive || !workspace.packageShaActive ||
      !workspace.firmwareShaActive) {
    copyText(message, messageCapacity,
             "Downloaded firmware package was incomplete");
    return false;
  }
  if (!workspace.buildIdSeen) {
    copyText(message, messageCapacity,
             "Firmware payload does not contain the declared build ID");
    return false;
  }

  uint8_t actualPackageSha[32]{};
  uint8_t actualFirmwareSha[32]{};
  if (mbedtls_sha256_finish(&workspace.packageSha, actualPackageSha) != 0 ||
      mbedtls_sha256_finish(&workspace.firmwareSha, actualFirmwareSha) != 0) {
    copyText(message, messageCapacity,
             "Remote OTA SHA-256 finalization failed");
    return false;
  }
  mbedtls_sha256_free(&workspace.packageSha);
  workspace.packageShaActive = false;
  mbedtls_sha256_free(&workspace.firmwareSha);
  workspace.firmwareShaActive = false;

  if (memcmp(actualPackageSha, release.packageSha256,
             sizeof(actualPackageSha)) != 0) {
    copyText(message, messageCapacity,
             "Downloaded package SHA-256 does not match the manifest");
    return false;
  }
  if (memcmp(actualFirmwareSha, release.firmwareSha256,
             sizeof(actualFirmwareSha)) != 0 ||
      memcmp(actualFirmwareSha, workspace.packageHeader.firmwareSha256,
             sizeof(actualFirmwareSha)) != 0) {
    copyText(message, messageCapacity,
             "Downloaded firmware SHA-256 does not match the manifest");
    return false;
  }

  const esp_err_t endResult = esp_ota_end(workspace.otaHandle);
  workspace.otaHandleActive = false;
  if (endResult != ESP_OK) {
    snprintf(message, messageCapacity, "ESP image validation failed: %s",
             esp_err_to_name(endResult));
    return false;
  }
  const esp_err_t bootResult =
      esp_ota_set_boot_partition(workspace.updatePartition);
  if (bootResult != ESP_OK) {
    snprintf(message, messageCapacity, "Boot partition update failed: %s",
             esp_err_to_name(bootResult));
    return false;
  }
  return true;
}

void setRestartState(RestartState state, const char* message) {
  char localMessage[sizeof(restartStatusMessage)]{};
  copyText(localMessage, sizeof(localMessage), message);
  portENTER_CRITICAL(&restartMux);
  currentRestartState = state;
  memcpy(restartStatusMessage, localMessage, sizeof(restartStatusMessage));
  portEXIT_CRITICAL(&restartMux);
}

void scheduleRestart(const Release& release) {
  char localMessage[sizeof(restartStatusMessage)]{};
  snprintf(localMessage, sizeof(localMessage),
           "%s verified; restarting radar", release.buildId);
  const uint32_t scheduledAt = millis() + RESTART_DELAY_MS;
  portENTER_CRITICAL(&restartMux);
  currentRestartState = RestartState::PENDING;
  memcpy(restartStatusMessage, localMessage, sizeof(restartStatusMessage));
  restartAtMs = scheduledAt;
  restartExecuteAtMs = 0;
  restartTaskHandle = nullptr;
  restartTaskCreationAttempted = false;
  __atomic_store_n(&restartLoopState, RESTART_LOOP_WAITING,
                   __ATOMIC_RELEASE);
  portEXIT_CRITICAL(&restartMux);
  Serial.printf("Remote OTA verified: %s (%lu bytes); restart scheduled\n",
                release.buildId,
                static_cast<unsigned long>(release.firmwareSize));
}

__attribute__((noinline)) bool IRAM_ATTR parkCoreOneForRestart() {
  const uint32_t enabledInterrupts = xthal_get_intenable();
  xt_ints_off(0xFFFFFFFFU);
  uint32_t expectedState = RESTART_LOOP_WAITING;
  if (!__atomic_compare_exchange_n(
          &restartLoopState, &expectedState, RESTART_LOOP_QUIESCED, false,
          __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
    xt_ints_on(enabledInterrupts);
    return false;
  }
  for (;;) {
    __asm__ __volatile__("nop");
  }
}

void restartTask(void*) {
  const TickType_t waitStarted = xTaskGetTickCount();
  const TickType_t waitTicks =
      pdMS_TO_TICKS(RESTART_LOOP_QUIESCE_TIMEOUT_MS);
  for (;;) {
    const uint32_t state =
        __atomic_load_n(&restartLoopState, __ATOMIC_ACQUIRE);
    if (state == RESTART_LOOP_QUIESCED) break;
    if (state == RESTART_LOOP_ABORTED) {
      restartTaskHandle = nullptr;
      vTaskDeleteWithCaps(nullptr);
      return;
    }
    if (xTaskGetTickCount() - waitStarted >= waitTicks) {
      uint32_t expectedState = RESTART_LOOP_WAITING;
      if (__atomic_compare_exchange_n(
              &restartLoopState, &expectedState, RESTART_LOOP_ABORTED, false,
              __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
        restartTaskHandle = nullptr;
        setRestartState(
            RestartState::FAILED,
            "Firmware verified; restart handoff failed. Power-cycle the radar.");
        Serial.println(
            "Remote OTA restart stopped: Core-1 loop did not quiesce");
        vTaskDeleteWithCaps(nullptr);
        return;
      }
      continue;
    }
    vTaskDelay(1);
  }
  Serial.printf("Remote OTA restart task: core=%d stack-hwm-bytes=%u\n",
                static_cast<int>(xPortGetCoreID()),
                static_cast<unsigned>(
                    uxTaskGetStackHighWaterMark(nullptr)));
  Serial.flush();
  esp_restart();
}

void createRestartTaskOnce() {
  if (restartTaskCreationAttempted) return;
  restartTaskCreationAttempted = true;

  const bool heapIntegrityOk = heap_caps_check_integrity_all(true);
  const size_t freeInternal = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestInternal = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf(
      "Remote OTA restart resources: integrity=%s heap=%u block=%u psram=%u "
      "loop-stack=%u\n",
      heapIntegrityOk ? "ok" : "FAILED",
      static_cast<unsigned>(freeInternal),
      static_cast<unsigned>(largestInternal), ESP.getFreePsram(),
      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));

  if (!heapIntegrityOk) {
    setRestartState(
        RestartState::FAILED,
        "Firmware verified; heap integrity failed. Power-cycle the radar.");
    return;
  }
  if (xPortGetCoreID() != RESTART_LOOP_CORE) {
    setRestartState(
        RestartState::FAILED,
        "Firmware verified; restart caller core is unsafe. Power-cycle the radar.");
    return;
  }

  const BaseType_t result = xTaskCreatePinnedToCoreWithCaps(
      restartTask, RESTART_TASK_NAME, RESTART_TASK_STACK_BYTES, nullptr,
      RESTART_TASK_PRIORITY, &restartTaskHandle, RESTART_TASK_CORE,
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (result != pdPASS) {
    restartTaskHandle = nullptr;
    setRestartState(
        RestartState::FAILED,
        "Firmware verified; automatic restart failed. Power-cycle the radar.");
    Serial.printf("Remote OTA restart task creation failed: result=%ld\n",
                  static_cast<long>(result));
    return;
  }
  Serial.printf(
      "Remote OTA restart task created: core=%d stack-bytes=%u\n",
      static_cast<int>(RESTART_TASK_CORE),
      static_cast<unsigned>(RESTART_TASK_STACK_BYTES));
  Serial.flush();
  if (!parkCoreOneForRestart()) {
    Serial.println(
        "Remote OTA restart loop park cancelled; firmware remains verified");
  }
}

}  // namespace

Result install(const Release& release, CancelCallback cancelRequested,
               ProgressCallback progress, char* message,
               size_t messageCapacity) {
  if (!message || messageCapacity == 0) return Result::FAILED;
  message[0] = 0;
  if (!update_policy::packageLayoutValid(release.packageSize,
                                         release.firmwareSize) ||
      !update_policy::boundedPrintableAscii(
          release.buildId, update_policy::MAX_BUILD_ID_LENGTH, false)) {
    copyText(message, messageCapacity,
             "Validated release metadata is incomplete");
    return Result::FAILED;
  }

  WorkspaceGuard guard;
  InstallWorkspace* workspace = guard.get();
  if (!workspace) {
    copyText(message, messageCapacity,
             "Remote OTA PSRAM workspace allocation failed");
    return Result::FAILED;
  }
  workspace->updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (!workspace->updatePartition) {
    copyText(message, messageCapacity,
             "No inactive OTA partition is available");
    return Result::FAILED;
  }
  workspace->flashWriteBuffer = static_cast<uint8_t*>(heap_caps_malloc(
      INTERNAL_FLASH_WRITE_BUFFER_BYTES,
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!workspace->flashWriteBuffer) {
    copyText(message, messageCapacity,
             "Internal OTA flash-write buffer allocation failed");
    return Result::FAILED;
  }
  if (!makeAssetUrl(release, workspace->currentUrl,
                    sizeof(workspace->currentUrl))) {
    copyText(message, messageCapacity,
             "Release asset URL could not be constructed safely");
    return Result::FAILED;
  }

  mbedtls_sha256_init(&workspace->packageSha);
  if (mbedtls_sha256_starts(&workspace->packageSha, 0) != 0) {
    copyText(message, messageCapacity,
             "Package SHA-256 initialization failed");
    return Result::FAILED;
  }
  workspace->packageShaActive = true;

  const uint32_t absoluteDeadlineMs = millis() + INSTALL_TOTAL_TIMEOUT_MS;
  if (progress) progress(0, release.packageSize);

  for (uint8_t redirect = 0; redirect <= MAX_REDIRECTS; ++redirect) {
    const StopReason beforeRequest = stopReason(
        cancelRequested, absoluteDeadlineMs, message, messageCapacity);
    if (beforeRequest != StopReason::NONE) {
      return resultForStopReason(beforeRequest);
    }
    workspace->host[0] = 0;
    if (!update_policy::parseAllowedHttpsUrl(
            workspace->currentUrl, workspace->host,
            sizeof(workspace->host))) {
      copyText(message, messageCapacity,
               "GitHub returned an unsafe release URL");
      return Result::FAILED;
    }
    workspace->headers = HttpHeaderState{};
    const uint32_t connectBudgetMs = std::min<uint32_t>(
        HTTP_CONNECT_TIMEOUT_MS, remainingToDeadline(absoluteDeadlineMs));
    if (connectBudgetMs < MINIMUM_HTTP_BUDGET_MS) {
      copyText(message, messageCapacity,
               "Remote OTA had insufficient HTTPS deadline remaining");
      return Result::FAILED;
    }
    const size_t urlLength = strlen(workspace->currentUrl);
    const size_t txBufferBytes =
        update_policy::httpTransmitBufferBytes(urlLength);
    if (txBufferBytes == 0) {
      copyText(message, messageCapacity,
               "Release URL exceeded the transmit-buffer policy");
      return Result::FAILED;
    }

    esp_http_client_config_t config{};
    config.url = workspace->currentUrl;
    config.user_agent = USER_AGENT;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = static_cast<int>(connectBudgetMs);
    config.disable_auto_redirect = true;
    config.max_redirection_count = 0;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.buffer_size = 2048;
    config.buffer_size_tx = static_cast<int>(txBufferBytes);
    config.keep_alive_enable = false;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.skip_cert_common_name_check = false;
    config.event_handler = httpEventHandler;
    config.user_data = &workspace->headers;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
      copyText(message, messageCapacity,
               "GitHub release HTTPS client allocation failed");
      return Result::FAILED;
    }
    esp_http_client_set_header(client, "Accept", "application/octet-stream");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Connection", "close");

    bool opened = false;
    const esp_err_t openResult = esp_http_client_open(client, 0);
    if (openResult != ESP_OK) {
      snprintf(message, messageCapacity,
               "GitHub release HTTPS open/send failed: %s (url=%u tx=%u)",
               esp_err_to_name(openResult),
               static_cast<unsigned>(urlLength),
               static_cast<unsigned>(txBufferBytes));
      releaseHttpClient(client, false);
      return Result::FAILED;
    }
    opened = true;
    const int64_t fetchedLength = esp_http_client_fetch_headers(client);
    if (workspace->headers.failure != HeaderFailure::NONE ||
        fetchedLength < 0) {
      if (workspace->headers.failure != HeaderFailure::NONE) {
        copyText(message, messageCapacity,
                 headerFailureMessage(workspace->headers.failure));
      } else {
        snprintf(message, messageCapacity,
                 "GitHub release header fetch failed: result=%lld errno=%d",
                 static_cast<long long>(fetchedLength),
                 esp_http_client_get_errno(client));
      }
      releaseHttpClient(client, opened);
      return Result::FAILED;
    }

    const int statusCode = esp_http_client_get_status_code(client);
    const bool redirectStatus = statusCode == 301 || statusCode == 302 ||
                                statusCode == 303 || statusCode == 307 ||
                                statusCode == 308;
    if (redirectStatus) {
      if (redirect >= MAX_REDIRECTS || !workspace->headers.location[0]) {
        copyText(message, messageCapacity,
                 "GitHub release redirect policy rejected response");
        releaseHttpClient(client, opened);
        return Result::FAILED;
      }
      workspace->redirectHost[0] = 0;
      if (!update_policy::parseAllowedHttpsUrl(
              workspace->headers.location, workspace->redirectHost,
              sizeof(workspace->redirectHost))) {
        copyText(message, messageCapacity,
                 "GitHub release redirect host was rejected");
        releaseHttpClient(client, opened);
        return Result::FAILED;
      }
      copyText(workspace->currentUrl, sizeof(workspace->currentUrl),
               workspace->headers.location);
      releaseHttpClient(client, opened);
      continue;
    }

    if (statusCode != 200) {
      snprintf(message, messageCapacity,
               "GitHub release asset returned HTTP %d", statusCode);
      releaseHttpClient(client, opened);
      return Result::FAILED;
    }
    const bool chunked = esp_http_client_is_chunked_response(client);
    if (!update_policy::framingIsUnambiguous(
            workspace->headers.contentLengthSeen,
            workspace->headers.transferEncodingSeen,
            workspace->headers.chunkedOnly, chunked)) {
      copyText(message, messageCapacity,
               "GitHub release response framing was ambiguous");
      releaseHttpClient(client, opened);
      return Result::FAILED;
    }
    if (workspace->headers.contentLengthSeen &&
        workspace->headers.contentLength != release.packageSize) {
      copyText(message, messageCapacity,
               "GitHub release package length does not match the manifest");
      releaseHttpClient(client, opened);
      return Result::FAILED;
    }

    esp_http_client_set_timeout_ms(client,
                                   HTTP_BODY_IDLE_TIMEOUT_MS);
    uint32_t lastProgressMs = millis();
    bool readFailed = false;
    while (workspace->packageReceived < release.packageSize) {
      const StopReason duringDownload = stopReason(
          cancelRequested, absoluteDeadlineMs, message, messageCapacity);
      if (duringDownload != StopReason::NONE) {
        releaseHttpClient(client, opened);
        return resultForStopReason(duringDownload);
      }
      if (millis() - lastProgressMs >= HTTP_BODY_IDLE_TIMEOUT_MS) {
        copyText(message, messageCapacity,
                 "GitHub release download stalled for 15 seconds");
        readFailed = true;
        break;
      }
      const size_t remaining =
          release.packageSize - workspace->packageReceived;
      const int requestBytes = static_cast<int>(
          std::min<size_t>(remaining, DOWNLOAD_BUFFER_BYTES));
      const int bytesRead = esp_http_client_read(
          client, reinterpret_cast<char*>(workspace->downloadBuffer),
          requestBytes);
      if (bytesRead > 0) {
        if (!processPackageBytes(
                *workspace, release, workspace->downloadBuffer,
                static_cast<size_t>(bytesRead), message, messageCapacity)) {
          readFailed = true;
          break;
        }
        lastProgressMs = millis();
        if (progress &&
            (workspace->packageReceived - workspace->lastProgressBytes >=
                 PROGRESS_GRANULARITY_BYTES ||
             workspace->packageReceived == release.packageSize)) {
          workspace->lastProgressBytes = workspace->packageReceived;
          progress(workspace->packageReceived, release.packageSize);
        }
        vTaskDelay(1);
      } else if (bytesRead == 0) {
        if (esp_http_client_is_complete_data_received(client)) break;
        readFailed = true;
        break;
      } else {
        const int socketError = esp_http_client_get_errno(client);
        if ((bytesRead == -ESP_ERR_HTTP_EAGAIN || socketError == EAGAIN ||
             socketError == EWOULDBLOCK || socketError == ETIMEDOUT) &&
            !deadlineReached(absoluteDeadlineMs)) {
          continue;
        }
        snprintf(message, messageCapacity,
                 "GitHub release body read failed: result=%d errno=%d",
                 bytesRead, socketError);
        readFailed = true;
        break;
      }
    }

    const bool complete = esp_http_client_is_complete_data_received(client);
    releaseHttpClient(client, opened);
    if (readFailed || !complete ||
        workspace->packageReceived != release.packageSize) {
      if (!message[0]) {
        copyText(message, messageCapacity,
                 "GitHub release package download was incomplete");
      }
      return Result::FAILED;
    }
    if (!finishPackage(*workspace, release, message, messageCapacity)) {
      return Result::FAILED;
    }
    scheduleRestart(release);
    copyText(message, messageCapacity,
             "Firmware verified; radar restart is pending");
    return Result::RESTART_PENDING;
  }

  copyText(message, messageCapacity,
           "GitHub release redirect count exceeded limit");
  return Result::FAILED;
}

void serviceRestart() {
  const uint32_t now = millis();
  bool beginSettle = false;
  bool beginRestart = false;
  portENTER_CRITICAL(&restartMux);
  if (currentRestartState == RestartState::PENDING) {
    if (restartAtMs && static_cast<int32_t>(now - restartAtMs) >= 0) {
      restartAtMs = 0;
      restartExecuteAtMs = now + RESTART_SETTLE_MS;
      beginSettle = true;
    } else if (restartExecuteAtMs &&
               static_cast<int32_t>(now - restartExecuteAtMs) >= 0) {
      restartExecuteAtMs = 0;
      beginRestart = true;
    }
  }
  portEXIT_CRITICAL(&restartMux);
  if (beginSettle) {
    Serial.println(
        "Remote OTA restart shutdown: settling network tasks");
  }
  if (beginRestart) createRestartTaskOnce();
}

RestartState restartState() {
  portENTER_CRITICAL(&restartMux);
  const RestartState state = currentRestartState;
  portEXIT_CRITICAL(&restartMux);
  return state;
}

void copyRestartMessage(char* destination, size_t capacity) {
  if (!destination || capacity == 0) return;
  portENTER_CRITICAL(&restartMux);
  copyText(destination, capacity, restartStatusMessage);
  portEXIT_CRITICAL(&restartMux);
}

}  // namespace github_ota_installer
