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

// True while the core-0 network task is quiescing dependent services or
// restarting the station radio. Core 1 must avoid OTA/Wi-Fi calls in this window.
bool wifiOperationInProgress();

// Read-only cancellation signal used by the serialized fetch owner. It
// becomes true when local OTA requests exclusive network maintenance.
bool fetchAbortRequested();

// Coordinates exclusive flash-update maintenance with the core-0 network task.
// Active transport checks the cancellation signal between bounded blocking
// calls, then acknowledges the hold without recording an ADS-B failure.
// Returns false only when a hard Wi-Fi recovery already owns the network.
bool requestMaintenanceHold();
bool maintenanceHoldActive();
void releaseMaintenanceHold();

const char* wifiStatusName(wl_status_t status);

}  // namespace adsb
