#include "display_power.h"

#include <Waveshare_ST7262_LVGL.h>

namespace display_power {
namespace {
int backlightState = 1;
}

void initialize() {
  // lcd_init() leaves the panel backlight enabled.
  backlightState = 1;
}

bool enabled() { return backlightState != 0; }

bool setEnabled(bool enabledValue) {
  const int desired = enabledValue ? 1 : 0;
  if (backlightState == desired) return true;
  toggle_backlight(backlightState);
  return backlightState == desired;
}

}  // namespace display_power
