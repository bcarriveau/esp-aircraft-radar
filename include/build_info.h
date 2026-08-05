#pragma once

#include <stdint.h>

// Authoritative implementation baseline: current main branch after Product 76,
// preserving the physically verified Product 73 remote GitHub OTA install path.

constexpr uint32_t FIRMWARE_VERSION_CODE = 77;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 77";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Keeps an open Aircraft Profile synchronized to current stable-ICAO snapshots and clearly marks last-known data when the aircraft is absent.";

constexpr const char* BUILD_ID =
    "7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE";
