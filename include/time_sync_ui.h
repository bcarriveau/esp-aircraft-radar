#pragma once

#include <stdint.h>

namespace time_sync_ui {

// Attaches to the existing header clock after ui::buildUi(). Must be called
// while the LVGL port lock is held.
bool build();

// Keeps the header honest while saved time is being used only for TLS. Must be
// called while the LVGL port lock is held.
void update(uint32_t now);

}  // namespace time_sync_ui
