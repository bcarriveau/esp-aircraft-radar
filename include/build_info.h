#pragma once

#include <stdint.h>

// Authoritative implementation baseline: main commit
// 0111ff5ad7c38ec4fe7464a3064c8a7e218791d6
// 7IN-20260803-PRODUCT69-BOUNDED-TRANSPORT

constexpr uint32_t FIRMWARE_VERSION_CODE = 70;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 70";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Restores the bounded GitHub stable-release notifier with a real CHECK NOW action and explicit scheduling diagnostics.";

constexpr const char* BUILD_ID =
    "7IN-20260803-PRODUCT70-GITHUB-UPDATE-CHECK";
