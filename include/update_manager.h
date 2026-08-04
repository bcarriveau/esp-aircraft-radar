#pragma once

#include <stdint.h>

namespace update_manager {

constexpr uint32_t STARTUP_DELAY_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t CHECK_INTERVAL_SECONDS = 24UL * 60UL * 60UL;
constexpr uint32_t MINIMUM_ADSB_SLACK_MS = 8000UL;

enum class CheckResult : uint8_t {
  NEVER = 0,
  QUEUED,
  CHECKING,
  CURRENT,
  UPDATE_AVAILABLE,
  NO_RELEASE,
  ABORTED,
  FAILED
};

struct Status {
  bool initialized = false;
  bool checking = false;
  bool updateAvailable = false;
  bool manualQueued = false;
  uint32_t statusVersion = 0;
  uint32_t remoteVersionCode = 0;
  uint32_t lastAttemptEpoch = 0;
  uint32_t lastSuccessEpoch = 0;
  CheckResult lastResult = CheckResult::NEVER;
  char remoteVersionLabel[32]{};
  char remoteBuildId[96]{};
  char notes[192]{};
  char message[96]{};
};

static_assert(sizeof(Status) <= 440,
              "Update status must remain a small fixed internal-DRAM object");

// Initializes the bounded persistent update cache. An older notifier cache is
// ignored by schema so it cannot suppress Product 70's first manual test.
bool begin();

// Queues one user-requested check. It bypasses only the five-minute startup and
// 24-hour automatic timers. The check still waits for a successful current ADS-B
// cycle, sufficient cadence slack, Wi-Fi, MQTT, and OTA serialization.
bool requestManualCheck();

// Called by the existing Core-0 ADS-B owner while its established MQTT exclusion
// is still active. It claims either a queued manual check or one due automatic
// check without creating another task.
bool prepareAfterSuccessfulAdsb(uint32_t nextAdsbPollAtMs);

// Runs the claimed check on Core 0 after ADS-B publication and diagnostics.
void performPreparedCheck();

// Range/reconnect/OTA commands use this to make an active check yield at the next
// bounded transport boundary. A queued manual request is not silently discarded.
void requestAbort();

// Main-loop MQTT gate only. The radar UI is never marked as ADS-B updating.
bool networkCheckInProgress();

void copyStatus(Status& status);
const char* checkResultName(CheckResult result);

}  // namespace update_manager
