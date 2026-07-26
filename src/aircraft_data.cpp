#include "aircraft_data.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "generated/aircraft_type_database.h"

namespace aircraft {
namespace {

constexpr uint64_t FNV1A64_OFFSET = UINT64_C(0xCBF29CE484222325);
constexpr uint64_t FNV1A64_PRIME = UINT64_C(0x100000001B3);
constexpr size_t MAX_NORMALIZED_DESCRIPTION = 63;
constexpr size_t MAX_DESCRIPTION_BOUNDARIES = 16;

double toRadians(double degrees) { return degrees * M_PI / 180.0; }

char asciiUpper(char value) {
  return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A')
                                      : value;
}

bool isAsciiAlphaNumeric(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9');
}

uint64_t packDesignator(const char* typeCode) {
  if (!typeCode || !typeCode[0]) return 0;

  char normalized[9]{};
  size_t length = 0;
  for (size_t i = 0; typeCode[i]; ++i) {
    if (!isAsciiAlphaNumeric(typeCode[i])) continue;
    if (length >= sizeof(normalized) - 1) return 0;
    normalized[length++] = asciiUpper(typeCode[i]);
  }
  if (length == 0) return 0;

  uint64_t packed = 0;
  for (size_t i = 0; i < length; ++i) {
    packed = (packed << 8) | static_cast<uint8_t>(normalized[i]);
  }
  for (size_t i = length; i < 8; ++i) packed <<= 8;
  return packed;
}

Category categoryFromGeneratedValue(uint8_t value) {
  return value <= static_cast<uint8_t>(Category::UNKNOWN)
             ? static_cast<Category>(value)
             : Category::UNKNOWN;
}

Category lookupTypeCode(uint64_t packedCode) {
  if (packedCode == 0) return Category::UNKNOWN;

  size_t low = 0;
  size_t high = generated::kTypeRecordCount;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const uint64_t candidate = generated::kTypeCodes[middle];
    if (candidate < packedCode) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  if (low >= generated::kTypeRecordCount ||
      generated::kTypeCodes[low] != packedCode) {
    return Category::UNKNOWN;
  }
  return categoryFromGeneratedValue(generated::kTypeCategories[low]);
}

uint64_t fnv1a64(const char* text, size_t length) {
  uint64_t value = FNV1A64_OFFSET;
  for (size_t i = 0; i < length; ++i) {
    value ^= static_cast<uint8_t>(text[i]);
    value *= FNV1A64_PRIME;
  }
  return value;
}

Category lookupDescriptionHash(uint64_t hash) {
  size_t low = 0;
  size_t high = generated::kDescriptionAliasCount;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const uint64_t candidate = generated::kDescriptionAliasHashes[middle];
    if (candidate < hash) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  if (low >= generated::kDescriptionAliasCount ||
      generated::kDescriptionAliasHashes[low] != hash) {
    return Category::UNKNOWN;
  }
  return categoryFromGeneratedValue(
      generated::kDescriptionAliasCategories[low]);
}

size_t normalizeDescription(const char* description, char* normalized,
                            size_t normalizedSize, uint8_t* boundaries,
                            size_t boundaryCapacity, size_t& boundaryCount) {
  boundaryCount = 0;
  if (!normalized || normalizedSize == 0) return 0;
  normalized[0] = 0;
  if (!description) return 0;

  size_t writeIndex = 0;
  bool inToken = false;
  for (size_t readIndex = 0;
       description[readIndex] && writeIndex + 1 < normalizedSize;
       ++readIndex) {
    const char value = description[readIndex];
    if (isAsciiAlphaNumeric(value)) {
      normalized[writeIndex++] = asciiUpper(value);
      inToken = true;
      continue;
    }
    if (inToken && boundaryCount < boundaryCapacity && writeIndex <= UINT8_MAX) {
      boundaries[boundaryCount++] = static_cast<uint8_t>(writeIndex);
    }
    inToken = false;
  }
  if (inToken && boundaryCount < boundaryCapacity && writeIndex <= UINT8_MAX) {
    boundaries[boundaryCount++] = static_cast<uint8_t>(writeIndex);
  }
  normalized[writeIndex] = 0;
  return writeIndex;
}

bool containsAny(const char* normalized, const char* const* keywords,
                 size_t keywordCount) {
  if (!normalized || !normalized[0]) return false;
  for (size_t i = 0; i < keywordCount; ++i) {
    if (strstr(normalized, keywords[i])) return true;
  }
  return false;
}

Category strongDescriptionFallback(const char* normalized) {
  static const char* const helicopterKeywords[] = {
    "HELICOPTER", "ROTORCRAFT", "ROBINSONR22", "ROBINSONR44",
    "ROBINSONR66", "BELL206", "BELL407", "SIKORSKY", "BLACKHAWK",
    "CHINOOK", "APACHE"
  };
  if (containsAny(normalized, helicopterKeywords,
                  sizeof(helicopterKeywords) / sizeof(helicopterKeywords[0]))) {
    return Category::HELICOPTER;
  }

  static const char* const militaryKeywords[] = {
    "GLOBEMASTER", "STRATOFORTRESS", "STRATOTANKER", "SUPERGALAXY",
    "FIGHTINGFALCON", "LIGHTNINGII", "GLOBALHAWK", "MILITARYTRANSPORT",
    "FIGHTERAIRCRAFT", "A400MATLAS", "C130HERCULES"
  };
  if (containsAny(normalized, militaryKeywords,
                  sizeof(militaryKeywords) / sizeof(militaryKeywords[0]))) {
    return Category::MILITARY_HEAVY;
  }

  static const char* const businessKeywords[] = {
    "BUSINESSJET", "CORPORATEJET", "CESSNACITATION", "CITATIONJET",
    "GULFSTREAM", "LEARJET", "DASSAULTFALCON", "BOMBARDIERCHALLENGER",
    "BOMBARDIERGLOBAL", "EMBRAERPHENOM", "EMBRAERLEGACY",
    "EMBRAERPRAETOR", "HONDAJET", "PILATUSPC24"
  };
  if (containsAny(normalized, businessKeywords,
                  sizeof(businessKeywords) / sizeof(businessKeywords[0]))) {
    return Category::BUSINESS_JET;
  }

  static const char* const turbopropKeywords[] = {
    "TURBOPROP", "KINGAIR", "PILATUSPC12", "PILATUSPC6", "TBM",
    "DASH8", "ATR42", "ATR72", "CESSNACARAVAN", "BEECH1900",
    "METROLINER", "PIAGGIOAVANTI", "TWINOTTER"
  };
  if (containsAny(normalized, turbopropKeywords,
                  sizeof(turbopropKeywords) / sizeof(turbopropKeywords[0]))) {
    return Category::TURBOPROP;
  }

  static const char* const pistonKeywords[] = {
    "PISTON", "SKYHAWK", "SKYLANE", "PIPERCHEROKEE", "PIPERARCHER",
    "PIPERWARRIOR", "PIPERARROW", "BONANZA", "CIRRUSSR20",
    "CIRRUSSR22", "MOONEY", "SUPERCUB"
  };
  if (containsAny(normalized, pistonKeywords,
                  sizeof(pistonKeywords) / sizeof(pistonKeywords[0]))) {
    return Category::PISTON;
  }

  static const char* const airlinerKeywords[] = {
    "AIRLINER", "BOEING737", "BOEING747", "BOEING757", "BOEING767",
    "BOEING777", "BOEING787", "AIRBUSA220", "AIRBUSA300",
    "AIRBUSA310", "AIRBUSA318", "AIRBUSA319", "AIRBUSA320",
    "AIRBUSA321", "AIRBUSA330", "AIRBUSA340", "AIRBUSA350",
    "AIRBUSA380", "BOMBARDIERCRJ", "EMBRAEREJET", "SUKHOISUPERJET"
  };
  if (containsAny(normalized, airlinerKeywords,
                  sizeof(airlinerKeywords) / sizeof(airlinerKeywords[0]))) {
    return Category::AIRLINER;
  }

  return Category::UNKNOWN;
}

AircraftBitmapId bitmapForCategory(Category category) {
  switch (category) {
    case Category::AIRLINER:
    case Category::MILITARY_HEAVY: return AircraftBitmapId::AIRLINER;
    case Category::BUSINESS_JET: return AircraftBitmapId::BUSINESS_JET;
    case Category::TURBOPROP: return AircraftBitmapId::TURBOPROP;
    case Category::PISTON: return AircraftBitmapId::PISTON;
    case Category::HELICOPTER: return AircraftBitmapId::HELICOPTER;
    default: return AircraftBitmapId::UNKNOWN;
  }
}

const char* kindNameForCategory(Category category) {
  switch (category) {
    case Category::AIRLINER: return "AIRLINER";
    case Category::BUSINESS_JET: return "BIZJET";
    case Category::MILITARY_HEAVY: return "MIL/HEAVY";
    case Category::TURBOPROP: return "TURBOPROP";
    case Category::PISTON: return "PISTON";
    case Category::HELICOPTER: return "HELI";
    default: return "UNKNOWN";
  }
}

}  // namespace

bool typeStartsWith(const char* typeCode, const char* prefix) {
  return typeCode && prefix && strncmp(typeCode, prefix, strlen(prefix)) == 0;
}

Category categoryForTypeCode(const char* typeCode) {
  return lookupTypeCode(packDesignator(typeCode));
}

Category categoryForDescription(const char* description) {
  char normalized[MAX_NORMALIZED_DESCRIPTION + 1];
  uint8_t boundaries[MAX_DESCRIPTION_BOUNDARIES]{};
  size_t boundaryCount = 0;
  const size_t length = normalizeDescription(
      description, normalized, sizeof(normalized), boundaries,
      sizeof(boundaries) / sizeof(boundaries[0]), boundaryCount);
  if (length == 0 || strcmp(normalized, "UNKNOWN") == 0) {
    return Category::UNKNOWN;
  }

  Category category = lookupDescriptionHash(fnv1a64(normalized, length));
  if (category != Category::UNKNOWN) return category;

  // Descriptions commonly append a variant or marketing name. Check only token
  // boundaries, longest first, so C170 wins before C17 and C190 before C19.
  while (boundaryCount > 0) {
    const size_t prefixLength = boundaries[--boundaryCount];
    if (prefixLength < length && prefixLength >= 3) {
      category = lookupDescriptionHash(fnv1a64(normalized, prefixLength));
      if (category != Category::UNKNOWN) return category;
    }

    // Registration databases sometimes append one or two variant letters to
    // the model number (for example, CESSNA 190A). Check only three bounded
    // trims at each token boundary, longest first.
    for (size_t trim = 1; trim <= 3 && prefixLength > trim; ++trim) {
      const size_t trimmedLength = prefixLength - trim;
      if (trimmedLength < 4) break;
      category = lookupDescriptionHash(fnv1a64(normalized, trimmedLength));
      if (category != Category::UNKNOWN) return category;
    }
  }

  return strongDescriptionFallback(normalized);
}

Category categoryForTarget(const Target& target) {
  const Category typeCategory = categoryForTypeCode(target.typeCode);
  return typeCategory != Category::UNKNOWN
             ? typeCategory
             : categoryForDescription(target.description);
}

Kind classify(const char* typeCode) {
  switch (categoryForTypeCode(typeCode)) {
    case Category::HELICOPTER: return Kind::HELICOPTER;
    case Category::TURBOPROP:
    case Category::PISTON: return Kind::PROP;
    case Category::AIRLINER:
    case Category::BUSINESS_JET:
    case Category::MILITARY_HEAVY: return Kind::JET;
    default: return Kind::UNKNOWN;
  }
}

AircraftBitmapId bitmapForTypeCode(const char* typeCode) {
  return bitmapForCategory(categoryForTypeCode(typeCode));
}

AircraftBitmapId bitmapForTarget(const Target& target) {
  return bitmapForCategory(categoryForTarget(target));
}

const char* kindNameForTypeCode(const char* typeCode) {
  return kindNameForCategory(categoryForTypeCode(typeCode));
}

const char* kindName(const Target& target) {
  return kindNameForCategory(categoryForTarget(target));
}

const char* primaryIdentifier(const Target& target) {
  if (target.id[0] && strcmp(target.id, "UNKNOWN") != 0) return target.id;
  if (target.registration[0] && strcmp(target.registration, "Unknown") != 0) {
    return target.registration;
  }
  return target.hex[0] ? target.hex : "UNKNOWN";
}

double haversineMiles(double lat1, double lon1, double lat2, double lon2) {
  constexpr double earthMiles = 3958.7613;
  const double dLat = toRadians(lat2 - lat1);
  const double dLon = toRadians(lon2 - lon1);
  const double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(toRadians(lat1)) * cos(toRadians(lat2)) *
                       sin(dLon / 2) * sin(dLon / 2);
  return earthMiles * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

double bearingDegrees(double lat1, double lon1, double lat2, double lon2) {
  const double y = sin(toRadians(lon2 - lon1)) * cos(toRadians(lat2));
  const double x = cos(toRadians(lat1)) * sin(toRadians(lat2)) -
                   sin(toRadians(lat1)) * cos(toRadians(lat2)) *
                       cos(toRadians(lon2 - lon1));
  const double bearing = atan2(y, x) * 180.0 / M_PI;
  return fmod(bearing + 360.0, 360.0);
}

const char* compassDirection(float bearing) {
  static const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return directions[((int)((bearing + 22.5f) / 45.0f)) & 7];
}

void formatWholeNumber(float value, char* out, size_t outSize) {
  const float nonNegative = value > 0.0f ? value : 0.0f;
  const long whole = lroundf(nonNegative);
  if (whole >= 1000) {
    snprintf(out, outSize, "%ld,%03ld", whole / 1000, whole % 1000);
  } else {
    snprintf(out, outSize, "%ld", whole);
  }
}

}  // namespace aircraft
