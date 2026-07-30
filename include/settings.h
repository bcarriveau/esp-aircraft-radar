#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace settings {

constexpr uint8_t AIRPORT_RANGE_COUNT = 3;
constexpr uint8_t AIRPORT_LABEL_OVERRIDE_CAPACITY = 64;

enum class AirportLabelMode : uint8_t {
  AUTO = 0,
  SHOW = 1,
  HIDE = 2
};

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
AirportLabelMode airportLabelMode(const char* ident);
const char* airportLabelModeName(AirportLabelMode mode);
uint8_t airportLabelOverrideCount(AirportLabelMode mode);
bool setAirportLabelMode(const char* ident, AirportLabelMode mode);
bool saveAirportSettings(bool enabled,
                         const uint8_t symbolMasks[AIRPORT_RANGE_COUNT],
                         const uint8_t labelMasks[AIRPORT_RANGE_COUNT]);

bool saveSettings(const String& title, const String& ssid,
                  const String& password, float latitude, float longitude);
bool coordinatesValid(float latitude, float longitude);

}  // namespace settings
