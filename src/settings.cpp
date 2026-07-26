#include "settings.h"

#include <WiFi.h>
#include <math.h>

#include "config.h"

namespace settings {
namespace {

Preferences preferences;
bool storageOpen = false;
bool storageHealthy = false;
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

bool storedStringMatches(const char* key, const String& value) {
  return preferences.getType(key) == PT_STR &&
         preferences.getString(key, String()) == value;
}

bool storedFloatMatches(const char* key, float value) {
  return preferences.getType(key) == PT_BLOB &&
         preferences.getBytesLength(key) == sizeof(float) &&
         preferences.getFloat(key, NAN) == value;
}

bool writeStringChecked(const char* key, const String& value) {
  if (!storageOpen || !storageHealthy) return false;
  if (storedStringMatches(key, value)) return true;

  const size_t expectedLength = value.length();
  const size_t writtenLength = preferences.putString(key, value.c_str());
  const bool lengthValid = expectedLength == 0
      ? writtenLength == 0
      : writtenLength == expectedLength;
  const bool storedValueValid = storedStringMatches(key, value);
  if (!lengthValid || !storedValueValid) {
    Serial.printf("NVS string write failed: %s (%u/%u bytes)\n", key,
                  (unsigned)writtenLength, (unsigned)expectedLength);
    return false;
  }
  return true;
}

bool writeFloatChecked(const char* key, float value) {
  if (!storageOpen || !storageHealthy) return false;
  if (storedFloatMatches(key, value)) return true;

  const size_t writtenLength = preferences.putFloat(key, value);
  const bool lengthValid = writtenLength == sizeof(float);
  const bool storedValueValid = storedFloatMatches(key, value);
  if (!lengthValid || !storedValueValid) {
    Serial.printf("NVS float write failed: %s (%u/%u bytes)\n", key,
                  (unsigned)writtenLength, (unsigned)sizeof(float));
    return false;
  }
  return true;
}

void markStorageError(const char* operation) {
  storageHealthy = false;
  Serial.printf("NVS ERROR: %s did not complete; saving disabled\n", operation);
}

}  // namespace

bool initialize() {
  storageOpen = preferences.begin(NAMESPACE, false);
  storageHealthy = storageOpen;
  if (!storageOpen) {
    Serial.println(
        "NVS ERROR: preferences namespace unavailable; using compile-time defaults");
    return false;
  }

  bool initialized = true;
  if (preferences.getType(KEY_TITLE) != PT_STR &&
      !writeStringChecked(KEY_TITLE, defaultTitle())) {
    initialized = false;
  }
  if (preferences.getType(KEY_WIFI_SSID) != PT_STR &&
      !writeStringChecked(KEY_WIFI_SSID, defaultWifiSsid())) {
    initialized = false;
  }
  if (preferences.getType(KEY_WIFI_PASS) != PT_STR &&
      !writeStringChecked(KEY_WIFI_PASS, defaultWifiPassword())) {
    initialized = false;
  }
  if ((preferences.getType(KEY_LAT) != PT_BLOB ||
       preferences.getBytesLength(KEY_LAT) != sizeof(float)) &&
      !writeFloatChecked(KEY_LAT, defaultLatitude())) {
    initialized = false;
  }
  if ((preferences.getType(KEY_LON) != PT_BLOB ||
       preferences.getBytesLength(KEY_LON) != sizeof(float)) &&
      !writeFloatChecked(KEY_LON, defaultLongitude())) {
    initialized = false;
  }

  storageHealthy = initialized;
  if (!initialized) {
    markStorageError("default initialization");
    return false;
  }

  Serial.println("NVS: READY");
  return true;
}

bool storageAvailable() {
  return storageOpen && storageHealthy;
}

bool resetToDefaults() {
  if (!storageAvailable()) return false;

  bool saved = true;
  if (!writeStringChecked(KEY_TITLE, defaultTitle())) saved = false;
  if (!writeStringChecked(KEY_WIFI_SSID, defaultWifiSsid())) saved = false;
  if (!writeStringChecked(KEY_WIFI_PASS, defaultWifiPassword())) saved = false;
  if (!writeFloatChecked(KEY_LAT, defaultLatitude())) saved = false;
  if (!writeFloatChecked(KEY_LON, defaultLongitude())) saved = false;
  if (!saved) markStorageError("reset to defaults");
  return saved;
}

String deviceTitle() {
  if (!storageOpen) return defaultTitle();
  return preferences.getString(KEY_TITLE, defaultTitle());
}

void setDeviceTitle(const String& title) {
  String cleaned = title;
  cleaned.trim();
  if (cleaned.length() > 0 && !writeStringChecked(KEY_TITLE, cleaned)) {
    markStorageError("display-name update");
  }
}

String wifiSsid() {
  if (!storageOpen) return defaultWifiSsid();
  return preferences.getString(KEY_WIFI_SSID, defaultWifiSsid());
}

void setWifiSsid(const String& ssid) {
  String cleaned = ssid;
  cleaned.trim();
  if (cleaned.length() > 0 && !writeStringChecked(KEY_WIFI_SSID, cleaned)) {
    markStorageError("Wi-Fi SSID update");
  }
}

String wifiPassword() {
  if (!storageOpen) return defaultWifiPassword();
  return preferences.getString(KEY_WIFI_PASS, defaultWifiPassword());
}

void setWifiPassword(const String& password) {
  if (!writeStringChecked(KEY_WIFI_PASS, password)) {
    markStorageError("Wi-Fi password update");
  }
}

float homeLatitude() {
  if (!storageOpen) return defaultLatitude();
  return preferences.getFloat(KEY_LAT, defaultLatitude());
}

float homeLongitude() {
  if (!storageOpen) return defaultLongitude();
  return preferences.getFloat(KEY_LON, defaultLongitude());
}

void setHomeLatitude(float latitude) {
  if (!writeFloatChecked(KEY_LAT, latitude)) {
    markStorageError("latitude update");
  }
}

void setHomeLongitude(float longitude) {
  if (!writeFloatChecked(KEY_LON, longitude)) {
    markStorageError("longitude update");
  }
}

bool saveSettings(const String& title, const String& ssid,
                  const String& password, float latitude, float longitude) {
  String cleanedTitle = title;
  String cleanedSsid = ssid;
  cleanedTitle.trim();
  cleanedSsid.trim();
  if (!cleanedTitle.length() || !cleanedSsid.length() ||
      !coordinatesValid(latitude, longitude) || !storageAvailable()) {
    return false;
  }

  bool saved = true;
  if (!writeStringChecked(KEY_TITLE, cleanedTitle)) saved = false;
  if (!writeStringChecked(KEY_WIFI_SSID, cleanedSsid)) saved = false;
  if (!writeStringChecked(KEY_WIFI_PASS, password)) saved = false;
  if (!writeFloatChecked(KEY_LAT, latitude)) saved = false;
  if (!writeFloatChecked(KEY_LON, longitude)) saved = false;
  if (!saved) markStorageError("settings save");
  return saved;
}

bool coordinatesValid(float latitude, float longitude) {
  return isfinite(latitude) && isfinite(longitude) && latitude >= -90.0f &&
         latitude <= 90.0f && longitude >= -180.0f && longitude <= 180.0f;
}

}  // namespace settings
