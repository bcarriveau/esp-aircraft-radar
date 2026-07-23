#include "settings.h"

#include <WiFi.h>
#include <math.h>

#include "config.h"

namespace settings {
namespace {

Preferences preferences;
constexpr const char* NAMESPACE = "radar_cfg";
constexpr const char* KEY_TITLE = "title";
constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
constexpr const char* KEY_WIFI_PASS = "wifi_pass";
constexpr const char* KEY_LAT = "home_lat";
constexpr const char* KEY_LON = "home_lon";

String defaultTitle() {
  return String("BILLS AIRCRAFT RADAR");
}

String defaultWifiSsid() {
  return String(WIFI_SSID);
}

String defaultWifiPassword() {
  return String(WIFI_PASS);
}

float defaultLatitude() {
  return HOME_LAT;
}

float defaultLongitude() {
  return HOME_LON;
}

}  // namespace

void initialize() {
  preferences.begin(NAMESPACE, false);

  if (!preferences.isKey(KEY_TITLE)) {
    preferences.putString(KEY_TITLE, defaultTitle());
  }

  if (!preferences.isKey(KEY_WIFI_SSID)) {
    preferences.putString(KEY_WIFI_SSID, defaultWifiSsid());
  }

  if (!preferences.isKey(KEY_WIFI_PASS)) {
    preferences.putString(KEY_WIFI_PASS, defaultWifiPassword());
  }

  if (!preferences.isKey(KEY_LAT)) {
    preferences.putFloat(KEY_LAT, defaultLatitude());
  }

  if (!preferences.isKey(KEY_LON)) {
    preferences.putFloat(KEY_LON, defaultLongitude());
  }
}

void resetToDefaults() {
  preferences.putString(KEY_TITLE, defaultTitle());
  preferences.putString(KEY_WIFI_SSID, defaultWifiSsid());
  preferences.putString(KEY_WIFI_PASS, defaultWifiPassword());
  preferences.putFloat(KEY_LAT, defaultLatitude());
  preferences.putFloat(KEY_LON, defaultLongitude());
}

String deviceTitle() {
  return preferences.getString(KEY_TITLE, defaultTitle().c_str());
}

void setDeviceTitle(const String& title) {
  String cleaned = title;
  cleaned.trim();
  if (cleaned.length() > 0) {
    preferences.putString(KEY_TITLE, cleaned.c_str());
  }
}

String wifiSsid() {
  return preferences.getString(KEY_WIFI_SSID, defaultWifiSsid().c_str());
}

void setWifiSsid(const String& ssid) {
  String cleaned = ssid;
  cleaned.trim();
  if (cleaned.length() > 0) {
    preferences.putString(KEY_WIFI_SSID, cleaned.c_str());
  }
}

String wifiPassword() {
  return preferences.getString(KEY_WIFI_PASS, defaultWifiPassword().c_str());
}

void setWifiPassword(const String& password) {
  preferences.putString(KEY_WIFI_PASS, password.c_str());
}

float homeLatitude() {
  return preferences.getFloat(KEY_LAT, defaultLatitude());
}

float homeLongitude() {
  return preferences.getFloat(KEY_LON, defaultLongitude());
}

void setHomeLatitude(float latitude) {
  preferences.putFloat(KEY_LAT, latitude);
}

void setHomeLongitude(float longitude) {
  preferences.putFloat(KEY_LON, longitude);
}

bool saveSettings(const String& title, const String& ssid,
                  const String& password, float latitude, float longitude) {
  String cleanedTitle = title;
  String cleanedSsid = ssid;
  cleanedTitle.trim();
  cleanedSsid.trim();
  if (!cleanedTitle.length() || !cleanedSsid.length() ||
      !coordinatesValid(latitude, longitude)) {
    return false;
  }
  preferences.putString(KEY_TITLE, cleanedTitle.c_str());
  preferences.putString(KEY_WIFI_SSID, cleanedSsid.c_str());
  preferences.putString(KEY_WIFI_PASS, password.c_str());
  preferences.putFloat(KEY_LAT, latitude);
  preferences.putFloat(KEY_LON, longitude);
  return true;
}

bool coordinatesValid(float latitude, float longitude) {
  return isfinite(latitude) && isfinite(longitude) && latitude >= -90.0f &&
         latitude <= 90.0f && longitude >= -180.0f && longitude <= 180.0f;
}

}  // namespace settings
