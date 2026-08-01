#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace adsb {
// Owns Wi-Fi lifecycle, fetch scheduling, and successful publication.

constexpr uint32_t FETCH_INTERVAL_MS = 15000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;

bool begin();
void service();
void reconnectOrRefresh();
void requestRefresh();
void requestWifiReconnect();

// Coordinates exclusive flash-update maintenance with the core-0 network task.
// A requested hold is acknowledged only after any active ADS-B request finishes.
void requestMaintenanceHold();
bool maintenanceHoldActive();
void releaseMaintenanceHold();

const char* wifiStatusName(wl_status_t status);

}  // namespace adsb
