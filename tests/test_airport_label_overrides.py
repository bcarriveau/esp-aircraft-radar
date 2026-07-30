#!/usr/bin/env python3
"""Host-compile and exercise Product 53R4 airport label override storage."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ARDUINO_H = r'''
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  String(const String&) = default;
  String& operator=(const String&) = default;
  String& operator=(const char* value) {
    value_ = value ? value : "";
    return *this;
  }
  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.size(); }
  void trim() {
    const auto first = value_.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      value_.clear();
      return;
    }
    const auto last = value_.find_last_not_of(" \t\r\n");
    value_ = value_.substr(first, last - first + 1);
  }
  bool operator==(const String& other) const { return value_ == other.value_; }
  bool operator!=(const String& other) const { return !(*this == other); }
 private:
  std::string value_;
};

struct SerialStub {
  template <typename... Args> void printf(const char*, Args...) {}
  void println(const char*) {}
};
inline SerialStub Serial;
'''

PREFERENCES_H = r'''
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "Arduino.h"

enum PreferenceType : uint8_t {
  PT_INVALID = 0,
  PT_U8,
  PT_I8,
  PT_U16,
  PT_I16,
  PT_U32,
  PT_I32,
  PT_U64,
  PT_I64,
  PT_STR,
  PT_BLOB
};

struct PreferenceEntry {
  PreferenceType type = PT_INVALID;
  std::vector<uint8_t> data;
};

class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  PreferenceType getType(const char* key) const {
    const auto it = entries_.find(key ? key : "");
    return it == entries_.end() ? PT_INVALID : it->second.type;
  }
  String getString(const char* key, const String& fallback = String()) const {
    const auto it = entries_.find(key ? key : "");
    if (it == entries_.end() || it->second.type != PT_STR) return fallback;
    return String(reinterpret_cast<const char*>(it->second.data.data()));
  }
  float getFloat(const char* key, float fallback = 0.0f) const {
    float value = fallback;
    const auto it = entries_.find(key ? key : "");
    if (it != entries_.end() && it->second.type == PT_BLOB &&
        it->second.data.size() == sizeof(value)) {
      std::memcpy(&value, it->second.data.data(), sizeof(value));
    }
    return value;
  }
  uint8_t getUChar(const char* key, uint8_t fallback = 0) const {
    const auto it = entries_.find(key ? key : "");
    if (it == entries_.end() || it->second.type != PT_U8 ||
        it->second.data.size() != 1) return fallback;
    return it->second.data[0];
  }
  size_t getBytesLength(const char* key) const {
    const auto it = entries_.find(key ? key : "");
    return it == entries_.end() ? 0 : it->second.data.size();
  }
  size_t getBytes(const char* key, void* out, size_t length) const {
    const auto it = entries_.find(key ? key : "");
    if (!out || it == entries_.end() || it->second.type != PT_BLOB ||
        length > it->second.data.size()) return 0;
    std::memcpy(out, it->second.data.data(), length);
    return length;
  }
  size_t putString(const char* key, const char* value) {
    const std::string text = value ? value : "";
    PreferenceEntry& entry = entries_[key ? key : ""];
    entry.type = PT_STR;
    entry.data.assign(text.begin(), text.end());
    entry.data.push_back(0);
    return text.size();
  }
  size_t putFloat(const char* key, float value) {
    return putBlob(key, &value, sizeof(value));
  }
  size_t putUChar(const char* key, uint8_t value) {
    PreferenceEntry& entry = entries_[key ? key : ""];
    entry.type = PT_U8;
    entry.data.assign(1, value);
    return 1;
  }
  size_t putBytes(const char* key, const void* value, size_t length) {
    return putBlob(key, value, length);
  }
 private:
  size_t putBlob(const char* key, const void* value, size_t length) {
    if (!value && length) return 0;
    PreferenceEntry& entry = entries_[key ? key : ""];
    entry.type = PT_BLOB;
    const auto* bytes = static_cast<const uint8_t*>(value);
    entry.data.assign(bytes, bytes + length);
    return length;
  }
  inline static std::unordered_map<std::string, PreferenceEntry> entries_{};
};
'''

HARNESS = r'''
#include <cassert>
#include <cstdio>
#include "settings.h"

using settings::AirportLabelMode;

int main() {
  assert(settings::initialize());
  assert(settings::airportLabelMode("KUES") == AirportLabelMode::AUTO);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) == 0);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::HIDE) == 0);

  assert(settings::setAirportLabelMode("kues", AirportLabelMode::SHOW));
  assert(settings::setAirportLabelMode("KMKE", AirportLabelMode::HIDE));
  assert(settings::setAirportLabelMode("KBUU", AirportLabelMode::SHOW));
  assert(settings::airportLabelMode("KUES") == AirportLabelMode::SHOW);
  assert(settings::airportLabelMode("kmke") == AirportLabelMode::HIDE);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) == 2);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::HIDE) == 1);

  assert(settings::setAirportLabelMode("KUES", AirportLabelMode::HIDE));
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) == 1);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::HIDE) == 2);
  assert(settings::setAirportLabelMode("KMKE", AirportLabelMode::AUTO));
  assert(settings::airportLabelMode("KMKE") == AirportLabelMode::AUTO);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::HIDE) == 1);

  assert(!settings::setAirportLabelMode("TOO-LONG", AirportLabelMode::SHOW));
  assert(settings::airportLabelMode("TOO-LONG") == AirportLabelMode::AUTO);

  // Re-initialization reloads the verified NVS blob into the RAM cache.
  assert(settings::initialize());
  assert(settings::airportLabelMode("KUES") == AirportLabelMode::HIDE);
  assert(settings::airportLabelMode("KBUU") == AirportLabelMode::SHOW);

  assert(settings::resetToDefaults());
  assert(settings::airportLabelMode("KUES") == AirportLabelMode::AUTO);
  assert(settings::airportLabelMode("KBUU") == AirportLabelMode::AUTO);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) == 0);
  assert(settings::airportLabelOverrideCount(AirportLabelMode::HIDE) == 0);

  char ident[8]{};
  for (unsigned index = 0;
       index < settings::AIRPORT_LABEL_OVERRIDE_CAPACITY; ++index) {
    std::snprintf(ident, sizeof(ident), "A%06u", index);
    assert(settings::setAirportLabelMode(ident, AirportLabelMode::SHOW));
  }
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) ==
         settings::AIRPORT_LABEL_OVERRIDE_CAPACITY);
  assert(!settings::setAirportLabelMode("B000000", AirportLabelMode::SHOW));
  assert(settings::resetToDefaults());
  assert(settings::airportLabelOverrideCount(AirportLabelMode::SHOW) == 0);

  std::puts("Airport label override state tests passed");
  return 0;
}
'''


def main() -> None:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("No host C++ compiler available")

    with tempfile.TemporaryDirectory(prefix="airport-overrides-") as temp_name:
        temp = Path(temp_name)
        (temp / "Arduino.h").write_text(ARDUINO_H, encoding="utf-8")
        (temp / "Preferences.h").write_text(PREFERENCES_H, encoding="utf-8")
        (temp / "WiFi.h").write_text("#pragma once\n", encoding="utf-8")
        (temp / "config.h").write_text(
            '#pragma once\n#define WIFI_SSID ""\n#define WIFI_PASS ""\n'
            '#define HOME_LAT 0.0f\n#define HOME_LON 0.0f\n',
            encoding="utf-8",
        )
        (temp / "harness.cpp").write_text(HARNESS, encoding="utf-8")
        executable = temp / "airport_override_test"
        subprocess.run(
            [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer",
                f"-I{temp}",
                f"-I{ROOT / 'include'}",
                str(ROOT / "src" / "settings.cpp"),
                str(temp / "harness.cpp"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
