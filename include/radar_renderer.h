#pragma once

#include <lvgl.h>

#include "aircraft_data.h"

namespace app_state { struct Snapshot; }

namespace radar {
// Owns radar canvas drawing and aircraft-preview rendering.

constexpr int WIDTH = 430;
constexpr int HEIGHT = 360;
constexpr int PREVIEW_WIDTH = 220;
constexpr int PREVIEW_HEIGHT = 150;
constexpr int SIDE_ICON_WIDTH = 28;
constexpr int SIDE_ICON_HEIGHT = 19;
constexpr int VERTICAL_STATE_ICON_WIDTH = 80;
constexpr int VERTICAL_STATE_ICON_HEIGHT = 36;

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
  lv_obj_t* leftOtherModeLabel = nullptr;
  lv_obj_t* leftOtherLabels[3]{};
  lv_obj_t* leftOtherIcons[3]{};
  lv_color_t* leftOtherIconBuffers[3]{};
  char* leftOtherHexes[3]{};
  lv_obj_t* aircraftModeLabel = nullptr;
  lv_obj_t* nearestCallsignLabel = nullptr;
  lv_obj_t* nearestSummaryLabel = nullptr;
  lv_obj_t* priorityIcon = nullptr;
  lv_color_t* priorityIconBuffer = nullptr;
  lv_obj_t* headingArrow = nullptr;
  lv_obj_t* headingLabel = nullptr;
  lv_obj_t* verticalStateIcon = nullptr;
  lv_color_t* verticalStateIconBuffer = nullptr;
  lv_obj_t* verticalStateLabel = nullptr;
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

struct PerformanceStats {
  uint32_t lastRenderUs = 0;
  uint32_t maximumRenderUs = 0;
  uint32_t lastFrameGapMs = 0;
  uint32_t maximumFrameGapMs = 0;
  uint32_t staticLayerRebuilds = 0;
  uint32_t snapshotCopies = 0;
  uint32_t fullCacheFallbacks = 0;
  uint32_t lastRestoredBytes = 0;
  uint32_t maximumRestoredBytes = 0;
  uint16_t lastDirtyRegionCount = 0;
  uint8_t lastContactCount = 0;
  bool staticLayerCached = false;
};

bool allocateWorkingBuffers();
void configure(const View& view);
bool render(aircraft::Target* workTargets, const char* selectedHex,
            app_state::Snapshot* renderedSnapshot = nullptr);
void copyPerformanceStats(PerformanceStats& stats);
bool hitTest(int canvasX, int canvasY, HitResult& result);
bool airportLabelCount(uint8_t rangeIndex, uint8_t labelMask,
                       uint16_t& count);
bool airportLabelVisible(uint8_t rangeIndex, uint8_t labelMask,
                         const char* ident, bool& visible);
void invalidateAirportLabelCount();
void focusAirport(const char* ident, uint32_t durationMs);
void clearAirportFocus();

void drawAircraftPreview(lv_obj_t* canvas, lv_color_t* buffer,
                         const aircraft::Target& target);
void drawSideBitmapIcon(lv_obj_t* canvas, lv_color_t* buffer,
                        AircraftBitmapId bitmapId);
void drawTrackBitmapIcon(lv_draw_ctx_t* drawContext, int centerX, int centerY,
                         AircraftBitmapId bitmapId);

}  // namespace radar
