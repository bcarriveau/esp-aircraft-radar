#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace adsb {
// Owns Wi-Fi lifecycle, fetch scheduling, and successful publication.

constexpr uint32_t FETCH_INTERVAL_MS = 15000;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;

void begin();
void service();
void reconnectOrRefresh();
void requestRefresh();
void requestWifiReconnect();

const char* wifiStatusName(wl_status_t status);

}  // namespace adsb
