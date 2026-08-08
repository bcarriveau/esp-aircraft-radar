#pragma once

#include <stdint.h>

// Authoritative implementation baseline: committed Product 81 on current main,
// preserving the physically verified Product 73 remote GitHub OTA install path.

constexpr uint32_t FIRMWARE_VERSION_CODE = 82;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 82";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Fixes Airspace live-highlight handoff so a tracked aircraft can be stopped and the tapped aircraft selected on Radar by stable ICAO.";

constexpr const char* BUILD_ID =
    "7IN-20260807-PRODUCT82-AIRSPACE-HANDOFF";
