#pragma once

// Product 94 factory/distribution build defaults.
//
// This header is selected only by the dedicated PlatformIO factory environment.
// It intentionally contains no owner-specific credentials or location data.
// Normal development builds continue to use the private include/config.h.

#define WIFI_SSID ""
#define WIFI_PASS ""

#define HOME_LAT 0.0
#define HOME_LON 0.0

#define RADAR_RANGE_MILES 80.0f

#define MQTT_ENABLED_DEFAULT 0
#define MQTT_BROKER_URI ""
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
