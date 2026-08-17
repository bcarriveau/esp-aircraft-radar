#pragma once

#include <stdint.h>

// Main branch: unified persistent airport storage for all build variants.

constexpr uint32_t FIRMWARE_VERSION_CODE = 98;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 98";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds muted nearest-aircraft halo associations on the 20-mile radar.";

constexpr const char* BUILD_ID =
    "7IN-20260817-PRODUCT98-NEAREST-HALO";

#if defined(RADAR_DISTRIBUTION_BUILD)
constexpr const char* FIRMWARE_BUILD_VARIANT = "distribution";
// Public release/factory tooling requires this exact marker in the application
// image before it will create distributable artifacts.
constexpr const char* FIRMWARE_DISTRIBUTION_MARKER =
    "RADAR-DISTRIBUTION-BUILD";
#else
constexpr const char* FIRMWARE_BUILD_VARIANT = "private";
#endif
