#pragma once

#include <stdint.h>

// Authoritative implementation baseline: main commit
// b20c2d47c5b3a758564e944e9b912fd562334c1d
// 7IN-20260803-PRODUCT70-GITHUB-UPDATE-CHECK

constexpr uint32_t FIRMWARE_VERSION_CODE = 71;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 71";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Accepts bounded real-world GitHub release redirects and reports exact response-header failures.";

constexpr const char* BUILD_ID =
    "7IN-20260803-PRODUCT71-GITHUB-REDIRECT-FIX";
