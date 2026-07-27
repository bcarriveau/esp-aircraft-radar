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
constexpr const char* KEY_AIRPORTS_ENABLED = "apt_on";
constexpr const char* KEY_AIRPORT_SYMBOLS[AIRPORT_RANGE_COUNT] = {
  "apt_s20", "apt_s40", "apt_s80"
};
constexpr const char* KEY_AIRPORT_LABELS[AIRPORT_RANGE_COUNT] = {
  "apt_l20", "apt_l40", "apt_l80"
};

// Category bits: major, public, private field, heliport.
constexpr uint8_t DEFAULT_AIRPORT_SYMBOLS[AIRPORT_RANGE_COUNT] = {
  0x03, 0x03, 0x03
};
constexpr uint8_t DEFAULT_AIRPORT_LABELS[AIRPORT_RANGE_COUNT] = {
  0x03, 0x03, 0x01
};

bool cachedAirportsEnabled = true;
uint8_t cachedAirportSymbols[AIRPORT_RANGE_COUNT] = {
  DEFAULT_AIRPORT_SYMBOLS[0], DEFAULT_AIRPORT_SYMBOLS[1],
  DEFAULT_AIRPORT_SYMBOLS[2]
};
uint8_t cachedAirportLabels[AIRPORT_RANGE_COUNT] = {
  DEFAULT_AIRPORT_LABELS[0], DEFAULT_AIRPORT_LABELS[1],
  DEFAULT_AIRPORT_LABELS[2]
};

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

bool storedUCharMatches(const char* key, uint8_t value) {
  return preferences.getType(key) == PT_U8 &&
         preferences.getUChar(key, 0) == value;
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

bool writeUCharChecked(const char* key, uint8_t value) {
  if (!storageOpen || !storageHealthy) return false;
  if (storedUCharMatches(key, value)) return true;

  const size_t writtenLength = preferences.putUChar(key, value);
  const bool lengthValid = writtenLength == sizeof(uint8_t);
  const bool storedValueValid = storedUCharMatches(key, value);
  if (!lengthValid || !storedValueValid) {
    Serial.printf("NVS byte write failed: %s (%u/%u bytes)\n", key,
                  (unsigned)writtenLength, (unsigned)sizeof(uint8_t));
    return false;
  }
  return true;
}

void markStorageError(const char* operation) {
  storageHealthy = false;
  Serial.printf("NVS ERROR: %s did not complete; saving disabled\n", operation);
}

bool initializeAirportDefaults() {
  bool initialized = true;
  if (preferences.getType(KEY_AIRPORTS_ENABLED) != PT_U8 &&
      !writeUCharChecked(KEY_AIRPORTS_ENABLED, 1)) {
    initialized = false;
  }
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    if (preferences.getType(KEY_AIRPORT_SYMBOLS[i]) != PT_U8 &&
        !writeUCharChecked(KEY_AIRPORT_SYMBOLS[i],
                           DEFAULT_AIRPORT_SYMBOLS[i])) {
      initialized = false;
    }
    if (preferences.getType(KEY_AIRPORT_LABELS[i]) != PT_U8 &&
        !writeUCharChecked(KEY_AIRPORT_LABELS[i],
                           DEFAULT_AIRPORT_LABELS[i])) {
      initialized = false;
    }
  }
  return initialized;
}

void setAirportSettingsCacheDefaults() {
  cachedAirportsEnabled = true;
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    cachedAirportSymbols[i] = DEFAULT_AIRPORT_SYMBOLS[i];
    cachedAirportLabels[i] = DEFAULT_AIRPORT_LABELS[i];
  }
}

void loadAirportSettingsCache() {
  cachedAirportsEnabled = storageOpen
      ? preferences.getUChar(KEY_AIRPORTS_ENABLED, 1) != 0
      : true;
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    cachedAirportSymbols[i] = storageOpen
        ? preferences.getUChar(KEY_AIRPORT_SYMBOLS[i],
                               DEFAULT_AIRPORT_SYMBOLS[i]) & 0x0F
        : DEFAULT_AIRPORT_SYMBOLS[i];
    cachedAirportLabels[i] = storageOpen
        ? preferences.getUChar(KEY_AIRPORT_LABELS[i],
                               DEFAULT_AIRPORT_LABELS[i]) & 0x0F
        : DEFAULT_AIRPORT_LABELS[i];
  }
}

}  // namespace

bool initialize() {
  storageOpen = preferences.begin(NAMESPACE, false);
  storageHealthy = storageOpen;
  if (!storageOpen) {
    setAirportSettingsCacheDefaults();
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
  if (!initializeAirportDefaults()) initialized = false;

  storageHealthy = initialized;
  if (!initialized) {
    setAirportSettingsCacheDefaults();
    markStorageError("default initialization");
    return false;
  }
  loadAirportSettingsCache();

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
  if (!writeUCharChecked(KEY_AIRPORTS_ENABLED, 1)) saved = false;
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    if (!writeUCharChecked(KEY_AIRPORT_SYMBOLS[i],
                           DEFAULT_AIRPORT_SYMBOLS[i])) {
      saved = false;
    }
    if (!writeUCharChecked(KEY_AIRPORT_LABELS[i],
                           DEFAULT_AIRPORT_LABELS[i])) {
      saved = false;
    }
  }
  if (!saved) {
    markStorageError("reset to defaults");
  } else {
    setAirportSettingsCacheDefaults();
  }
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

bool airportsEnabled() { return cachedAirportsEnabled; }

uint8_t airportSymbolMask(uint8_t rangeIndex) {
  if (rangeIndex >= AIRPORT_RANGE_COUNT) rangeIndex = AIRPORT_RANGE_COUNT - 1;
  return cachedAirportSymbols[rangeIndex];
}

uint8_t airportLabelMask(uint8_t rangeIndex) {
  if (rangeIndex >= AIRPORT_RANGE_COUNT) rangeIndex = AIRPORT_RANGE_COUNT - 1;
  return cachedAirportLabels[rangeIndex];
}

bool saveAirportSettings(bool enabled,
                         const uint8_t symbolMasks[AIRPORT_RANGE_COUNT],
                         const uint8_t labelMasks[AIRPORT_RANGE_COUNT]) {
  if (!storageAvailable() || !symbolMasks || !labelMasks) return false;
  bool saved = writeUCharChecked(KEY_AIRPORTS_ENABLED, enabled ? 1 : 0);
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    if (!writeUCharChecked(KEY_AIRPORT_SYMBOLS[i], symbolMasks[i] & 0x0F)) {
      saved = false;
    }
    if (!writeUCharChecked(KEY_AIRPORT_LABELS[i], labelMasks[i] & 0x0F)) {
      saved = false;
    }
  }
  if (!saved) {
    markStorageError("airport settings save");
  } else {
    cachedAirportsEnabled = enabled;
    for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
      cachedAirportSymbols[i] = symbolMasks[i] & 0x0F;
      cachedAirportLabels[i] = labelMasks[i] & 0x0F;
    }
  }
  return saved;
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
