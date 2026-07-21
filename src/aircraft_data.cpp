#include "aircraft_data.h"

#include <Arduino.h>
#include <math.h>

namespace aircraft {
namespace {

double toRadians(double degrees) { return degrees * M_PI / 180.0; }

bool matchesAny(const char* typeCode, const char* const* values,
                size_t valueCount) {
  if (!typeCode) return false;
  for (size_t i = 0; i < valueCount; ++i) {
    if (strcmp(typeCode, values[i]) == 0) return true;
  }
  return false;
}

bool startsWithAny(const char* typeCode, const char* const* prefixes,
                   size_t prefixCount) {
  if (!typeCode) return false;
  for (size_t i = 0; i < prefixCount; ++i) {
    if (typeStartsWith(typeCode, prefixes[i])) return true;
  }
  return false;
}

}  // namespace

bool typeStartsWith(const char* typeCode, const char* prefix) {
  return typeCode && prefix && strncmp(typeCode, prefix, strlen(prefix)) == 0;
}

Category categoryForType(const char* typeCode) {
  if (!typeCode || !typeCode[0] || strcmp(typeCode, "Unknown") == 0 ||
      strcmp(typeCode, "UNKNOWN") == 0) {
    return Category::UNKNOWN;
  }

  // ICAO designators are intentionally explicit here. Broad checks such as
  // "C1", "BE", or "PC" misclassify C17, BE40, and PC24.
  static const char* const helicopterPrefixes[] = {
    "R22", "R44", "R66", "EC20", "EC25", "EC30", "EC35", "EC45",
    "EC55", "AS32", "AS35", "AS50", "AS55", "AS65", "B06", "B47",
    "B105", "B212", "B214", "B222", "B230", "B234", "B412", "S76",
    "S92", "AW09", "AW10", "AW11", "AW13", "AW16", "AW18", "MD5",
    "MD6", "H135", "H145", "BK17", "UH1", "UH60", "CH47", "CH53",
    "AH64", "KMAX", "CABR", "EN28"
  };
  if (startsWithAny(typeCode, helicopterPrefixes,
                    sizeof(helicopterPrefixes) / sizeof(helicopterPrefixes[0]))) {
    return Category::HELICOPTER;
  }

  static const char* const militaryExact[] = {
    "C17", "C5M", "C5", "C130", "C30J", "A400", "B1", "B2", "B52",
    "K35R", "KC135", "K46A", "E3TF", "E6", "E8", "P8", "A10", "F15",
    "F16", "F18", "F22", "F35", "V22", "RQ4"
  };
  if (matchesAny(typeCode, militaryExact,
                 sizeof(militaryExact) / sizeof(militaryExact[0]))) {
    return Category::MILITARY_HEAVY;
  }

  static const char* const businessExact[] = {
    "PC24", "BE40", "GLEX", "HDJT", "PRM1", "EA50", "SF50", "F2TH",
    "F7X", "F8X"
  };
  static const char* const businessPrefixes[] = {
    "GLF", "C25", "C50", "C51", "C52", "C55", "C56", "C65", "C68",
    "C70", "C75", "LJ", "CL3", "CL6", "FA", "F900", "H25", "E50",
    "E55", "E545", "E550", "E55P", "HS25"
  };
  if (matchesAny(typeCode, businessExact,
                 sizeof(businessExact) / sizeof(businessExact[0])) ||
      startsWithAny(typeCode, businessPrefixes,
                    sizeof(businessPrefixes) / sizeof(businessPrefixes[0]))) {
    return Category::BUSINESS_JET;
  }

  static const char* const turbopropPrefixes[] = {
    "DH8", "AT4", "AT7", "PC12", "PC6", "TBM", "BE20", "BE30", "B350",
    "B190", "SW4", "SF34", "C208", "P180", "E120", "JS32", "JS41",
    "D328", "AN12", "AN24", "AN26", "AN32", "AN38"
  };
  if (startsWithAny(typeCode, turbopropPrefixes,
                    sizeof(turbopropPrefixes) / sizeof(turbopropPrefixes[0]))) {
    return Category::TURBOPROP;
  }

  static const char* const pistonPrefixes[] = {
    "C15", "C17", "C18", "C20", "C21", "C30", "C31", "C32", "C33",
    "C34", "C40", "C41", "C42", "P28", "P32", "P46", "PA18", "PA20",
    "PA22", "PA24", "PA28", "PA30", "PA32", "PA34", "PA38", "BE23",
    "BE24", "BE33", "BE35", "BE36", "BE55", "BE58", "SR20", "SR22",
    "DA20", "DA40", "DA42", "M20", "AA5", "RV"
  };
  // C17 is already caught as an exact military designator; C172/C175 etc.
  // continue here as piston Cessnas.
  if (startsWithAny(typeCode, pistonPrefixes,
                    sizeof(pistonPrefixes) / sizeof(pistonPrefixes[0]))) {
    return Category::PISTON;
  }

  static const char* const airlinerPrefixes[] = {
    "A19", "A20", "A21", "A22", "A3", "B37", "B38", "B39", "B7",
    "E17", "E19", "CRJ", "BCS", "MD8", "MD9", "DC9", "DC10", "IL6",
    "IL7", "IL8", "TU1", "TU2", "SU9", "A124", "A225"
  };
  if (startsWithAny(typeCode, airlinerPrefixes,
                    sizeof(airlinerPrefixes) / sizeof(airlinerPrefixes[0]))) {
    return Category::AIRLINER;
  }
  return Category::UNKNOWN;
}

Kind classify(const char* typeCode) {
  switch (categoryForType(typeCode)) {
    case Category::HELICOPTER: return Kind::HELICOPTER;
    case Category::TURBOPROP:
    case Category::PISTON: return Kind::PROP;
    case Category::AIRLINER:
    case Category::BUSINESS_JET:
    case Category::MILITARY_HEAVY: return Kind::JET;
    default: return Kind::UNKNOWN;
  }
}

AircraftBitmapId bitmapForType(const char* typeCode) {
  switch (categoryForType(typeCode)) {
    case Category::AIRLINER:
    case Category::MILITARY_HEAVY: return AircraftBitmapId::AIRLINER;
    case Category::BUSINESS_JET: return AircraftBitmapId::BUSINESS_JET;
    case Category::TURBOPROP: return AircraftBitmapId::TURBOPROP;
    case Category::PISTON: return AircraftBitmapId::PISTON;
    case Category::HELICOPTER: return AircraftBitmapId::HELICOPTER;
    default: return AircraftBitmapId::UNKNOWN;
  }
}

const char* kindName(const char* typeCode) {
  switch (categoryForType(typeCode)) {
    case Category::AIRLINER: return "AIRLINER";
    case Category::BUSINESS_JET: return "BIZJET";
    case Category::MILITARY_HEAVY: return "MIL/HEAVY";
    case Category::TURBOPROP: return "TURBOPROP";
    case Category::PISTON: return "PISTON";
    case Category::HELICOPTER: return "HELI";
    default: return "UNKNOWN";
  }
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
  double dLat = toRadians(lat2 - lat1);
  double dLon = toRadians(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(toRadians(lat1)) * cos(toRadians(lat2)) *
             sin(dLon / 2) * sin(dLon / 2);
  return earthMiles * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

double bearingDegrees(double lat1, double lon1, double lat2, double lon2) {
  double y = sin(toRadians(lon2 - lon1)) * cos(toRadians(lat2));
  double x = cos(toRadians(lat1)) * sin(toRadians(lat2)) -
             sin(toRadians(lat1)) * cos(toRadians(lat2)) *
                 cos(toRadians(lon2 - lon1));
  double bearing = atan2(y, x) * 180.0 / M_PI;
  return fmod(bearing + 360.0, 360.0);
}

const char* compassDirection(float bearing) {
  static const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return directions[((int)((bearing + 22.5f) / 45.0f)) & 7];
}

void formatWholeNumber(float value, char* out, size_t outSize) {
  long whole = lroundf(max(value, 0.0f));
  if (whole >= 1000) {
    snprintf(out, outSize, "%ld,%03ld", whole / 1000, whole % 1000);
  } else {
    snprintf(out, outSize, "%ld", whole);
  }
}

}  // namespace aircraft
