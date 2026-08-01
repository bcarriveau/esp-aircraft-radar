#pragma once

namespace display_power {

// The Waveshare board exposes only a binary CH422G backlight enable.
void initialize();
bool enabled();
bool setEnabled(bool enabled);

}  // namespace display_power
