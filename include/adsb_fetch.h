#pragma once

#include <Arduino.h>

#include "aircraft_data.h"
#include "app_state.h"

namespace adsb_fetch {
// Fetches and parses one ADS-B snapshot; publication stays with networking.

struct Result {
  bool success = false;
  app_state::FetchFailureStage failureStage =
      app_state::FetchFailureStage::NONE;
  uint32_t durationMs = 0;
  uint32_t responseBytes = 0;
  uint32_t requestGeneration = 0;
  uint16_t receivedCount = 0;
  uint16_t eligibleCount = 0;
  uint8_t acceptedCount = 0;
  uint16_t capacityDroppedCount = 0;
  float requestedRangeMiles = 0;
};

Result fetchAircraft(aircraft::Target* out);

}  // namespace adsb_fetch
