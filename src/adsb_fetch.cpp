#include "adsb_fetch.h"

#include <ArduinoJson.h>
#include <cerrno>
#include <cstring>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>

#include "adsb_network.h"
#include "adsb_transport_policy.h"
#include "adsb_diagnostics.h"
#include "config.h"
#include "settings.h"

namespace adsb_fetch {
namespace {

namespace policy = adsb_transport_policy;

constexpr size_t MAX_RESPONSE_BYTES = 250000;
constexpr size_t MAX_HTTP_HEADER_BYTES = 8192;
constexpr size_t MAX_HTTP_LINE_BYTES = 512;
constexpr size_t MAX_CHUNK_FRAMING_BYTES = 16384;
constexpr uint8_t MAX_NATIVE_ATTEMPTS = 2;

uint32_t verboseDiagnosticUs = 0;

#if ADSB_VERBOSE_FETCH_LOGGING
#define ADSB_VERBOSE_PRINTF(...) \
  do { \
    const uint32_t logStartedUs = micros(); \
    Serial.printf(__VA_ARGS__); \
    verboseDiagnosticUs += micros() - logStartedUs; \
  } while (0)
#define ADSB_VERBOSE_PRINTLN(text) \
  do { \
    const uint32_t logStartedUs = micros(); \
    Serial.println(text); \
    verboseDiagnosticUs += micros() - logStartedUs; \
  } while (0)
#else
#define ADSB_VERBOSE_PRINTF(...) do { } while (0)
#define ADSB_VERBOSE_PRINTLN(text) do { } while (0)
#endif

void logMemoryStage(const char* stage) {
  app_state::observeFetchMemory(stage);
  ADSB_VERBOSE_PRINTF(
      "MEM ADSB %-18s heap=%u block=%u psram=%u\n",
      stage ? stage : "unknown", ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      ESP.getFreePsram());
}

struct AttemptResult {
  bool success = false;
  bool cancelled = false;
  bool transportBudgetExhausted = false;
  bool fallbackEligible = false;
  bool requiresWifiRecovery = false;
  app_state::FetchFailureStage failureStage =
      app_state::FetchFailureStage::NONE;
  uint32_t responseBytes = 0;
  uint32_t jsonDeserializeUs = 0;
};

enum class BoundedWaitResult : uint8_t {
  COMPLETE,
  CANCELLED,
  BUDGET_EXHAUSTED,
};

bool abortRequested() {
  return adsb::fetchAbortRequested();
}

uint32_t transportRemainingMs(uint32_t fetchStarted) {
  return policy::transportRemainingMs(fetchStarted, millis());
}

BoundedWaitResult waitInterruptible(uint32_t durationMs,
                                    uint32_t fetchStarted) {
  const uint32_t waitStarted = millis();
  while (static_cast<uint32_t>(millis() - waitStarted) < durationMs) {
    if (abortRequested()) return BoundedWaitResult::CANCELLED;
    const uint32_t remaining = transportRemainingMs(fetchStarted);
    if (remaining == 0) return BoundedWaitResult::BUDGET_EXHAUSTED;
    const uint32_t elapsed = millis() - waitStarted;
    const uint32_t waitRemaining = durationMs - elapsed;
    delay(min(min(waitRemaining, remaining), static_cast<uint32_t>(10)));
  }
  return BoundedWaitResult::COMPLETE;
}

AttemptResult cancelled(uint32_t responseBytes = 0) {
  AttemptResult result;
  result.cancelled = true;
  result.responseBytes = responseBytes;
  return result;
}

AttemptResult failed(app_state::FetchFailureStage stage,
                     uint32_t responseBytes = 0,
                     bool fallbackEligible = false,
                     bool requiresWifiRecovery = false,
                     bool transportBudgetExhausted = false) {
  AttemptResult result;
  result.failureStage = stage;
  result.responseBytes = responseBytes;
  result.fallbackEligible = fallbackEligible;
  result.requiresWifiRecovery = requiresWifiRecovery;
  result.transportBudgetExhausted = transportBudgetExhausted;
  return result;
}

uint8_t failureProgress(app_state::FetchFailureStage stage) {
  switch (stage) {
    case app_state::FetchFailureStage::WIFI: return 1;
    case app_state::FetchFailureStage::DNS: return 2;
    case app_state::FetchFailureStage::TCP: return 3;
    case app_state::FetchFailureStage::TLS: return 4;
    case app_state::FetchFailureStage::HTTP_HEADERS: return 5;
    case app_state::FetchFailureStage::HTTP_STATUS: return 6;
    case app_state::FetchFailureStage::RESPONSE_BODY: return 7;
    case app_state::FetchFailureStage::JSON: return 8;
    default: return 0;
  }
}

void preserveMostAdvancedFailure(AttemptResult& preserved,
                                 const AttemptResult& candidate) {
  if (candidate.success || candidate.cancelled) return;
  const bool budgetExhausted = preserved.transportBudgetExhausted ||
                               candidate.transportBudgetExhausted;
  const bool madeMoreProgress =
      candidate.responseBytes > preserved.responseBytes;
  const bool reachedLaterStage =
      candidate.responseBytes == preserved.responseBytes &&
      failureProgress(candidate.failureStage) >
          failureProgress(preserved.failureStage);
  if (madeMoreProgress || reachedLaterStage ||
      preserved.failureStage == app_state::FetchFailureStage::NONE) {
    preserved = candidate;
  }
  preserved.transportBudgetExhausted = budgetExhausted;
}

void settleReleasedTransport(uint32_t fetchStarted) {
  if (abortRequested()) return;
  const uint32_t remaining = transportRemainingMs(fetchStarted);
  if (remaining == 0) return;
  waitInterruptible(min(policy::TRANSPORT_RELEASE_DELAY_MS, remaining),
                    fetchStarted);
}

void releaseNativeClient(esp_http_client_handle_t client, bool opened,
                         uint32_t fetchStarted) {
  if (!client) return;
  if (opened) esp_http_client_close(client);
  esp_http_client_cleanup(client);
  // Give lwIP/esp-tls a short bounded release window before another socket.
  settleReleasedTransport(fetchStarted);
}

void releaseFallbackClient(WiFiClientSecure& client,
                           uint32_t fetchStarted) {
  client.stop();
  settleReleasedTransport(fetchStarted);
}

app_state::FetchFailureStage diagnoseSecureConnectFailure(
    const IPAddress& serverIp, uint32_t fetchStarted) {
  // A short raw TCP probe only runs while shared transport budget remains.
  const uint32_t remaining = transportRemainingMs(fetchStarted);
  if (remaining < policy::MIN_BLOCKING_CALL_BUDGET_MS || abortRequested()) {
    return app_state::FetchFailureStage::TLS;
  }
  const uint32_t timeoutMs = policy::boundedTimeoutMs(
      remaining, policy::TCP_PROBE_TIMEOUT_MS);
  WiFiClient probe;
  probe.setTimeout(timeoutMs);
  const bool tcpConnected = probe.connect(
      serverIp, 443, static_cast<int32_t>(timeoutMs));
  probe.stop();
  return tcpConnected ? app_state::FetchFailureStage::TLS
                      : app_state::FetchFailureStage::TCP;
}

// PSRAM-only allocator for both ADS-B JsonDocuments. The parsed response
// tree is the significant allocation; keeping the small filter on the same
// allocator removes the last default-heap allocation from the parse path.
class PsramAllocator final : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  void deallocate(void* ptr) override { heap_caps_free(ptr); }

  void* reallocate(void* ptr, size_t newSize) override {
    return heap_caps_realloc(ptr, newSize,
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
};

PsramAllocator psramJsonAllocator;

// Prefer PSRAM. Never fall back to internal malloc - a mid-sized response
// landing in internal DRAM would damage the largest free block during the
// highest-pressure stage of the fetch.
uint8_t* allocatePayload(size_t capacity) {
  return static_cast<uint8_t*>(heap_caps_malloc(
      capacity + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

class BundleVerifiedSecureClient final : public WiFiClientSecure {
 public:
  void useDefaultCaBundle() {
    // Arduino-ESP32 3.0.7 exposes these as protected members. Enabling the
    // built-in bundle keeps hostname/certificate verification active while
    // still using the independent WiFiClientSecure transport implementation.
    _use_insecure = false;
    attach_ssl_certificate_bundle(sslclient.get(), true);
    _use_ca_bundle = true;
  }
};

enum class SecureReadResult : uint8_t {
  OK,
  CLOSED,
  TIMEOUT,
  CANCELLED,
  BUDGET_EXHAUSTED,
  TOO_LARGE,
  IO_ERROR,
};

struct FallbackResponseHeaders {
  int statusCode = 0;
  int64_t contentLength = -1;
  bool chunked = false;
  bool transferEncodingSeen = false;
};

const char* secureReadResultName(SecureReadResult result) {
  switch (result) {
    case SecureReadResult::OK: return "ok";
    case SecureReadResult::CLOSED: return "closed";
    case SecureReadResult::TIMEOUT: return "timeout";
    case SecureReadResult::CANCELLED: return "cancelled";
    case SecureReadResult::BUDGET_EXHAUSTED: return "budget-exhausted";
    case SecureReadResult::TOO_LARGE: return "too-large";
    case SecureReadResult::IO_ERROR: return "io-error";
  }
  return "unknown";
}

bool elapsedAtLeast(uint32_t now, uint32_t started, uint32_t intervalMs) {
  return static_cast<uint32_t>(now - started) >= intervalMs;
}

SecureReadResult waitForSecureData(WiFiClientSecure& client,
                                   uint32_t fetchStarted,
                                   uint32_t responseStarted,
                                   uint32_t& lastProgress,
                                   uint32_t phaseStarted,
                                   uint32_t phaseTimeoutMs) {
  while (true) {
    if (abortRequested()) return SecureReadResult::CANCELLED;
    if (transportRemainingMs(fetchStarted) == 0) {
      return SecureReadResult::BUDGET_EXHAUSTED;
    }
    if (client.available() > 0) return SecureReadResult::OK;
    if (!client.connected()) return SecureReadResult::CLOSED;

    const uint32_t now = millis();
    if (elapsedAtLeast(now, lastProgress, policy::IDLE_TIMEOUT_MS) ||
        (phaseTimeoutMs > 0 &&
         elapsedAtLeast(now, phaseStarted, phaseTimeoutMs))) {
      return SecureReadResult::TIMEOUT;
    }
    (void)responseStarted;
    delay(2);
  }
}

SecureReadResult readSecureByte(WiFiClientSecure& client, uint8_t& value,
                                uint32_t fetchStarted,
                                uint32_t responseStarted,
                                uint32_t& lastProgress,
                                uint32_t phaseStarted,
                                uint32_t phaseTimeoutMs) {
  const SecureReadResult waitResult =
      waitForSecureData(client, fetchStarted, responseStarted, lastProgress,
                        phaseStarted, phaseTimeoutMs);
  if (waitResult != SecureReadResult::OK) return waitResult;

  const int readValue = client.read();
  if (readValue < 0) return SecureReadResult::IO_ERROR;
  value = static_cast<uint8_t>(readValue);
  lastProgress = millis();
  return SecureReadResult::OK;
}

SecureReadResult readSecureLine(WiFiClientSecure& client, char* line,
                                size_t lineCapacity, size_t& wireBytes,
                                size_t wireLimit, uint32_t fetchStarted,
                                uint32_t responseStarted,
                                uint32_t& lastProgress,
                                uint32_t phaseStarted,
                                uint32_t phaseTimeoutMs) {
  if (!line || lineCapacity < 2) return SecureReadResult::TOO_LARGE;

  size_t length = 0;
  while (true) {
    uint8_t value = 0;
    const SecureReadResult readResult =
        readSecureByte(client, value, fetchStarted, responseStarted,
                       lastProgress, phaseStarted, phaseTimeoutMs);
    if (readResult != SecureReadResult::OK) return readResult;

    ++wireBytes;
    if (wireBytes > wireLimit) return SecureReadResult::TOO_LARGE;
    if (value == '\n') {
      if (length == 0 || line[length - 1] != '\r') {
        return SecureReadResult::IO_ERROR;
      }
      line[--length] = 0;
      return SecureReadResult::OK;
    }
    if (value == 0 || value == 0x7f ||
        (value < 0x20 && value != '\r' && value != '\t')) {
      return SecureReadResult::IO_ERROR;
    }
    if (length + 1 >= lineCapacity) return SecureReadResult::TOO_LARGE;
    line[length++] = static_cast<char>(value);
  }
}

char asciiLower(char value) {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A'))
                                     : value;
}

bool equalsIgnoreCase(const char* left, const char* right) {
  if (!left || !right) return false;
  while (*left && *right) {
    if (asciiLower(*left++) != asciiLower(*right++)) return false;
  }
  return *left == 0 && *right == 0;
}

bool validHeaderFieldName(const char* name) {
  if (!name || !name[0]) return false;
  for (const char* value = name; *value; ++value) {
    const unsigned char ch = static_cast<unsigned char>(*value);
    const bool alphaNumeric =
        (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9');
    const bool punctuation =
        ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' ||
        ch == '\'' || ch == '*' || ch == '+' || ch == '-' || ch == '.' ||
        ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~';
    if (!alphaNumeric && !punctuation) return false;
  }
  return true;
}

const char* skipHeaderWhitespace(const char* value) {
  while (value && (*value == ' ' || *value == '\t')) ++value;
  return value;
}

bool transferEncodingIsChunkedOnly(const char* value) {
  value = skipHeaderWhitespace(value);
  if (!value) return false;
  constexpr char chunkedToken[] = "chunked";
  for (size_t i = 0; i < sizeof(chunkedToken) - 1; ++i) {
    if (asciiLower(value[i]) != chunkedToken[i]) return false;
  }
  value += sizeof(chunkedToken) - 1;
  value = skipHeaderWhitespace(value);
  return value && *value == 0;
}

bool parseHttpStatus(const char* line, int& statusCode) {
  if (!line || strncmp(line, "HTTP/", 5) != 0) return false;
  const char* value = strchr(line, ' ');
  if (!value) return false;
  value = skipHeaderWhitespace(value);
  if (!value || value[0] < '0' || value[0] > '9' || value[1] < '0' ||
      value[1] > '9' || value[2] < '0' || value[2] > '9') {
    return false;
  }
  if (value[3] != 0 && value[3] != ' ' && value[3] != '\t') {
    return false;
  }
  statusCode = (value[0] - '0') * 100 + (value[1] - '0') * 10 +
               (value[2] - '0');
  return statusCode >= 100 && statusCode <= 999;
}

bool parseContentLength(const char* value, size_t& contentLength) {
  value = skipHeaderWhitespace(value);
  if (!value || *value < '0' || *value > '9') return false;

  size_t parsed = 0;
  while (*value >= '0' && *value <= '9') {
    const size_t digit = static_cast<size_t>(*value - '0');
    if (parsed > (MAX_RESPONSE_BYTES - digit) / 10) return false;
    parsed = parsed * 10 + digit;
    ++value;
  }
  value = skipHeaderWhitespace(value);
  if (*value != 0) return false;
  contentLength = parsed;
  return true;
}

SecureReadResult readFallbackHeaders(WiFiClientSecure& client,
                                     FallbackResponseHeaders& headers,
                                     uint32_t fetchStarted,
                                     uint32_t responseStarted,
                                     uint32_t& lastProgress) {
  char line[MAX_HTTP_LINE_BYTES]{};
  size_t headerBytes = 0;
  const uint32_t headerStarted = millis();
  SecureReadResult readResult = readSecureLine(
      client, line, sizeof(line), headerBytes, MAX_HTTP_HEADER_BYTES,
      fetchStarted, responseStarted, lastProgress, headerStarted,
      policy::HEADER_TIMEOUT_MS);
  if (readResult != SecureReadResult::OK) return readResult;
  if (!parseHttpStatus(line, headers.statusCode)) {
    return SecureReadResult::IO_ERROR;
  }

  while (true) {
    readResult = readSecureLine(
        client, line, sizeof(line), headerBytes, MAX_HTTP_HEADER_BYTES,
        fetchStarted, responseStarted, lastProgress, headerStarted,
      policy::HEADER_TIMEOUT_MS);
    if (readResult != SecureReadResult::OK) return readResult;
    if (line[0] == 0) break;

    char* separator = strchr(line, ':');
    if (!separator) return SecureReadResult::IO_ERROR;
    *separator = 0;
    if (!validHeaderFieldName(line)) return SecureReadResult::IO_ERROR;
    const char* value = skipHeaderWhitespace(separator + 1);
    if (equalsIgnoreCase(line, "Content-Length")) {
      size_t parsedLength = 0;
      if (!parseContentLength(value, parsedLength)) {
        return SecureReadResult::TOO_LARGE;
      }
      if (headers.contentLength >= 0 &&
          static_cast<size_t>(headers.contentLength) != parsedLength) {
        return SecureReadResult::IO_ERROR;
      }
      headers.contentLength = static_cast<int64_t>(parsedLength);
    } else if (equalsIgnoreCase(line, "Transfer-Encoding")) {
      if (headers.transferEncodingSeen) return SecureReadResult::IO_ERROR;
      headers.transferEncodingSeen = true;
      if (!transferEncodingIsChunkedOnly(value)) {
        return SecureReadResult::IO_ERROR;
      }
      headers.chunked = true;
    }
  }

  if (headers.transferEncodingSeen && !headers.chunked) {
    return SecureReadResult::IO_ERROR;
  }
  if (headers.chunked && headers.contentLength >= 0) {
    return SecureReadResult::IO_ERROR;
  }
  return SecureReadResult::OK;
}

SecureReadResult readSecureRaw(WiFiClientSecure& client, uint8_t* destination,
                               size_t length, size_t& bytesRead,
                               uint32_t fetchStarted,
                               uint32_t responseStarted,
                               uint32_t& lastProgress) {
  while (bytesRead < length) {
    const SecureReadResult waitResult =
        waitForSecureData(client, fetchStarted, responseStarted,
                          lastProgress, 0, 0);
    if (waitResult != SecureReadResult::OK) return waitResult;

    const size_t availableBytes = static_cast<size_t>(client.available());
    const size_t remaining = length - bytesRead;
    const size_t toRead = min(
        min(remaining, availableBytes), static_cast<size_t>(4096));
    if (toRead == 0) continue;
    const int readCount = client.read(destination + bytesRead, toRead);
    if (readCount <= 0) return SecureReadResult::IO_ERROR;
    bytesRead += static_cast<size_t>(readCount);
    lastProgress = millis();
  }
  return SecureReadResult::OK;
}

SecureReadResult parseChunkSize(const char* line, size_t& chunkSize) {
  if (!line) return SecureReadResult::IO_ERROR;
  const char* value = line;
  bool sawDigit = false;
  size_t parsed = 0;
  while (*value) {
    uint8_t digit = 0;
    if (*value >= '0' && *value <= '9') {
      digit = static_cast<uint8_t>(*value - '0');
    } else if (*value >= 'a' && *value <= 'f') {
      digit = static_cast<uint8_t>(*value - 'a' + 10);
    } else if (*value >= 'A' && *value <= 'F') {
      digit = static_cast<uint8_t>(*value - 'A' + 10);
    } else {
      break;
    }
    sawDigit = true;
    if (parsed > (MAX_RESPONSE_BYTES - digit) / 16) {
      return SecureReadResult::TOO_LARGE;
    }
    parsed = parsed * 16 + digit;
    ++value;
  }
  if (!sawDigit) return SecureReadResult::IO_ERROR;
  value = skipHeaderWhitespace(value);
  if (*value != 0 && *value != ';') return SecureReadResult::IO_ERROR;
  chunkSize = parsed;
  return SecureReadResult::OK;
}

bool validChunkTrailer(const char* line) {
  if (!line || !line[0]) return false;
  const char* separator = strchr(line, ':');
  if (!separator || separator == line) return false;

  char name[MAX_HTTP_LINE_BYTES]{};
  const size_t nameLength = static_cast<size_t>(separator - line);
  if (nameLength >= sizeof(name)) return false;
  memcpy(name, line, nameLength);
  name[nameLength] = 0;
  if (!validHeaderFieldName(name)) return false;
  return !equalsIgnoreCase(name, "Content-Length") &&
         !equalsIgnoreCase(name, "Transfer-Encoding");
}

SecureReadResult readChunkedBody(WiFiClientSecure& client, uint8_t* payload,
                                 size_t& received, uint32_t fetchStarted,
                                 uint32_t responseStarted,
                                 uint32_t& lastProgress) {
  size_t framingBytes = 0;
  char line[MAX_HTTP_LINE_BYTES]{};
  while (true) {
    SecureReadResult readResult = readSecureLine(
        client, line, sizeof(line), framingBytes, MAX_CHUNK_FRAMING_BYTES,
        fetchStarted, responseStarted, lastProgress, 0, 0);
    if (readResult != SecureReadResult::OK) return readResult;
    size_t chunkSize = 0;
    readResult = parseChunkSize(line, chunkSize);
    if (readResult != SecureReadResult::OK) return readResult;
    if (chunkSize == 0) {
      while (true) {
        readResult = readSecureLine(
            client, line, sizeof(line), framingBytes,
            MAX_CHUNK_FRAMING_BYTES, fetchStarted, responseStarted,
            lastProgress, 0, 0);
        if (readResult != SecureReadResult::OK) return readResult;
        if (line[0] == 0) return SecureReadResult::OK;
        if (!validChunkTrailer(line)) return SecureReadResult::IO_ERROR;
      }
    }
    if (chunkSize > MAX_RESPONSE_BYTES - received) {
      return SecureReadResult::TOO_LARGE;
    }

    size_t chunkRead = 0;
    readResult = readSecureRaw(client, payload + received, chunkSize, chunkRead,
                               fetchStarted, responseStarted, lastProgress);
    received += chunkRead;
    if (readResult != SecureReadResult::OK) return readResult;

    uint8_t terminator[2]{};
    size_t terminatorRead = 0;
    readResult = readSecureRaw(client, terminator, sizeof(terminator),
                               terminatorRead, fetchStarted, responseStarted,
                               lastProgress);
    if (readResult != SecureReadResult::OK) return readResult;
    framingBytes += sizeof(terminator);
    if (framingBytes > MAX_CHUNK_FRAMING_BYTES) {
      return SecureReadResult::TOO_LARGE;
    }
    if (terminator[0] != '\r' || terminator[1] != '\n') {
      return SecureReadResult::IO_ERROR;
    }
  }
}

SecureReadResult readCloseDelimitedBody(WiFiClientSecure& client,
                                        uint8_t* payload, size_t& received,
                                        uint32_t fetchStarted,
                                        uint32_t responseStarted,
                                        uint32_t& lastProgress) {
  while (true) {
    const SecureReadResult waitResult =
        waitForSecureData(client, fetchStarted, responseStarted,
                          lastProgress, 0, 0);
    if (waitResult == SecureReadResult::CLOSED) return SecureReadResult::OK;
    if (waitResult != SecureReadResult::OK) return waitResult;
    if (received >= MAX_RESPONSE_BYTES) return SecureReadResult::TOO_LARGE;

    const size_t availableBytes = static_cast<size_t>(client.available());
    const size_t remaining = MAX_RESPONSE_BYTES - received;
    const size_t toRead = min(
        min(remaining, availableBytes), static_cast<size_t>(4096));
    if (toRead == 0) continue;
    const int readCount = client.read(payload + received, toRead);
    if (readCount <= 0) return SecureReadResult::IO_ERROR;
    received += static_cast<size_t>(readCount);
    lastProgress = millis();
  }
}

bool writeSecureText(WiFiClientSecure& client, const char* text,
                     size_t length, uint32_t fetchStarted,
                     uint32_t writeStarted,
                     uint32_t& lastProgress) {
  if (!text) return false;
  size_t written = 0;
  while (written < length) {
    if (abortRequested() || transportRemainingMs(fetchStarted) == 0 ||
        !client.connected()) {
      return false;
    }
    const uint32_t now = millis();
    if (elapsedAtLeast(now, writeStarted, policy::HEADER_TIMEOUT_MS) ||
        elapsedAtLeast(now, lastProgress, policy::BODY_READ_TIMEOUT_MS)) {
      return false;
    }
    const size_t writeCount = client.write(
        reinterpret_cast<const uint8_t*>(text + written), length - written);
    if (writeCount > 0) {
      written += writeCount;
      lastProgress = millis();
      continue;
    }
    delay(2);
  }
  return true;
}

template <size_t N>
bool writeSecureLiteral(WiFiClientSecure& client, const char (&text)[N],
                        uint32_t fetchStarted, uint32_t writeStarted,
                        uint32_t& lastProgress) {
  return writeSecureText(client, text, N - 1, fetchStarted, writeStarted,
                         lastProgress);
}

bool writeFallbackRequest(WiFiClientSecure& client, const char* path,
                          uint32_t fetchStarted) {
  const uint32_t writeStarted = millis();
  uint32_t lastProgress = writeStarted;
  return writeSecureLiteral(client, "GET ", fetchStarted, writeStarted,
                            lastProgress) &&
         writeSecureText(client, path, strlen(path), fetchStarted,
                         writeStarted, lastProgress) &&
         writeSecureLiteral(client, " HTTP/1.1\r\n", fetchStarted,
                            writeStarted, lastProgress) &&
         writeSecureLiteral(client, "Host: opendata.adsb.fi\r\n",
                            fetchStarted, writeStarted, lastProgress) &&
         writeSecureLiteral(
             client, "User-Agent: BILLS-Aircraft-Radar-7in/18\r\n",
             fetchStarted, writeStarted, lastProgress) &&
         writeSecureLiteral(client, "Accept: application/json\r\n",
                            fetchStarted, writeStarted, lastProgress) &&
         writeSecureLiteral(client, "Accept-Encoding: identity\r\n",
                            fetchStarted, writeStarted, lastProgress) &&
         writeSecureLiteral(client, "Connection: close\r\n\r\n",
                            fetchStarted, writeStarted, lastProgress);
}

AttemptResult fetchAttemptWithSecureClient(const char* path,
                                           JsonDocument& filter,
                                           JsonDocument& doc,
                                           uint32_t fetchStarted) {
  if (abortRequested()) return cancelled();
  uint32_t remainingBudget = transportRemainingMs(fetchStarted);
  if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
    return failed(app_state::FetchFailureStage::TLS, 0, false, false, true);
  }

  ADSB_VERBOSE_PRINTLN(
      "ADSB.fi fallback HTTPS via verified WiFiClientSecure stream");
  BundleVerifiedSecureClient client;
  client.useDefaultCaBundle();
  const uint32_t connectTimeout = policy::boundedTimeoutMs(
      remainingBudget, policy::CONNECT_TIMEOUT_MS);
  client.setTimeout(policy::boundedTimeoutMs(
      remainingBudget, policy::BODY_READ_TIMEOUT_MS));
  client.setHandshakeTimeout((connectTimeout + 999) / 1000);

  const uint32_t connectStarted = millis();
  logMemoryStage("fallback-start");
  if (!client.connect("opendata.adsb.fi", 443,
                      static_cast<int32_t>(connectTimeout))) {
    char tlsError[160]{};
    const int tlsErrorCode = client.lastError(tlsError, sizeof(tlsError));
    releaseFallbackClient(client, fetchStarted);
    if (abortRequested()) return cancelled();
    const bool budgetExhausted = transportRemainingMs(fetchStarted) == 0;
    Serial.printf(
        "ADSB.fi fallback verified TLS connect failed after %lu ms: %d (%s)%s\n",
        (unsigned long)(millis() - connectStarted), tlsErrorCode,
        tlsError[0] ? tlsError : "no mbedTLS detail",
        budgetExhausted ? " [shared budget exhausted]" : "");
    return failed(app_state::FetchFailureStage::TLS, 0, false, false,
                  budgetExhausted);
  }

  if (abortRequested()) {
    releaseFallbackClient(client, fetchStarted);
    return cancelled();
  }
  logMemoryStage("fallback-tls");
  remainingBudget = transportRemainingMs(fetchStarted);
  if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
    releaseFallbackClient(client, fetchStarted);
    return failed(app_state::FetchFailureStage::TCP, 0, false, false, true);
  }
  client.setTimeout(policy::boundedTimeoutMs(
      remainingBudget, policy::BODY_READ_TIMEOUT_MS));
  if (!writeFallbackRequest(client, path, fetchStarted)) {
    releaseFallbackClient(client, fetchStarted);
    if (abortRequested()) return cancelled();
    const bool budgetExhausted = transportRemainingMs(fetchStarted) == 0;
    Serial.printf("ADSB.fi fallback request write failed%s\n",
                  budgetExhausted ? " [shared budget exhausted]" : "");
    return failed(app_state::FetchFailureStage::TCP, 0, false, false,
                  budgetExhausted);
  }

  const uint32_t responseStarted = millis();
  uint32_t lastProgress = responseStarted;
  FallbackResponseHeaders headers;
  const SecureReadResult headerResult = readFallbackHeaders(
      client, headers, fetchStarted, responseStarted, lastProgress);
  if (headerResult != SecureReadResult::OK) {
    releaseFallbackClient(client, fetchStarted);
    if (headerResult == SecureReadResult::CANCELLED || abortRequested()) {
      return cancelled();
    }
    const bool budgetExhausted =
        headerResult == SecureReadResult::BUDGET_EXHAUSTED ||
        transportRemainingMs(fetchStarted) == 0;
    Serial.printf("ADSB.fi fallback header failure: %s%s\n",
                  secureReadResultName(headerResult),
                  budgetExhausted ? " [shared budget exhausted]" : "");
    return failed(app_state::FetchFailureStage::HTTP_HEADERS, 0, false,
                  false, budgetExhausted);
  }

  if (headers.statusCode != 200) {
    releaseFallbackClient(client, fetchStarted);
    Serial.printf("ADSB.fi fallback HTTP error: %d\n", headers.statusCode);
    return failed(app_state::FetchFailureStage::HTTP_STATUS);
  }
  logMemoryStage("fallback-headers");

  const size_t capacity =
      headers.contentLength >= 0
          ? static_cast<size_t>(headers.contentLength)
          : MAX_RESPONSE_BYTES;
  uint8_t* payload = allocatePayload(capacity);
  if (!payload) {
    releaseFallbackClient(client, fetchStarted);
    Serial.printf("ADSB.fi fallback payload allocation failed: need %u bytes\n",
                  (unsigned)(capacity + 1));
    return failed(app_state::FetchFailureStage::RESPONSE_BODY,
                  static_cast<uint32_t>(capacity));
  }
  logMemoryStage("fallback-payload");

  size_t received = 0;
  SecureReadResult bodyResult = SecureReadResult::OK;
  if (headers.chunked) {
    bodyResult = readChunkedBody(client, payload, received, fetchStarted,
                                 responseStarted, lastProgress);
  } else if (headers.contentLength >= 0) {
    size_t bodyRead = 0;
    bodyResult = readSecureRaw(client, payload, capacity, bodyRead,
                               fetchStarted, responseStarted, lastProgress);
    received = bodyRead;
  } else {
    bodyResult = readCloseDelimitedBody(client, payload, received,
                                        fetchStarted, responseStarted,
                                        lastProgress);
  }
  releaseFallbackClient(client, fetchStarted);
  logMemoryStage("fallback-release");

  if (bodyResult == SecureReadResult::CANCELLED || abortRequested()) {
    free(payload);
    return cancelled(static_cast<uint32_t>(received));
  }
  if (bodyResult != SecureReadResult::OK) {
    const bool budgetExhausted =
        bodyResult == SecureReadResult::BUDGET_EXHAUSTED ||
        transportRemainingMs(fetchStarted) == 0;
    Serial.printf(
        "ADSB.fi fallback body failure: %s after %u bytes in %lu ms%s\n",
        secureReadResultName(bodyResult), (unsigned)received,
        (unsigned long)(millis() - responseStarted),
        budgetExhausted ? " [shared budget exhausted]" : "");
    free(payload);
    const app_state::FetchFailureStage stage =
        WiFi.status() == WL_CONNECTED
            ? app_state::FetchFailureStage::RESPONSE_BODY
            : app_state::FetchFailureStage::WIFI;
    return failed(stage, received, false, false, budgetExhausted);
  }

  payload[received] = 0;
  ADSB_VERBOSE_PRINTF(
      "ADSB.fi fallback response complete: %u bytes in %lu ms, chunked=%s\n",
      (unsigned)received, (unsigned long)(millis() - responseStarted),
      headers.chunked ? "yes" : "no");

  if (abortRequested()) {
    free(payload);
    return cancelled(static_cast<uint32_t>(received));
  }
  doc.clear();
  const uint32_t deserializeStartedUs = micros();
  DeserializationError error = deserializeJson(
      doc, payload, received, DeserializationOption::Filter(filter));
  const uint32_t deserializeUs = micros() - deserializeStartedUs;
  free(payload);
  logMemoryStage("fallback-json-deserialized");
  if (abortRequested()) return cancelled(static_cast<uint32_t>(received));
  if (error) {
    Serial.printf("ADSB.fi fallback JSON error: %s\n", error.c_str());
    return failed(app_state::FetchFailureStage::JSON, received);
  }

  JsonArray parsed = doc["ac"].as<JsonArray>();
  if (parsed.isNull()) {
    Serial.println("ADSB.fi fallback response did not contain an ac array");
    return failed(app_state::FetchFailureStage::JSON, received);
  }

  AttemptResult result;
  result.success = true;
  result.responseBytes = received;
  result.jsonDeserializeUs = deserializeUs;
  return result;
}

// Native attempts are completed first for connection and header failures. A
// partial response-body transport failure returns immediately so the existing
// network recovery ladder can recycle WiFi before another TLS connection. The
// independent verified fallback is invoked at most once when enough of the
// same shared transport budget remains.

AttemptResult fetchAttempt(const char* path, JsonDocument& filter,
                           JsonDocument& doc, uint8_t attempt,
                           uint32_t fetchStarted) {
  if (abortRequested()) return cancelled();
  uint32_t remainingBudget = transportRemainingMs(fetchStarted);
  if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
    return failed(app_state::FetchFailureStage::TLS, 0, false, false, true);
  }

  ADSB_VERBOSE_PRINTF("ADSB.fi native HTTPS attempt %u\n", attempt);
  logMemoryStage("native-start");
  ADSB_VERBOSE_PRINTF("ADSB RSSI: %d dBm\n", WiFi.RSSI());

  IPAddress serverIp;
  if (!WiFi.hostByName("opendata.adsb.fi", serverIp)) {
    if (abortRequested()) return cancelled();
    const bool budgetExhausted = transportRemainingMs(fetchStarted) == 0;
    Serial.printf("ADSB DNS failed: opendata.adsb.fi%s\n",
                  budgetExhausted ? " [shared budget exhausted]" : "");
    return failed(app_state::FetchFailureStage::DNS, 0, false, false,
                  budgetExhausted);
  }
  if (abortRequested()) return cancelled();
  if (transportRemainingMs(fetchStarted) == 0) {
    return failed(app_state::FetchFailureStage::DNS, 0, false, false, true);
  }
  ADSB_VERBOSE_PRINTF("ADSB DNS attempt %u: opendata.adsb.fi -> %s\n",
                      attempt, serverIp.toString().c_str());
  logMemoryStage("after-dns");

  char url[128];
  const int urlLength =
      snprintf(url, sizeof(url), "https://opendata.adsb.fi%s", path);
  if (urlLength < 0 || static_cast<size_t>(urlLength) >= sizeof(url)) {
    Serial.println("ADSB.fi request URL exceeded fixed buffer");
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }

  remainingBudget = transportRemainingMs(fetchStarted);
  if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
    return failed(app_state::FetchFailureStage::TLS, 0, false, false, true);
  }
  const uint32_t connectTimeout = policy::boundedTimeoutMs(
      remainingBudget, policy::CONNECT_TIMEOUT_MS);
  esp_http_client_config_t config{};
  config.url = url;
  config.user_agent = "BILLS-Aircraft-Radar-7in/18";
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = connectTimeout;
  config.disable_auto_redirect = false;
  config.max_redirection_count = 3;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = 4096;
  config.buffer_size_tx = 1024;
  config.keep_alive_enable = false;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.skip_cert_common_name_check = false;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    Serial.println("ADSB.fi native HTTPS client allocation failed");
    return failed(app_state::FetchFailureStage::TLS);
  }
  logMemoryStage("client-init");

  esp_http_client_set_header(client, "Accept", "application/json");
  esp_http_client_set_header(client, "Accept-Encoding", "identity");
  esp_http_client_set_header(client, "Connection", "close");

  const uint32_t connectStarted = millis();
  logMemoryStage("tls-handshake");
  const esp_err_t openError = esp_http_client_open(client, 0);
  if (openError != ESP_OK) {
    const int socketError = esp_http_client_get_errno(client);
    if (abortRequested()) {
      releaseNativeClient(client, false, fetchStarted);
      return cancelled();
    }
    const app_state::FetchFailureStage stage =
        WiFi.status() == WL_CONNECTED
            ? diagnoseSecureConnectFailure(serverIp, fetchStarted)
            : app_state::FetchFailureStage::WIFI;
    releaseNativeClient(client, false, fetchStarted);
    const bool budgetExhausted = transportRemainingMs(fetchStarted) == 0;
    Serial.printf(
        "ADSB.fi native %s connect failed after %lu ms: %s (0x%x), errno=%d%s\n",
        app_state::failureStageName(stage),
        (unsigned long)(millis() - connectStarted),
        esp_err_to_name(openError), (unsigned)openError, socketError,
        budgetExhausted ? " [shared budget exhausted]" : "");
    const bool fallbackEligible =
        stage == app_state::FetchFailureStage::TCP ||
        stage == app_state::FetchFailureStage::TLS;
    return failed(stage, 0, fallbackEligible, false, budgetExhausted);
  }
  ADSB_VERBOSE_PRINTF("ADSB.fi native TLS connected in %lu ms\n",
                      (unsigned long)(millis() - connectStarted));
  logMemoryStage("tls-connected");
  const bool clientOpened = true;

  if (abortRequested()) {
    releaseNativeClient(client, clientOpened, fetchStarted);
    return cancelled();
  }
  remainingBudget = transportRemainingMs(fetchStarted);
  if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
    releaseNativeClient(client, clientOpened, fetchStarted);
    return failed(app_state::FetchFailureStage::HTTP_HEADERS, 0, true, false,
                  true);
  }
  esp_http_client_set_timeout_ms(
      client, policy::boundedTimeoutMs(remainingBudget,
                                       policy::HEADER_TIMEOUT_MS));
  const int64_t headerLength = esp_http_client_fetch_headers(client);
  if (headerLength < 0) {
    if (abortRequested()) {
      releaseNativeClient(client, clientOpened, fetchStarted);
      return cancelled();
    }
    const app_state::FetchFailureStage stage =
        WiFi.status() == WL_CONNECTED
            ? app_state::FetchFailureStage::HTTP_HEADERS
            : app_state::FetchFailureStage::WIFI;
    releaseNativeClient(client, clientOpened, fetchStarted);
    const bool budgetExhausted = transportRemainingMs(fetchStarted) == 0;
    Serial.printf("ADSB.fi native header failure: %lld%s\n",
                  static_cast<long long>(headerLength),
                  budgetExhausted ? " [shared budget exhausted]" : "");
    return failed(stage, 0,
                  stage == app_state::FetchFailureStage::HTTP_HEADERS,
                  false, budgetExhausted);
  }

  const int statusCode = esp_http_client_get_status_code(client);
  if (statusCode != 200) {
    Serial.printf("ADSB.fi HTTP error: %d\n", statusCode);
    releaseNativeClient(client, clientOpened, fetchStarted);
    return failed(app_state::FetchFailureStage::HTTP_STATUS);
  }

  const bool chunked = esp_http_client_is_chunked_response(client);
  const bool lengthKnown = headerLength > 0;
  if (lengthKnown && static_cast<uint64_t>(headerLength) > MAX_RESPONSE_BYTES) {
    Serial.printf("ADSB.fi response too large: %lld bytes\n",
                  static_cast<long long>(headerLength));
    releaseNativeClient(client, clientOpened, fetchStarted);
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }
  const size_t capacity =
      lengthKnown ? static_cast<size_t>(headerLength) : MAX_RESPONSE_BYTES;
  ADSB_VERBOSE_PRINTF(
      "ADSB.fi HTTP 200, content length: %lld, chunked: %s\n",
      static_cast<long long>(headerLength), chunked ? "yes" : "no");
  logMemoryStage("headers-complete");

  uint8_t* payload = allocatePayload(capacity);
  if (!payload) {
    Serial.printf("ADSB.fi payload allocation failed: need %u bytes\n",
                  (unsigned)(capacity + 1));
    releaseNativeClient(client, clientOpened, fetchStarted);
    return failed(app_state::FetchFailureStage::RESPONSE_BODY,
                  static_cast<uint32_t>(capacity));
  }
  logMemoryStage("payload-ready");

  size_t received = 0;
  const uint32_t readStarted = millis();
  uint32_t lastProgress = readStarted;
  bool readFailed = false;
  bool endedEarly = false;
  bool cancelledDuringRead = false;
  bool budgetExhausted = false;
  while (received < capacity) {
    if (abortRequested()) {
      cancelledDuringRead = true;
      break;
    }
    const uint32_t now = millis();
    remainingBudget = transportRemainingMs(fetchStarted);
    if (remainingBudget < policy::MIN_BLOCKING_CALL_BUDGET_MS) {
      budgetExhausted = true;
      break;
    }
    if (now - lastProgress >= policy::IDLE_TIMEOUT_MS) break;

    esp_http_client_set_timeout_ms(
        client, policy::boundedTimeoutMs(remainingBudget,
                                         policy::BODY_READ_TIMEOUT_MS));
    const size_t remaining = capacity - received;
    const int toRead = static_cast<int>(
        min(remaining, static_cast<size_t>(4096)));
    const int bytesRead = esp_http_client_read(
        client, reinterpret_cast<char*>(payload + received), toRead);
    if (abortRequested()) {
      cancelledDuringRead = true;
      break;
    }
    if (bytesRead > 0) {
      received += static_cast<size_t>(bytesRead);
      lastProgress = millis();
    } else if (bytesRead == 0) {
      if (esp_http_client_is_complete_data_received(client)) break;
      Serial.printf(
          "ADSB.fi body ended early: received %u of %u bytes\n",
          (unsigned)received, (unsigned)capacity);
      endedEarly = true;
      break;
    } else {
      const int readErrno = esp_http_client_get_errno(client);
      const bool retryable =
          bytesRead == -ESP_ERR_HTTP_EAGAIN || readErrno == EAGAIN ||
          readErrno == EWOULDBLOCK || readErrno == ETIMEDOUT;
      if (retryable && WiFi.status() == WL_CONNECTED) {
        const uint32_t stalledForMs = millis() - lastProgress;
        Serial.printf(
            "ADSB.fi native body timeout: received %u of %u bytes, "
            "no progress for %lu ms, read=%d, errno=%d\n",
            (unsigned)received, (unsigned)capacity,
            (unsigned long)stalledForMs, bytesRead, readErrno);
        readFailed = true;
        break;
      }
      Serial.printf("ADSB.fi native body read failed: %d, errno=%d\n",
                    bytesRead, readErrno);
      readFailed = true;
      break;
    }
  }
  payload[received] = 0;
  const bool responseComplete =
      esp_http_client_is_complete_data_received(client);
  releaseNativeClient(client, clientOpened, fetchStarted);
  logMemoryStage("transport-released");

  if (cancelledDuringRead || abortRequested()) {
    free(payload);
    return cancelled(static_cast<uint32_t>(received));
  }
  budgetExhausted = budgetExhausted ||
                    transportRemainingMs(fetchStarted) == 0;
  const bool lengthMismatch = lengthKnown && received != capacity;
  if (readFailed || endedEarly || lengthMismatch || !responseComplete) {
    Serial.printf(
        "ADSB.fi incomplete response: received %u of %u bytes, complete=%s%s\n",
        (unsigned)received, (unsigned)capacity,
        responseComplete ? "yes" : "no",
        budgetExhausted ? " [shared budget exhausted]" : "");
    free(payload);
    const app_state::FetchFailureStage stage =
        WiFi.status() == WL_CONNECTED
            ? app_state::FetchFailureStage::RESPONSE_BODY
            : app_state::FetchFailureStage::WIFI;
    const bool bodyTransportFailure =
        stage == app_state::FetchFailureStage::RESPONSE_BODY;
    return failed(stage, received, false, bodyTransportFailure,
                  budgetExhausted);
  }
  ADSB_VERBOSE_PRINTF("ADSB.fi response complete: %u bytes in %lu ms\n",
                      (unsigned)received,
                      (unsigned long)(millis() - readStarted));

  if (abortRequested()) {
    free(payload);
    return cancelled(static_cast<uint32_t>(received));
  }
  doc.clear();
  const uint32_t deserializeStartedUs = micros();
  DeserializationError error = deserializeJson(
      doc, payload, received, DeserializationOption::Filter(filter));
  const uint32_t deserializeUs = micros() - deserializeStartedUs;
  free(payload);
  logMemoryStage("json-deserialized");
  if (abortRequested()) return cancelled(static_cast<uint32_t>(received));
  if (error) {
    Serial.printf("ADSB.fi JSON error: %s\n", error.c_str());
    return failed(app_state::FetchFailureStage::JSON, received);
  }
  JsonArray parsed = doc["ac"].as<JsonArray>();
  if (parsed.isNull()) {
    Serial.println("ADSB.fi response did not contain an ac array");
    return failed(app_state::FetchFailureStage::JSON, received);
  }
  AttemptResult result;
  result.success = true;
  result.responseBytes = received;
  result.jsonDeserializeUs = deserializeUs;
  if (attempt > 1) ADSB_VERBOSE_PRINTLN("ADSB.fi retry succeeded");
  return result;
}

bool containsHex(const aircraft::Target* targets, uint8_t count,
                 const char* hex) {
  if (!targets || !hex || !hex[0]) return false;
  for (uint8_t i = 0; i < count; ++i) {
    if (strcmp(targets[i].hex, hex) == 0) return true;
  }
  return false;
}

void insertSorted(aircraft::Target* out, int insertAt,
                  const aircraft::Target& target) {
  while (insertAt > 0 &&
         target.distanceMiles < out[insertAt - 1].distanceMiles) {
    out[insertAt] = out[insertAt - 1];
    --insertAt;
  }
  out[insertAt] = target;
}

void parseAircraft(JsonDocument& doc, float requestedRangeMiles,
                   double homeLatitude, double homeLongitude,
                   aircraft::Target* out, uint8_t& outCount,
                   uint16_t& receivedCount, uint16_t& eligibleCount,
                   uint16_t& capacityDroppedCount,
                   uint16_t& extractionYieldCount) {
  outCount = 0;
  eligibleCount = 0;
  capacityDroppedCount = 0;
  extractionYieldCount = 0;
  JsonArray aircraftJson = doc["ac"].as<JsonArray>();
  receivedCount = aircraftJson.size();
  ADSB_VERBOSE_PRINTF(
      "ADSB JSON parsed: %u aircraft received, overflow=%s\n",
      (unsigned)receivedCount, doc.overflowed() ? "YES" : "no");

  char trackedHex[7]{};
  const bool trackingActive =
      app_state::copyTrackedHex(trackedHex, sizeof(trackedHex));
  aircraft::Target trackedCandidate;
  bool trackedCandidateFound = false;
  bool forcedTrackedRetention = false;

  uint16_t missingPosition = 0;
  uint16_t onGround = 0;
  uint16_t outsideRange = 0;
  uint16_t processedSinceYield = 0;
  for (JsonObject plane : aircraftJson) {
    if (++processedSinceYield >=
        adsb_diagnostics::EXTRACTION_YIELD_INTERVAL) {
      processedSinceYield = 0;
      ++extractionYieldCount;
      // One bounded scheduler tick every 16 records reduces PSRAM/cache-bus
      // contention without publishing a partial aircraft snapshot.
      delay(1);
    }
    if (plane["lat"].isNull() || plane["lon"].isNull()) {
      ++missingPosition;
      continue;
    }
    JsonVariant altitude = plane["alt_baro"];
    if (altitude.is<const char*>() &&
        strcmp(altitude.as<const char*>(), "ground") == 0) {
      ++onGround;
      continue;
    }
    double latitude = plane["lat"].as<double>();
    double longitude = plane["lon"].as<double>();
    float distance = (float)aircraft::haversineMiles(
        homeLatitude, homeLongitude, latitude, longitude);
    if (distance > requestedRangeMiles) {
      ++outsideRange;
      continue;
    }
    ++eligibleCount;

    aircraft::Target target;
    const char* flight = plane["flight"] | "";
    const char* hex = plane["hex"] | "";
    const char* registration = plane["r"] | "";
    strncpy(target.id, flight, sizeof(target.id) - 1);
    size_t identifierLength = strlen(target.id);
    while (identifierLength > 0 && target.id[identifierLength - 1] == ' ') {
      target.id[--identifierLength] = 0;
    }
    if (!target.id[0]) {
      strncpy(target.id, registration[0] ? registration : hex,
              sizeof(target.id) - 1);
    }
    if (!target.id[0]) strcpy(target.id, "UNKNOWN");
    strncpy(target.hex, hex, sizeof(target.hex) - 1);
    const char* typeCode = plane["t"] | "";
    strncpy(target.typeCode, typeCode[0] ? typeCode : "Unknown",
            sizeof(target.typeCode) - 1);
    const char* operatorName = plane["ownOp"] | "";
    const char* description = plane["desc"] | "";
    strncpy(target.registration, registration[0] ? registration : "Unknown",
            sizeof(target.registration) - 1);
    strncpy(target.operatorName, operatorName[0] ? operatorName : "Unknown",
            sizeof(target.operatorName) - 1);
    strncpy(target.description, description[0] ? description : "Unknown",
            sizeof(target.description) - 1);
    target.distanceMiles = distance;
    target.bearing = (float)aircraft::bearingDegrees(
        homeLatitude, homeLongitude, latitude, longitude);
    if (altitude.is<float>() || altitude.is<int>() || altitude.is<long>()) {
      target.altitudeFt = altitude.as<float>();
    } else if (!plane["alt_geom"].isNull()) {
      target.altitudeFt = plane["alt_geom"].as<float>();
    }
    target.speedKt = plane["gs"] | 0.0f;
    target.hasTrack = !plane["track"].isNull();
    target.track = plane["track"] | 0.0f;
    target.verticalRateFpm = plane["baro_rate"] | 0.0f;
    target.valid = true;

    if (trackingActive && target.hex[0] &&
        strcmp(target.hex, trackedHex) == 0) {
      trackedCandidate = target;
      trackedCandidateFound = true;
    }

    int insertAt = -1;
    if (outCount < aircraft::MAX_TARGETS) {
      insertAt = outCount++;
    } else if (target.distanceMiles < out[outCount - 1].distanceMiles) {
      insertAt = outCount - 1;
    }
    if (insertAt >= 0) insertSorted(out, insertAt, target);
  }

  if (trackedCandidateFound && outCount > 0 &&
      !containsHex(out, outCount, trackedHex)) {
    insertSorted(out, outCount - 1, trackedCandidate);
    forcedTrackedRetention = true;
  }
  capacityDroppedCount = eligibleCount > outCount
                             ? eligibleCount - outCount
                             : 0;
  ADSB_VERBOSE_PRINTF(
      "ADSB accepted=%u eligible=%u capacity-dropped=%u "
      "missing-position=%u ground=%u outside-range=%u tracked-forced=%s\n",
      (unsigned)outCount, (unsigned)eligibleCount,
      (unsigned)capacityDroppedCount, (unsigned)missingPosition,
      (unsigned)onGround, (unsigned)outsideRange,
      forcedTrackedRetention ? "yes" : "no");
}

}  // namespace

Result fetchAircraft(aircraft::Target* out) {
  Result result;
  verboseDiagnosticUs = 0;
  const uint32_t fetchStarted = millis();
  result.requestGeneration = app_state::rangeGeneration();
  result.requestedRangeMiles = app_state::radarRangeMiles();
  const double homeLatitude = settings::homeLatitude();
  const double homeLongitude = settings::homeLongitude();

  if (abortRequested()) {
    result.cancelled = true;
    result.durationMs = millis() - fetchStarted;
    return result;
  }

  const wl_status_t wifiStatus = app_state::wifiStatus();
  if (wifiStatus != WL_CONNECTED) {
    Serial.printf("ADSB skipped: WiFi status=%s (%d)\n",
                  adsb::wifiStatusName(wifiStatus), wifiStatus);
    result.failureStage = app_state::FetchFailureStage::WIFI;
    result.durationMs = millis() - fetchStarted;
    return result;
  }

  float radiusNm = min(250.0f, (result.requestedRangeMiles / 1.15078f) + 5.0f);
  // Fixed buffer avoids temporary String allocations on the 15 s hot path.
  char path[96];
  const int pathLength = snprintf(
      path, sizeof(path), "/api/v3/lat/%.5f/lon/%.5f/dist/%.1f",
      homeLatitude, homeLongitude, radiusNm);
  if (pathLength < 0 || static_cast<size_t>(pathLength) >= sizeof(path)) {
    Serial.println("ADSB request path exceeded fixed buffer");
    result.failureStage = app_state::FetchFailureStage::HTTP_HEADERS;
    result.durationMs = millis() - fetchStarted;
    return result;
  }
  ADSB_VERBOSE_PRINTF(
      "ADSB request: https://opendata.adsb.fi%s [generation %lu]\n",
      path, (unsigned long)result.requestGeneration);
  JsonDocument filter(&psramJsonAllocator);
  JsonObject ac = filter["ac"].add<JsonObject>();
  ac["lat"] = true; ac["lon"] = true; ac["flight"] = true; ac["hex"] = true;
  ac["alt_baro"] = true; ac["alt_geom"] = true;
  ac["gs"] = true; ac["track"] = true; ac["t"] = true;
  ac["baro_rate"] = true; ac["r"] = true; ac["ownOp"] = true;
  ac["desc"] = true;
  if (filter.overflowed()) {
    Serial.println("ADSB JSON filter PSRAM allocation failed");
    result.failureStage = app_state::FetchFailureStage::JSON;
    result.durationMs = millis() - fetchStarted;
    return result;
  }

  // Both ArduinoJson documents are PSRAM-only. The response document is the
  // significant allocation; applying the same policy to the small filter
  // removes the last default-heap allocation from the fetch parse path.
  JsonDocument doc(&psramJsonAllocator);
  AttemptResult attemptResult;
  AttemptResult preservedFailure;
  bool fallbackAttempted = false;
  uint8_t nativeAttemptsUsed = 0;
  for (uint8_t attempt = 1; attempt <= MAX_NATIVE_ATTEMPTS; ++attempt) {
    nativeAttemptsUsed = attempt;
    attemptResult = fetchAttempt(path, filter, doc, attempt, fetchStarted);
    if (attemptResult.success || attemptResult.cancelled) break;
    preserveMostAdvancedFailure(preservedFailure, attemptResult);
    if (attemptResult.requiresWifiRecovery) {
      Serial.println(
          "ADSB.fi response-body transport failure requires WiFi recovery; "
          "skipping in-association retry and fallback");
      break;
    }
    if (attempt >= MAX_NATIVE_ATTEMPTS) break;

    const uint32_t remainingBudget = transportRemainingMs(fetchStarted);
    if (!policy::canStartNativeRetry(remainingBudget)) {
      attemptResult.transportBudgetExhausted = remainingBudget == 0;
      preservedFailure.transportBudgetExhausted =
          preservedFailure.transportBudgetExhausted ||
          attemptResult.transportBudgetExhausted;
      Serial.printf(
          "ADSB.fi native retry skipped: %lu ms shared budget remains\n",
          (unsigned long)remainingBudget);
      break;
    }
    const BoundedWaitResult retryWait =
        waitInterruptible(policy::RETRY_DELAY_MS, fetchStarted);
    if (retryWait == BoundedWaitResult::CANCELLED) {
      attemptResult = cancelled();
      break;
    }
    if (retryWait == BoundedWaitResult::BUDGET_EXHAUSTED) {
      attemptResult.transportBudgetExhausted = true;
      preservedFailure.transportBudgetExhausted = true;
      break;
    }
  }

  if (!attemptResult.success && !attemptResult.cancelled &&
      attemptResult.fallbackEligible && WiFi.status() == WL_CONNECTED) {
    uint32_t remainingBudget = transportRemainingMs(fetchStarted);
    if (policy::canStartFallback(remainingBudget)) {
      Serial.printf(
          "ADSB.fi native retries ended at %s; waiting %lu ms before "
          "one verified fallback attempt\n",
          app_state::failureStageName(attemptResult.failureStage),
          (unsigned long)policy::FALLBACK_START_DELAY_MS);
      const BoundedWaitResult fallbackWait = waitInterruptible(
          policy::FALLBACK_START_DELAY_MS, fetchStarted);
      if (fallbackWait == BoundedWaitResult::CANCELLED) {
        attemptResult = cancelled();
      } else if (fallbackWait == BoundedWaitResult::BUDGET_EXHAUSTED) {
        attemptResult.transportBudgetExhausted = true;
        preservedFailure.transportBudgetExhausted = true;
      } else {
        remainingBudget = transportRemainingMs(fetchStarted);
        if (policy::canStartFallback(remainingBudget)) {
          fallbackAttempted = true;
          const AttemptResult fallbackResult =
              fetchAttemptWithSecureClient(path, filter, doc, fetchStarted);
          if (fallbackResult.success) {
            attemptResult = fallbackResult;
            Serial.println("ADSB.fi verified fallback succeeded");
          } else {
            preserveMostAdvancedFailure(preservedFailure, fallbackResult);
            attemptResult = fallbackResult;
          }
        }
      }
    } else {
      Serial.printf(
          "ADSB.fi verified fallback skipped: %lu ms shared budget remains\n",
          (unsigned long)remainingBudget);
      if (remainingBudget == 0) {
        attemptResult.transportBudgetExhausted = true;
        preservedFailure.transportBudgetExhausted = true;
      }
    }
  }

  if (attemptResult.cancelled || abortRequested()) {
    result.cancelled = true;
    result.responseBytes = attemptResult.responseBytes;
    result.durationMs = millis() - fetchStarted;
    result.verboseDiagnosticUs = verboseDiagnosticUs;
    return result;
  }

  result.responseBytes = attemptResult.success
                             ? attemptResult.responseBytes
                             : preservedFailure.responseBytes;
  result.transportBudgetExhausted =
      attemptResult.transportBudgetExhausted ||
      preservedFailure.transportBudgetExhausted;
  if (!attemptResult.success) {
    result.failureStage =
        preservedFailure.failureStage != app_state::FetchFailureStage::NONE
            ? preservedFailure.failureStage
            : attemptResult.failureStage;
    result.durationMs = millis() - fetchStarted;
    if (result.failureStage != attemptResult.failureStage) {
      Serial.printf(
          "ADSB.fi preserving most advanced failure stage: %s "
          "(final transport ended at %s)\n",
          app_state::failureStageName(result.failureStage),
          app_state::failureStageName(attemptResult.failureStage));
    }
    Serial.printf(
        "ADSB.fi request failed after %u native attempts%s at %s stage%s\n",
        nativeAttemptsUsed,
        fallbackAttempted ? " and one fallback" : "",
        app_state::failureStageName(result.failureStage),
        result.transportBudgetExhausted
            ? " [shared transport budget exhausted]" : "");
    result.verboseDiagnosticUs = verboseDiagnosticUs;
    return result;
  }

  result.jsonDeserializeUs = attemptResult.jsonDeserializeUs;
  if (abortRequested()) {
    result.cancelled = true;
    result.durationMs = millis() - fetchStarted;
    result.verboseDiagnosticUs = verboseDiagnosticUs;
    return result;
  }
  logMemoryStage("json-extract");
  const uint32_t extractStartedUs = micros();
  parseAircraft(doc, result.requestedRangeMiles, homeLatitude, homeLongitude,
                out, result.acceptedCount, result.receivedCount,
                result.eligibleCount, result.capacityDroppedCount,
                result.extractionYieldCount);
  result.jsonExtractUs = micros() - extractStartedUs;
  logMemoryStage("json-complete");
  result.verboseDiagnosticUs = verboseDiagnosticUs;
  result.success = true;
  result.failureStage = app_state::FetchFailureStage::NONE;
  result.durationMs = millis() - fetchStarted;
  return result;
}

}  // namespace adsb_fetch
