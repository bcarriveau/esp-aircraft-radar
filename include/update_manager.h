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

enum class InstallResult : uint8_t {
  IDLE = 0,
  QUEUED,
  VERIFYING,
  DOWNLOADING,
  RESTARTING,
  CANCELLED,
  FAILED,
  RESTART_FAILED
};

struct Status {
  bool initialized = false;
  bool checking = false;
  bool updateAvailable = false;
  bool manualQueued = false;
  bool installQueued = false;
  bool installing = false;
  uint8_t installProgressPercent = 0;
  InstallResult installResult = InstallResult::IDLE;
  uint32_t statusVersion = 0;
  uint32_t remoteVersionCode = 0;
  uint32_t remotePackageSize = 0;
  uint32_t remoteFirmwareSize = 0;
  uint32_t lastAttemptEpoch = 0;
  uint32_t lastSuccessEpoch = 0;
  uint32_t installReceivedBytes = 0;
  uint32_t installPackageBytes = 0;
  CheckResult lastResult = CheckResult::NEVER;
  uint8_t remotePackageSha256[32]{};
  uint8_t remoteFirmwareSha256[32]{};
  char remoteVersionLabel[32]{};
  char remoteBuildId[96]{};
  char notes[192]{};
  char message[128]{};
};

static_assert(sizeof(Status) <= 576,
              "Update status must remain a small fixed internal-DRAM object");

bool begin();

// Called from the normal Core-1 loop. It services only the bounded hardened
// restart handoff after a completely verified remote installation.
void service();

bool requestManualCheck();

// Requires a previously validated newer stable release. The release is checked
// again immediately before download so a changed release is never installed
// without another user confirmation.
bool requestInstall();
void requestInstallAbort();

// Called by the existing Core-0 ADS-B owner while its established MQTT
// exclusion is still active. A queued install has priority over a metadata
// check. Installs intentionally suspend the normal ADS-B cadence until they
// finish, fail, are cancelled, or reboot.
bool prepareAfterSuccessfulAdsb(uint32_t nextAdsbPollAtMs);
void performPreparedCheck();

// Range/reconnect/local-OTA commands use this to stop a disposable check or
// active remote installation at the next bounded transport boundary.
void requestAbort();

// Compatibility name retained for main.cpp. This is true for either a GitHub
// metadata check or a remote package installation, so MQTT remains serialized.
bool networkCheckInProgress();

void copyStatus(Status& status);
const char* checkResultName(CheckResult result);
const char* installResultName(InstallResult result);

}  // namespace update_manager
