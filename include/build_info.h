#pragma once

#include <stdint.h>

// Authoritative implementation baseline: committed Product 82 on current main,
// preserving the physically verified Product 73 remote GitHub OTA install path.

constexpr uint32_t FIRMWARE_VERSION_CODE = 83;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 83";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Keeps the active System settings field visible above the on-screen keyboard and restores the form when editing ends.";

constexpr const char* BUILD_ID =
    "7IN-20260808-PRODUCT83-SETTINGS-KEYBOARD-VISIBILITY";
