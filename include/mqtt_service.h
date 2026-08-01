#pragma once

#include <stdint.h>

namespace mqtt_service {

enum class State : uint8_t {
  INACTIVE = 0,
  NOT_CONFIGURED,
  WAITING_FOR_WIFI,
  STARTING,
  CONNECTING,
  CONNECTED,
  STOPPING,
  MAINTENANCE,
  ERROR
};

struct Status {
  State state = State::INACTIVE;
  bool configured = false;
  bool enabled = false;
  bool clientRunning = false;
  bool connected = false;
  bool maintenanceActive = false;
  char deviceId[40]{};
  char message[128]{};
};

// Optional subsystem. Disabled mode creates no MQTT task and allocates no
// aircraft snapshot or JSON buffer.
bool begin();
void service();
bool setEnabled(bool enabled);
void requestDiscoveryRefresh();

void requestMaintenanceHold();
bool maintenanceHoldActive();
void releaseMaintenanceHold();

void copyStatus(Status& status);
const char* stateName(State state);

}  // namespace mqtt_service
