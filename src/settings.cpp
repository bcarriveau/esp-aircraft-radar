#include "settings.h"

#include <WiFi.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "config.h"

#ifndef MQTT_ENABLED_DEFAULT
#define MQTT_ENABLED_DEFAULT 0
#endif

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
constexpr const char* KEY_MQTT_ENABLED = "mqtt_on";
constexpr const char* KEY_AIRPORTS_ENABLED = "apt_on";
constexpr const char* KEY_AIRPORT_OVERRIDES = "apt_ovr";
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
  0x03, 0x03, 0x03
};

bool cachedMqttEnabled = MQTT_ENABLED_DEFAULT != 0;
bool cachedAirportsEnabled = true;
uint8_t cachedAirportSymbols[AIRPORT_RANGE_COUNT] = {
  DEFAULT_AIRPORT_SYMBOLS[0], DEFAULT_AIRPORT_SYMBOLS[1],
  DEFAULT_AIRPORT_SYMBOLS[2]
};
uint8_t cachedAirportLabels[AIRPORT_RANGE_COUNT] = {
  DEFAULT_AIRPORT_LABELS[0], DEFAULT_AIRPORT_LABELS[1],
  DEFAULT_AIRPORT_LABELS[2]
};

constexpr uint8_t AIRPORT_OVERRIDE_STORAGE_VERSION = 1;
constexpr size_t AIRPORT_IDENT_CAPACITY = 8;

struct StoredAirportLabelOverride {
  char ident[AIRPORT_IDENT_CAPACITY]{};
  uint8_t mode = static_cast<uint8_t>(AirportLabelMode::AUTO);
};

struct StoredAirportLabelOverrides {
  uint8_t version = AIRPORT_OVERRIDE_STORAGE_VERSION;
  uint8_t count = 0;
  uint16_t reserved = 0;
  StoredAirportLabelOverride entries[AIRPORT_LABEL_OVERRIDE_CAPACITY]{};
};

static_assert(sizeof(StoredAirportLabelOverride) == 9,
              "Airport override record layout changed");
static_assert(sizeof(StoredAirportLabelOverrides) ==
                  4 + 9 * AIRPORT_LABEL_OVERRIDE_CAPACITY,
              "Airport override storage layout changed");

StoredAirportLabelOverrides cachedAirportOverrides{};

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

bool storedBytesMatch(const char* key, const void* value, size_t length) {
  if (!value || preferences.getType(key) != PT_BLOB ||
      preferences.getBytesLength(key) != length) {
    return false;
  }
  StoredAirportLabelOverrides stored{};
  if (length != sizeof(stored) ||
      preferences.getBytes(key, &stored, sizeof(stored)) != sizeof(stored)) {
    return false;
  }
  return memcmp(&stored, value, length) == 0;
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

bool writeBytesChecked(const char* key, const void* value, size_t length) {
  if (!storageOpen || !storageHealthy || !value || length == 0) return false;
  if (storedBytesMatch(key, value, length)) return true;

  const size_t writtenLength = preferences.putBytes(key, value, length);
  const bool lengthValid = writtenLength == length;
  const bool storedValueValid = storedBytesMatch(key, value, length);
  if (!lengthValid || !storedValueValid) {
    Serial.printf("NVS blob write failed: %s (%u/%u bytes)\n", key,
                  (unsigned)writtenLength, (unsigned)length);
    return false;
  }
  return true;
}

bool airportIdentValid(const char* ident) {
  if (!ident || !ident[0]) return false;
  size_t length = 0;
  for (; ident[length] && length < AIRPORT_IDENT_CAPACITY; ++length) {
    const unsigned char character = static_cast<unsigned char>(ident[length]);
    if (!isalnum(character) && character != '-') return false;
  }
  return length > 0 && length < AIRPORT_IDENT_CAPACITY && ident[length] == 0;
}

void normalizeAirportIdent(const char* ident, char out[AIRPORT_IDENT_CAPACITY]) {
  memset(out, 0, AIRPORT_IDENT_CAPACITY);
  if (!ident) return;
  for (size_t index = 0; index + 1 < AIRPORT_IDENT_CAPACITY && ident[index];
       ++index) {
    out[index] = static_cast<char>(toupper(
        static_cast<unsigned char>(ident[index])));
  }
}

void clearAirportOverrideCache() {
  cachedAirportOverrides = StoredAirportLabelOverrides{};
}

int compareAirportIdent(const char* first, const char* second) {
  return strcmp(first ? first : "", second ? second : "");
}

uint8_t airportOverrideLowerBound(const char* ident, bool& found) {
  uint8_t low = 0;
  uint8_t high = cachedAirportOverrides.count;
  while (low < high) {
    const uint8_t middle = static_cast<uint8_t>(low + (high - low) / 2);
    const int comparison = compareAirportIdent(
        cachedAirportOverrides.entries[middle].ident, ident);
    if (comparison < 0) {
      low = static_cast<uint8_t>(middle + 1);
    } else {
      high = middle;
    }
  }
  found = low < cachedAirportOverrides.count &&
          compareAirportIdent(cachedAirportOverrides.entries[low].ident,
                              ident) == 0;
  return low;
}

bool airportOverrideStorageValid(const StoredAirportLabelOverrides& storage) {
  if (storage.version != AIRPORT_OVERRIDE_STORAGE_VERSION ||
      storage.count > AIRPORT_LABEL_OVERRIDE_CAPACITY) {
    return false;
  }
  const char* previous = nullptr;
  for (uint8_t index = 0; index < storage.count; ++index) {
    const StoredAirportLabelOverride& entry = storage.entries[index];
    char normalized[AIRPORT_IDENT_CAPACITY]{};
    normalizeAirportIdent(entry.ident, normalized);
    if (!airportIdentValid(entry.ident) ||
        strcmp(normalized, entry.ident) != 0 ||
        (entry.mode != static_cast<uint8_t>(AirportLabelMode::SHOW) &&
         entry.mode != static_cast<uint8_t>(AirportLabelMode::HIDE))) {
      return false;
    }
    if (previous && compareAirportIdent(previous, entry.ident) >= 0) {
      return false;
    }
    previous = entry.ident;
  }
  return true;
}

bool initializeAirportOverrideDefaults() {
  if (preferences.getType(KEY_AIRPORT_OVERRIDES) == PT_BLOB &&
      preferences.getBytesLength(KEY_AIRPORT_OVERRIDES) ==
          sizeof(StoredAirportLabelOverrides)) {
    return true;
  }
  StoredAirportLabelOverrides empty{};
  return writeBytesChecked(KEY_AIRPORT_OVERRIDES, &empty, sizeof(empty));
}

bool loadAirportOverrideCache() {
  clearAirportOverrideCache();
  if (!storageOpen) return true;
  StoredAirportLabelOverrides loaded{};
  if (preferences.getType(KEY_AIRPORT_OVERRIDES) == PT_BLOB &&
      preferences.getBytesLength(KEY_AIRPORT_OVERRIDES) == sizeof(loaded) &&
      preferences.getBytes(KEY_AIRPORT_OVERRIDES, &loaded, sizeof(loaded)) ==
          sizeof(loaded) &&
      airportOverrideStorageValid(loaded)) {
    cachedAirportOverrides = loaded;
    return true;
  }
  Serial.println("Airport label overrides invalid; resetting to AUTO");
  StoredAirportLabelOverrides empty{};
  if (!writeBytesChecked(KEY_AIRPORT_OVERRIDES, &empty, sizeof(empty))) {
    return false;
  }
  cachedAirportOverrides = empty;
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
  if (!initializeAirportOverrideDefaults()) initialized = false;
  return initialized;
}

void setAirportSettingsCacheDefaults() {
  cachedAirportsEnabled = true;
  for (uint8_t i = 0; i < AIRPORT_RANGE_COUNT; ++i) {
    cachedAirportSymbols[i] = DEFAULT_AIRPORT_SYMBOLS[i];
    cachedAirportLabels[i] = DEFAULT_AIRPORT_LABELS[i];
  }
  clearAirportOverrideCache();
}

bool loadAirportSettingsCache() {
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
  return loadAirportOverrideCache();
}

}  // namespace

bool initialize() {
  storageOpen = preferences.begin(NAMESPACE, false);
  storageHealthy = storageOpen;
  if (!storageOpen) {
    cachedMqttEnabled = MQTT_ENABLED_DEFAULT != 0;
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
  if (preferences.getType(KEY_MQTT_ENABLED) != PT_U8 &&
      !writeUCharChecked(KEY_MQTT_ENABLED, MQTT_ENABLED_DEFAULT ? 1 : 0)) {
    initialized = false;
  }
  if (!initializeAirportDefaults()) initialized = false;

  storageHealthy = initialized;
  if (!initialized) {
    setAirportSettingsCacheDefaults();
    markStorageError("default initialization");
    return false;
  }
  cachedMqttEnabled = preferences.getUChar(
      KEY_MQTT_ENABLED, MQTT_ENABLED_DEFAULT ? 1 : 0) != 0;
  if (!loadAirportSettingsCache()) {
    cachedMqttEnabled = MQTT_ENABLED_DEFAULT != 0;
    setAirportSettingsCacheDefaults();
    markStorageError("airport override initialization");
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
  if (!writeUCharChecked(KEY_MQTT_ENABLED,
                         MQTT_ENABLED_DEFAULT ? 1 : 0)) saved = false;
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
  StoredAirportLabelOverrides emptyOverrides{};
  if (!writeBytesChecked(KEY_AIRPORT_OVERRIDES, &emptyOverrides,
                         sizeof(emptyOverrides))) {
    saved = false;
  }
  if (!saved) {
    markStorageError("reset to defaults");
  } else {
    cachedMqttEnabled = MQTT_ENABLED_DEFAULT != 0;
    setAirportSettingsCacheDefaults();
  }
  return saved;
}

bool mqttEnabled() { return cachedMqttEnabled; }

bool setMqttEnabled(bool enabled) {
  if (!storageAvailable()) return false;
  if (!writeUCharChecked(KEY_MQTT_ENABLED, enabled ? 1 : 0)) {
    markStorageError("MQTT enabled-state save");
    return false;
  }
  cachedMqttEnabled = enabled;
  return true;
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

AirportLabelMode airportLabelMode(const char* ident) {
  if (!airportIdentValid(ident) || cachedAirportOverrides.count == 0) {
    return AirportLabelMode::AUTO;
  }
  char normalized[AIRPORT_IDENT_CAPACITY]{};
  normalizeAirportIdent(ident, normalized);
  bool found = false;
  const uint8_t index = airportOverrideLowerBound(normalized, found);
  if (!found) return AirportLabelMode::AUTO;
  return static_cast<AirportLabelMode>(cachedAirportOverrides.entries[index].mode);
}

const char* airportLabelModeName(AirportLabelMode mode) {
  switch (mode) {
    case AirportLabelMode::SHOW: return "SHOW";
    case AirportLabelMode::HIDE: return "HIDE";
    case AirportLabelMode::AUTO:
    default: return "AUTO";
  }
}

uint8_t airportLabelOverrideCount(AirportLabelMode mode) {
  if (mode == AirportLabelMode::AUTO) return 0;
  uint8_t count = 0;
  for (uint8_t index = 0; index < cachedAirportOverrides.count; ++index) {
    if (cachedAirportOverrides.entries[index].mode ==
        static_cast<uint8_t>(mode)) {
      ++count;
    }
  }
  return count;
}

bool setAirportLabelMode(const char* ident, AirportLabelMode mode) {
  if (!storageAvailable() || !airportIdentValid(ident) ||
      (mode != AirportLabelMode::AUTO && mode != AirportLabelMode::SHOW &&
       mode != AirportLabelMode::HIDE)) {
    return false;
  }

  char normalized[AIRPORT_IDENT_CAPACITY]{};
  normalizeAirportIdent(ident, normalized);
  bool found = false;
  const uint8_t index = airportOverrideLowerBound(normalized, found);
  StoredAirportLabelOverrides updated = cachedAirportOverrides;

  if (mode == AirportLabelMode::AUTO) {
    if (!found) return true;
    const uint8_t following = static_cast<uint8_t>(
        updated.count - index - 1);
    if (following > 0) {
      memmove(&updated.entries[index], &updated.entries[index + 1],
              following * sizeof(updated.entries[0]));
    }
    --updated.count;
    updated.entries[updated.count] = StoredAirportLabelOverride{};
  } else if (found) {
    updated.entries[index].mode = static_cast<uint8_t>(mode);
  } else {
    if (updated.count >= AIRPORT_LABEL_OVERRIDE_CAPACITY) return false;
    const uint8_t following = static_cast<uint8_t>(updated.count - index);
    if (following > 0) {
      memmove(&updated.entries[index + 1], &updated.entries[index],
              following * sizeof(updated.entries[0]));
    }
    updated.entries[index] = StoredAirportLabelOverride{};
    memcpy(updated.entries[index].ident, normalized, AIRPORT_IDENT_CAPACITY);
    updated.entries[index].mode = static_cast<uint8_t>(mode);
    ++updated.count;
  }

  if (!writeBytesChecked(KEY_AIRPORT_OVERRIDES, &updated, sizeof(updated))) {
    markStorageError("airport label override save");
    return false;
  }
  cachedAirportOverrides = updated;
  return true;
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
