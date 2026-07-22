#include "adsb_fetch.h"

#include <ArduinoJson.h>
#include <cerrno>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>

#include "adsb_network.h"
#include "config.h"
#include "settings.h"

namespace adsb_fetch {
namespace {

constexpr uint32_t HTTP_NETWORK_TIMEOUT_MS = 15000;
constexpr uint32_t BODY_READ_TIMEOUT_MS = 5000;
constexpr uint32_t TCP_PROBE_TIMEOUT_MS = 3000;
constexpr uint32_t IDLE_TIMEOUT_MS = 30000;
constexpr uint32_t TOTAL_TIMEOUT_MS = 90000;
constexpr size_t MAX_RESPONSE_BYTES = 250000;
constexpr uint8_t MAX_ATTEMPTS = 2;
constexpr uint32_t RETRY_DELAY_MS = 500;

struct AttemptResult {
  bool success = false;
  app_state::FetchFailureStage failureStage =
      app_state::FetchFailureStage::NONE;
  uint32_t responseBytes = 0;
};

void waitForRetry(uint8_t attempt) {
  if (attempt < MAX_ATTEMPTS) delay(RETRY_DELAY_MS);
}

AttemptResult failed(app_state::FetchFailureStage stage,
                     uint32_t responseBytes = 0) {
  AttemptResult result;
  result.failureStage = stage;
  result.responseBytes = responseBytes;
  return result;
}

app_state::FetchFailureStage diagnoseSecureConnectFailure(
    const IPAddress& serverIp) {
  // A short raw TCP probe only runs after a secure connect failure. This lets
  // diagnostics distinguish an unreachable host/port from a TLS handshake.
  WiFiClient probe;
  probe.setTimeout(TCP_PROBE_TIMEOUT_MS);
  const bool tcpConnected = probe.connect(
      serverIp, 443, static_cast<int32_t>(TCP_PROBE_TIMEOUT_MS));
  probe.stop();
  return tcpConnected ? app_state::FetchFailureStage::TLS
                      : app_state::FetchFailureStage::TCP;
}

uint8_t* allocatePayload(size_t capacity) {
  uint8_t* payload = static_cast<uint8_t*>(heap_caps_malloc(
      capacity + 1,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!payload) {
    payload = static_cast<uint8_t*>(malloc(capacity + 1));
  }
  return payload;
}

AttemptResult fetchAttemptWithSecureClient(const String& path,
                                             JsonDocument& filter,
                                             JsonDocument& doc) {
  Serial.println("ADSB.fi fallback HTTPS via WiFiClientSecure");
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(HTTP_NETWORK_TIMEOUT_MS / 1000);
  client.setHandshakeTimeout(HTTP_NETWORK_TIMEOUT_MS / 1000);

  if (!client.connect("opendata.adsb.fi", 443)) {
    Serial.println("ADSB.fi fallback TLS connect failed");
    return failed(app_state::FetchFailureStage::TLS);
  }

  client.print(String("GET ") + path + " HTTP/1.1\r\n");
  client.print("Host: opendata.adsb.fi\r\n");
  client.print("User-Agent: BILLS-Aircraft-Radar-7in/18\r\n");
  client.print("Accept: application/json\r\n");
  client.print("Accept-Encoding: identity\r\n");
  client.print("Connection: close\r\n\r\n");

  const uint32_t responseStarted = millis();
  while (!client.available() && millis() - responseStarted < HTTP_NETWORK_TIMEOUT_MS) {
    delay(2);
  }
  if (!client.available()) {
    client.stop();
    Serial.println("ADSB.fi fallback response timed out");
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }

  String response = client.readString();
  client.stop();

  const int headerEnd = response.indexOf("\r\n\r\n");
  if (headerEnd < 0) {
    Serial.println("ADSB.fi fallback response had no header/body separator");
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }

  const String headers = response.substring(0, headerEnd);
  const String body = response.substring(headerEnd + 4);
  if (body.length() > MAX_RESPONSE_BYTES) {
    Serial.printf("ADSB.fi fallback response too large: %u bytes\n",
                  (unsigned)body.length());
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }

  if (headers.indexOf("HTTP/1.1 200") < 0 && headers.indexOf("HTTP/1.0 200") < 0) {
    Serial.printf("ADSB.fi fallback HTTP error headers: %s\n",
                  headers.c_str());
    return failed(app_state::FetchFailureStage::HTTP_STATUS);
  }

  doc.clear();
  DeserializationError error = deserializeJson(
      doc, body.c_str(), body.length(), DeserializationOption::Filter(filter));
  if (error) {
    Serial.printf("ADSB.fi fallback JSON error: %s\n", error.c_str());
    return failed(app_state::FetchFailureStage::JSON, body.length());
  }

  JsonArray parsed = doc["ac"].as<JsonArray>();
  if (parsed.isNull()) {
    Serial.println("ADSB.fi fallback response did not contain an ac array");
    return failed(app_state::FetchFailureStage::JSON, body.length());
  }

  AttemptResult result;
  result.success = true;
  result.responseBytes = body.length();
  return result;
}

AttemptResult fetchAttempt(const String& path, JsonDocument& filter,
                           JsonDocument& doc, uint8_t attempt) {
  Serial.printf("ADSB.fi native HTTPS attempt %u\n", attempt);
  Serial.printf("Heap before request: %u, free PSRAM: %u, RSSI: %d dBm\n",
                ESP.getFreeHeap(), ESP.getFreePsram(), WiFi.RSSI());

  // Resolve for each attempt. A failed Cloudflare edge is never pinned across
  // both retries, and the address remains available for TCP-vs-TLS diagnosis.
  IPAddress serverIp;
  if (!WiFi.hostByName("opendata.adsb.fi", serverIp)) {
    Serial.println("ADSB DNS failed: opendata.adsb.fi");
    return failed(app_state::FetchFailureStage::DNS);
  }
  Serial.printf("ADSB DNS attempt %u: opendata.adsb.fi -> %s\n", attempt,
                serverIp.toString().c_str());

  String url = "https://opendata.adsb.fi" + path;
  esp_http_client_config_t config{};
  config.url = url.c_str();
  config.user_agent = "BILLS-Aircraft-Radar-7in/18";
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = HTTP_NETWORK_TIMEOUT_MS;
  config.disable_auto_redirect = false;
  config.max_redirection_count = 3;
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = 4096;
  config.buffer_size_tx = 1024;
  config.keep_alive_enable = false;
  // Native esp-tls requires an explicit server-verification method. Use the
  // full CA bundle already supplied by the pinned Arduino/ESP-IDF framework
  // and retain hostname verification for opendata.adsb.fi.
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.skip_cert_common_name_check = false;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    Serial.println("ADSB.fi native HTTPS client allocation failed");
    return failed(app_state::FetchFailureStage::TLS);
  }

  esp_http_client_set_header(client, "Accept", "application/json");
  esp_http_client_set_header(client, "Accept-Encoding", "identity");
  esp_http_client_set_header(client, "Connection", "close");

  const uint32_t connectStarted = millis();
  const esp_err_t openError = esp_http_client_open(client, 0);
  if (openError != ESP_OK) {
    const int socketError = esp_http_client_get_errno(client);
    const app_state::FetchFailureStage stage =
        WiFi.status() == WL_CONNECTED
            ? diagnoseSecureConnectFailure(serverIp)
            : app_state::FetchFailureStage::WIFI;
    Serial.printf(
        "ADSB.fi native %s connect failed after %lu ms: %s (0x%x), errno=%d\n",
        app_state::failureStageName(stage),
        (unsigned long)(millis() - connectStarted),
        esp_err_to_name(openError), (unsigned)openError, socketError);
    esp_http_client_cleanup(client);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(stage);
  }
  Serial.printf("ADSB.fi native TLS connected in %lu ms\n",
                (unsigned long)(millis() - connectStarted));

  const int64_t headerLength = esp_http_client_fetch_headers(client);
  if (headerLength < 0) {
    Serial.printf("ADSB.fi native header failure: %lld\n",
                  static_cast<long long>(headerLength));
    esp_http_client_cleanup(client);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }

  const int statusCode = esp_http_client_get_status_code(client);
  if (statusCode != 200) {
    Serial.printf("ADSB.fi HTTP error: %d\n", statusCode);
    esp_http_client_cleanup(client);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(app_state::FetchFailureStage::HTTP_STATUS);
  }

  const bool chunked = esp_http_client_is_chunked_response(client);
  const bool lengthKnown = headerLength > 0;
  if (lengthKnown && static_cast<uint64_t>(headerLength) > MAX_RESPONSE_BYTES) {
    Serial.printf("ADSB.fi response too large: %lld bytes\n",
                  static_cast<long long>(headerLength));
    esp_http_client_cleanup(client);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }
  const size_t capacity =
      lengthKnown ? static_cast<size_t>(headerLength) : MAX_RESPONSE_BYTES;
  Serial.printf("ADSB.fi HTTP 200, content length: %lld, chunked: %s\n",
                static_cast<long long>(headerLength), chunked ? "yes" : "no");

  uint8_t* payload = allocatePayload(capacity);
  if (!payload) {
    Serial.printf("ADSB.fi payload allocation failed: need %u bytes\n",
                  (unsigned)(capacity + 1));
    esp_http_client_cleanup(client);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(app_state::FetchFailureStage::RESPONSE_BODY,
                  static_cast<uint32_t>(capacity));
  }

  size_t received = 0;
  const uint32_t readStarted = millis();
  uint32_t lastProgress = readStarted;
  bool readFailed = false;
  bool waitLogged = false;
  esp_http_client_set_timeout_ms(client, BODY_READ_TIMEOUT_MS);
  while (received < capacity) {
    uint32_t now = millis();
    if (now - lastProgress >= IDLE_TIMEOUT_MS ||
        now - readStarted >= TOTAL_TIMEOUT_MS) {
      break;
    }
    const size_t remaining = capacity - received;
    const int toRead = static_cast<int>(min(remaining, static_cast<size_t>(4096)));
    const int bytesRead = esp_http_client_read(
        client, reinterpret_cast<char*>(payload + received), toRead);
    if (bytesRead > 0) {
      received += static_cast<size_t>(bytesRead);
      lastProgress = millis();
      waitLogged = false;
    } else if (bytesRead == 0) {
      if (esp_http_client_is_complete_data_received(client)) break;
      delay(2);
    } else {
      const int readErrno = esp_http_client_get_errno(client);
      const bool retryable =
          bytesRead == -ESP_ERR_HTTP_EAGAIN || readErrno == EAGAIN ||
          readErrno == EWOULDBLOCK || readErrno == ETIMEDOUT;
      if (retryable && WiFi.status() == WL_CONNECTED) {
        if (!waitLogged) {
          Serial.printf(
              "ADSB.fi body read waiting: received %u of %u bytes, errno=%d\n",
              (unsigned)received, (unsigned)capacity, readErrno);
          waitLogged = true;
        }
        delay(2);
        continue;
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
  esp_http_client_cleanup(client);

  const bool lengthMismatch = lengthKnown && received != capacity;
  if (readFailed || lengthMismatch || !responseComplete) {
    Serial.printf(
        "ADSB.fi incomplete response: received %u of %u bytes, complete=%s\n",
        (unsigned)received, (unsigned)capacity,
        responseComplete ? "yes" : "no");
    free(payload);
    const AttemptResult fallbackResult =
        fetchAttemptWithSecureClient(path, filter, doc);
    if (fallbackResult.success) {
      return fallbackResult;
    }
    return failed(app_state::FetchFailureStage::RESPONSE_BODY, received);
  }
  Serial.printf("ADSB.fi response complete: %u bytes in %lu ms\n",
                (unsigned)received,
                (unsigned long)(millis() - readStarted));

  doc.clear();
  DeserializationError error = deserializeJson(
      doc, payload, received, DeserializationOption::Filter(filter));
  free(payload);
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
  if (result.success && attempt > 1) Serial.println("ADSB.fi retry succeeded");
  return result;
}

void parseAircraft(JsonDocument& doc, float requestedRangeMiles,
                   double homeLatitude, double homeLongitude,
                   aircraft::Target* out, uint8_t& outCount,
                   uint16_t& receivedCount) {
  outCount = 0;
  JsonArray aircraftJson = doc["ac"].as<JsonArray>();
  receivedCount = aircraftJson.size();
  Serial.printf("ADSB JSON parsed: %u aircraft received, overflow=%s\n",
                (unsigned)receivedCount, doc.overflowed() ? "YES" : "no");

  uint16_t missingPosition = 0;
  uint16_t onGround = 0;
  uint16_t outsideRange = 0;
  for (JsonObject plane : aircraftJson) {
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

    int insertAt = -1;
    if (outCount < aircraft::MAX_TARGETS) {
      insertAt = outCount++;
    } else if (target.distanceMiles < out[outCount - 1].distanceMiles) {
      insertAt = outCount - 1;
    }
    if (insertAt >= 0) {
      while (insertAt > 0 &&
             target.distanceMiles < out[insertAt - 1].distanceMiles) {
        out[insertAt] = out[insertAt - 1];
        --insertAt;
      }
      out[insertAt] = target;
    }
  }
  Serial.printf("ADSB accepted=%u missing-position=%u ground=%u outside-range=%u\n",
                outCount, missingPosition, onGround, outsideRange);
}

}  // namespace

Result fetchAircraft(aircraft::Target* out) {
  Result result;
  const uint32_t fetchStarted = millis();
  result.requestGeneration = app_state::rangeGeneration();
  result.requestedRangeMiles = app_state::radarRangeMiles();
  const double homeLatitude = settings::homeLatitude();
  const double homeLongitude = settings::homeLongitude();

  const wl_status_t wifiStatus = app_state::wifiStatus();
  if (wifiStatus != WL_CONNECTED) {
    Serial.printf("ADSB skipped: WiFi status=%s (%d)\n",
                  adsb::wifiStatusName(wifiStatus), wifiStatus);
    result.failureStage = app_state::FetchFailureStage::WIFI;
    result.durationMs = millis() - fetchStarted;
    return result;
  }

  float radiusNm = min(250.0f, (result.requestedRangeMiles / 1.15078f) + 5.0f);
  String path = "/api/v3/lat/" + String(homeLatitude, 5) +
                "/lon/" + String(homeLongitude, 5) +
                "/dist/" + String(radiusNm, 1);
  Serial.printf("ADSB request: https://opendata.adsb.fi%s [generation %lu]\n",
                path.c_str(), (unsigned long)result.requestGeneration);
  JsonDocument filter;
  JsonObject ac = filter["ac"].add<JsonObject>();
  ac["lat"] = true; ac["lon"] = true; ac["flight"] = true; ac["hex"] = true;
  ac["alt_baro"] = true; ac["alt_geom"] = true;
  ac["gs"] = true; ac["track"] = true; ac["t"] = true;
  ac["baro_rate"] = true; ac["r"] = true; ac["ownOp"] = true;
  ac["desc"] = true;

  JsonDocument doc;
  AttemptResult attemptResult;
  for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
    attemptResult = fetchAttempt(path, filter, doc, attempt);
    if (attemptResult.success) break;
    waitForRetry(attempt);
  }
  result.responseBytes = attemptResult.responseBytes;
  if (!attemptResult.success) {
    result.failureStage = attemptResult.failureStage;
    result.durationMs = millis() - fetchStarted;
    Serial.printf("ADSB.fi request failed after %u attempts at %s stage\n",
                  MAX_ATTEMPTS,
                  app_state::failureStageName(result.failureStage));
    return result;
  }

  parseAircraft(doc, result.requestedRangeMiles, homeLatitude, homeLongitude,
                out, result.acceptedCount, result.receivedCount);
  result.success = true;
  result.failureStage = app_state::FetchFailureStage::NONE;
  result.durationMs = millis() - fetchStarted;
  return result;
}

}  // namespace adsb_fetch
