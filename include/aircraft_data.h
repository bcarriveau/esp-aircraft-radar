#pragma once

#include <stddef.h>
#include <stdint.h>

#include "aircraft_bitmap_types.h"

namespace aircraft {

constexpr uint8_t MAX_TARGETS = 100;

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
Category categoryForType(const char* typeCode);
Kind classify(const char* typeCode);
AircraftBitmapId bitmapForType(const char* typeCode);
const char* kindName(const char* typeCode);
const char* primaryIdentifier(const Target& target);

double haversineMiles(double lat1, double lon1, double lat2, double lon2);
double bearingDegrees(double lat1, double lon1, double lat2, double lon2);
const char* compassDirection(float bearing);
void formatWholeNumber(float value, char* out, size_t outSize);

}  // namespace aircraft
