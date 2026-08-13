#pragma once

#include <stdint.h>

// Airport-separation branch: mobile local airport-database upload using the
// persistent Product 87 installer, with compiled Product 85 data still retained
// as the safe runtime fallback.

constexpr uint32_t FIRMWARE_VERSION_CODE = 88;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 88";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds mobile-friendly local airport database upload.";

constexpr const char* BUILD_ID =
    "7IN-20260813-PRODUCT88-AIRPORT-WEB-UPLOAD";
