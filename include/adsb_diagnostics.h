#pragma once

#include <stdint.h>

// Normal Product builds keep the 15-second ADS-B path quiet so Serial output
// cannot add avoidable scheduling stalls. Add
//   -DADSB_VERBOSE_FETCH_LOGGING=1
// to a local diagnostic build to restore per-stage transport and memory logs.
#ifndef ADSB_VERBOSE_FETCH_LOGGING
#define ADSB_VERBOSE_FETCH_LOGGING 0
#endif

namespace adsb_diagnostics {

constexpr uint16_t EXTRACTION_YIELD_INTERVAL = 16;

}  // namespace adsb_diagnostics
