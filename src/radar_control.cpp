#include "radar_control.h"

#include <math.h>

#include "adsb_network.h"
#include "app_state.h"
#include "radar_renderer.h"
#include "settings.h"

namespace radar_control {

bool setManualRangeMiles(float rangeMiles) {
  const bool supported = fabsf(rangeMiles - 20.0f) < 0.5f ||
                         fabsf(rangeMiles - 40.0f) < 0.5f ||
                         fabsf(rangeMiles - 80.0f) < 0.5f;
  if (!supported) return false;

  radar::clearAirportFocus();
  if (!app_state::setRadarRangeMiles(rangeMiles)) return false;

  const uint8_t savedRange = static_cast<uint8_t>(rangeMiles + 0.5f);
  if (!settings::setRadarRangeMiles(savedRange)) {
    Serial.printf(
        "WARNING: radar range changed to %u miles but NVS save failed\n",
        static_cast<unsigned>(savedRange));
  }

  adsb::requestRefresh();
  return true;
}

}  // namespace radar_control
