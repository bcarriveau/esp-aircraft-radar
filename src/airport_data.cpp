#include "airport_data.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

#include "generated_airport_database.h"

namespace airport_data {
namespace {

constexpr float EARTH_RADIUS_MILES = 3958.7613f;
constexpr uint16_t CATEGORY_LIMITS[CATEGORY_COUNT] = {32, 64, 48, 48};
static_assert(CATEGORY_LIMITS[0] + CATEGORY_LIMITS[1] + CATEGORY_LIMITS[2] +
                  CATEGORY_LIMITS[3] == MAX_NEARBY_AIRPORTS,
              "Airport category limits must match cache capacity");

NearbyAirport* nearbyAirports = nullptr;
uint16_t nearbyAirportCount = 0;
bool databaseReady = false;
float cacheLatitude = 0.0f;
float cacheLongitude = 0.0f;

float degreesToRadians(float degrees) {
  return degrees * (float)M_PI / 180.0f;
}

void distanceAndBearing(float fromLatitude, float fromLongitude,
                        float toLatitude, float toLongitude, float& distanceMiles,
                        float& bearingDegrees) {
  const float latitude1 = degreesToRadians(fromLatitude);
  const float latitude2 = degreesToRadians(toLatitude);
  const float deltaLatitude = degreesToRadians(toLatitude - fromLatitude);
  const float deltaLongitude = degreesToRadians(toLongitude - fromLongitude);
  const float sinHalfLatitude = sinf(deltaLatitude * 0.5f);
  const float sinHalfLongitude = sinf(deltaLongitude * 0.5f);
  const float haversine = sinHalfLatitude * sinHalfLatitude +
      cosf(latitude1) * cosf(latitude2) *
          sinHalfLongitude * sinHalfLongitude;
  const float centralAngle = 2.0f * atan2f(
      sqrtf(haversine), sqrtf(fmaxf(0.0f, 1.0f - haversine)));
  distanceMiles = EARTH_RADIUS_MILES * centralAngle;

  const float east = sinf(deltaLongitude) * cosf(latitude2);
  const float north = cosf(latitude1) * sinf(latitude2) -
      sinf(latitude1) * cosf(latitude2) * cosf(deltaLongitude);
  bearingDegrees = fmodf(atan2f(east, north) * 180.0f / (float)M_PI + 360.0f,
                         360.0f);
}

void readRecord(uint16_t index, generated_airports::Record& record) {
#if defined(ARDUINO_ARCH_ESP32)
  memcpy_P(&record, &generated_airports::RECORDS[index], sizeof(record));
#else
  memcpy(&record, &generated_airports::RECORDS[index], sizeof(record));
#endif
}

bool insertForCategory(const NearbyAirport& candidate,
                       uint16_t categoryCount[CATEGORY_COUNT]) {
  const uint8_t category = static_cast<uint8_t>(candidate.category);
  if (category >= CATEGORY_COUNT) return false;

  const uint16_t categoryLimit = CATEGORY_LIMITS[category];
  if (categoryCount[category] < categoryLimit &&
      nearbyAirportCount < MAX_NEARBY_AIRPORTS) {
    nearbyAirports[nearbyAirportCount++] = candidate;
    ++categoryCount[category];
    return true;
  }

  int farthestIndex = -1;
  float farthestDistance = -1.0f;
  for (uint16_t i = 0; i < nearbyAirportCount; ++i) {
    if (nearbyAirports[i].category != candidate.category) continue;
    if (nearbyAirports[i].distanceMiles > farthestDistance) {
      farthestDistance = nearbyAirports[i].distanceMiles;
      farthestIndex = i;
    }
  }
  if (farthestIndex < 0 || candidate.distanceMiles >= farthestDistance) {
    return false;
  }
  nearbyAirports[farthestIndex] = candidate;
  return true;
}

void sortByDistance() {
  for (uint16_t i = 1; i < nearbyAirportCount; ++i) {
    NearbyAirport candidate = nearbyAirports[i];
    uint16_t position = i;
    while (position > 0) {
      const NearbyAirport& previous = nearbyAirports[position - 1];
      const bool closer = candidate.distanceMiles < previous.distanceMiles;
      const bool stableTie = candidate.distanceMiles == previous.distanceMiles &&
          strcmp(candidate.ident, previous.ident) < 0;
      if (!closer && !stableTie) break;
      nearbyAirports[position] = previous;
      --position;
    }
    nearbyAirports[position] = candidate;
  }
}

}  // namespace

bool initialize(float homeLatitude, float homeLongitude) {
  if (!nearbyAirports) {
    nearbyAirports = static_cast<NearbyAirport*>(heap_caps_calloc(
        MAX_NEARBY_AIRPORTS, sizeof(NearbyAirport),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!nearbyAirports) {
    databaseReady = false;
    Serial.println("Airport overlay disabled: PSRAM cache allocation failed");
    return false;
  }
  Serial.printf("Airport cache in PSRAM: %u bytes\n",
                (unsigned)(MAX_NEARBY_AIRPORTS * sizeof(NearbyAirport)));
  return rebuild(homeLatitude, homeLongitude);
}

bool rebuild(float homeLatitude, float homeLongitude) {
  if (!nearbyAirports || !isfinite(homeLatitude) || !isfinite(homeLongitude) ||
      homeLatitude < -90.0f || homeLatitude > 90.0f ||
      homeLongitude < -180.0f || homeLongitude > 180.0f) {
    databaseReady = false;
    nearbyAirportCount = 0;
    return false;
  }

  nearbyAirportCount = 0;
  uint16_t categoryCount[CATEGORY_COUNT]{};
  const float latitudeSpan = CACHE_RADIUS_MILES / 69.0f + 0.1f;
  const float longitudeScale = fmaxf(0.05f, cosf(degreesToRadians(homeLatitude)));
  const float longitudeSpan =
      fminf(180.0f, CACHE_RADIUS_MILES / (69.0f * longitudeScale) + 0.1f);
  for (uint16_t i = 0; i < generated_airports::RECORD_COUNT; ++i) {
    generated_airports::Record record{};
    readRecord(i, record);
    const float latitude = record.latitudeE6 / 1000000.0f;
    const float longitude = record.longitudeE6 / 1000000.0f;

    // Cheap rectangular rejection before trigonometry. Longitude span expands
    // toward the poles so generated regional databases remain location-safe.
    if (fabsf(latitude - homeLatitude) > latitudeSpan ||
        fabsf(longitude - homeLongitude) > longitudeSpan) {
      continue;
    }

    NearbyAirport candidate{};
    strncpy(candidate.ident, record.ident, sizeof(candidate.ident) - 1);
    strncpy(candidate.name, record.name, sizeof(candidate.name) - 1);
    distanceAndBearing(homeLatitude, homeLongitude, latitude, longitude,
                       candidate.distanceMiles, candidate.bearingDegrees);
    if (candidate.distanceMiles > CACHE_RADIUS_MILES) continue;
    candidate.elevationFt = record.elevationFt;
    candidate.runwayLengthFt = record.runwayLengthFt;
    candidate.runwayHeadingDegrees = record.runwayHeadingDegrees;
    candidate.category = record.category < CATEGORY_COUNT
        ? static_cast<Category>(record.category)
        : Category::PUBLIC;
    insertForCategory(candidate, categoryCount);
  }

  sortByDistance();
  cacheLatitude = homeLatitude;
  cacheLongitude = homeLongitude;
  databaseReady = true;
  Serial.printf(
      "Airport database ready: %u records, %u within %.0f miles of %.5f,%.5f\n",
      (unsigned)generated_airports::RECORD_COUNT,
      (unsigned)nearbyAirportCount, CACHE_RADIUS_MILES,
      homeLatitude, homeLongitude);
  return true;
}

bool ready() { return databaseReady && nearbyAirports; }

uint16_t cachedCount() { return ready() ? nearbyAirportCount : 0; }

void copyStatus(Status& status) {
  status = Status{};
  status.ready = ready();
  status.databaseCount = generated_airports::RECORD_COUNT;
  status.cachedCount = cachedCount();
  status.centerLatitude = cacheLatitude;
  status.centerLongitude = cacheLongitude;
}

uint16_t copyNearby(NearbyAirport* out, uint16_t capacity, float rangeMiles,
                    uint8_t categoryMask) {
  if (!out || capacity == 0 || !ready()) return 0;
  uint16_t copied = 0;
  for (uint16_t i = 0; i < nearbyAirportCount && copied < capacity; ++i) {
    const NearbyAirport& airport = nearbyAirports[i];
    if ((categoryMask & categoryBit(airport.category)) == 0) continue;
    if (rangeMiles > 0.0f && airport.distanceMiles > rangeMiles) continue;
    out[copied++] = airport;
  }
  return copied;
}

uint16_t visibleCount(float rangeMiles, uint8_t categoryMask) {
  if (!ready()) return 0;
  uint16_t count = 0;
  for (uint16_t i = 0; i < nearbyAirportCount; ++i) {
    const NearbyAirport& airport = nearbyAirports[i];
    if (airport.distanceMiles > rangeMiles) continue;
    if ((categoryMask & categoryBit(airport.category)) == 0) continue;
    ++count;
  }
  return count;
}

uint8_t categoryBit(Category category) {
  const uint8_t value = static_cast<uint8_t>(category);
  return value < CATEGORY_COUNT ? static_cast<uint8_t>(1U << value) : 0;
}

const char* categoryName(Category category) {
  switch (category) {
    case Category::MAJOR: return "MAJOR";
    case Category::PUBLIC: return "PUBLIC";
    case Category::PRIVATE_FIELD: return "PRIVATE";
    case Category::HELIPORT: return "HELIPORT";
    default: return "UNKNOWN";
  }
}

uint8_t rangeIndex(float rangeMiles) {
  if (rangeMiles <= 20.1f) return 0;
  if (rangeMiles <= 40.1f) return 1;
  return 2;
}

const char* databaseDate() { return generated_airports::DATABASE_DATE; }

const char* databaseCoverage() { return generated_airports::DATABASE_COVERAGE; }

}  // namespace airport_data
