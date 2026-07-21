#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace adsb {

inline bool shouldScheduleWifiReconnect(wl_status_t wifiStatus,
                                        uint32_t nowMs,
                                        uint32_t lastAttemptMs,
                                        uint32_t retryIntervalMs) {
  return wifiStatus != WL_CONNECTED &&
         (nowMs - lastAttemptMs) >= retryIntervalMs;
}

}  // namespace adsb
