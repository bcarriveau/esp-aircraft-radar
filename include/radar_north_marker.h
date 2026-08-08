#pragma once

namespace radar_north_marker {

// Adds one non-interactive north marker to the existing live radar canvas.
// Call once after ui::buildUi() while the LVGL port lock is held.
bool attach();

}  // namespace radar_north_marker
