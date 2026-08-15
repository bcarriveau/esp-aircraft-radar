#pragma once

#include <stdint.h>

// Airport-separation branch: browser-side regional database generation with
// direct validated upload to persistent airport storage.

constexpr uint32_t FIRMWARE_VERSION_CODE = 94;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 94";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds a guarded distribution release path and destructive factory installer.";

constexpr const char* BUILD_ID =
    "7IN-20260814-PRODUCT94-FACTORY-DISTRIBUTION";

#if defined(RADAR_DISTRIBUTION_BUILD)
constexpr const char* FIRMWARE_BUILD_VARIANT = "distribution";
// build_radar_ota.py requires this exact marker in the application image before
// it will create a public .radarota package or GitHub release manifest.
constexpr const char* FIRMWARE_DISTRIBUTION_MARKER =
    "RADAR-DISTRIBUTION-BUILD";
#else
constexpr const char* FIRMWARE_BUILD_VARIANT = "private";
#endif
