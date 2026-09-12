#include "time_sync_ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstring>

#include "adsb_network.h"

namespace time_sync_ui {
namespace {

constexpr uint32_t UPDATE_INTERVAL_MS = 80U;

lv_obj_t* clockLabel = nullptr;
uint32_t lastUpdateMs = 0;

bool isLabel(const lv_obj_t* object) {
  return object && lv_obj_check_type(object, &lv_label_class);
}

lv_obj_t* findLabelRecursive(lv_obj_t* parent, const char* exactText) {
  if (!parent || !exactText) return nullptr;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(index));
    if (!child) continue;
    if (isLabel(child)) {
      const char* text = lv_label_get_text(child);
      if (text && strcmp(text, exactText) == 0) return child;
    }
    if (lv_obj_t* nested = findLabelRecursive(child, exactText)) return nested;
  }
  return nullptr;
}

}  // namespace

bool build() {
  clockLabel = findLabelRecursive(lv_scr_act(), "--:--:--");
  if (!clockLabel) {
    Serial.println("Product 101 clock sync indicator attachment failed");
    return false;
  }
  return true;
}

void update(uint32_t now) {
  if (!clockLabel || adsb::timeSynchronized()) return;
  if (now - lastUpdateMs < UPDATE_INTERVAL_MS) return;
  lastUpdateMs = now;

  const char* current = lv_label_get_text(clockLabel);
  if (!current || strcmp(current, "SYNCING") != 0) {
    lv_label_set_text(clockLabel, "SYNCING");
  }
}

}  // namespace time_sync_ui
