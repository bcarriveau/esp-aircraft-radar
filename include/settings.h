#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace settings {

constexpr uint8_t AIRPORT_RANGE_COUNT = 3;

bool initialize();
bool storageAvailable();
bool resetToDefaults();

String deviceTitle();
void setDeviceTitle(const String& title);

String wifiSsid();
void setWifiSsid(const String& ssid);

String wifiPassword();
void setWifiPassword(const String& password);

float homeLatitude();
void setHomeLatitude(float latitude);

float homeLongitude();
void setHomeLongitude(float longitude);

bool airportsEnabled();
uint8_t airportSymbolMask(uint8_t rangeIndex);
uint8_t airportLabelMask(uint8_t rangeIndex);
bool saveAirportSettings(bool enabled,
                         const uint8_t symbolMasks[AIRPORT_RANGE_COUNT],
                         const uint8_t labelMasks[AIRPORT_RANGE_COUNT]);

bool saveSettings(const String& title, const String& ssid,
                  const String& password, float latitude, float longitude);
bool coordinatesValid(float latitude, float longitude);

}  // namespace settings
