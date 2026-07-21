#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace settings {

void initialize();
void resetToDefaults();

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

bool saveSettings(const String& title, const String& ssid,
                  const String& password, float latitude, float longitude);
bool coordinatesValid(float latitude, float longitude);

}  // namespace settings
