#pragma once

#include <stdint.h>

namespace ui {
// Owns LVGL construction, page updates, and UI event handling.

bool allocateTargetBuffer();
bool buildUi();
void showFatalStatus(const char* message);
void update(uint32_t now);

}  // namespace ui
