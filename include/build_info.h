#pragma once

#include <stdint.h>

// Authoritative implementation baseline: Product 78 from current main,
// preserving the physically verified Product 73 remote GitHub OTA install path.

constexpr uint32_t FIRMWARE_VERSION_CODE = 79;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 79";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds stable scaled aircraft symbols at 40 and 80 miles without per-frame allocation or new bitmap assets.";

constexpr const char* BUILD_ID =
    "7IN-20260806-PRODUCT79-RANGE-SYMBOLS";
