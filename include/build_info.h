#pragma once

#include <stdint.h>

// Airport-separation branch: persistent airport package reader with compiled
// Product 85 regional data retained as a safe fallback during hardware proving.

constexpr uint32_t FIRMWARE_VERSION_CODE = 86;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 86";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds persistent airport storage with compiled-database fallback.";

constexpr const char* BUILD_ID =
    "7IN-20260813-PRODUCT86-AIRPORT-PERSISTENT-STORE";
