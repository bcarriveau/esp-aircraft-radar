#include "radar_north_marker.h"

#include <lvgl.h>
#include <stdint.h>

#include "radar_renderer.h"

namespace radar_north_marker {
namespace {

lv_obj_t* northLabel = nullptr;

lv_obj_t* findRadarCanvas(lv_obj_t* parent) {
  if (!parent) return nullptr;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t index = 0; index < childCount; ++index) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(index));
    if (!child) continue;
    if (lv_obj_check_type(child, &lv_canvas_class) &&
        lv_obj_get_width(child) == radar::WIDTH &&
        lv_obj_get_height(child) == radar::HEIGHT) {
      return child;
    }
    if (lv_obj_t* found = findRadarCanvas(child)) return found;
  }
  return nullptr;
}

}  // namespace

bool attach() {
  if (northLabel) return true;
  lv_obj_t* radarCanvas = findRadarCanvas(lv_scr_act());
  if (!radarCanvas) return false;

  northLabel = lv_label_create(radarCanvas);
  if (!northLabel) return false;
  lv_label_set_text(northLabel, "N");
  lv_obj_set_width(northLabel, 20);
  lv_obj_set_pos(northLabel, (radar::WIDTH - 20) / 2, 0);
  lv_obj_set_style_text_font(northLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(northLabel, lv_color_make(110, 220, 255), 0);
  lv_obj_set_style_text_align(northLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_opa(northLabel, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(northLabel, LV_OBJ_FLAG_CLICKABLE);
  return true;
}

}  // namespace radar_north_marker
