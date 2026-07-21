#pragma once

#include <lvgl.h>

#include "aircraft_data.h"

namespace radar {
// Owns radar canvas drawing and aircraft-preview rendering.

constexpr int WIDTH = 430;
constexpr int HEIGHT = 360;
constexpr int PREVIEW_WIDTH = 220;
constexpr int PREVIEW_HEIGHT = 150;

struct View {
  lv_obj_t* canvas = nullptr;
  lv_color_t* buffer = nullptr;
  lv_obj_t* countLabel = nullptr;
  lv_obj_t* nearestCallsignLabel = nullptr;
  lv_obj_t* nearestSummaryLabel = nullptr;
  lv_obj_t* listLabels[5]{};
};

void configure(const View& view);
void render(aircraft::Target* workTargets);

void drawAircraftPreview(lv_obj_t* canvas, lv_color_t* buffer,
                         const aircraft::Target& target);
void drawTrackBitmapIcon(lv_draw_ctx_t* drawContext, int centerX, int centerY,
                         AircraftBitmapId bitmapId);

}  // namespace radar
