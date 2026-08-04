#pragma once

#include <stdint.h>

// Authoritative implementation baseline: main commit
// a52ee1cd39f1d39182730418de7192b9779a4307
// 7IN-20260803-PRODUCT71-GITHUB-REDIRECT-FIX

constexpr uint32_t FIRMWARE_VERSION_CODE = 72;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 72";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Right-sizes the verified GitHub HTTPS transmit buffer for long signed release redirects.";

constexpr const char* BUILD_ID =
    "7IN-20260803-PRODUCT72-GITHUB-TX-BUFFER-FIX";
