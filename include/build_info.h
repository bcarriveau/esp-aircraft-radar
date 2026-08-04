#pragma once

#include <stdint.h>

// Authoritative implementation baseline: current main branch after the
// physically verified Product 73 remote GitHub OTA install.

constexpr uint32_t FIRMWARE_VERSION_CODE = 75;
constexpr const char* FIRMWARE_VERSION_LABEL = "Product 75";
constexpr const char* FIRMWARE_HARDWARE_ID =
    "waveshare-esp32-s3-touch-lcd-7";
constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";
constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;
constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;
constexpr const char* FIRMWARE_RELEASE_NOTES =
    "Refines the update UI layout, fixes the System-page update button overlap, and clears transient GitHub update state on reboot before the next bounded recheck.";

constexpr const char* BUILD_ID =
    "7IN-20260804-PRODUCT75-UPDATE-UI-BOOT-CLEAR";
