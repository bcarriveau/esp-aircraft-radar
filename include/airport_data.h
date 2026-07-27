#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace airport_data {

constexpr uint8_t CATEGORY_COUNT = 4;
constexpr uint8_t RANGE_COUNT = 3;
constexpr uint8_t CATEGORY_MASK_ALL = (1U << CATEGORY_COUNT) - 1U;
constexpr float CACHE_RADIUS_MILES = 90.0f;
constexpr uint16_t MAX_NEARBY_AIRPORTS = 192;

enum class Category : uint8_t {
  MAJOR = 0,
  PUBLIC = 1,
  PRIVATE_FIELD = 2,
  HELIPORT = 3
};

struct NearbyAirport {
  char ident[8]{};
  char name[32]{};
  float distanceMiles = 0.0f;
  float bearingDegrees = 0.0f;
  int16_t elevationFt = 0;
  uint16_t runwayLengthFt = 0;
  uint16_t runwayHeadingDegrees = 0;
  Category category = Category::PUBLIC;
};

struct Status {
  bool ready = false;
  uint16_t databaseCount = 0;
  uint16_t cachedCount = 0;
  float centerLatitude = 0.0f;
  float centerLongitude = 0.0f;
};

// Airport data is an optional display subsystem. Failure disables only the
// airport overlay and never prevents the aircraft radar from starting.
bool initialize(float homeLatitude, float homeLongitude);
bool rebuild(float homeLatitude, float homeLongitude);
bool ready();
uint16_t cachedCount();
void copyStatus(Status& status);

// Copies nearby airports in increasing distance order. categoryMask uses one
// bit per Category value. A range <= 0 copies all cached entries.
uint16_t copyNearby(NearbyAirport* out, uint16_t capacity, float rangeMiles,
                    uint8_t categoryMask = CATEGORY_MASK_ALL);
uint16_t visibleCount(float rangeMiles, uint8_t categoryMask);

uint8_t categoryBit(Category category);
const char* categoryName(Category category);
uint8_t rangeIndex(float rangeMiles);
const char* databaseDate();
const char* databaseCoverage();

}  // namespace airport_data
