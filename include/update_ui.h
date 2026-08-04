#pragma once

#include <stdint.h>

namespace update_ui {

// Builds the static header indicator and the System-page update detail overlay.
// Must be called while the existing LVGL lock is held, after ui::buildUi().
bool build();

// Applies update-manager status changes only when its version changes.
// Must be called while the existing LVGL lock is held.
void update(uint32_t now);

}  // namespace update_ui
