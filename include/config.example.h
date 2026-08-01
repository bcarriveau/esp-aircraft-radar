#pragma once

// Copy this file to include/config.h and edit it.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"

// Your radar center.
#define HOME_LAT 43.000000
#define HOME_LON -88.000000

// Visible radar radius in statute miles.
#define RADAR_RANGE_MILES 80.0f

// Optional Home Assistant MQTT integration.
// MQTT is disabled by default and allocates no MQTT task or aircraft buffer.
// Product 56 supports a local unencrypted mqtt:// broker only.
#define MQTT_ENABLED_DEFAULT 0
#define MQTT_BROKER_URI "mqtt://192.168.1.10:1883"
#define MQTT_USERNAME "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
