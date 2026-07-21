#pragma once

#include <stdint.h>

namespace ui {
// Owns LVGL construction, page updates, and UI event handling.

bool allocateTargetBuffer();
void buildUi();
void update(uint32_t now);

}  // namespace ui
