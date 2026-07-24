#pragma once

#include <lvgl.h>

#include "aircraft_data.h"

namespace radar {
// Owns radar canvas drawing and aircraft-preview rendering.

constexpr int WIDTH = 430;
constexpr int HEIGHT = 360;
constexpr int PREVIEW_WIDTH = 220;
constexpr int PREVIEW_HEIGHT = 150;
constexpr int SIDE_ICON_WIDTH = 28;
constexpr int SIDE_ICON_HEIGHT = 19;

struct View {
  lv_obj_t* canvas = nullptr;
  lv_color_t* buffer = nullptr;
  lv_obj_t* countLabel = nullptr;
  lv_obj_t* leftNearestModeLabel = nullptr;
  lv_obj_t* leftNearestCallsignLabel = nullptr;
  lv_obj_t* leftNearestSummaryLabel = nullptr;
  lv_obj_t* leftNearestIcon = nullptr;
  lv_color_t* leftNearestIconBuffer = nullptr;
  lv_obj_t* leftNearestHeadingArrow = nullptr;
  lv_obj_t* leftNearestHeadingLabel = nullptr;
  lv_obj_t* aircraftModeLabel = nullptr;
  lv_obj_t* nearestCallsignLabel = nullptr;
  lv_obj_t* nearestSummaryLabel = nullptr;
  lv_obj_t* priorityIcon = nullptr;
  lv_color_t* priorityIconBuffer = nullptr;
  lv_obj_t* headingArrow = nullptr;
  lv_obj_t* headingLabel = nullptr;
  char* leftNearestHex = nullptr;
  lv_obj_t* listLabels[5]{};
  lv_obj_t* listIcons[5]{};
  lv_color_t* listIconBuffers[5]{};
  char* listHexes[5]{};
};

struct HitResult {
  char hex[7]{};
  bool tracked = false;
  bool selected = false;
};

bool allocateWorkingBuffers();
void configure(const View& view);
bool render(aircraft::Target* workTargets, const char* selectedHex);
bool hitTest(int canvasX, int canvasY, HitResult& result);

void drawAircraftPreview(lv_obj_t* canvas, lv_color_t* buffer,
                         const aircraft::Target& target);
void drawSideBitmapIcon(lv_obj_t* canvas, lv_color_t* buffer,
                        AircraftBitmapId bitmapId);
void drawTrackBitmapIcon(lv_draw_ctx_t* drawContext, int centerX, int centerY,
                         AircraftBitmapId bitmapId);

}  // namespace radar
