#pragma once

#include <stdint.h>

// Authoritative implementation baseline: current main branch after the
// physically verified Product 73 remote GitHub OTA install.

constexpr uint32_t FIRMWARE_VERSION_CODE = 76;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 76";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Shows the three aircraft nearest to the selected or tracked aircraft using bounded stable-ICAO separation calculations.";

constexpr const char* BUILD_ID =
    "7IN-20260804-PRODUCT76-PRIORITY-NEIGHBORS";
