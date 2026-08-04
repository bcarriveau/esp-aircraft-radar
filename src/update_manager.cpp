#include "update_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <new>
#include <strings.h>

#include "adsb_network.h"
#include "app_state.h"
#include "build_info.h"
#include "mqtt_service.h"
#include "ota_update.h"
#include "update_policy.h"

namespace update_manager {
namespace {

constexpr char NVS_NAMESPACE[] = "radar_update";
constexpr char NVS_KEY[] = "state";
constexpr uint32_t STORED_MAGIC = 0x55504454UL;  // UPDT
constexpr uint16_t STORED_SCHEMA = 2;
constexpr uint32_t VALID_EPOCH_MINIMUM = 1700000000UL;
constexpr uint32_t CHECK_TOTAL_TIMEOUT_MS = 6000UL;
constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 2500UL;
constexpr uint32_t HTTP_BODY_IDLE_TIMEOUT_MS = 1500UL;
constexpr uint32_t ADSB_POLL_GUARD_MS = 1500UL;
constexpr uint32_t MINIMUM_HTTP_BUDGET_MS = 250UL;
constexpr uint32_t TRANSPORT_RELEASE_DELAY_MS = 75UL;
constexpr size_t MAX_MANIFEST_RESPONSE_BYTES =
    update_policy::MAX_MANIFEST_BYTES;
constexpr uint8_t MAX_REDIRECTS = 3;
constexpr char MANIFEST_ASSET_NAME[] =
    "waveshare-esp32-s3-touch-lcd-7.manifest.json";
constexpr char LATEST_MANIFEST_URL[] =
    "https://github.com/bcarriveau/esp-aircraft-radar/releases/latest/download/"
    "waveshare-esp32-s3-touch-lcd-7.manifest.json";
constexpr char RELEASE_DOWNLOAD_PREFIX[] =
    "https://github.com/bcarriveau/esp-aircraft-radar/releases/download/";
constexpr char USER_AGENT[] = "BILLS-Aircraft-Radar-7in-Updater/2";

#pragma pack(push, 1)
struct StoredState {
  uint32_t magic = STORED_MAGIC;
  uint16_t schema = STORED_SCHEMA;
  uint16_t reserved = 0;
  uint32_t lastAttemptEpoch = 0;
  uint32_t lastSuccessEpoch = 0;
  uint32_t remoteVersionCode = 0;
  uint8_t lastResult = static_cast<uint8_t>(CheckResult::NEVER);
  uint8_t updateAvailable = 0;
  uint8_t attemptedWithoutTime = 0;
  uint8_t reservedByte = 0;
  char remoteVersionLabel[32]{};
  char remoteBuildId[96]{};
  char notes[192]{};
  char message[96]{};
};
#pragma pack(pop)

static_assert(sizeof(StoredState) == 440,
              "Update-check NVS state layout changed");

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

struct HttpResponse {
  int statusCode = 0;
  uint8_t* body = nullptr;
  size_t length = 0;
  char releaseTag[64]{};
};

struct HttpWorkspace {
  HttpHeaderState headers;
  char currentUrl[update_policy::MAX_REDIRECT_URL_LENGTH + 1U]{};
  char host[96]{};
  char redirectHost[96]{};
};

static_assert(sizeof(HttpWorkspace) <= 9U * 1024U,
              "Release-check workspace exceeded its bounded PSRAM budget");

class WorkspaceGuard {
 public:
  WorkspaceGuard() {
    workspace_ = static_cast<HttpWorkspace*>(heap_caps_malloc(
        sizeof(HttpWorkspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (workspace_) new (workspace_) HttpWorkspace{};
  }
  ~WorkspaceGuard() {
    if (workspace_) {
      workspace_->~HttpWorkspace();
      heap_caps_free(workspace_);
    }
  }
  HttpWorkspace* get() const { return workspace_; }

 private:
  HttpWorkspace* workspace_ = nullptr;
};

class PsramAllocator final : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  void deallocate(void* pointer) override { heap_caps_free(pointer); }
  void* reallocate(void* pointer, size_t newSize) override {
    return heap_caps_realloc(pointer, newSize,
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
};

Preferences preferences;
PsramAllocator psramAllocator;
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
Status currentStatus;
bool persistenceReady = false;
bool bootAttemptedWithoutTime = false;
bool persistedAttemptWithoutTime = false;
uint32_t bootStartedMs = 0;
uint32_t lastAttemptUptimeMs = 0;
uint32_t preparedCheckDeadlineMs = 0;
bool preparedCheckReady = false;
bool preparedCheckManual = false;
bool abortRequested = false;

enum class DeferredReason : uint8_t {
  NONE = 0,
  STARTUP_TIMER,
  DAILY_TIMER,
  NO_TIME_TIMER,
  WIFI,
  OTA,
  MQTT,
  FETCH_STATE,
  WIFI_RECOVERY,
  SLACK
};

enum class AbortCause : uint8_t {
  NONE = 0,
  COMMAND,
  OTA,
  WIFI,
  DEADLINE
};

DeferredReason lastDeferredReason = DeferredReason::NONE;
bool lastDeferredWasManual = false;

bool deadlineReached(uint32_t deadlineMs) {
  return static_cast<int32_t>(millis() - deadlineMs) >= 0;
}

uint32_t currentEpoch() {
  const time_t value = time(nullptr);
  if (value < static_cast<time_t>(VALID_EPOCH_MINIMUM) ||
      value > static_cast<time_t>(UINT32_MAX)) {
    return 0;
  }
  return static_cast<uint32_t>(value);
}

bool isNullTerminated(const char* text, size_t capacity) {
  return text && memchr(text, 0, capacity) != nullptr;
}

bool isLowerHexDigest(const char* digest) {
  return update_policy::lowerHexDigest(digest);
}

bool isSafeText(const char* text, size_t maximumLength, bool allowEmpty) {
  return update_policy::boundedPrintableAscii(text, maximumLength, allowEmpty);
}

void copyText(char* destination, size_t capacity, const char* source) {
  if (!destination || capacity == 0) return;
  snprintf(destination, capacity, "%s", source ? source : "");
}

void clearRelease(Status& status) {
  status.updateAvailable = false;
  status.remoteVersionCode = 0;
  status.remoteVersionLabel[0] = 0;
  status.remoteBuildId[0] = 0;
  status.notes[0] = 0;
}

void publishStatus(const Status& status) {
  portENTER_CRITICAL(&statusMux);
  const uint32_t nextVersion = currentStatus.statusVersion + 1U;
  currentStatus = status;
  currentStatus.statusVersion = nextVersion ? nextVersion : 1U;
  portEXIT_CRITICAL(&statusMux);
}

Status snapshotStatus() {
  Status status;
  portENTER_CRITICAL(&statusMux);
  status = currentStatus;
  portEXIT_CRITICAL(&statusMux);
  return status;
}

bool abortWasRequested() {
  portENTER_CRITICAL(&statusMux);
  const bool requested = abortRequested;
  portEXIT_CRITICAL(&statusMux);
  return requested;
}

void clearAbortRequest() {
  portENTER_CRITICAL(&statusMux);
  abortRequested = false;
  portEXIT_CRITICAL(&statusMux);
}

uint32_t remainingToDeadline(uint32_t deadlineMs) {
  const uint32_t now = millis();
  return static_cast<int32_t>(deadlineMs - now) > 0 ? deadlineMs - now : 0U;
}

const char* deferredReasonName(DeferredReason reason) {
  switch (reason) {
    case DeferredReason::STARTUP_TIMER:
      return "the five-minute startup timer has not elapsed";
    case DeferredReason::DAILY_TIMER:
      return "the 24-hour automatic timer has not elapsed";
    case DeferredReason::NO_TIME_TIMER:
      return "a no-time attempt is already retained for this schedule";
    case DeferredReason::WIFI: return "Wi-Fi is not connected";
    case DeferredReason::OTA: return "local OTA owns or has armed the network";
    case DeferredReason::MQTT: return "MQTT is starting, connecting, or stopping";
    case DeferredReason::FETCH_STATE: return "ADS-B serialization is not active";
    case DeferredReason::WIFI_RECOVERY: return "Wi-Fi recovery owns the network";
    case DeferredReason::SLACK: return "less than eight seconds remain before ADS-B";
    case DeferredReason::NONE:
    default: return "ready";
  }
}

void logDeferred(DeferredReason reason, bool manual) {
  if (reason == DeferredReason::NONE) {
    lastDeferredReason = DeferredReason::NONE;
    lastDeferredWasManual = false;
    return;
  }
  if (reason != lastDeferredReason || manual != lastDeferredWasManual) {
    Serial.printf("GitHub update check %s deferred: %s\n",
                  manual ? "manual" : "automatic",
                  deferredReasonName(reason));
    lastDeferredReason = reason;
    lastDeferredWasManual = manual;
  }

  if (!manual) return;
  Status status = snapshotStatus();
  char message[sizeof(status.message)]{};
  snprintf(message, sizeof(message), "CHECK NOW queued: %s",
           deferredReasonName(reason));
  if (strcmp(status.message, message) != 0 ||
      status.lastResult != CheckResult::QUEUED || !status.manualQueued) {
    status.manualQueued = true;
    status.lastResult = CheckResult::QUEUED;
    copyText(status.message, sizeof(status.message), message);
    publishStatus(status);
  }
}

StoredState toStored(const Status& status) {
  StoredState stored;
  stored.lastAttemptEpoch = status.lastAttemptEpoch;
  stored.lastSuccessEpoch = status.lastSuccessEpoch;
  stored.remoteVersionCode = status.remoteVersionCode;
  stored.lastResult = static_cast<uint8_t>(status.lastResult);
  stored.updateAvailable = status.updateAvailable ? 1 : 0;
  stored.attemptedWithoutTime = persistedAttemptWithoutTime ? 1 : 0;
  memcpy(stored.remoteVersionLabel, status.remoteVersionLabel,
         sizeof(stored.remoteVersionLabel));
  memcpy(stored.remoteBuildId, status.remoteBuildId,
         sizeof(stored.remoteBuildId));
  memcpy(stored.notes, status.notes, sizeof(stored.notes));
  memcpy(stored.message, status.message, sizeof(stored.message));
  return stored;
}

bool storedStateValid(const StoredState& stored) {
  if (stored.magic != STORED_MAGIC || stored.schema != STORED_SCHEMA ||
      stored.lastResult > static_cast<uint8_t>(CheckResult::FAILED) ||
      stored.updateAvailable > 1 || stored.attemptedWithoutTime > 1) {
    return false;
  }
  return isNullTerminated(stored.remoteVersionLabel,
                          sizeof(stored.remoteVersionLabel)) &&
         isNullTerminated(stored.remoteBuildId,
                          sizeof(stored.remoteBuildId)) &&
         isNullTerminated(stored.notes, sizeof(stored.notes)) &&
         isNullTerminated(stored.message, sizeof(stored.message));
}

Status fromStored(const StoredState& stored) {
  Status status;
  status.initialized = true;
  status.lastAttemptEpoch = stored.lastAttemptEpoch;
  status.lastSuccessEpoch = stored.lastSuccessEpoch;
  status.remoteVersionCode = stored.remoteVersionCode;
  status.lastResult = static_cast<CheckResult>(stored.lastResult);
  if (status.lastResult == CheckResult::QUEUED ||
      status.lastResult == CheckResult::CHECKING ||
      status.lastResult == CheckResult::ABORTED) {
    status.lastResult = CheckResult::NEVER;
  }
  status.checking = false;
  status.manualQueued = false;
  status.updateAvailable =
      stored.updateAvailable != 0 &&
      stored.remoteVersionCode > FIRMWARE_VERSION_CODE;
  memcpy(status.remoteVersionLabel, stored.remoteVersionLabel,
         sizeof(status.remoteVersionLabel));
  memcpy(status.remoteBuildId, stored.remoteBuildId,
         sizeof(status.remoteBuildId));
  memcpy(status.notes, stored.notes, sizeof(status.notes));
  memcpy(status.message, stored.message, sizeof(status.message));
  if (!status.updateAvailable &&
      status.remoteVersionCode <= FIRMWARE_VERSION_CODE) {
    clearRelease(status);
  }
  return status;
}

void persist(const Status& status) {
  if (!persistenceReady) return;
  const StoredState stored = toStored(status);
  const size_t written = preferences.putBytes(NVS_KEY, &stored, sizeof(stored));
  if (written != sizeof(stored)) {
    persistenceReady = false;
    Serial.printf("Update-check NVS write failed: %u/%u bytes\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(sizeof(stored)));
  }
}

bool equalsIgnoreCase(const char* left, const char* right) {
  return left && right && strcasecmp(left, right) == 0;
}

const char* skipWhitespace(const char* text) {
  while (text && (*text == ' ' || *text == '\t')) ++text;
  return text;
}

bool equalsHeaderToken(const char* text, const char* expected) {
  text = skipWhitespace(text);
  if (!text || !expected) return false;
  const size_t expectedLength = strlen(expected);
  if (strncasecmp(text, expected, expectedLength) != 0) return false;
  text += expectedLength;
  text = skipWhitespace(text);
  return *text == 0;
}

bool parseUnsignedHeader(const char* text, uint64_t& value) {
  text = skipWhitespace(text);
  if (!text || !text[0]) return false;
  uint64_t parsed = 0;
  while (*text >= '0' && *text <= '9') {
    const uint8_t digit = static_cast<uint8_t>(*text - '0');
    if (parsed > (UINT64_MAX - digit) / 10U) return false;
    parsed = parsed * 10U + digit;
    ++text;
  }
  text = skipWhitespace(text);
  if (*text != 0) return false;
  value = parsed;
  return true;
}

const char* headerFailureMessage(HeaderFailure failure) {
  switch (failure) {
    case HeaderFailure::TOTAL_BYTES:
      return "GitHub response headers exceeded the 16384-byte limit";
    case HeaderFailure::CONTENT_LENGTH:
      return "GitHub Content-Length header was invalid or conflicting";
    case HeaderFailure::TRANSFER_ENCODING:
      return "GitHub Transfer-Encoding header was invalid or repeated";
    case HeaderFailure::LOCATION_TOO_LONG:
      return "GitHub redirect Location exceeded the 4095-byte limit";
    case HeaderFailure::CONFLICTING_FRAMING:
      return "GitHub response contained conflicting framing headers";
    case HeaderFailure::NONE:
    default:
      return "GitHub response headers were invalid";
  }
}

void setHeaderFailure(HttpHeaderState& state, HeaderFailure failure) {
  if (state.failure == HeaderFailure::NONE) state.failure = failure;
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

bool validateHttpsUrl(const char* url, char* host, size_t hostCapacity) {
  return update_policy::parseAllowedHttpsUrl(url, host, hostCapacity);
}

bool captureReleaseTag(const char* redirectUrl, char* destination,
                       size_t capacity) {
  if (!redirectUrl || !destination || capacity == 0 || destination[0]) {
    return true;
  }
  const size_t prefixLength = sizeof(RELEASE_DOWNLOAD_PREFIX) - 1U;
  if (strncmp(redirectUrl, RELEASE_DOWNLOAD_PREFIX, prefixLength) != 0) {
    return true;
  }
  const char* tag = redirectUrl + prefixLength;
  const char* slash = strchr(tag, '/');
  if (!slash || slash == tag || strcmp(slash + 1, MANIFEST_ASSET_NAME) != 0) {
    return false;
  }
  const size_t length = static_cast<size_t>(slash - tag);
  if (length >= capacity) return false;
  memcpy(destination, tag, length);
  destination[length] = 0;
  return isSafeText(destination, capacity - 1U, false);
}

void releaseHttpClient(esp_http_client_handle_t client, bool opened) {
  if (!client) return;
  if (opened) esp_http_client_close(client);
  esp_http_client_cleanup(client);
  delay(TRANSPORT_RELEASE_DELAY_MS);
}

void freeResponse(HttpResponse& response) {
  if (response.body) heap_caps_free(response.body);
  response = HttpResponse{};
}

bool shouldAbortForOta() {
  ota_update::Status ota;
  ota_update::copyStatus(ota);
  return ota.serverRunning || ota_update::busy();
}

bool mqttBusyForUpdateCheck() {
  mqtt_service::Status mqtt;
  mqtt_service::copyStatus(mqtt);
  return mqtt.maintenanceActive || mqtt.state == mqtt_service::State::STARTING ||
         mqtt.state == mqtt_service::State::CONNECTING ||
         mqtt.state == mqtt_service::State::STOPPING;
}

AbortCause currentAbortCause(uint32_t absoluteDeadlineMs) {
  if (deadlineReached(absoluteDeadlineMs)) return AbortCause::DEADLINE;
  if (abortWasRequested()) return AbortCause::COMMAND;
  if (shouldAbortForOta()) return AbortCause::OTA;
  if (WiFi.status() != WL_CONNECTED) return AbortCause::WIFI;
  return AbortCause::NONE;
}

const char* abortCauseMessage(AbortCause cause) {
  switch (cause) {
    case AbortCause::COMMAND:
      return "Update check cancelled for an ADS-B refresh or reconnect";
    case AbortCause::OTA:
      return "Update check cancelled for local OTA maintenance";
    case AbortCause::WIFI:
      return "Wi-Fi disconnected during the update check";
    case AbortCause::DEADLINE:
      return "Update check exceeded its six-second deadline";
    case AbortCause::NONE:
    default:
      return "Update check stopped";
  }
}

bool stopCheckIfNeeded(uint32_t absoluteDeadlineMs, char* failure,
                       size_t failureCapacity, bool& yielded) {
  const AbortCause cause = currentAbortCause(absoluteDeadlineMs);
  if (cause == AbortCause::NONE) return false;
  yielded = cause == AbortCause::COMMAND || cause == AbortCause::OTA;
  copyText(failure, failureCapacity, abortCauseMessage(cause));
  return true;
}

bool httpGetManifest(uint32_t absoluteDeadlineMs, HttpResponse& response,
                     char* failure, size_t failureCapacity, bool& yielded) {
  freeResponse(response);
  yielded = false;
  WorkspaceGuard workspaceGuard;
  HttpWorkspace* workspace = workspaceGuard.get();
  if (!workspace) {
    copyText(failure, failureCapacity,
             "Release-check PSRAM workspace allocation failed");
    return false;
  }
  copyText(workspace->currentUrl, sizeof(workspace->currentUrl),
           LATEST_MANIFEST_URL);

  for (uint8_t redirect = 0; redirect <= MAX_REDIRECTS; ++redirect) {
    if (stopCheckIfNeeded(absoluteDeadlineMs, failure, failureCapacity,
                          yielded)) {
      return false;
    }

    workspace->host[0] = 0;
    if (!validateHttpsUrl(workspace->currentUrl, workspace->host,
                          sizeof(workspace->host))) {
      copyText(failure, failureCapacity, "GitHub returned an unsafe URL");
      return false;
    }

    workspace->headers = HttpHeaderState{};
    const uint32_t connectBudgetMs = std::min<uint32_t>(
        HTTP_CONNECT_TIMEOUT_MS, remainingToDeadline(absoluteDeadlineMs));
    if (connectBudgetMs < MINIMUM_HTTP_BUDGET_MS) {
      copyText(failure, failureCapacity,
               "Update check had insufficient HTTPS deadline remaining");
      return false;
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
    config.buffer_size_tx = 512;
    config.keep_alive_enable = false;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.skip_cert_common_name_check = false;
    config.event_handler = httpEventHandler;
    config.user_data = &workspace->headers;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
      copyText(failure, failureCapacity,
               "GitHub HTTPS client allocation failed");
      return false;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Connection", "close");

    bool opened = false;
    const esp_err_t openResult = esp_http_client_open(client, 0);
    if (openResult != ESP_OK) {
      const AbortCause cause = currentAbortCause(absoluteDeadlineMs);
      if (cause != AbortCause::NONE) {
        yielded = cause == AbortCause::COMMAND || cause == AbortCause::OTA;
        copyText(failure, failureCapacity, abortCauseMessage(cause));
      } else {
        snprintf(failure, failureCapacity, "GitHub TLS connect failed: %s",
                 esp_err_to_name(openResult));
      }
      releaseHttpClient(client, false);
      return false;
    }
    opened = true;
    if (stopCheckIfNeeded(absoluteDeadlineMs, failure, failureCapacity,
                          yielded)) {
      releaseHttpClient(client, opened);
      return false;
    }

    const int64_t fetchedLength = esp_http_client_fetch_headers(client);
    if (workspace->headers.failure != HeaderFailure::NONE ||
        fetchedLength < 0) {
      const AbortCause cause = currentAbortCause(absoluteDeadlineMs);
      if (cause != AbortCause::NONE) {
        yielded = cause == AbortCause::COMMAND || cause == AbortCause::OTA;
        copyText(failure, failureCapacity, abortCauseMessage(cause));
      } else if (workspace->headers.failure != HeaderFailure::NONE) {
        copyText(failure, failureCapacity,
                 headerFailureMessage(workspace->headers.failure));
      } else {
        const int socketError = esp_http_client_get_errno(client);
        snprintf(failure, failureCapacity,
                 "GitHub header fetch failed: result=%lld errno=%d",
                 static_cast<long long>(fetchedLength), socketError);
      }
      releaseHttpClient(client, opened);
      return false;
    }
    if (stopCheckIfNeeded(absoluteDeadlineMs, failure, failureCapacity,
                          yielded)) {
      releaseHttpClient(client, opened);
      return false;
    }

    const int statusCode = esp_http_client_get_status_code(client);
    const bool redirectStatus = statusCode == 301 || statusCode == 302 ||
                                statusCode == 303 || statusCode == 307 ||
                                statusCode == 308;
    if (redirectStatus) {
      if (redirect >= MAX_REDIRECTS || !workspace->headers.location[0] ||
          !captureReleaseTag(workspace->headers.location, response.releaseTag,
                             sizeof(response.releaseTag))) {
        copyText(failure, failureCapacity,
                 "GitHub redirect policy rejected response");
        releaseHttpClient(client, opened);
        return false;
      }
      workspace->redirectHost[0] = 0;
      if (!validateHttpsUrl(workspace->headers.location,
                            workspace->redirectHost,
                            sizeof(workspace->redirectHost))) {
        copyText(failure, failureCapacity,
                 "GitHub redirect host was rejected");
        releaseHttpClient(client, opened);
        return false;
      }
      copyText(workspace->currentUrl, sizeof(workspace->currentUrl),
               workspace->headers.location);
      releaseHttpClient(client, opened);
      continue;
    }

    response.statusCode = statusCode;
    if (statusCode != 200) {
      releaseHttpClient(client, opened);
      return true;
    }

    const bool chunked = esp_http_client_is_chunked_response(client);
    if (!update_policy::framingIsUnambiguous(
            workspace->headers.contentLengthSeen,
            workspace->headers.transferEncodingSeen,
            workspace->headers.chunkedOnly, chunked)) {
      copyText(failure, failureCapacity,
               "GitHub response framing was ambiguous");
      releaseHttpClient(client, opened);
      return false;
    }
    if (workspace->headers.contentLengthSeen &&
        workspace->headers.contentLength > MAX_MANIFEST_RESPONSE_BYTES) {
      copyText(failure, failureCapacity,
               "Release manifest exceeded size limit");
      releaseHttpClient(client, opened);
      return false;
    }

    const size_t capacity = workspace->headers.contentLengthSeen
        ? static_cast<size_t>(workspace->headers.contentLength)
        : MAX_MANIFEST_RESPONSE_BYTES;
    if (capacity == 0) {
      copyText(failure, failureCapacity,
               "GitHub returned an empty manifest");
      releaseHttpClient(client, opened);
      return false;
    }
    response.body = static_cast<uint8_t*>(heap_caps_malloc(
        capacity + 1U, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!response.body) {
      copyText(failure, failureCapacity,
               "Release manifest PSRAM allocation failed");
      releaseHttpClient(client, opened);
      return false;
    }

    const uint32_t bodyBudgetMs = std::min<uint32_t>(
        HTTP_BODY_IDLE_TIMEOUT_MS, remainingToDeadline(absoluteDeadlineMs));
    if (bodyBudgetMs < MINIMUM_HTTP_BUDGET_MS) {
      copyText(failure, failureCapacity,
               "Update check had insufficient body-read deadline remaining");
      releaseHttpClient(client, opened);
      freeResponse(response);
      return false;
    }
    esp_http_client_set_timeout_ms(client, static_cast<int>(bodyBudgetMs));
    size_t received = 0;
    uint32_t lastProgressMs = millis();
    bool readFailed = false;
    while (received < capacity) {
      if (stopCheckIfNeeded(absoluteDeadlineMs, failure, failureCapacity,
                            yielded) ||
          millis() - lastProgressMs >= HTTP_BODY_IDLE_TIMEOUT_MS) {
        if (!failure[0]) {
          copyText(failure, failureCapacity,
                   "Release manifest body idle timeout");
        }
        readFailed = true;
        break;
      }
      const size_t remaining = capacity - received;
      const int requestBytes = static_cast<int>(
          std::min<size_t>(remaining, 1024U));
      const int bytesRead = esp_http_client_read(
          client, reinterpret_cast<char*>(response.body + received),
          requestBytes);
      if (bytesRead > 0) {
        received += static_cast<size_t>(bytesRead);
        lastProgressMs = millis();
      } else if (bytesRead == 0) {
        if (esp_http_client_is_complete_data_received(client)) break;
        readFailed = true;
        break;
      } else {
        const int socketError = esp_http_client_get_errno(client);
        if ((bytesRead == -ESP_ERR_HTTP_EAGAIN || socketError == EAGAIN ||
             socketError == EWOULDBLOCK || socketError == ETIMEDOUT) &&
            currentAbortCause(absoluteDeadlineMs) == AbortCause::NONE) {
          continue;
        }
        readFailed = true;
        break;
      }
    }

    const bool complete = esp_http_client_is_complete_data_received(client);
    releaseHttpClient(client, opened);
    const bool lengthMatches =
        !workspace->headers.contentLengthSeen ||
        received == workspace->headers.contentLength;
    if (readFailed || !complete || !lengthMatches ||
        received > MAX_MANIFEST_RESPONSE_BYTES) {
      if (!failure[0]) {
        copyText(failure, failureCapacity,
                 "Release manifest body was incomplete");
      }
      freeResponse(response);
      return false;
    }
    response.body[received] = 0;
    response.length = received;
    return true;
  }

  copyText(failure, failureCapacity,
           "GitHub redirect count exceeded limit");
  return false;
}

DeferredReason automaticWaitReason(const Status& status) {
  const uint32_t nowMs = millis();
  if (nowMs - bootStartedMs < STARTUP_DELAY_MS) {
    return DeferredReason::STARTUP_TIMER;
  }
  if (lastAttemptUptimeMs &&
      nowMs - lastAttemptUptimeMs < CHECK_INTERVAL_SECONDS * 1000UL) {
    return DeferredReason::DAILY_TIMER;
  }
  const uint32_t nowEpoch = currentEpoch();
  if (nowEpoch) {
    if (status.lastAttemptEpoch && status.lastAttemptEpoch <= nowEpoch &&
        nowEpoch - status.lastAttemptEpoch < CHECK_INTERVAL_SECONDS) {
      return DeferredReason::DAILY_TIMER;
    }
    return DeferredReason::NONE;
  }
  return bootAttemptedWithoutTime || persistedAttemptWithoutTime
             ? DeferredReason::NO_TIME_TIMER
             : DeferredReason::NONE;
}

bool automaticCheckDue(const Status& status) {
  return automaticWaitReason(status) == DeferredReason::NONE;
}

void commitAttemptTimestamp(Status& status) {
  lastAttemptUptimeMs = millis();
  const uint32_t epoch = currentEpoch();
  if (epoch) {
    status.lastAttemptEpoch = epoch;
    persistedAttemptWithoutTime = false;
  } else {
    bootAttemptedWithoutTime = true;
    persistedAttemptWithoutTime = true;
  }
}

void finishAborted(Status status, const char* message, bool manual) {
  status.checking = false;
  status.manualQueued = manual;
  status.lastResult = manual ? CheckResult::QUEUED : CheckResult::ABORTED;
  if (manual) {
    char requeuedMessage[sizeof(status.message)]{};
    snprintf(requeuedMessage, sizeof(requeuedMessage),
             "CHECK NOW requeued: %s", message ? message : "check yielded");
    copyText(status.message, sizeof(status.message), requeuedMessage);
  } else {
    copyText(status.message, sizeof(status.message), message);
  }
  publishStatus(status);
  Serial.printf("GitHub update check aborted%s: %s\n",
                manual ? " and requeued" : "", status.message);
}

void finishFailure(Status status, const char* message) {
  status.checking = false;
  status.manualQueued = false;
  status.lastResult = CheckResult::FAILED;
  commitAttemptTimestamp(status);
  copyText(status.message, sizeof(status.message), message);
  publishStatus(status);
  persist(status);
  Serial.printf("GitHub update check failed: %s\n", status.message);
}

void finishNoUpdate(Status status, CheckResult result, const char* message) {
  status.checking = false;
  status.manualQueued = false;
  status.lastResult = result;
  status.lastSuccessEpoch = currentEpoch();
  commitAttemptTimestamp(status);
  clearRelease(status);
  copyText(status.message, sizeof(status.message), message);
  publishStatus(status);
  persist(status);
  Serial.printf("GitHub update check: %s\n", message);
}

bool validateManifest(JsonDocument& manifest, const char* releaseTag,
                      Status& status, char* failure,
                      size_t failureCapacity) {
  const uint32_t schema = manifest["schema"] | 0U;
  const char* tag = manifest["tag"] | "";
  const char* hardware = manifest["hardware"] | "";
  const char* channel = manifest["channel"] | "";
  const uint32_t versionCode = manifest["version_code"] | 0U;
  const char* versionLabel = manifest["version_label"] | "";
  const char* buildId = manifest["build_id"] | "";
  const char* asset = manifest["asset"] | "";
  const uint32_t packageSize = manifest["package_size"] | 0U;
  const char* packageSha = manifest["package_sha256"] | "";
  const uint32_t firmwareSize = manifest["firmware_size"] | 0U;
  const char* firmwareSha = manifest["firmware_sha256"] | "";
  const uint32_t minimumUpdater = manifest["min_updater"] | UINT32_MAX;
  const char* notes = manifest["notes"] | "";

  if (schema != FIRMWARE_MANIFEST_SCHEMA) {
    copyText(failure, failureCapacity,
             "Release manifest schema is unsupported");
    return false;
  }
  if ((releaseTag && releaseTag[0] && strcmp(tag, releaseTag) != 0) ||
      !update_policy::identityMatches(
          schema, FIRMWARE_MANIFEST_SCHEMA, hardware, FIRMWARE_HARDWARE_ID,
          channel, FIRMWARE_RELEASE_CHANNEL, minimumUpdater,
          FIRMWARE_UPDATER_VERSION)) {
    copyText(failure, failureCapacity,
             minimumUpdater > FIRMWARE_UPDATER_VERSION
                 ? "Release requires a newer updater"
                 : "Release identity, hardware, or channel is invalid");
    return false;
  }
  if (versionCode == 0 ||
      !isSafeText(tag, update_policy::MAX_TAG_LENGTH, false) ||
      !isSafeText(versionLabel, update_policy::MAX_VERSION_LABEL_LENGTH, false) ||
      !isSafeText(buildId, update_policy::MAX_BUILD_ID_LENGTH, false) ||
      strncmp(buildId, "7IN-", 4) != 0 ||
      !update_policy::assetNameValid(asset) ||
      !update_policy::packageLayoutValid(packageSize, firmwareSize) ||
      !isLowerHexDigest(packageSha) || !isLowerHexDigest(firmwareSha) ||
      !isSafeText(notes, update_policy::MAX_NOTES_LENGTH, true)) {
    copyText(failure, failureCapacity,
             "Release manifest fields are invalid");
    return false;
  }

  char expectedTag[32]{};
  char expectedLabel[32]{};
  char expectedAsset[160]{};
  char expectedBuildToken[32]{};
  snprintf(expectedTag, sizeof(expectedTag), "product-%lu",
           static_cast<unsigned long>(versionCode));
  snprintf(expectedLabel, sizeof(expectedLabel), "Product %lu",
           static_cast<unsigned long>(versionCode));
  snprintf(expectedAsset, sizeof(expectedAsset), "%s-product-%lu.radarota",
           FIRMWARE_HARDWARE_ID, static_cast<unsigned long>(versionCode));
  snprintf(expectedBuildToken, sizeof(expectedBuildToken), "-PRODUCT%lu-",
           static_cast<unsigned long>(versionCode));
  if (strcmp(tag, expectedTag) != 0 ||
      strcmp(versionLabel, expectedLabel) != 0 ||
      strcmp(asset, expectedAsset) != 0 ||
      strstr(buildId, expectedBuildToken) == nullptr) {
    copyText(failure, failureCapacity,
             "Release tag, label, asset, or build version is inconsistent");
    return false;
  }

  status.remoteVersionCode = versionCode;
  copyText(status.remoteVersionLabel, sizeof(status.remoteVersionLabel),
           versionLabel);
  copyText(status.remoteBuildId, sizeof(status.remoteBuildId), buildId);
  copyText(status.notes, sizeof(status.notes), notes);
  return true;
}

void performCheck(uint32_t checkDeadlineMs, bool manual) {
  Status status = snapshotStatus();
  if (!status.checking) return;

  char failure[128]{};
  HttpResponse response;
  bool yielded = false;
  if (!httpGetManifest(checkDeadlineMs, response, failure,
                       sizeof(failure), yielded)) {
    if (yielded) {
      finishAborted(status, failure, manual);
    } else {
      finishFailure(status, failure);
    }
    return;
  }
  if (response.statusCode == 404) {
    freeResponse(response);
    finishNoUpdate(status, CheckResult::NO_RELEASE,
                   "No stable GitHub Release is published");
    return;
  }
  if (response.statusCode != 200) {
    snprintf(failure, sizeof(failure),
             "GitHub release download returned HTTP %d",
             response.statusCode);
    freeResponse(response);
    finishFailure(status, failure);
    return;
  }

  JsonDocument filter(&psramAllocator);
  const char* fields[] = {
      "schema", "tag", "hardware", "channel", "version_code",
      "version_label", "build_id", "asset", "package_size",
      "package_sha256", "firmware_size", "firmware_sha256",
      "min_updater", "notes"};
  for (const char* field : fields) filter[field] = true;
  JsonDocument manifest(&psramAllocator);
  const DeserializationError error = deserializeJson(
      manifest, response.body, response.length,
      DeserializationOption::Filter(filter));
  char releaseTag[sizeof(response.releaseTag)]{};
  copyText(releaseTag, sizeof(releaseTag), response.releaseTag);
  freeResponse(response);
  if (error || manifest.overflowed() ||
      !validateManifest(manifest, releaseTag, status, failure,
                        sizeof(failure))) {
    finishFailure(status, failure[0] ? failure
                                     : "Release manifest JSON was invalid");
    return;
  }

  status.checking = false;
  status.manualQueued = false;
  status.lastSuccessEpoch = currentEpoch();
  commitAttemptTimestamp(status);
  const update_policy::VersionRelation relation =
      update_policy::compareVersion(status.remoteVersionCode,
                                    FIRMWARE_VERSION_CODE);
  if (relation == update_policy::VersionRelation::NEWER) {
    status.updateAvailable = true;
    status.lastResult = CheckResult::UPDATE_AVAILABLE;
    copyText(status.message, sizeof(status.message),
             "A compatible stable firmware update is available");
  } else {
    status.updateAvailable = false;
    status.lastResult = CheckResult::CURRENT;
    copyText(status.message, sizeof(status.message),
             relation == update_policy::VersionRelation::CURRENT
                 ? "Installed firmware is current"
                 : "Latest stable release is older than installed firmware");
    clearRelease(status);
  }
  publishStatus(status);
  persist(status);
  Serial.printf("GitHub update check complete (%s): %s\n",
                manual ? "manual" : "automatic", status.message);
}

}  // namespace

bool begin() {
  bootStartedMs = millis();
  lastAttemptUptimeMs = 0;
  bootAttemptedWithoutTime = false;
  persistedAttemptWithoutTime = false;
  preparedCheckDeadlineMs = 0;
  preparedCheckReady = false;
  preparedCheckManual = false;
  abortRequested = false;
  lastDeferredReason = DeferredReason::NONE;
  lastDeferredWasManual = false;

  currentStatus = Status{};
  currentStatus.initialized = true;
  copyText(currentStatus.message, sizeof(currentStatus.message),
           "Automatic check waits five minutes; CHECK NOW is ready");

  persistenceReady = preferences.begin(NVS_NAMESPACE, false);
  if (persistenceReady && preferences.getType(NVS_KEY) == PT_BLOB) {
    const size_t storedLength = preferences.getBytesLength(NVS_KEY);
    if (storedLength == sizeof(StoredState)) {
      StoredState stored;
      if (preferences.getBytes(NVS_KEY, &stored, sizeof(stored)) ==
              sizeof(stored) &&
          storedStateValid(stored)) {
        persistedAttemptWithoutTime = stored.attemptedWithoutTime != 0;
        currentStatus = fromStored(stored);
        currentStatus.initialized = true;
      } else if (storedLength > 0) {
        Serial.println(
            "GitHub update cache ignored: incompatible pre-Product-70 schema");
      }
    } else if (storedLength > 0) {
      Serial.println(
          "GitHub update cache ignored: incompatible pre-Product-70 size");
    }
  }
  currentStatus.checking = false;
  currentStatus.manualQueued = false;
  currentStatus.statusVersion = 1;
  Serial.println(
      "GitHub update checker ready: automatic 5-minute/24-hour schedule; "
      "manual CHECK NOW enabled");
  return persistenceReady;
}

bool requestManualCheck() {
  Status status = snapshotStatus();
  if (!status.initialized) {
    Serial.println("GitHub update CHECK NOW rejected: manager unavailable");
    return false;
  }
  if (status.checking) {
    Serial.println("GitHub update CHECK NOW ignored: check already running");
    return false;
  }
  if (status.manualQueued) {
    Serial.println("GitHub update CHECK NOW already queued");
    return true;
  }

  status.manualQueued = true;
  status.lastResult = CheckResult::QUEUED;
  copyText(status.message, sizeof(status.message),
           "CHECK NOW queued; waiting for a safe ADS-B window");
  publishStatus(status);
  Serial.println(
      "GitHub update CHECK NOW queued; awaiting successful ADS-B cycle");
  return true;
}

bool prepareAfterSuccessfulAdsb(uint32_t nextAdsbPollAtMs) {
  Status status = snapshotStatus();
  if (!status.initialized || status.checking) return false;

  const bool manual = status.manualQueued;
  if (!manual && !automaticCheckDue(status)) {
    const DeferredReason scheduleReason = automaticWaitReason(status);
    logDeferred(scheduleReason, false);
    return false;
  }

  DeferredReason reason = DeferredReason::NONE;
  if (WiFi.status() != WL_CONNECTED) {
    reason = DeferredReason::WIFI;
  } else if (shouldAbortForOta()) {
    reason = DeferredReason::OTA;
  } else if (mqttBusyForUpdateCheck()) {
    reason = DeferredReason::MQTT;
  } else if (adsb::wifiOperationInProgress()) {
    reason = DeferredReason::WIFI_RECOVERY;
  } else if (!app_state::fetchInProgress()) {
    reason = DeferredReason::FETCH_STATE;
  }

  const uint32_t now = millis();
  if (reason == DeferredReason::NONE &&
      !update_policy::enoughSlack(now, nextAdsbPollAtMs,
                                  MINIMUM_ADSB_SLACK_MS)) {
    reason = DeferredReason::SLACK;
  }
  if (reason != DeferredReason::NONE) {
    logDeferred(reason, manual);
    return false;
  }
  logDeferred(DeferredReason::NONE, manual);

  preparedCheckDeadlineMs = update_policy::boundedDeadline(
      now, nextAdsbPollAtMs, CHECK_TOTAL_TIMEOUT_MS, ADSB_POLL_GUARD_MS);
  preparedCheckReady = true;
  preparedCheckManual = manual;
  clearAbortRequest();
  status.checking = true;
  status.manualQueued = false;
  status.lastResult = CheckResult::CHECKING;
  copyText(status.message, sizeof(status.message),
           manual ? "Running CHECK NOW against stable GitHub release"
                  : "Running scheduled stable GitHub release check");
  publishStatus(status);
  Serial.printf(
      "GitHub update check starting: %s, slack=%lu ms, budget=%lu ms\n",
      manual ? "manual" : "automatic",
      static_cast<unsigned long>(nextAdsbPollAtMs - now),
      static_cast<unsigned long>(remainingToDeadline(preparedCheckDeadlineMs)));
  return true;
}

void performPreparedCheck() {
  if (!preparedCheckReady) return;
  const uint32_t deadline = preparedCheckDeadlineMs;
  const bool manual = preparedCheckManual;
  preparedCheckReady = false;
  preparedCheckManual = false;
  if (!networkCheckInProgress()) return;

  const uint32_t activityStarted = millis();
  performCheck(deadline, manual);
  app_state::recordActivityWindow(app_state::ActivityStage::OTHER,
                                  activityStarted, millis());
}

void requestAbort() {
  portENTER_CRITICAL(&statusMux);
  if (currentStatus.checking) abortRequested = true;
  portEXIT_CRITICAL(&statusMux);
}

bool networkCheckInProgress() {
  portENTER_CRITICAL(&statusMux);
  const bool checking = currentStatus.checking;
  portEXIT_CRITICAL(&statusMux);
  return checking;
}

void copyStatus(Status& status) {
  portENTER_CRITICAL(&statusMux);
  status = currentStatus;
  portEXIT_CRITICAL(&statusMux);
}

const char* checkResultName(CheckResult result) {
  switch (result) {
    case CheckResult::NEVER: return "NOT CHECKED";
    case CheckResult::QUEUED: return "QUEUED";
    case CheckResult::CHECKING: return "CHECKING";
    case CheckResult::CURRENT: return "CURRENT";
    case CheckResult::UPDATE_AVAILABLE: return "AVAILABLE";
    case CheckResult::NO_RELEASE: return "NO RELEASE";
    case CheckResult::ABORTED: return "ABORTED";
    case CheckResult::FAILED: return "FAILED";
    default: return "UNKNOWN";
  }
}

}  // namespace update_manager
