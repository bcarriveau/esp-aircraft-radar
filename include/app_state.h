#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "aircraft_data.h"

namespace app_state {
// Thread-safe ownership boundary for live application data.

enum class FetchFailureStage : uint8_t {
  NONE,
  WIFI,
  DNS,
  TCP,
  TLS,
  HTTP_STATUS,
  HTTP_HEADERS,
  RESPONSE_BODY,
  JSON,
  STALE_RESULT
};

struct Snapshot {
  uint8_t count = 0;
  uint32_t targetVersion = 0;
  uint32_t rangeGeneration = 0;
  uint32_t trackingVersion = 0;
  uint32_t lastUpdateMs = 0;
  float rangeMiles = 0;
  bool manualTracking = false;
  char trackedHex[7]{};
};

struct Diagnostics {
  uint32_t lastAttemptMs = 0;
  uint32_t lastSuccessMs = 0;
  uint32_t lastDurationMs = 0;
  uint32_t lastResponseBytes = 0;
  uint32_t totalAttempts = 0;
  uint32_t networkRecoveries = 0;
  uint32_t discardedResponses = 0;
  uint32_t minimumFreeHeap = 0;
  uint32_t minimumFreePsram = 0;
  uint16_t lastReceivedCount = 0;
  uint8_t lastAcceptedCount = 0;
  uint16_t consecutiveFailures = 0;
  FetchFailureStage lastFailureStage = FetchFailureStage::NONE;
};

void initialize();

void publishTargets(const aircraft::Target* targets, uint8_t count,
                    uint32_t updatedAtMs);
void copySnapshot(aircraft::Target* out, Snapshot& snapshot);
void copyVisibleTargets(aircraft::Target* out, uint8_t& count);
uint8_t targetCount();
uint32_t targetVersion();

void setWifiStatus(wl_status_t status);
wl_status_t wifiStatus();
void setLastDisconnectReason(int reason);
int lastDisconnectReason();

float radarRangeMiles();
bool setRadarRangeMiles(float rangeMiles);
uint32_t rangeGeneration();
void invalidateRequests();

void selectManualTracking(const aircraft::Target& target);
void clearManualTracking();
bool hasManualTracking();
bool isManuallyTracked(const aircraft::Target& target);
bool isManuallyTracked(const aircraft::Target& target,
                       const Snapshot& snapshot);

void setFetchInProgress(bool inProgress);
bool fetchInProgress();
uint32_t lastUpdateMs();

void beginFetch();
void recordFetchSuccess(uint32_t durationMs, uint32_t responseBytes,
                        uint16_t receivedCount, uint8_t acceptedCount);
void recordFetchFailure(FetchFailureStage stage, uint32_t durationMs,
                        uint32_t responseBytes = 0);
void recordDiscardedResponse(uint32_t durationMs, uint32_t responseBytes,
                             uint16_t receivedCount,
                             uint8_t acceptedCount);
void recordNetworkRecovery();
void observeMemory();
void copyDiagnostics(Diagnostics& diagnostics);
const char* failureStageName(FetchFailureStage stage);

}  // namespace app_state
