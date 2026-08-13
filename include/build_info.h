#pragma once

#include <stdint.h>

// Airport-separation branch: browser-side regional database generation with
// direct validated upload to persistent airport storage.

constexpr uint32_t FIRMWARE_VERSION_CODE = 89;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 89";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds browser-built regional airport database installation.";

constexpr const char* BUILD_ID =
    "7IN-20260813-PRODUCT89-BROWSER-AIRPORT-BUILDER";
