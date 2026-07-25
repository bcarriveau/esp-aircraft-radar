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

// Description keywords live in compact NUL-separated tables. This avoids a
// RAM-resident pointer for every rule while keeping the full reference chart
// in flash/rodata. More-specific categories are checked before broad classes.
// Helicopters and rotorcraft.
static constexpr char HELICOPTER_DESCRIPTION_KEYWORDS[] =
    "ROBINSONR22\0"
    "ROBINSONR44\0"
    "ROBINSONR66\0"
    "BELL47\0"
    "BELL206\0"
    "BELL212\0"
    "BELL214\0"
    "BELL222\0"
    "BELL230\0"
    "BELL234\0"
    "BELL407\0"
    "BELL412\0"
    "BELL429\0"
    "BELL505\0"
    "AIRBUSHELICOPTERS\0"
    "EUROCOPTER\0"
    "EC120\0"
    "EC130\0"
    "EC135\0"
    "EC145\0"
    "EC155\0"
    "EC225\0"
    "AS350\0"
    "AS355\0"
    "AS365\0"
    "BO105\0"
    "BK117\0"
    "SIKORSKY\0"
    "AGUSTAWESTLAND\0"
    "LEONARDOAW\0"
    "MDHELICOPTERS\0"
    "HUGHES269\0"
    "HUGHES369\0"
    "ENSTROM\0"
    "CABRIG2\0"
    "KMAX\0"
    "BLACKHAWK\0"
    "CHINOOK\0"
    "HELICOPTER\0"
    "ROTORCRAFT\0"
    "AH64\0"
    "BELLHELICOPTER\0";

// Explicit military aircraft.
static constexpr char MILITARY_HEAVY_DESCRIPTION_KEYWORDS[] =
    "GLOBEMASTER\0"
    "C130HERCULES\0"
    "A400MATLAS\0"
    "KC135\0"
    "KC46\0"
    "E3SENTRY\0"
    "E6MERCURY\0"
    "E8JSTARS\0"
    "POSEIDON\0"
    "B1LANCER\0"
    "B2SPIRIT\0"
    "B52STRATOFORTRESS\0"
    "A10THUNDERBOLT\0"
    "F15EAGLE\0"
    "F16FIGHTINGFALCON\0"
    "SUPERHORNET\0"
    "F22RAPTOR\0"
    "LIGHTNINGII\0"
    "V22OSPREY\0"
    "RQ4GLOBALHAWK\0"
    "MILITARYTRANSPORT\0"
    "FIGHTERAIRCRAFT\0"
    "LOCKHEEDC5\0"
    "SUPERGALAXY\0"
    "C17\0"
    "C130\0"
    "A400M\0"
    "P8A\0"
    "B1B\0"
    "B2A\0"
    "B52\0"
    "A10\0"
    "F15\0"
    "F16\0"
    "FA18\0"
    "F18\0"
    "F22\0"
    "F35\0"
    "V22\0"
    "RQ4\0";

// Business and corporate jets.
static constexpr char BUSINESS_JET_DESCRIPTION_KEYWORDS[] =
    "CESSNACITATION\0"
    "CITATIONJET\0"
    "GULFSTREAM\0"
    "LEARJET\0"
    "BOMBARDIERCHALLENGER\0"
    "CANADAIRCHALLENGER\0"
    "BOMBARDIERGLOBAL\0"
    "GLOBALEXPRESS\0"
    "DASSAULTFALCON\0"
    "EMBRAERPHENOM\0"
    "EMBRAERLEGACY\0"
    "EMBRAERPRAETOR\0"
    "HONDAJET\0"
    "PILATUSPC24\0"
    "HAWKER800\0"
    "HAWKER900\0"
    "BEECHJET\0"
    "PREMIER1\0"
    "ECLIPSE500\0"
    "ECLIPSE550\0"
    "CIRRUSVISION\0"
    "VISIONJET\0"
    "SABRELINER\0"
    "ISRAELWESTWIND\0"
    "BUSINESSJET\0"
    "CORPORATEJET\0"
    "HAWKER400\0"
    "HAWKER750\0"
    "HAWKER1000\0"
    "CITATION\0"
    "PHENOM\0"
    "PC24\0"
    "FALCON7X\0"
    "FALCON8X\0"
    "FALCON900\0"
    "FALCON2000\0"
    "BD700\0"
    "GLOBAL5000\0"
    "GLOBAL6000\0"
    "GLOBAL6500\0"
    "GLOBAL7500\0"
    "LEGACY450\0"
    "LEGACY500\0"
    "LEGACY600\0"
    "LEGACY650\0"
    "PRAETOR500\0"
    "PRAETOR600\0";

// Turboprops.
static constexpr char TURBOPROP_DESCRIPTION_KEYWORDS[] =
    "BEECHCRAFTKINGAIR\0"
    "BEECHKINGAIR\0"
    "KINGAIR\0"
    "PILATUSPC12\0"
    "PILATUSPC6\0"
    "SOCATATBM\0"
    "DAHERTBM\0"
    "DEHAVILLANDDASH8\0"
    "BOMBARDIERDASH8\0"
    "ATR42\0"
    "ATR72\0"
    "SAAB340\0"
    "SAAB2000\0"
    "CESSNACARAVAN\0"
    "GRANDCARAVAN\0"
    "BEECH1900\0"
    "FAIRCHILDMETRO\0"
    "METROLINER\0"
    "PIAGGIOP180\0"
    "PIAGGIOAVANTI\0"
    "EMBRAER120\0"
    "FOKKER27\0"
    "FOKKER50\0"
    "JETSTREAM31\0"
    "JETSTREAM32\0"
    "JETSTREAM41\0"
    "TWINOTTER\0"
    "LET410\0"
    "CASA212\0"
    "CASA235\0"
    "CASA295\0"
    "C27JSPARTAN\0"
    "ANTONOVAN12\0"
    "ANTONOVAN24\0"
    "ANTONOVAN26\0"
    "ANTONOVAN32\0"
    "ANTONOVAN38\0"
    "TURBOPROP\0"
    "DORNIER328100\0"
    "PC12\0"
    "PC6\0"
    "DHC8\0"
    "DHC6\0"
    "CESSNA208\0"
    "EMB120\0"
    "BRASILIA\0";

// Piston-powered general aviation aircraft.
static constexpr char PISTON_DESCRIPTION_KEYWORDS[] =
    "CESSNA120\0"
    "CESSNA140\0"
    "CESSNA150\0"
    "CESSNA152\0"
    "CESSNA170\0"
    "CESSNA172\0"
    "CESSNA175\0"
    "CESSNA177\0"
    "CESSNA180\0"
    "CESSNA182\0"
    "CESSNA185\0"
    "CESSNA195\0"
    "CESSNA205\0"
    "CESSNA206\0"
    "CESSNA207\0"
    "CESSNA210\0"
    "CESSNA310\0"
    "CESSNA320\0"
    "CESSNA335\0"
    "CESSNA337\0"
    "CESSNA340\0"
    "CESSNA401\0"
    "CESSNA402\0"
    "CESSNA404\0"
    "CESSNA411\0"
    "CESSNA414\0"
    "CESSNA421\0"
    "SKYHAWK\0"
    "SKYLANE\0"
    "PIPERCHEROKEE\0"
    "PIPERARCHER\0"
    "PIPERWARRIOR\0"
    "PIPERARROW\0"
    "PIPERSENECA\0"
    "PIPERSEMINOLE\0"
    "PIPERPA18\0"
    "PIPERPA23\0"
    "PIPERPA28\0"
    "PIPERPA32\0"
    "PIPERPA34\0"
    "PIPERPA38\0"
    "CIRRUSSR20\0"
    "CIRRUSSR22\0"
    "DIAMONDDA20\0"
    "DIAMONDDA40\0"
    "DIAMONDDA42\0"
    "DIAMONDDA62\0"
    "BEECHBONANZA\0"
    "BEECHCRAFTG36\0"
    "BEECHBARON\0"
    "BEECHMUSKETEER\0"
    "BEECHSUNDOWNER\0"
    "BEECHSIERRA\0"
    "MOONEY\0"
    "VANSRV\0"
    "GRUMMANAA5\0"
    "AMERICANCHAMPION\0"
    "AVIATHUSKY\0"
    "MAULE\0"
    "LANCAIR\0"
    "COLUMBIA300\0"
    "COLUMBIA350\0"
    "COLUMBIA400\0"
    "TECNAM\0"
    "SOCATATOBAGO\0"
    "SOCATATRINIDAD\0"
    "COMMANDER112\0"
    "COMMANDER114\0"
    "EXTRA300\0"
    "PITTS\0"
    "DECATHLON\0"
    "SUPERDECATHLON\0"
    "SUPERCUB\0"
    "PISTON\0"
    "SR20\0"
    "SR22\0"
    "DA20\0"
    "DA40\0"
    "DA42\0"
    "DA62\0"
    "BONANZA\0"
    "BARON\0"
    "PA23\0"
    "PA28\0"
    "PA32\0"
    "PA34\0"
    "PA38\0";

// Airliners and regional jets.
static constexpr char AIRLINER_DESCRIPTION_KEYWORDS[] =
    "BOEING737\0"
    "BOEING747\0"
    "BOEING757\0"
    "BOEING767\0"
    "BOEING777\0"
    "BOEING787\0"
    "AIRBUSA220\0"
    "AIRBUSA300\0"
    "AIRBUSA310\0"
    "AIRBUSA318\0"
    "AIRBUSA319\0"
    "AIRBUSA320\0"
    "AIRBUSA321\0"
    "AIRBUSA330\0"
    "AIRBUSA340\0"
    "AIRBUSA350\0"
    "AIRBUSA380\0"
    "EMBRAER170\0"
    "EMBRAER175\0"
    "EMBRAER190\0"
    "EMBRAER195\0"
    "EMBRAEREJET\0"
    "CANADAIRREGIONALJET\0"
    "BOMBARDIERCRJ\0"
    "BOMBARDIERCSERIES\0"
    "MCDONNELDOUGLAS\0"
    "LOCKHEEDL1011\0"
    "FOKKER70\0"
    "FOKKER100\0"
    "SUKHOISUPERJET\0"
    "COMACARJ21\0"
    "COMACC919\0"
    "ANTONOVAN124\0"
    "ANTONOVAN225\0"
    "AIRLINER\0"
    "DORNIER328JET\0"
    "DORNIER328300\0"
    "ERJ170\0"
    "ERJ175\0"
    "ERJ190\0"
    "ERJ195\0"
    "E170\0"
    "E175\0"
    "E190\0"
    "E195\0"
    "CRJ100\0"
    "CRJ200\0"
    "CRJ700\0"
    "CRJ900\0"
    "CRJ1000\0"
    "A220\0"
    "A300\0"
    "A310\0"
    "A318\0"
    "A319\0"
    "A320\0"
    "A321\0"
    "A330\0"
    "A340\0"
    "A350\0"
    "A380\0"
    "MD80\0"
    "MD90\0"
    "DC9\0"
    "DC10\0"
    "SUPERJET\0"
    "ARJ21\0"
    "C919\0";

char asciiUpper(char value) {
  return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A')
                                      : value;
}

bool isAsciiAlphaNumeric(char value) {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9');
}

void normalizeDescription(const char* description, char* normalized,
                          size_t normalizedSize) {
  if (!normalized || normalizedSize == 0) return;
  normalized[0] = 0;
  if (!description) return;

  size_t writeIndex = 0;
  for (size_t readIndex = 0;
       description[readIndex] && writeIndex + 1 < normalizedSize;
       ++readIndex) {
    const char value = description[readIndex];
    if (!isAsciiAlphaNumeric(value)) continue;
    normalized[writeIndex++] = asciiUpper(value);
  }
  normalized[writeIndex] = 0;
}

bool matchesNormalizedKeywordList(const char* normalized,
                                  const char* keywordList) {
  if (!normalized || !normalized[0] || !keywordList) return false;
  for (const char* keyword = keywordList; keyword[0];
       keyword += strlen(keyword) + 1) {
    if (strstr(normalized, keyword)) return true;
  }
  return false;
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

Category categoryForDescription(const char* description) {
  char normalized[64];
  normalizeDescription(description, normalized, sizeof(normalized));
  if (!normalized[0] || strcmp(normalized, "UNKNOWN") == 0) {
    return Category::UNKNOWN;
  }

  if (matchesNormalizedKeywordList(normalized,
                                   HELICOPTER_DESCRIPTION_KEYWORDS)) {
    return Category::HELICOPTER;
  }
  if (matchesNormalizedKeywordList(normalized,
                                   MILITARY_HEAVY_DESCRIPTION_KEYWORDS)) {
    return Category::MILITARY_HEAVY;
  }
  if (matchesNormalizedKeywordList(normalized,
                                   BUSINESS_JET_DESCRIPTION_KEYWORDS)) {
    return Category::BUSINESS_JET;
  }
  if (matchesNormalizedKeywordList(normalized,
                                   TURBOPROP_DESCRIPTION_KEYWORDS)) {
    return Category::TURBOPROP;
  }
  if (matchesNormalizedKeywordList(normalized,
                                   PISTON_DESCRIPTION_KEYWORDS)) {
    return Category::PISTON;
  }
  if (matchesNormalizedKeywordList(normalized,
                                   AIRLINER_DESCRIPTION_KEYWORDS)) {
    return Category::AIRLINER;
  }
  return Category::UNKNOWN;
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
