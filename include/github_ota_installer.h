#pragma once

#include <stddef.h>
#include <stdint.h>

namespace github_ota_installer {

struct Release {
  char tag[64]{};
  char asset[128]{};
  char buildId[96]{};
  uint32_t packageSize = 0;
  uint32_t firmwareSize = 0;
  uint8_t packageSha256[32]{};
  uint8_t firmwareSha256[32]{};
};

enum class Result : uint8_t {
  FAILED = 0,
  CANCELLED,
  RESTART_PENDING
};

enum class RestartState : uint8_t {
  IDLE = 0,
  PENDING,
  FAILED
};

using CancelCallback = bool (*)();
using ProgressCallback = void (*)(uint32_t receivedBytes,
                                  uint32_t packageBytes);

// Downloads the exact validated GitHub release asset, verifies the complete
// .radarota package and firmware payload while streaming, writes only the
// inactive OTA partition, and selects it only after every check succeeds.
Result install(const Release& release, CancelCallback cancelRequested,
               ProgressCallback progress, char* message,
               size_t messageCapacity);

// Runs from the normal Core-1 loop. It mirrors the existing hardened local-OTA
// restart handoff: network resources settle, a high-priority Core-0 task is
// created from internal RAM, and Core 1 parks in IRAM before esp_restart().
void serviceRestart();
RestartState restartState();
void copyRestartMessage(char* destination, size_t capacity);

}  // namespace github_ota_installer
