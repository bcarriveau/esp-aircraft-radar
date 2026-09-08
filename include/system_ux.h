#pragma once

#include <stdint.h>

namespace system_ux {

// Attaches Product 100 system-page helpers to the UI that ui::buildUi()
// already created. Must be called while the LVGL port lock is held.
bool build();

// Services the asynchronous Wi-Fi scan and OTA QR display. Must be called
// while the LVGL port lock is held.
void update(uint32_t now);

}  // namespace system_ux
