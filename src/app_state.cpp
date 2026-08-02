#include "app_state.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "config.h"

namespace app_state {
namespace {

constexpr float MIN_RADAR_RANGE_MILES = 20.0f;
constexpr float MAX_RADAR_RANGE_MILES = 80.0f;
constexpr uint8_t TRACKING_MISS_LIMIT = 3;

struct SharedState {
  aircraft::Target* targets = nullptr;
  uint8_t targetCount = 0;
  uint32_t targetVersion = 0;
  wl_status_t wifiStatus = WL_IDLE_STATUS;
  int lastDisconnectReason = 0;
  float radarRangeMiles = constrain((float)RADAR_RANGE_MILES,
                                    MIN_RADAR_RANGE_MILES,
                                    MAX_RADAR_RANGE_MILES);
  uint32_t rangeGeneration = 1;
  bool manualTracking = false;
  bool locationUpdatePending = false;
  char trackedHex[7]{};
  uint32_t trackingVersion = 0;
  uint8_t trackingMissCount = 0;
  bool fetchInProgress = false;
  uint32_t lastUpdateMs = 0;
  Diagnostics diagnostics;
};

SharedState state;
SemaphoreHandle_t stateMutex = nullptr;

bool lockState() {
  return stateMutex && xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE;
}

void unlockState(bool locked) {
  if (locked) xSemaphoreGive(stateMutex);
}

void copyMemoryStage(char* destination, size_t capacity,
                     const char* stage) {
  if (!destination || capacity == 0) return;
  const char* value = stage && stage[0] ? stage : "unlabelled";
  strncpy(destination, value, capacity - 1);
  destination[capacity - 1] = 0;
}

void observeMemoryLocked(const char* stage = nullptr) {
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t largestInternalBlock =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t freePsram = ESP.getFreePsram();

  if (state.diagnostics.minimumFreeHeap == 0 ||
      freeHeap < state.diagnostics.minimumFreeHeap) {
    state.diagnostics.minimumFreeHeap = freeHeap;
  }
  if (state.diagnostics.minimumLargestInternalBlock == 0 ||
      largestInternalBlock <
          state.diagnostics.minimumLargestInternalBlock) {
    state.diagnostics.minimumLargestInternalBlock = largestInternalBlock;
    copyMemoryStage(state.diagnostics.minimumBlockStage,
                    sizeof(state.diagnostics.minimumBlockStage), stage);
  }
  if (freePsram > 0 && (state.diagnostics.minimumFreePsram == 0 ||
                       freePsram < state.diagnostics.minimumFreePsram)) {
    state.diagnostics.minimumFreePsram = freePsram;
  }

  if (!state.fetchInProgress) return;

  if (state.diagnostics.lastFetchMinimumFreeHeap == 0 ||
      freeHeap < state.diagnostics.lastFetchMinimumFreeHeap) {
    state.diagnostics.lastFetchMinimumFreeHeap = freeHeap;
  }
  if (state.diagnostics.lastFetchMinimumLargestInternalBlock == 0 ||
      largestInternalBlock <
          state.diagnostics.lastFetchMinimumLargestInternalBlock) {
    state.diagnostics.lastFetchMinimumLargestInternalBlock =
        largestInternalBlock;
    copyMemoryStage(state.diagnostics.lastFetchMinimumBlockStage,
                    sizeof(state.diagnostics.lastFetchMinimumBlockStage),
                    stage);
  }
}

}  // namespace

void initialize() {
  if (!stateMutex) {
    stateMutex = xSemaphoreCreateMutex();
    if (!stateMutex) Serial.println("FATAL: App-state mutex allocation failed");
  }
  if (!state.targets) {
    state.targets = static_cast<aircraft::Target*>(heap_caps_calloc(
        aircraft::MAX_TARGETS, sizeof(aircraft::Target),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!state.targets) {
      Serial.println("FATAL: App-state target buffer PSRAM allocation failed");
    } else {
      Serial.printf("App-state target buffer in PSRAM: %u bytes\n",
                    (unsigned)(aircraft::MAX_TARGETS *
                               sizeof(aircraft::Target)));
    }
  }
  observeMemory();
}

bool targetStorageReady() { return stateMutex && state.targets; }

void publishTargets(const aircraft::Target* targets, uint8_t count,
                    uint32_t updatedAtMs) {
  if (!state.targets || !targets) return;
  if (count > aircraft::MAX_TARGETS) {
    count = static_cast<uint8_t>(aircraft::MAX_TARGETS);
  }
  char lostTrackedHex[7]{};
  bool trackingCleared = false;
  bool locked = lockState();
  if (state.manualTracking && state.trackedHex[0]) {
    bool trackedPresent = false;
    for (uint8_t i = 0; i < count; ++i) {
      if (targets[i].valid && targets[i].hex[0] &&
          strcmp(targets[i].hex, state.trackedHex) == 0) {
        trackedPresent = true;
        break;
      }
    }
    if (trackedPresent) {
      state.trackingMissCount = 0;
    } else {
      if (state.trackingMissCount < TRACKING_MISS_LIMIT) {
        ++state.trackingMissCount;
      }
      if (state.trackingMissCount >= TRACKING_MISS_LIMIT) {
        strncpy(lostTrackedHex, state.trackedHex,
                sizeof(lostTrackedHex) - 1);
        state.manualTracking = false;
        state.trackedHex[0] = 0;
        state.trackingMissCount = 0;
        ++state.trackingVersion;
        trackingCleared = true;
      }
    }
  } else {
    state.trackingMissCount = 0;
  }
  if (count > 0) {
    memcpy(state.targets, targets, sizeof(aircraft::Target) * count);
  }
  state.targetCount = count;
  ++state.targetVersion;
  state.lastUpdateMs = updatedAtMs;
  state.locationUpdatePending = false;
  unlockState(locked);
  if (trackingCleared) {
    Serial.printf(
        "Tracked aircraft %s absent from %u consecutive updates; "
        "tracking cleared\n",
        lostTrackedHex, (unsigned)TRACKING_MISS_LIMIT);
  }
}

void copySnapshot(aircraft::Target* out, Snapshot& snapshot) {
  snapshot = Snapshot{};
  if (!out || !state.targets) return;
  bool locked = lockState();
  snapshot.rangeMiles = state.radarRangeMiles;
  snapshot.targetVersion = state.targetVersion;
  snapshot.rangeGeneration = state.rangeGeneration;
  snapshot.trackingVersion = state.trackingVersion;
  snapshot.lastUpdateMs = state.lastUpdateMs;
  snapshot.locationUpdatePending = state.locationUpdatePending;
  snapshot.manualTracking =
      state.manualTracking && !state.locationUpdatePending;
  strncpy(snapshot.trackedHex, state.trackedHex,
          sizeof(snapshot.trackedHex) - 1);
  if (!state.locationUpdatePending) {
    for (uint8_t i = 0;
         i < state.targetCount && snapshot.count < aircraft::MAX_TARGETS; ++i) {
      if (state.targets[i].valid &&
          state.targets[i].distanceMiles <= snapshot.rangeMiles) {
        out[snapshot.count++] = state.targets[i];
      }
    }
  }
  unlockState(locked);
}

void copyVisibleTargets(aircraft::Target* out, uint8_t& count) {
  Snapshot snapshot;
  copySnapshot(out, snapshot);
  count = snapshot.count;
}

uint8_t targetCount() {
  bool locked = lockState();
  uint8_t count = state.targetCount;
  unlockState(locked);
  return count;
}

uint32_t targetVersion() {
  bool locked = lockState();
  uint32_t version = state.targetVersion;
  unlockState(locked);
  return version;
}

uint32_t trackingVersion() {
  bool locked = lockState();
  uint32_t version = state.trackingVersion;
  unlockState(locked);
  return version;
}

void setWifiStatus(wl_status_t status) {
  bool locked = lockState();
  state.wifiStatus = status;
  unlockState(locked);
}

wl_status_t wifiStatus() {
  bool locked = lockState();
  wl_status_t status = state.wifiStatus;
  unlockState(locked);
  return status;
}

void setLastDisconnectReason(int reason) {
  bool locked = lockState();
  state.lastDisconnectReason = reason;
  unlockState(locked);
}

int lastDisconnectReason() {
  bool locked = lockState();
  int reason = state.lastDisconnectReason;
  unlockState(locked);
  return reason;
}

float radarRangeMiles() {
  bool locked = lockState();
  float rangeMiles = state.radarRangeMiles;
  unlockState(locked);
  return rangeMiles;
}

bool setRadarRangeMiles(float rangeMiles) {
  rangeMiles = constrain(rangeMiles, MIN_RADAR_RANGE_MILES,
                         MAX_RADAR_RANGE_MILES);
  bool locked = lockState();
  bool changed = fabsf(state.radarRangeMiles - rangeMiles) >= 1.0f;
  if (changed) {
    state.radarRangeMiles = rangeMiles;
    ++state.rangeGeneration;
  }
  unlockState(locked);
  return changed;
}

uint32_t rangeGeneration() {
  bool locked = lockState();
  uint32_t generation = state.rangeGeneration;
  unlockState(locked);
  return generation;
}

void invalidateRequests() {
  bool locked = lockState();
  ++state.rangeGeneration;
  unlockState(locked);
}

void invalidateLocation() {
  bool locked = lockState();
  const uint8_t invalidatedCount = state.targetCount;
  const uint32_t invalidatedGeneration = ++state.rangeGeneration;
  state.targetCount = 0;
  ++state.targetVersion;
  state.lastUpdateMs = 0;
  state.locationUpdatePending = true;
  unlockState(locked);
  Serial.printf(
      "Radar location changed; invalidated %u published aircraft and "
      "queued generation %lu\n",
      (unsigned)invalidatedCount, (unsigned long)invalidatedGeneration);
}

void selectManualTracking(const aircraft::Target& target) {
  bool locked = lockState();
  state.manualTracking = true;
  state.trackingMissCount = 0;
  strncpy(state.trackedHex, target.hex, sizeof(state.trackedHex) - 1);
  state.trackedHex[sizeof(state.trackedHex) - 1] = 0;
  ++state.trackingVersion;
  unlockState(locked);
}

void clearManualTracking() {
  bool locked = lockState();
  if (state.manualTracking || state.trackedHex[0]) ++state.trackingVersion;
  state.manualTracking = false;
  state.trackedHex[0] = 0;
  state.trackingMissCount = 0;
  unlockState(locked);
}

bool hasManualTracking() {
  bool locked = lockState();
  bool active = state.manualTracking;
  unlockState(locked);
  return active;
}

bool copyTrackedHex(char* out, size_t outSize) {
  if (!out || outSize == 0) return false;
  out[0] = 0;
  bool locked = lockState();
  const bool active = state.manualTracking && state.trackedHex[0];
  if (active) {
    strncpy(out, state.trackedHex, outSize - 1);
    out[outSize - 1] = 0;
  }
  unlockState(locked);
  return active;
}

bool isManuallyTracked(const aircraft::Target& target) {
  bool locked = lockState();
  bool tracked = state.manualTracking && target.hex[0] &&
                 strcmp(target.hex, state.trackedHex) == 0;
  unlockState(locked);
  return tracked;
}

bool isManuallyTracked(const aircraft::Target& target,
                       const Snapshot& snapshot) {
  return snapshot.manualTracking && target.hex[0] &&
         strcmp(target.hex, snapshot.trackedHex) == 0;
}

void setFetchInProgress(bool inProgress) {
  bool locked = lockState();
  state.fetchInProgress = inProgress;
  unlockState(locked);
}

bool fetchInProgress() {
  bool locked = lockState();
  bool inProgress = state.fetchInProgress;
  unlockState(locked);
  return inProgress;
}

uint32_t lastUpdateMs() {
  bool locked = lockState();
  uint32_t updatedAtMs = state.lastUpdateMs;
  unlockState(locked);
  return updatedAtMs;
}

void beginFetch() {
  bool locked = lockState();
  state.fetchInProgress = true;
  state.diagnostics.lastAttemptMs = millis();
  ++state.diagnostics.totalAttempts;
  // Reset per-fetch lows so the System page can show both lifetime MIN and
  // the low-water mark of the most recent request.
  state.diagnostics.lastFetchMinimumFreeHeap = 0;
  state.diagnostics.lastFetchMinimumLargestInternalBlock = 0;
  state.diagnostics.lastFetchMinimumBlockStage[0] = 0;
  observeMemoryLocked("begin-fetch");
  unlockState(locked);
}

void recordFetchSuccess(uint32_t durationMs, uint32_t responseBytes,
                        uint16_t receivedCount, uint16_t eligibleCount,
                        uint8_t acceptedCount,
                        uint16_t capacityDroppedCount) {
  bool locked = lockState();
  observeMemoryLocked("fetch-complete");
  state.fetchInProgress = false;
  state.diagnostics.lastSuccessMs = millis();
  state.diagnostics.lastDurationMs = durationMs;
  state.diagnostics.lastResponseBytes = responseBytes;
  state.diagnostics.lastReceivedCount = receivedCount;
  state.diagnostics.lastEligibleCount = eligibleCount;
  state.diagnostics.lastAcceptedCount = acceptedCount;
  state.diagnostics.lastCapacityDroppedCount = capacityDroppedCount;
  state.diagnostics.consecutiveFailures = 0;
  state.diagnostics.lastFailureStage = FetchFailureStage::NONE;
  unlockState(locked);
}

void recordFetchFailure(FetchFailureStage stage, uint32_t durationMs,
                        uint32_t responseBytes) {
  bool locked = lockState();
  observeMemoryLocked("fetch-complete");
  state.fetchInProgress = false;
  state.diagnostics.lastDurationMs = durationMs;
  state.diagnostics.lastResponseBytes = responseBytes;
  ++state.diagnostics.consecutiveFailures;
  state.diagnostics.lastFailureStage = stage;
  unlockState(locked);
}

void recordDiscardedResponse(uint32_t durationMs, uint32_t responseBytes,
                             uint16_t receivedCount, uint16_t eligibleCount,
                             uint8_t acceptedCount,
                             uint16_t capacityDroppedCount) {
  bool locked = lockState();
  observeMemoryLocked("fetch-complete");
  state.fetchInProgress = false;
  ++state.diagnostics.discardedResponses;
  state.diagnostics.lastDurationMs = durationMs;
  state.diagnostics.lastResponseBytes = responseBytes;
  state.diagnostics.lastReceivedCount = receivedCount;
  state.diagnostics.lastEligibleCount = eligibleCount;
  state.diagnostics.lastAcceptedCount = acceptedCount;
  state.diagnostics.lastCapacityDroppedCount = capacityDroppedCount;
  state.diagnostics.consecutiveFailures = 0;
  state.diagnostics.lastFailureStage = FetchFailureStage::NONE;
  unlockState(locked);
}

void recordNetworkRecovery() {
  bool locked = lockState();
  ++state.diagnostics.networkRecoveries;
  unlockState(locked);
}

void recordAdsbTaskStackFreeBytes(uint32_t freeBytes) {
  if (freeBytes == 0) return;
  bool locked = lockState();
  if (state.diagnostics.minimumAdsbTaskStackFreeBytes == 0 ||
      freeBytes < state.diagnostics.minimumAdsbTaskStackFreeBytes) {
    state.diagnostics.minimumAdsbTaskStackFreeBytes = freeBytes;
  }
  unlockState(locked);
}

void observeMemory(const char* stage) {
  bool locked = lockState();
  observeMemoryLocked(stage);
  unlockState(locked);
}

void copyDiagnostics(Diagnostics& diagnostics) {
  bool locked = lockState();
  diagnostics = state.diagnostics;
  unlockState(locked);
}

const char* failureStageName(FetchFailureStage stage) {
  switch (stage) {
    case FetchFailureStage::NONE: return "none";
    case FetchFailureStage::WIFI: return "WiFi";
    case FetchFailureStage::DNS: return "DNS";
    case FetchFailureStage::TCP: return "TCP";
    case FetchFailureStage::TLS: return "TLS";
    case FetchFailureStage::HTTP_STATUS: return "HTTP status";
    case FetchFailureStage::HTTP_HEADERS: return "HTTP headers";
    case FetchFailureStage::RESPONSE_BODY: return "response body";
    case FetchFailureStage::JSON: return "JSON";
    case FetchFailureStage::STALE_RESULT: return "stale range";
    default: return "unknown";
  }
}

}  // namespace app_state
