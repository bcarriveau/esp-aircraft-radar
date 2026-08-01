#pragma once

#include <stdint.h>

namespace ota_update {

constexpr uint32_t ENABLE_WINDOW_MS = 5UL * 60UL * 1000UL;

enum class State : uint8_t {
  UNAVAILABLE = 0,
  DISABLED,
  ARMED,
  PREPARING,
  READY,
  UPLOADING,
  ERROR,
  SUCCESS
};

struct Status {
  State state = State::UNAVAILABLE;
  bool available = false;
  bool serverRunning = false;
  bool maintenanceActive = false;
  uint32_t secondsRemaining = 0;
  uint32_t firmwareBytes = 0;
  uint32_t writtenBytes = 0;
  uint8_t progressPercent = 0;
  char accessCode[7]{};
  char ipAddress[64]{};
  char mdnsAddress[64]{};
  char message[128]{};
};

// Initializes the local OTA service without opening a listening socket.
// Failure is nonfatal to normal radar operation.
bool begin();
void service();

bool enable();
void disable();
void copyStatus(Status& status);

const char* stateName(State state);
bool busy();

}  // namespace ota_update
