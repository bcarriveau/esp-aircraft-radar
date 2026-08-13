#pragma once

#include <stdint.h>

// Airport-separation branch: validated persistent airport installer foundation
// with Product 85 compiled regional data retained as the safe runtime fallback.

constexpr uint32_t FIRMWARE_VERSION_CODE = 87;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 87";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds validated persistent airport package installation support.";

constexpr const char* BUILD_ID =
    "7IN-20260813-PRODUCT87-AIRPORT-PACKAGE-INSTALLER";
