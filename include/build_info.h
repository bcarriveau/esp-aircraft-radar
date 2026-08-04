#pragma once

#include <stdint.h>

// Authoritative implementation baseline: main commit
// f999347897185d41761dc6c896229e002cb7482f
// 7IN-20260803-PRODUCT72-GITHUB-TX-BUFFER-FIX

constexpr uint32_t FIRMWARE_VERSION_CODE = 74;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 74";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Adds user-confirmed verified GitHub package download and installation while preserving local browser OTA recovery.";

constexpr const char* BUILD_ID =
    "7IN-20260804-PRODUCT74-GITHUB-OTA-TEST";
