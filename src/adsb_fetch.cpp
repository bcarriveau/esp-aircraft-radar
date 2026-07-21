#include "adsb_fetch.h"

#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "adsb_network.h"
#include "config.h"
#include "settings.h"

namespace adsb_fetch {
namespace {

constexpr uint32_t SOCKET_TIMEOUT_MS = 10000;
constexpr uint32_t TCP_PROBE_TIMEOUT_MS = 3000;
constexpr uint32_t IDLE_TIMEOUT_MS = 15000;
constexpr uint32_t TOTAL_TIMEOUT_MS = 90000;
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

bool readSecureHttpLine(WiFiClientSecure& client, String& line,
                        uint32_t timeoutMs) {
  line = "";
  const uint32_t started = millis();
  uint32_t lastProgress = started;
  while (millis() - started < timeoutMs &&
         millis() - lastProgress < timeoutMs) {
    int availableBytes = client.available();
    if (availableBytes <= 0) {
      delay(2);
      continue;
    }
    int value = client.read();
    if (value < 0) {
      delay(2);
      continue;
    }
    lastProgress = millis();
    char ch = static_cast<char>(value);
    if (ch == '\n') {
      if (line.endsWith("\r")) line.remove(line.length() - 1);
      return true;
    }
    if (line.length() >= 511) return false;
    line += ch;
  }
  return false;
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

AttemptResult openRequest(WiFiClientSecure& client, const String& path,
                          const IPAddress& serverIp, uint8_t attempt) {
  Serial.printf("ADSB.fi request attempt %u\n", attempt);
  Serial.printf("Heap before request: %u, free PSRAM: %u\n",
                ESP.getFreeHeap(), ESP.getFreePsram());

  client.setInsecure();
  client.setTimeout(SOCKET_TIMEOUT_MS);
  client.setHandshakeTimeout((SOCKET_TIMEOUT_MS + 999) / 1000);

  const uint32_t connectStarted = millis();
  if (!client.connect("opendata.adsb.fi", 443,
                      static_cast<int32_t>(SOCKET_TIMEOUT_MS))) {
    const app_state::FetchFailureStage stage =
        diagnoseSecureConnectFailure(serverIp);
    Serial.printf("ADSB.fi %s connect failed after %lu ms\n",
                  app_state::failureStageName(stage),
                  (unsigned long)(millis() - connectStarted));
    client.stop();
    return failed(stage);
  }
  Serial.printf("ADSB.fi TLS connected in %lu ms\n",
                (unsigned long)(millis() - connectStarted));

  String request = "GET " + path + " HTTP/1.0\r\n"
                   "Host: opendata.adsb.fi\r\n"
                   "Accept: application/json\r\n"
                   "Accept-Encoding: identity\r\n"
                   "User-Agent: BILLS-Aircraft-Radar-7in/15\r\n"
                   "Connection: close\r\n\r\n";
  size_t requestBytes = client.print(request);
  if (requestBytes != request.length()) {
    Serial.printf("ADSB.fi request write failed: %u of %u bytes\n",
                  (unsigned)requestBytes, (unsigned)request.length());
    client.stop();
    return failed(app_state::FetchFailureStage::TCP);
  }
  AttemptResult result;
  result.success = true;
  return result;
}

AttemptResult readResponseHeaders(WiFiClientSecure& client,
                                  int& expectedLength) {
  String headerLine;
  if (!readSecureHttpLine(client, headerLine, SOCKET_TIMEOUT_MS)) {
    Serial.println("ADSB.fi status-line timeout");
    client.stop();
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }
  int firstSpace = headerLine.indexOf(' ');
  int code = firstSpace >= 0 ? headerLine.substring(firstSpace + 1).toInt() : 0;
  expectedLength = -1;
  bool headersComplete = false;
  while (readSecureHttpLine(client, headerLine, SOCKET_TIMEOUT_MS)) {
    if (headerLine.length() == 0) {
      headersComplete = true;
      break;
    }
    String lowerHeader = headerLine;
    lowerHeader.toLowerCase();
    if (lowerHeader.startsWith("content-length:")) {
      expectedLength = headerLine.substring(15).toInt();
    }
  }
  if (!headersComplete) {
    Serial.println("ADSB.fi header timeout");
    client.stop();
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }
  if (code != 200) {
    Serial.printf("ADSB.fi HTTP error: %d\n", code);
    client.stop();
    return failed(app_state::FetchFailureStage::HTTP_STATUS);
  }
  Serial.printf("ADSB.fi HTTP 200, content length: %d\n", expectedLength);
  if (expectedLength <= 0 || expectedLength > 250000) {
    Serial.printf("ADSB.fi invalid content length: %d\n", expectedLength);
    client.stop();
    return failed(app_state::FetchFailureStage::HTTP_HEADERS);
  }
  AttemptResult result;
  result.success = true;
  result.responseBytes = expectedLength;
  return result;
}

AttemptResult readResponseDocument(WiFiClientSecure& client,
                                   int expectedLength, JsonDocument& filter,
                                   JsonDocument& doc) {
  uint8_t* payload = static_cast<uint8_t*>(heap_caps_malloc(
      static_cast<size_t>(expectedLength) + 1,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!payload) {
    payload = static_cast<uint8_t*>(
        malloc(static_cast<size_t>(expectedLength) + 1));
  }
  if (!payload) {
    Serial.printf("ADSB.fi payload allocation failed: need %d bytes\n",
                  expectedLength + 1);
    client.stop();
    return failed(app_state::FetchFailureStage::RESPONSE_BODY,
                  expectedLength);
  }

  size_t received = 0;
  const uint32_t readStarted = millis();
  uint32_t lastProgress = readStarted;
  while (received < static_cast<size_t>(expectedLength)) {
    uint32_t now = millis();
    if (now - lastProgress >= IDLE_TIMEOUT_MS ||
        now - readStarted >= TOTAL_TIMEOUT_MS) {
      break;
    }
    int availableBytes = client.available();
    if (availableBytes > 0) {
      size_t remaining = static_cast<size_t>(expectedLength) - received;
      size_t toRead = min(static_cast<size_t>(availableBytes), remaining);
      toRead = min(toRead, static_cast<size_t>(4096));
      int bytesRead = client.read(payload + received, toRead);
      if (bytesRead > 0) {
        received += static_cast<size_t>(bytesRead);
        lastProgress = millis();
      } else {
        delay(2);
      }
    } else {
      delay(2);
    }
  }
  payload[received] = 0;
  client.stop();

  if (received != static_cast<size_t>(expectedLength)) {
    Serial.printf("ADSB.fi truncated response: received %u of %d bytes\n",
                  (unsigned)received, expectedLength);
    free(payload);
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
  return result;
}

AttemptResult fetchAttempt(const String& path, const IPAddress& serverIp,
                           JsonDocument& filter, JsonDocument& doc,
                           uint8_t attempt) {
  WiFiClientSecure client;
  AttemptResult result = openRequest(client, path, serverIp, attempt);
  if (!result.success) return result;
  int expectedLength = -1;
  result = readResponseHeaders(client, expectedLength);
  if (!result.success) return result;
  result = readResponseDocument(client, expectedLength, filter, doc);
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
  IPAddress adsbIp;
  if (!WiFi.hostByName("opendata.adsb.fi", adsbIp)) {
    Serial.println("ADSB DNS failed: opendata.adsb.fi");
    result.failureStage = app_state::FetchFailureStage::DNS;
    result.durationMs = millis() - fetchStarted;
    return result;
  }
  Serial.printf("ADSB DNS: opendata.adsb.fi -> %s\n",
                adsbIp.toString().c_str());

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
    attemptResult = fetchAttempt(path, adsbIp, filter, doc, attempt);
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
