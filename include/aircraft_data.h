#pragma once

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include "aircraft_bitmap_types.h"

namespace aircraft {

constexpr size_t MAX_TARGETS = 200;
static_assert(MAX_TARGETS > 0, "MAX_TARGETS must be positive");
static_assert(MAX_TARGETS <= 255,
              "MAX_TARGETS exceeds uint8_t count/index capacity");

struct Target {
  char id[10]{};
  char hex[7]{};
  char typeCode[9]{};
  char registration[12]{};
  char operatorName[24]{};
  char description[28]{};
  float distanceMiles = 0;
  float bearing = 0;
  float altitudeFt = 0;
  float speedKt = 0;
  float track = 0;
  float verticalRateFpm = 0;
  bool hasTrack = false;
  bool valid = false;
};

enum class Kind : uint8_t { JET, PROP, HELICOPTER, UNKNOWN };

enum class Category : uint8_t {
  AIRLINER,
  BUSINESS_JET,
  MILITARY_HEAVY,
  TURBOPROP,
  PISTON,
  HELICOPTER,
  UNKNOWN
};

bool typeStartsWith(const char* typeCode, const char* prefix);
Category categoryForTypeCode(const char* typeCode);
Category categoryForDescription(const char* description);
Category categoryForTarget(const Target& target);
Kind classify(const char* typeCode);
AircraftBitmapId bitmapForTypeCode(const char* typeCode);
AircraftBitmapId bitmapForTarget(const Target& target);
const char* kindNameForTypeCode(const char* typeCode);
const char* kindName(const Target& target);
const char* primaryIdentifier(const Target& target);

namespace detail {

template <typename T>
struct IsTypeCodeInput {
  using Raw = typename std::remove_reference<T>::type;
  using Element = typename std::remove_cv<
      typename std::remove_extent<Raw>::type>::type;
  static constexpr bool value =
      (std::is_array<Raw>::value && std::is_same<Element, char>::value) ||
      std::is_convertible<T, const char*>::value;
};

template <typename T>
const Target& targetFromTypeCodeMember(T&& typeCode) {
  using Raw = typename std::remove_reference<T>::type;
  static_assert(std::is_array<Raw>::value,
                "Target type-code dispatch requires an array member");
  static_assert(std::extent<Raw>::value == sizeof(Target::typeCode),
                "Unexpected aircraft type-code buffer size");
  static_assert(std::is_standard_layout<Target>::value,
                "Target must remain standard-layout for member dispatch");
  const auto* memberBytes = reinterpret_cast<const uint8_t*>(&typeCode[0]);
  const auto* targetBytes = memberBytes - offsetof(Target, typeCode);
  return *reinterpret_cast<const Target*>(targetBytes);
}

}  // namespace detail

// Existing call sites pass Target::typeCode directly. These templates keep
// plain C-string calls type-code-only, while member-array calls can use the
// target's stored description as a fallback without changing Target size or
// touching UI/radar source files.
template <typename T,
          typename std::enable_if<detail::IsTypeCodeInput<T>::value, int>::type = 0>
Category categoryForType(T&& typeCode) {
  using Raw = typename std::remove_reference<T>::type;
  if constexpr (std::is_array<Raw>::value &&
                std::extent<Raw>::value == sizeof(Target::typeCode)) {
    return categoryForTarget(
        detail::targetFromTypeCodeMember(static_cast<T&&>(typeCode)));
  } else {
    const char* code = typeCode;
    return categoryForTypeCode(code);
  }
}

template <typename T,
          typename std::enable_if<detail::IsTypeCodeInput<T>::value, int>::type = 0>
AircraftBitmapId bitmapForType(T&& typeCode) {
  using Raw = typename std::remove_reference<T>::type;
  if constexpr (std::is_array<Raw>::value &&
                std::extent<Raw>::value == sizeof(Target::typeCode)) {
    return bitmapForTarget(
        detail::targetFromTypeCodeMember(static_cast<T&&>(typeCode)));
  } else {
    const char* code = typeCode;
    return bitmapForTypeCode(code);
  }
}

template <typename T,
          typename std::enable_if<detail::IsTypeCodeInput<T>::value, int>::type = 0>
const char* kindName(T&& typeCode) {
  using Raw = typename std::remove_reference<T>::type;
  if constexpr (std::is_array<Raw>::value &&
                std::extent<Raw>::value == sizeof(Target::typeCode)) {
    return kindName(
        detail::targetFromTypeCodeMember(static_cast<T&&>(typeCode)));
  } else {
    const char* code = typeCode;
    return kindNameForTypeCode(code);
  }
}

double haversineMiles(double lat1, double lon1, double lat2, double lon2);
double bearingDegrees(double lat1, double lon1, double lat2, double lon2);
const char* compassDirection(float bearing);
void formatWholeNumber(float value, char* out, size_t outSize);

}  // namespace aircraft
