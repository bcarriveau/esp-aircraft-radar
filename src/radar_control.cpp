#include "radar_control.h"

#include <math.h>

#include "adsb_network.h"
#include "app_state.h"
#include "radar_renderer.h"

namespace radar_control {

bool setManualRangeMiles(float rangeMiles) {
  const bool supported = fabsf(rangeMiles - 20.0f) < 0.5f ||
                         fabsf(rangeMiles - 40.0f) < 0.5f ||
                         fabsf(rangeMiles - 80.0f) < 0.5f;
  if (!supported) return false;

  radar::clearAirportFocus();
  if (!app_state::setRadarRangeMiles(rangeMiles)) return false;
  adsb::requestRefresh();
  return true;
}

}  // namespace radar_control
