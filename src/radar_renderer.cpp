#include "radar_renderer.h"

#include <Arduino.h>
#include <limits.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_bitmaps.h"
#include "airport_data.h"
#include "radar_contact_bitmaps.h"
#include "settings.h"
#include "vertical_state.h"
#include "vertical_state_bitmaps.h"

namespace radar {
namespace {

constexpr int CENTER_X = WIDTH / 2;
constexpr int CENTER_Y = HEIGHT / 2;
constexpr int RADIUS = 168;
constexpr uint8_t PRIORITY_OTHER_COUNT = 3;
constexpr int RANGE_CONTROL_X1 = WIDTH - 120;
constexpr int RANGE_CONTROL_Y1 = HEIGHT - 52;
constexpr int RADAR_CONTACT_OVERLAP_RADIUS = 16;
constexpr int RADAR_CONTACT_TAG_CLEARANCE = 16;
constexpr int DOT_TAG_CLEARANCE = 7;
constexpr uint16_t MAX_AIRPORT_LABELS = 12;
constexpr uint16_t LABEL_BOX_CAPACITY =
    aircraft::MAX_TARGETS + MAX_AIRPORT_LABELS + 2;
constexpr size_t RADAR_BUFFER_BYTES =
    static_cast<size_t>(WIDTH) * HEIGHT * sizeof(lv_color_t);
constexpr float SWEEP_DEGREES_PER_SECOND = 27.5f;
constexpr uint32_t SWEEP_PERIOD_MS = 13091;
constexpr uint32_t ACTIVE_FRAME_GAP_LIMIT_MS = 1000;
constexpr int BITMAP_CONTACT_CLEAR_RADIUS = 14;
constexpr int DOT_CONTACT_CLEAR_RADIUS = 6;
constexpr uint8_t DIRTY_RESTORE_CONTACT_LIMIT = 64;

View radarView;
float sweepDegrees = 0;
uint32_t lastSweepUpdateMs = 0;
uint32_t lastRenderStartedMs = 0;
lv_color_t* radarBaseBuffer = nullptr;
bool radarBaseValid = false;
uint32_t radarBaseRangeGeneration = UINT32_MAX;
PerformanceStats performanceStats{};
lv_point_t leftNearestHeadingPoints[5]{};
lv_point_t priorityHeadingPoints[5]{};
vertical_state::State priorityVerticalState = vertical_state::State::LEVEL;
char priorityVerticalStateHex[7]{};
bool priorityVerticalStateInitialized = false;

inline lv_color_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return lv_color_make(red, green, blue);
}

void putPixel(int x, int y, lv_color_t color) {
  if (!radarView.buffer || x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
  radarView.buffer[y * WIDTH + x] = color;
}

void drawLine(int x0, int y0, int x1, int y1, lv_color_t color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    putPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int doubledError = 2 * error;
    if (doubledError >= dy) { error += dy; x0 += sx; }
    if (doubledError <= dx) { error += dx; y0 += sy; }
  }
}

void fillCircle(int centerX, int centerY, int radius, lv_color_t color) {
  for (int y = -radius; y <= radius; ++y) {
    int span = (int)sqrtf((float)(radius * radius - y * y));
    for (int x = -span; x <= span; ++x) putPixel(centerX + x, centerY + y, color);
  }
}

void drawCircle(int centerX, int centerY, int radius, lv_color_t color) {
  int x = radius, y = 0, error = 0;
  while (x >= y) {
    putPixel(centerX + x, centerY + y, color);
    putPixel(centerX + y, centerY + x, color);
    putPixel(centerX - y, centerY + x, color);
    putPixel(centerX - x, centerY + y, color);
    putPixel(centerX - x, centerY - y, color);
    putPixel(centerX - y, centerY - x, color);
    putPixel(centerX + y, centerY - x, color);
    putPixel(centerX + x, centerY - y, color);
    if (error <= 0) { ++y; error += 2 * y + 1; }
    if (error > 0) { --x; error -= 2 * x + 1; }
  }
}

void previewPixel(lv_color_t* buffer, int x, int y, lv_color_t color) {
  if (!buffer || x < 0 || y < 0 || x >= PREVIEW_WIDTH || y >= PREVIEW_HEIGHT) return;
  buffer[y * PREVIEW_WIDTH + x] = color;
}

void previewLine(lv_color_t* buffer, int x0, int y0, int x1, int y1,
                 lv_color_t color, int width = 1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    int half = width / 2;
    for (int py = -half; py <= half; ++py) {
      for (int px = -half; px <= half; ++px) {
        previewPixel(buffer, x0 + px, y0 + py, color);
      }
    }
    if (x0 == x1 && y0 == y1) break;
    int doubledError = 2 * error;
    if (doubledError >= dy) { error += dy; x0 += sx; }
    if (doubledError <= dx) { error += dx; y0 += sy; }
  }
}

lv_color_t bitmapColor(uint16_t pixel) {
  uint8_t red = ((pixel >> 11) & 0x1F) * 255 / 31;
  uint8_t green = ((pixel >> 5) & 0x3F) * 255 / 63;
  uint8_t blue = (pixel & 0x1F) * 255 / 31;
  return rgb(red, green, blue);
}

struct ScreenContact {
  uint8_t targetIndex;
  uint8_t hitIndex;
  int16_t x;
  int16_t y;
  bool tracked;
  bool selected;
  bool needsOutline;
  bool sweepHighlighted;
  uint8_t headingIndex;
};

struct LabelBox { int16_t x1, y1, x2, y2; };

struct HitRegion {
  char hex[7]{};
  int16_t contactX = 0;
  int16_t contactY = 0;
  bool tracked = false;
  bool selected = false;
  bool hasTag = false;
  LabelBox tag{};
};

struct ContactFrame {
  ScreenContact* contacts = nullptr;
  uint8_t count = 0;
  bool trackedVisible = false;
  int trackedX = 0;
  int trackedY = 0;
  int trackedTargetIndex = -1;
};

HitRegion* renderedHits = nullptr;
ScreenContact* renderedContacts = nullptr;
LabelBox* renderedLabelBoxes = nullptr;
airport_data::NearbyAirport* airportWork = nullptr;
uint16_t airportFrameCount = 0;
uint8_t airportFrameSymbolMask = 0;
uint8_t airportFrameLabelMask = 0;
LabelBox* airportLabelBoxes = nullptr;
using AirportIdent = char[8];
AirportIdent* airportVisibleLabelIdents = nullptr;
AirportIdent airportFocusIdent{};
uint32_t airportFocusExpiresAt = 0;
bool airportLabelStatsValid = false;
uint8_t airportLabelStatsRangeIndex = 0;
uint8_t airportLabelStatsMask = 0;
uint16_t airportLabelStatsCount = 0;
ContactFrame contactFrame;
uint8_t renderedHitCount = 0;
bool renderedTwentyMileRange = false;

void invalidateStaticRadarLayer() {
  radarBaseValid = false;
  airportLabelStatsValid = false;
  airportLabelStatsCount = 0;
}

bool airportFocusActive() {
  if (!airportFocusIdent[0]) return false;
  if (static_cast<int32_t>(millis() - airportFocusExpiresAt) >= 0) {
    airportFocusIdent[0] = 0;
    airportFocusExpiresAt = 0;
    invalidateStaticRadarLayer();
    return false;
  }
  return true;
}

bool airportIsFocused(const airport_data::NearbyAirport& airport) {
  return airportFocusIdent[0] &&
      strncmp(airportFocusIdent, airport.ident, sizeof(AirportIdent)) == 0;
}

void recordVisibleAirportLabel(const airport_data::NearbyAirport& airport,
                               const LabelBox& placement) {
  if (airportLabelStatsCount >= airport_data::MAX_NEARBY_AIRPORTS) return;
  airportLabelBoxes[airportLabelStatsCount] = placement;
  if (airportVisibleLabelIdents) {
    strncpy(airportVisibleLabelIdents[airportLabelStatsCount],
            airport.ident, sizeof(AirportIdent) - 1);
    airportVisibleLabelIdents[airportLabelStatsCount]
                             [sizeof(AirportIdent) - 1] = 0;
  }
  ++airportLabelStatsCount;
}

bool drawPlacedTag(int dotX, int dotY, const char* const* lines,
                   const lv_color_t* lineColors, uint8_t lineCount,
                   lv_color_t backgroundColor, lv_color_t borderColor,
                   int maxWidth, int contactClearance, uint8_t hitIndex,
                   LabelBox* labelBoxes, uint16_t& labelBoxCount) {
  if (!lines || !lineColors || lineCount == 0 || !lines[0] || !lines[0][0]) {
    return false;
  }
  int widestText = 0;
  for (uint8_t line = 0; line < lineCount; ++line) {
    lv_point_t textSize{};
    lv_txt_get_size(&textSize, lines[line], &lv_font_montserrat_12,
                    0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    widestText = max(widestText, (int)textSize.x);
  }
  const int lineHeight = lv_font_get_line_height(&lv_font_montserrat_12);
  const int labelHeight = lineHeight * lineCount + 6;
  const int labelWidth = constrain(widestText + 10, 44, maxWidth);
  int candidateX[6] = {
    dotX + contactClearance, dotX - labelWidth - contactClearance,
    dotX - labelWidth / 2, dotX - labelWidth / 2,
    dotX + contactClearance, dotX - labelWidth - contactClearance
  };
  int candidateY[6] = {
    dotY - labelHeight / 2, dotY - labelHeight / 2,
    dotY - labelHeight - contactClearance, dotY + contactClearance,
    dotY + contactClearance, dotY + contactClearance
  };
  for (int candidate = 0; candidate < 6; ++candidate) {
    int labelX = constrain(candidateX[candidate], 2, WIDTH - labelWidth - 2);
    int labelY = constrain(candidateY[candidate], 2, HEIGHT - labelHeight - 2);
    int x2 = labelX + labelWidth;
    int y2 = labelY + labelHeight;
    bool overlaps = false;
    for (uint16_t box = 0; box < labelBoxCount; ++box) {
      const LabelBox& used = labelBoxes[box];
      if (!(x2 + 3 < used.x1 || labelX - 3 > used.x2 ||
            y2 + 2 < used.y1 || labelY - 2 > used.y2)) {
        overlaps = true;
        break;
      }
    }
    if (overlaps) continue;
    lv_draw_rect_dsc_t rectangle;
    lv_draw_rect_dsc_init(&rectangle);
    rectangle.bg_opa = LV_OPA_COVER;
    rectangle.bg_color = backgroundColor;
    rectangle.border_opa = LV_OPA_COVER;
    rectangle.border_color = borderColor;
    rectangle.border_width = 1;
    rectangle.radius = 4;
    lv_canvas_draw_rect(radarView.canvas, labelX, labelY, labelWidth,
                        labelHeight, &rectangle);
    for (uint8_t line = 0; line < lineCount; ++line) {
      lv_draw_label_dsc_t labelDescription;
      lv_draw_label_dsc_init(&labelDescription);
      labelDescription.color = lineColors[line];
      labelDescription.font = &lv_font_montserrat_12;
      lv_canvas_draw_text(radarView.canvas, labelX + 5,
                          labelY + 3 + line * lineHeight, labelWidth - 10,
                          &labelDescription, lines[line]);
    }
    if (labelBoxCount < LABEL_BOX_CAPACITY) {
      labelBoxes[labelBoxCount++] = {
        (int16_t)labelX, (int16_t)labelY, (int16_t)x2, (int16_t)y2
      };
    }
    if (hitIndex < renderedHitCount) {
      renderedHits[hitIndex].hasTag = true;
      renderedHits[hitIndex].tag = {
        (int16_t)labelX, (int16_t)labelY, (int16_t)x2, (int16_t)y2
      };
    }
    return true;
  }
  return false;
}

}  // namespace

bool allocateWorkingBuffers() {
  if (renderedHits && renderedContacts && renderedLabelBoxes) return true;

  renderedHits = static_cast<HitRegion*>(heap_caps_calloc(
      aircraft::MAX_TARGETS, sizeof(HitRegion),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  renderedContacts = static_cast<ScreenContact*>(heap_caps_calloc(
      aircraft::MAX_TARGETS, sizeof(ScreenContact),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  renderedLabelBoxes = static_cast<LabelBox*>(heap_caps_calloc(
      LABEL_BOX_CAPACITY, sizeof(LabelBox),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  airportWork = static_cast<airport_data::NearbyAirport*>(heap_caps_calloc(
      airport_data::MAX_NEARBY_AIRPORTS,
      sizeof(airport_data::NearbyAirport),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  airportLabelBoxes = static_cast<LabelBox*>(heap_caps_calloc(
      airport_data::MAX_NEARBY_AIRPORTS, sizeof(LabelBox),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  airportVisibleLabelIdents = static_cast<AirportIdent*>(heap_caps_calloc(
      airport_data::MAX_NEARBY_AIRPORTS, sizeof(AirportIdent),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (!renderedHits || !renderedContacts || !renderedLabelBoxes) {
    free(renderedHits);
    free(renderedContacts);
    free(renderedLabelBoxes);
    free(airportWork);
    free(airportLabelBoxes);
    free(airportVisibleLabelIdents);
    renderedHits = nullptr;
    renderedContacts = nullptr;
    renderedLabelBoxes = nullptr;
    airportWork = nullptr;
    airportLabelBoxes = nullptr;
    airportVisibleLabelIdents = nullptr;
    contactFrame.contacts = nullptr;
    Serial.println("FATAL: Radar working-buffer PSRAM allocation failed");
    return false;
  }

  if (!radarBaseBuffer) {
    radarBaseBuffer = static_cast<lv_color_t*>(heap_caps_malloc(
        RADAR_BUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }

  contactFrame.contacts = renderedContacts;
  Serial.printf("Radar working buffers in PSRAM: %u bytes\n",
                (unsigned)(aircraft::MAX_TARGETS * sizeof(HitRegion) +
                           aircraft::MAX_TARGETS * sizeof(ScreenContact) +
                           LABEL_BOX_CAPACITY * sizeof(LabelBox)));
  if (airportWork) {
    Serial.printf("Radar airport work buffer in PSRAM: %u bytes\n",
                  (unsigned)(airport_data::MAX_NEARBY_AIRPORTS *
                             sizeof(airport_data::NearbyAirport)));
  } else {
    Serial.println(
        "Airport overlay disabled: radar airport work-buffer allocation failed");
  }
  if (airportLabelBoxes) {
    Serial.printf("Radar airport label buffer in PSRAM: %u bytes\n",
                  (unsigned)(airport_data::MAX_NEARBY_AIRPORTS *
                             sizeof(LabelBox)));
  } else {
    Serial.println(
        "Airport labels disabled: placement-buffer allocation failed");
  }
  if (airportVisibleLabelIdents) {
    Serial.printf("Radar airport label-status buffer in PSRAM: %u bytes\n",
                  (unsigned)(airport_data::MAX_NEARBY_AIRPORTS *
                             sizeof(AirportIdent)));
  } else {
    Serial.println(
        "Airport directory label-status icons disabled: buffer allocation failed");
  }
  if (radarBaseBuffer) {
    Serial.printf("Radar static base cache in PSRAM: %u bytes\n",
                  (unsigned)RADAR_BUFFER_BYTES);
  } else {
    Serial.println(
        "Radar static base cache unavailable: using full-frame fallback");
  }
  return true;
}

void configure(const View& view) {
  radarView = view;
  radarBaseValid = false;
  radarBaseRangeGeneration = UINT32_MAX;
  lastSweepUpdateMs = 0;
  lastRenderStartedMs = 0;
  performanceStats = PerformanceStats{};
  performanceStats.staticLayerCached = radarBaseBuffer != nullptr;
}

void focusAirport(const char* ident, uint32_t durationMs) {
  if (!ident || !ident[0] || durationMs == 0) {
    clearAirportFocus();
    return;
  }
  strncpy(airportFocusIdent, ident, sizeof(AirportIdent) - 1);
  airportFocusIdent[sizeof(AirportIdent) - 1] = 0;
  airportFocusExpiresAt = millis() + durationMs;
  invalidateStaticRadarLayer();
}

void clearAirportFocus() {
  if (!airportFocusIdent[0] && airportFocusExpiresAt == 0) return;
  airportFocusIdent[0] = 0;
  airportFocusExpiresAt = 0;
  invalidateStaticRadarLayer();
}

bool airportLabelCount(uint8_t rangeIndex, uint8_t labelMask,
                       uint16_t& count) {
  if (!airportLabelStatsValid || airportLabelStatsRangeIndex != rangeIndex ||
      airportLabelStatsMask != labelMask) {
    return false;
  }
  count = airportLabelStatsCount;
  return true;
}

bool airportLabelVisible(uint8_t rangeIndex, uint8_t labelMask,
                         const char* ident, bool& visible) {
  visible = false;
  if (!ident || !ident[0] || !airportVisibleLabelIdents ||
      !airportLabelStatsValid || airportLabelStatsRangeIndex != rangeIndex ||
      airportLabelStatsMask != labelMask) {
    return false;
  }
  for (uint16_t index = 0; index < airportLabelStatsCount; ++index) {
    if (strncmp(airportVisibleLabelIdents[index], ident,
                sizeof(AirportIdent)) == 0) {
      visible = true;
      break;
    }
  }
  return true;
}

void invalidateAirportLabelCount() {
  invalidateStaticRadarLayer();
}

void drawAircraftPreview(lv_obj_t* canvas, lv_color_t* buffer,
                         const aircraft::Target& target) {
  if (!buffer || !canvas) return;
  const lv_color_t background = rgb(5, 14, 21);
  const lv_color_t grid = rgb(16, 45, 54);
  for (int i = 0; i < PREVIEW_WIDTH * PREVIEW_HEIGHT; ++i) buffer[i] = background;
  previewLine(buffer, 10, PREVIEW_HEIGHT / 2,
              PREVIEW_WIDTH - 10, PREVIEW_HEIGHT / 2, grid);
  previewLine(buffer, PREVIEW_WIDTH / 2, 10,
              PREVIEW_WIDTH / 2, PREVIEW_HEIGHT - 10, grid);

  const uint16_t* sprite = aircraftBitmap(aircraft::bitmapForTarget(target));
  constexpr int scale = 2;
  const int startX = (PREVIEW_WIDTH - AIRCRAFT_BITMAP_W * scale) / 2;
  const int startY = (PREVIEW_HEIGHT - AIRCRAFT_BITMAP_H * scale) / 2;
  for (int sourceY = 0; sourceY < AIRCRAFT_BITMAP_H; ++sourceY) {
    for (int sourceX = 0; sourceX < AIRCRAFT_BITMAP_W; ++sourceX) {
      uint16_t pixel = pgm_read_word(
          sprite + sourceY * AIRCRAFT_BITMAP_W + sourceX);
      if (!pixel) continue;
      for (int pixelY = 0; pixelY < scale; ++pixelY) {
        for (int pixelX = 0; pixelX < scale; ++pixelX) {
          previewPixel(buffer, startX + sourceX * scale + pixelX + 2,
                       startY + sourceY * scale + pixelY + 2, rgb(2, 6, 10));
        }
      }
    }
  }
  for (int sourceY = 0; sourceY < AIRCRAFT_BITMAP_H; ++sourceY) {
    for (int sourceX = 0; sourceX < AIRCRAFT_BITMAP_W; ++sourceX) {
      uint16_t pixel = pgm_read_word(
          sprite + sourceY * AIRCRAFT_BITMAP_W + sourceX);
      if (!pixel) continue;
      lv_color_t color = bitmapColor(pixel);
      for (int pixelY = 0; pixelY < scale; ++pixelY) {
        for (int pixelX = 0; pixelX < scale; ++pixelX) {
          previewPixel(buffer, startX + sourceX * scale + pixelX,
                       startY + sourceY * scale + pixelY, color);
        }
      }
    }
  }
  lv_obj_invalidate(canvas);
}

void drawSideBitmapIcon(lv_obj_t* canvas, lv_color_t* buffer,
                        AircraftBitmapId bitmapId) {
  if (!canvas || !buffer) return;
  const lv_color_t background = rgb(10, 18, 25);
  for (int i = 0; i < SIDE_ICON_WIDTH * SIDE_ICON_HEIGHT; ++i) {
    buffer[i] = background;
  }

  const uint16_t* sprite = aircraftBitmap(bitmapId);
  for (int destinationY = 0; destinationY < SIDE_ICON_HEIGHT;
       ++destinationY) {
    const int sourceY =
        destinationY * AIRCRAFT_BITMAP_H / SIDE_ICON_HEIGHT;
    for (int destinationX = 0; destinationX < SIDE_ICON_WIDTH;
         ++destinationX) {
      const int sourceX =
          destinationX * AIRCRAFT_BITMAP_W / SIDE_ICON_WIDTH;
      const uint16_t pixel = pgm_read_word(
          sprite + sourceY * AIRCRAFT_BITMAP_W + sourceX);
      if (!pixel) continue;
      buffer[destinationY * SIDE_ICON_WIDTH + destinationX] =
          bitmapColor(pixel);
    }
  }
  lv_obj_invalidate(canvas);
}

void drawTrackBitmapIcon(lv_draw_ctx_t* drawContext, int centerX, int centerY,
                         AircraftBitmapId bitmapId) {
  constexpr int iconWidth = 28;
  constexpr int iconHeight = 19;
  const int startX = centerX - iconWidth / 2;
  const int startY = centerY - iconHeight / 2;
  const uint16_t* sprite = aircraftBitmap(bitmapId);
  lv_draw_rect_dsc_t rectangle;
  lv_draw_rect_dsc_init(&rectangle);
  rectangle.bg_opa = LV_OPA_COVER;
  rectangle.border_opa = LV_OPA_TRANSP;
  rectangle.radius = 0;

  for (int destinationY = 0; destinationY < iconHeight; ++destinationY) {
    int sourceY = destinationY * AIRCRAFT_BITMAP_H / iconHeight;
    int destinationX = 0;
    while (destinationX < iconWidth) {
      int sourceX = destinationX * AIRCRAFT_BITMAP_W / iconWidth;
      uint16_t pixel = pgm_read_word(
          sprite + sourceY * AIRCRAFT_BITMAP_W + sourceX);
      if (!pixel) {
        ++destinationX;
        continue;
      }
      int runStart = destinationX;
      uint16_t runColor = pixel;
      while (destinationX < iconWidth) {
        sourceX = destinationX * AIRCRAFT_BITMAP_W / iconWidth;
        pixel = pgm_read_word(sprite + sourceY * AIRCRAFT_BITMAP_W + sourceX);
        if (!pixel) break;
        ++destinationX;
      }
      rectangle.bg_color = bitmapColor(runColor);
      lv_area_t runArea = {
        (lv_coord_t)(startX + runStart), (lv_coord_t)(startY + destinationY),
        (lv_coord_t)(startX + destinationX - 1),
        (lv_coord_t)(startY + destinationY)
      };
      lv_draw_rect(drawContext, &rectangle, &runArea);
    }
  }
}

// Radar frames are rendered in stable background, contact, label, and summary phases.
namespace {

void drawRadarBase() {
  const lv_color_t background = rgb(2, 8, 12);
  const lv_color_t grid = rgb(20, 58, 55);
  for (int i = 0; i < WIDTH * HEIGHT; ++i) radarView.buffer[i] = background;
  drawCircle(CENTER_X, CENTER_Y, RADIUS, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS * 3 / 4, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS / 2, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS / 4, grid);
  drawLine(CENTER_X - RADIUS, CENTER_Y, CENTER_X + RADIUS, CENTER_Y, grid);
  drawLine(CENTER_X, CENTER_Y - RADIUS, CENTER_X, CENTER_Y + RADIUS, grid);
}

void drawRadarSweep(float angleDegrees) {
  const lv_color_t green = rgb(50, 255, 135);
  for (int tail = 18; tail >= 0; --tail) {
    const float angle =
        (angleDegrees - tail * 1.6f) * M_PI / 180.0f;
    const int endX = CENTER_X + (int)(sin(angle) * RADIUS);
    const int endY = CENTER_Y - (int)(cos(angle) * RADIUS);
    const uint8_t intensity = 28 + (18 - tail) * 7;
    drawLine(CENTER_X, CENTER_Y, endX, endY,
             rgb(0, intensity, intensity / 2));
  }
  const float angle = angleDegrees * M_PI / 180.0f;
  drawLine(CENTER_X, CENTER_Y,
           CENTER_X + (int)(sin(angle) * RADIUS),
           CENTER_Y - (int)(cos(angle) * RADIUS), green);
  fillCircle(CENTER_X, CENTER_Y, 3, green);
}


void advanceSweep(uint32_t nowMs) {
  if (lastSweepUpdateMs == 0) {
    lastSweepUpdateMs = nowMs;
    return;
  }
  uint32_t elapsedMs = nowMs - lastSweepUpdateMs;
  lastSweepUpdateMs = nowMs;
  elapsedMs %= SWEEP_PERIOD_MS;
  sweepDegrees = fmodf(
      sweepDegrees + elapsedMs * (SWEEP_DEGREES_PER_SECOND / 1000.0f),
      360.0f);
}

void bindRadarCanvasBuffer(lv_color_t* buffer) {
  radarView.buffer = buffer;
  lv_canvas_set_buffer(radarView.canvas, buffer, WIDTH, HEIGHT,
                       LV_IMG_CF_TRUE_COLOR);
}

void restorePixelFromBase(int x, int y) {
  if (!radarBaseBuffer || !radarView.buffer || x < 0 || y < 0 ||
      x >= WIDTH || y >= HEIGHT) {
    return;
  }
  radarView.buffer[y * WIDTH + x] = radarBaseBuffer[y * WIDTH + x];
}

void restoreLineFromBase(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    restorePixelFromBase(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    const int doubledError = 2 * error;
    if (doubledError >= dy) { error += dy; x0 += sx; }
    if (doubledError <= dx) { error += dx; y0 += sy; }
  }
}

void restoreRectFromBase(int x1, int y1, int x2, int y2) {
  if (!radarBaseBuffer || !radarView.buffer) return;
  x1 = constrain(x1, 0, WIDTH - 1);
  y1 = constrain(y1, 0, HEIGHT - 1);
  x2 = constrain(x2, 0, WIDTH - 1);
  y2 = constrain(y2, 0, HEIGHT - 1);
  if (x2 < x1 || y2 < y1) return;
  const size_t rowBytes = static_cast<size_t>(x2 - x1 + 1) *
                          sizeof(lv_color_t);
  for (int y = y1; y <= y2; ++y) {
    memcpy(radarView.buffer + y * WIDTH + x1,
           radarBaseBuffer + y * WIDTH + x1, rowBytes);
  }
}

void restorePreviousDynamicLayer(float previousSweepDegrees) {
  for (int tail = 18; tail >= 0; --tail) {
    const float angle =
        (previousSweepDegrees - tail * 1.6f) * M_PI / 180.0f;
    restoreLineFromBase(
        CENTER_X, CENTER_Y,
        CENTER_X + (int)(sin(angle) * RADIUS),
        CENTER_Y - (int)(cos(angle) * RADIUS));
  }
  const float mainAngle = previousSweepDegrees * M_PI / 180.0f;
  restoreLineFromBase(
      CENTER_X, CENTER_Y,
      CENTER_X + (int)(sin(mainAngle) * RADIUS),
      CENTER_Y - (int)(cos(mainAngle) * RADIUS));
  restoreRectFromBase(CENTER_X - 3, CENTER_Y - 3,
                      CENTER_X + 3, CENTER_Y + 3);

  for (uint8_t index = 0; index < contactFrame.count; ++index) {
    const ScreenContact& contact = contactFrame.contacts[index];
    const int clearRadius =
        renderedTwentyMileRange || contact.selected || contact.tracked
            ? BITMAP_CONTACT_CLEAR_RADIUS
            : DOT_CONTACT_CLEAR_RADIUS;
    restoreRectFromBase(contact.x - clearRadius, contact.y - clearRadius,
                        contact.x + clearRadius, contact.y + clearRadius);
  }
  for (uint8_t index = 0; index < renderedHitCount; ++index) {
    const HitRegion& hit = renderedHits[index];
    if (!hit.hasTag) continue;
    restoreRectFromBase(hit.tag.x1 - 1, hit.tag.y1 - 1,
                        hit.tag.x2 + 1, hit.tag.y2 + 1);
  }
}

bool airportScreenPosition(const airport_data::NearbyAirport& airport,
                           float rangeMiles, int& x, int& y) {
  if (rangeMiles <= 0.0f || airport.distanceMiles > rangeMiles) return false;
  const float ratio = airport.distanceMiles / rangeMiles;
  const float bearingRadians = airport.bearingDegrees * (float)M_PI / 180.0f;
  x = CENTER_X + (int)lroundf(sinf(bearingRadians) * RADIUS * ratio);
  y = CENTER_Y - (int)lroundf(cosf(bearingRadians) * RADIUS * ratio);
  return x >= CENTER_X - RADIUS && x <= CENTER_X + RADIUS &&
         y >= CENTER_Y - RADIUS && y <= CENTER_Y + RADIUS;
}

lv_color_t airportColor(airport_data::Category category) {
  switch (category) {
    case airport_data::Category::MAJOR: return rgb(52, 126, 136);
    case airport_data::Category::PUBLIC: return rgb(34, 94, 101);
    case airport_data::Category::PRIVATE_FIELD: return rgb(62, 76, 80);
    case airport_data::Category::HELIPORT: return rgb(52, 86, 84);
    default: return rgb(40, 84, 90);
  }
}

lv_color_t airportLabelColor(airport_data::Category category) {
  switch (category) {
    case airport_data::Category::MAJOR: return rgb(72, 142, 152);
    case airport_data::Category::PUBLIC: return rgb(48, 104, 112);
    case airport_data::Category::PRIVATE_FIELD: return rgb(65, 82, 86);
    case airport_data::Category::HELIPORT: return rgb(58, 95, 92);
    default: return rgb(48, 96, 102);
  }
}

void prepareAirportFrame(float rangeMiles) {
  airportFrameCount = 0;
  airportFrameSymbolMask = 0;
  airportFrameLabelMask = 0;
  const bool hasFocus = airportFocusActive();
  if (!airportWork || !airport_data::ready() ||
      (!settings::airportsEnabled() && !hasFocus)) {
    return;
  }
  const uint8_t range = airport_data::rangeIndex(rangeMiles);
  airportFrameSymbolMask = settings::airportSymbolMask(range);
  airportFrameLabelMask = settings::airportLabelMask(range);
  const uint8_t unionMask = airportFrameSymbolMask | airportFrameLabelMask;
  const bool hasManualShow = settings::airportLabelOverrideCount(
      settings::AirportLabelMode::SHOW) > 0;
  if (unionMask == 0 && !hasManualShow && !hasFocus) return;
  airportFrameCount = airport_data::copyNearby(
      airportWork, airport_data::MAX_NEARBY_AIRPORTS, rangeMiles,
      airport_data::CATEGORY_MASK_ALL);
}

void drawAirportSymbols(float rangeMiles) {
  for (uint16_t index = 0; index < airportFrameCount; ++index) {
    const airport_data::NearbyAirport& airport = airportWork[index];
    const bool focused = airportIsFocused(airport);
    if (!focused &&
        (airportFrameSymbolMask & airport_data::categoryBit(airport.category)) == 0) {
      continue;
    }
    int x = 0;
    int y = 0;
    if (!airportScreenPosition(airport, rangeMiles, x, y)) continue;
    if (x >= RANGE_CONTROL_X1 - 7 && y >= RANGE_CONTROL_Y1 - 7) continue;
    const lv_color_t color = focused
        ? rgb(255, 190, 70) : airportColor(airport.category);

    if (airport.category == airport_data::Category::HELIPORT) {
      drawLine(x - 3, y - 3, x - 3, y + 3, color);
      drawLine(x + 3, y - 3, x + 3, y + 3, color);
      drawLine(x - 3, y, x + 3, y, color);
      if (focused) {
        drawCircle(x, y, 6, color);
        drawCircle(x, y, 9, color);
      }
      continue;
    }

    const int halfLength = airport.category == airport_data::Category::MAJOR
        ? 6 : (airport.category == airport_data::Category::PUBLIC ? 5 : 4);
    const float heading = airport.runwayHeadingDegrees > 0
        ? airport.runwayHeadingDegrees * (float)M_PI / 180.0f
        : 45.0f * (float)M_PI / 180.0f;
    const int dx = (int)lroundf(sinf(heading) * halfLength);
    const int dy = (int)lroundf(-cosf(heading) * halfLength);
    drawLine(x - dx, y - dy, x + dx, y + dy, color);
    if (airport.category == airport_data::Category::MAJOR) {
      drawCircle(x, y, 2, color);
    } else {
      putPixel(x, y, color);
    }
    if (focused) {
      drawCircle(x, y, 6, color);
      drawCircle(x, y, 9, color);
    }
  }
}

void drawAirportLabels(float rangeMiles);

bool rebuildStaticRadarLayer(float rangeMiles, uint32_t rangeGeneration) {
  if (!radarBaseBuffer || !radarView.canvas || !radarView.buffer) return false;
  lv_color_t* const displayBuffer = radarView.buffer;
  bindRadarCanvasBuffer(radarBaseBuffer);
  drawRadarBase();
  prepareAirportFrame(rangeMiles);
  drawAirportLabels(rangeMiles);
  drawAirportSymbols(rangeMiles);
  bindRadarCanvasBuffer(displayBuffer);
  radarBaseValid = true;
  radarBaseRangeGeneration = rangeGeneration;
  ++performanceStats.staticLayerRebuilds;
  return true;
}

void copyStaticRadarLayerToDisplay() {
  if (!radarBaseBuffer || !radarView.buffer) return;
  memcpy(radarView.buffer, radarBaseBuffer, RADAR_BUFFER_BYTES);
}

bool labelBoxesOverlap(const LabelBox& first, const LabelBox& second,
                       int horizontalGap = 3, int verticalGap = 2) {
  return !(first.x2 + horizontalGap < second.x1 ||
           first.x1 - horizontalGap > second.x2 ||
           first.y2 + verticalGap < second.y1 ||
           first.y1 - verticalGap > second.y2);
}

bool placeAirportLabel(const airport_data::NearbyAirport& airport,
                       float rangeMiles, LabelBox* placedLabels,
                       uint16_t placedLabelCount, LabelBox& placement) {
  int airportX = 0;
  int airportY = 0;
  if (!airportScreenPosition(airport, rangeMiles, airportX, airportY)) {
    return false;
  }

  lv_point_t textSize{};
  lv_txt_get_size(&textSize, airport.ident, &lv_font_montserrat_12,
                  0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  const int labelWidth = constrain((int)textSize.x + 8, 30, 66);
  const int labelHeight = lv_font_get_line_height(&lv_font_montserrat_12) + 4;
  constexpr int CLEARANCE = 7;

  const int candidateX[8] = {
    airportX + CLEARANCE,
    airportX - labelWidth - CLEARANCE,
    airportX - labelWidth / 2,
    airportX - labelWidth / 2,
    airportX + CLEARANCE,
    airportX - labelWidth - CLEARANCE,
    airportX + CLEARANCE,
    airportX - labelWidth - CLEARANCE
  };
  const int candidateY[8] = {
    airportY - labelHeight / 2,
    airportY - labelHeight / 2,
    airportY - labelHeight - CLEARANCE,
    airportY + CLEARANCE,
    airportY - labelHeight - CLEARANCE,
    airportY - labelHeight - CLEARANCE,
    airportY + CLEARANCE,
    airportY + CLEARANCE
  };

  // Prefer the outward side of the radar, then deterministic nearby alternatives.
  const float bearingRadians = airport.bearingDegrees * (float)M_PI / 180.0f;
  const float radialX = sinf(bearingRadians);
  const float radialY = -cosf(bearingRadians);
  int order[8]{};
  if (fabsf(radialX) >= fabsf(radialY)) {
    const int preferred[8] = {
      radialX >= 0.0f ? 0 : 1,
      radialX >= 0.0f ? 4 : 5,
      radialX >= 0.0f ? 6 : 7,
      radialY < 0.0f ? 2 : 3,
      radialY < 0.0f ? 3 : 2,
      radialX >= 0.0f ? 1 : 0,
      radialX >= 0.0f ? 5 : 4,
      radialX >= 0.0f ? 7 : 6
    };
    memcpy(order, preferred, sizeof(order));
  } else {
    const int preferred[8] = {
      radialY < 0.0f ? 2 : 3,
      radialX >= 0.0f ? (radialY < 0.0f ? 4 : 6)
                      : (radialY < 0.0f ? 5 : 7),
      radialX >= 0.0f ? (radialY < 0.0f ? 5 : 7)
                      : (radialY < 0.0f ? 4 : 6),
      radialX >= 0.0f ? 0 : 1,
      radialX >= 0.0f ? 1 : 0,
      radialY < 0.0f ? 3 : 2,
      radialY < 0.0f ? 6 : 4,
      radialY < 0.0f ? 7 : 5
    };
    memcpy(order, preferred, sizeof(order));
  }

  const LabelBox rangeControl = {
    (int16_t)(RANGE_CONTROL_X1 - 4), (int16_t)(RANGE_CONTROL_Y1 - 4),
    (int16_t)(WIDTH - 1), (int16_t)(HEIGHT - 1)
  };
  const LabelBox homeClearance = {
    (int16_t)(CENTER_X - 10), (int16_t)(CENTER_Y - 10),
    (int16_t)(CENTER_X + 10), (int16_t)(CENTER_Y + 10)
  };

  for (uint8_t candidateIndex = 0; candidateIndex < 8; ++candidateIndex) {
    const int candidate = order[candidateIndex];
    const int x1 = constrain(candidateX[candidate], 2, WIDTH - labelWidth - 2);
    const int y1 = constrain(candidateY[candidate], 2, HEIGHT - labelHeight - 2);
    const LabelBox box = {
      (int16_t)x1, (int16_t)y1,
      (int16_t)(x1 + labelWidth), (int16_t)(y1 + labelHeight)
    };
    if (labelBoxesOverlap(box, rangeControl, 1, 1) ||
        labelBoxesOverlap(box, homeClearance, 1, 1)) {
      continue;
    }

    bool overlaps = false;
    for (uint16_t used = 0; used < placedLabelCount; ++used) {
      if (labelBoxesOverlap(box, placedLabels[used])) {
        overlaps = true;
        break;
      }
    }
    if (overlaps) continue;
    placement = box;
    return true;
  }
  return false;
}

void drawAirportLabel(const airport_data::NearbyAirport& airport,
                      const LabelBox& placement, bool focused = false) {
  const int labelWidth = placement.x2 - placement.x1;
  const int labelHeight = placement.y2 - placement.y1;

  lv_draw_rect_dsc_t rectangle;
  lv_draw_rect_dsc_init(&rectangle);
  rectangle.bg_opa = focused ? static_cast<lv_opa_t>(155)
                             : static_cast<lv_opa_t>(96);
  rectangle.bg_color = focused ? rgb(34, 23, 7) : rgb(3, 12, 17);
  rectangle.border_opa = focused ? LV_OPA_COVER
                                 : static_cast<lv_opa_t>(115);
  rectangle.border_color = focused ? rgb(255, 190, 70)
                                   : airportColor(airport.category);
  rectangle.border_width = 1;
  rectangle.radius = focused ? 2 : 1;
  lv_canvas_draw_rect(radarView.canvas, placement.x1, placement.y1,
                      labelWidth, labelHeight, &rectangle);

  lv_draw_label_dsc_t label;
  lv_draw_label_dsc_init(&label);
  label.font = &lv_font_montserrat_12;
  label.color = focused ? rgb(255, 225, 135)
                        : airportLabelColor(airport.category);
  lv_canvas_draw_text(radarView.canvas, placement.x1 + 4, placement.y1 + 2,
                      labelWidth - 8, &label, airport.ident);
}

void drawAirportLabels(float rangeMiles) {
  airportLabelStatsRangeIndex = airport_data::rangeIndex(rangeMiles);
  airportLabelStatsMask = airportFrameLabelMask;
  airportLabelStatsCount = 0;
  airportLabelStatsValid = true;
  const bool hasManualShow = settings::airportLabelOverrideCount(
      settings::AirportLabelMode::SHOW) > 0;
  const bool hasFocus = airportFocusActive();
  if (!airportWork || !airportLabelBoxes ||
      (airportFrameLabelMask == 0 && !hasManualShow && !hasFocus)) {
    return;
  }

  // A temporary Airport Profile focus is placed first and can override HIDE,
  // disabled categories, and the global airport-overlay switch without changing
  // any saved setting. It still obeys the fixed bounds and reserved UI areas.
  if (hasFocus) {
    for (uint16_t index = 0; index < airportFrameCount; ++index) {
      const airport_data::NearbyAirport& airport = airportWork[index];
      if (!airportIsFocused(airport)) continue;
      LabelBox placement{};
      if (placeAirportLabel(airport, rangeMiles, airportLabelBoxes,
                            airportLabelStatsCount, placement)) {
        drawAirportLabel(airport, placement, true);
        recordVisibleAirportLabel(airport, placement);
      }
      break;
    }
  }

  // Manual SHOW entries are placed next, nearest first, so they can displace
  // automatic labels without changing the fixed collision rules.
  for (uint16_t index = 0; index < airportFrameCount; ++index) {
    const airport_data::NearbyAirport& airport = airportWork[index];
    if (airportIsFocused(airport) ||
        settings::airportLabelMode(airport.ident) !=
            settings::AirportLabelMode::SHOW) {
      continue;
    }
    LabelBox placement{};
    if (!placeAirportLabel(airport, rangeMiles, airportLabelBoxes,
                           airportLabelStatsCount, placement)) {
      continue;
    }
    drawAirportLabel(airport, placement);
    recordVisibleAirportLabel(airport, placement);
  }

  // AUTO entries retain deterministic major/public/private/heliport priority,
  // with distance ordering supplied by the bounded nearby cache. HIDE entries
  // are never attempted and SHOW/focused entries are not drawn a second time.
  for (uint8_t category = 0; category < airport_data::CATEGORY_COUNT;
       ++category) {
    const uint8_t categoryMask = static_cast<uint8_t>(1U << category);
    if ((airportFrameLabelMask & categoryMask) == 0) continue;
    for (uint16_t index = 0; index < airportFrameCount; ++index) {
      const airport_data::NearbyAirport& airport = airportWork[index];
      if (airportIsFocused(airport) ||
          static_cast<uint8_t>(airport.category) != category ||
          settings::airportLabelMode(airport.ident) !=
              settings::AirportLabelMode::AUTO) {
        continue;
      }
      LabelBox placement{};
      if (!placeAirportLabel(airport, rangeMiles, airportLabelBoxes,
                             airportLabelStatsCount, placement)) {
        continue;
      }
      drawAirportLabel(airport, placement);
      recordVisibleAirportLabel(airport, placement);
    }
  }
}

uint8_t radarContactHeadingIndex(float trackDegrees) {
  if (!isfinite(trackDegrees)) return 0;
  float normalized = fmodf(trackDegrees, 360.0f);
  if (normalized < 0.0f) normalized += 360.0f;
  const float bucketWidth =
      360.0f / static_cast<float>(RADAR_CONTACT_HEADING_COUNT);
  const uint8_t index = static_cast<uint8_t>(
      floorf((normalized + bucketWidth * 0.5f) / bucketWidth));
  return index & (RADAR_CONTACT_HEADING_COUNT - 1U);
}

void drawRadarBitmapContact(int centerX, int centerY,
                            AircraftBitmapId bitmapId,
                            uint8_t headingIndex, lv_color_t color) {
  const int startX = centerX - RADAR_CONTACT_BITMAP_W / 2;
  const int startY = centerY - RADAR_CONTACT_BITMAP_H / 2;
  const uint8_t* sprite = radarContactBitmap(bitmapId, headingIndex);

  for (uint8_t destinationY = 0; destinationY < RADAR_CONTACT_BITMAP_H;
       ++destinationY) {
    for (uint8_t destinationX = 0; destinationX < RADAR_CONTACT_BITMAP_W;
         ++destinationX) {
      if (!radarContactPixel(sprite, destinationX, destinationY)) continue;

      const int x = startX + destinationX;
      const int y = startY + destinationY;
      if (x >= RANGE_CONTROL_X1 && y >= RANGE_CONTROL_Y1) continue;
      putPixel(x, y, color);
    }
  }
}

void drawRadarBitmapContactWithOutline(int centerX, int centerY,
                                       AircraftBitmapId bitmapId,
                                       uint8_t headingIndex,
                                       lv_color_t color) {
  const lv_color_t outline = rgb(2, 8, 12);
  drawRadarBitmapContact(centerX - 1, centerY, bitmapId, headingIndex, outline);
  drawRadarBitmapContact(centerX + 1, centerY, bitmapId, headingIndex, outline);
  drawRadarBitmapContact(centerX, centerY - 1, bitmapId, headingIndex, outline);
  drawRadarBitmapContact(centerX, centerY + 1, bitmapId, headingIndex, outline);
  drawRadarBitmapContact(centerX, centerY, bitmapId, headingIndex, color);
}

void drawContacts(aircraft::Target* workTargets, uint8_t count,
                  float projectionSeconds, float rangeMiles,
                  const app_state::Snapshot& snapshot, const char* selectedHex,
                  ContactFrame& frame) {
  const lv_color_t cyan = rgb(80, 210, 255);
  const lv_color_t sweepBitmapTint = rgb(225, 205, 105);
  const lv_color_t sweepDotHalo = rgb(255, 220, 80);
  const lv_color_t amber = rgb(255, 190, 70);
  const lv_color_t red = rgb(255, 80, 80);
  frame.count = 0;
  frame.trackedVisible = false;
  frame.trackedX = 0;
  frame.trackedY = 0;
  frame.trackedTargetIndex = -1;
  renderedHitCount = 0;

  for (uint8_t i = 0; i < count; ++i) {
    float projectedDistance = workTargets[i].distanceMiles;
    float projectedBearing = workTargets[i].bearing;
    if (workTargets[i].hasTrack && workTargets[i].speedKt > 5.0f &&
        projectionSeconds > 0.0f) {
      float bearingRadians = workTargets[i].bearing * M_PI / 180.0f;
      float northMiles = cosf(bearingRadians) * workTargets[i].distanceMiles;
      float eastMiles = sinf(bearingRadians) * workTargets[i].distanceMiles;
      float travelMiles = workTargets[i].speedKt * 1.15078f *
                          projectionSeconds / 3600.0f;
      float trackRadians = workTargets[i].track * M_PI / 180.0f;
      northMiles += cosf(trackRadians) * travelMiles;
      eastMiles += sinf(trackRadians) * travelMiles;
      projectedDistance = sqrtf(northMiles * northMiles + eastMiles * eastMiles);
      projectedBearing = fmodf(
          atan2f(eastMiles, northMiles) * 180.0f / M_PI + 360.0f, 360.0f);
    }
    float ratio = projectedDistance / rangeMiles;
    if (ratio > 1.0f) continue;
    float bearingRadians = projectedBearing * M_PI / 180.0f;
    int x = CENTER_X + (int)(sin(bearingRadians) * RADIUS * ratio);
    int y = CENTER_Y - (int)(cos(bearingRadians) * RADIUS * ratio);
    const float behind =
        fmodf(sweepDegrees - projectedBearing + 360.0f, 360.0f);
    bool contactIsTracked =
        app_state::isManuallyTracked(workTargets[i], snapshot);
    bool contactIsSelected = !snapshot.manualTracking && selectedHex &&
                             selectedHex[0] && workTargets[i].hex[0] &&
                             strcmp(workTargets[i].hex, selectedHex) == 0;

    uint8_t hitIndex = UINT8_MAX;
    if (workTargets[i].hex[0] && renderedHitCount < aircraft::MAX_TARGETS) {
      hitIndex = renderedHitCount++;
      HitRegion& hit = renderedHits[hitIndex];
      hit = HitRegion{};
      strncpy(hit.hex, workTargets[i].hex, sizeof(hit.hex) - 1);
      hit.contactX = x;
      hit.contactY = y;
      hit.tracked = contactIsTracked;
      hit.selected = contactIsSelected;
    }
    if (frame.count < aircraft::MAX_TARGETS) {
      const uint8_t headingIndex =
          workTargets[i].hasTrack
              ? radarContactHeadingIndex(workTargets[i].track)
              : 0;
      frame.contacts[frame.count++] = {
        i, hitIndex, (int16_t)x, (int16_t)y, contactIsTracked,
        contactIsSelected, false, behind < 24.0f, headingIndex
      };
    }
  }

  uint8_t priorityOrder[aircraft::MAX_TARGETS]{};
  for (uint8_t contact = 0; contact < frame.count; ++contact) {
    priorityOrder[contact] = contact;
  }

  auto higherPriority = [&](uint8_t leftIndex, uint8_t rightIndex) {
    const ScreenContact& left = frame.contacts[leftIndex];
    const ScreenContact& right = frame.contacts[rightIndex];
    const uint8_t leftPriority = left.tracked ? 2 : (left.selected ? 1 : 0);
    const uint8_t rightPriority = right.tracked ? 2 : (right.selected ? 1 : 0);
    if (leftPriority != rightPriority) return leftPriority > rightPriority;

    const aircraft::Target& leftTarget = workTargets[left.targetIndex];
    const aircraft::Target& rightTarget = workTargets[right.targetIndex];
    if (leftTarget.distanceMiles != rightTarget.distanceMiles) {
      return leftTarget.distanceMiles < rightTarget.distanceMiles;
    }
    const int hexOrder = strcmp(leftTarget.hex, rightTarget.hex);
    if (hexOrder != 0) return hexOrder < 0;
    return left.targetIndex < right.targetIndex;
  };

  for (uint8_t contact = 1; contact < frame.count; ++contact) {
    const uint8_t candidate = priorityOrder[contact];
    uint8_t position = contact;
    while (position > 0 &&
           higherPriority(candidate, priorityOrder[position - 1])) {
      priorityOrder[position] = priorityOrder[position - 1];
      --position;
    }
    priorityOrder[position] = candidate;
  }

  if (rangeMiles <= 20.1f) {
    constexpr int overlapDistanceSquared =
        RADAR_CONTACT_OVERLAP_RADIUS * RADAR_CONTACT_OVERLAP_RADIUS;
    for (uint8_t higherRank = 0; higherRank < frame.count; ++higherRank) {
      ScreenContact& higher = frame.contacts[priorityOrder[higherRank]];
      for (uint8_t lowerRank = higherRank + 1; lowerRank < frame.count;
           ++lowerRank) {
        const ScreenContact& lower = frame.contacts[priorityOrder[lowerRank]];
        const int deltaX = higher.x - lower.x;
        const int deltaY = higher.y - lower.y;
        if (deltaX * deltaX + deltaY * deltaY <= overlapDistanceSquared) {
          higher.needsOutline = true;
          break;
        }
      }
    }
  }

  // Draw low-priority contacts first so tracked, selected, and nearer aircraft
  // remain on top without suppressing any contact. At 20 miles the sweep briefly
  // tints the normal bitmap at its existing size; at 40/80 miles the compact dot
  // retains its small yellow halo. Tags and priority-state colors remain stable.
  for (uint8_t drawRank = frame.count; drawRank > 0; --drawRank) {
    const ScreenContact& screen =
        frame.contacts[priorityOrder[drawRank - 1]];
    const bool sweepActive =
        screen.sweepHighlighted && !screen.selected && !screen.tracked;
    if (rangeMiles <= 20.1f) {
      const lv_color_t iconColor =
          screen.tracked
              ? red
              : (screen.selected
                     ? amber
                     : (sweepActive ? sweepBitmapTint : cyan));
      const AircraftBitmapId bitmapId =
          aircraft::bitmapForTarget(workTargets[screen.targetIndex]);
      if (screen.needsOutline) {
        drawRadarBitmapContactWithOutline(
            screen.x, screen.y, bitmapId, screen.headingIndex, iconColor);
      } else {
        drawRadarBitmapContact(
            screen.x, screen.y, bitmapId, screen.headingIndex, iconColor);
      }
    } else {
      const int contactRadius = screen.tracked ? 4 : 2;
      if (sweepActive) {
        fillCircle(screen.x, screen.y, contactRadius + 2, sweepDotHalo);
      }
      fillCircle(screen.x, screen.y, contactRadius,
                 screen.tracked ? red : (screen.selected ? amber : cyan));
    }

    if (screen.selected) {
      drawCircle(screen.x, screen.y, 8, amber);
      drawCircle(screen.x, screen.y, 12, amber);
    }
    if (screen.tracked) {
      drawCircle(screen.x, screen.y, 10, red);
      frame.trackedVisible = true;
      frame.trackedX = screen.x;
      frame.trackedY = screen.y;
      frame.trackedTargetIndex = screen.targetIndex;
    }
  }
}

void drawContactLabels(aircraft::Target* workTargets, float rangeMiles,
                       const app_state::Snapshot& snapshot,
                       const ContactFrame& frame) {
  (void)snapshot;
  LabelBox* labelBoxes = renderedLabelBoxes;
  uint16_t labelBoxCount = 0;

  // Reserve the visible range control, including its MILES caption.
  labelBoxes[labelBoxCount++] = {
    (int16_t)RANGE_CONTROL_X1, (int16_t)RANGE_CONTROL_Y1,
    (int16_t)(WIDTH - 1), (int16_t)(HEIGHT - 1)
  };

  if (frame.trackedVisible && frame.trackedTargetIndex >= 0) {
    const aircraft::Target& trackedTarget = workTargets[frame.trackedTargetIndex];
    const char* identifier = aircraft::primaryIdentifier(trackedTarget);
    char speedText[20];
    snprintf(speedText, sizeof(speedText), "%.0f MPH",
             trackedTarget.speedKt * 1.15078f);
    const char* lines[] = {"TRACKED", identifier, speedText};
    const lv_color_t colors[] = {
      rgb(255, 120, 100), rgb(255, 205, 195), rgb(255, 150, 130)
    };
    uint8_t hitIndex = UINT8_MAX;
    for (uint8_t contact = 0; contact < frame.count; ++contact) {
      if (frame.contacts[contact].tracked) {
        hitIndex = frame.contacts[contact].hitIndex;
        break;
      }
    }
    drawPlacedTag(
        frame.trackedX, frame.trackedY, lines, colors, 3, rgb(35, 12, 18),
        rgb(255, 105, 95), 118,
        rangeMiles <= 20.1f ? RADAR_CONTACT_TAG_CLEARANCE : DOT_TAG_CLEARANCE,
        hitIndex, labelBoxes, labelBoxCount);
  }

  for (uint8_t contact = 0; contact < frame.count; ++contact) {
    const ScreenContact& screen = frame.contacts[contact];
    if (!screen.selected || screen.tracked) continue;
    const aircraft::Target& target = workTargets[screen.targetIndex];
    const char* identifier = aircraft::primaryIdentifier(target);
    char distanceText[20];
    snprintf(distanceText, sizeof(distanceText), "%.1f MI",
             target.distanceMiles);
    const char* lines[] = {identifier, distanceText};
    const lv_color_t colors[] = {rgb(110, 225, 255), rgb(255, 195, 75)};
    drawPlacedTag(
        screen.x, screen.y, lines, colors, 2, rgb(7, 22, 27),
        rgb(255, 190, 70), 104,
        rangeMiles <= 20.1f ? RADAR_CONTACT_TAG_CLEARANCE : DOT_TAG_CLEARANCE,
        screen.hitIndex, labelBoxes, labelBoxCount);
  }

  if (rangeMiles <= 20.1f) {
    for (uint8_t contact = 0; contact < frame.count; ++contact) {
      const ScreenContact& screen = frame.contacts[contact];
      if (screen.tracked || screen.selected) continue;
      const aircraft::Target& target = workTargets[screen.targetIndex];
      const char* identifier = aircraft::primaryIdentifier(target);
      const char* lines[] = {identifier};
      const lv_color_t colors[] = {rgb(115, 225, 255)};
      drawPlacedTag(
          screen.x, screen.y, lines, colors, 1, rgb(5, 20, 28),
          rgb(28, 100, 104), 88, RADAR_CONTACT_TAG_CLEARANCE,
          screen.hitIndex, labelBoxes, labelBoxCount);
    }
  }

}

void updateSideIcon(lv_obj_t* canvas, lv_color_t* buffer,
                    const aircraft::Target* target, bool visible) {
  if (!canvas || !buffer) return;
  if (!visible || !target) {
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  drawSideBitmapIcon(canvas, buffer,
                     aircraft::bitmapForTarget(*target));
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
}

void updateHeadingDisplay(lv_obj_t* arrow, lv_obj_t* label,
                          lv_point_t* points,
                          const aircraft::Target* target) {
  if (!arrow || !label || !points) return;
  if (!target || !target->hasTrack) {
    lv_obj_add_flag(arrow, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(label, "HDG\n--");
    return;
  }

  constexpr float center = 21.0f;
  const float radians = target->track * M_PI / 180.0f;
  const float directionX = sinf(radians);
  const float directionY = -cosf(radians);
  const float perpendicularX = -directionY;
  const float perpendicularY = directionX;
  const float tipX = center + directionX * 17.0f;
  const float tipY = center + directionY * 17.0f;
  const float headBaseX = tipX - directionX * 9.0f;
  const float headBaseY = tipY - directionY * 9.0f;
  points[0] = {(lv_coord_t)(center - directionX * 14.0f),
               (lv_coord_t)(center - directionY * 14.0f)};
  points[1] = {(lv_coord_t)tipX, (lv_coord_t)tipY};
  points[2] = {(lv_coord_t)(headBaseX + perpendicularX * 6.0f),
               (lv_coord_t)(headBaseY + perpendicularY * 6.0f)};
  points[3] = points[1];
  points[4] = {(lv_coord_t)(headBaseX - perpendicularX * 6.0f),
               (lv_coord_t)(headBaseY - perpendicularY * 6.0f)};
  lv_line_set_points(arrow, points, 5);
  lv_obj_clear_flag(arrow, LV_OBJ_FLAG_HIDDEN);
  int headingDegrees = (int)lroundf(target->track) % 360;
  if (headingDegrees < 0) headingDegrees += 360;
  char headingText[24];
  snprintf(headingText, sizeof(headingText), "HDG\n%03d %s", headingDegrees,
           aircraft::compassDirection(target->track));
  lv_label_set_text(label, headingText);
}

void resetVerticalStateDisplay() {
  priorityVerticalState = vertical_state::State::LEVEL;
  priorityVerticalStateHex[0] = 0;
  priorityVerticalStateInitialized = false;
}

void updateVerticalStateDisplay(lv_obj_t* canvas, lv_color_t* buffer,
                                lv_obj_t* label,
                                const aircraft::Target* target) {
  if (!canvas || !buffer || !label) return;
  if (!target) {
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    resetVerticalStateDisplay();
    return;
  }

  const bool sameAircraft =
      priorityVerticalStateInitialized && target->hex[0] &&
      strcmp(priorityVerticalStateHex, target->hex) == 0;
  if (sameAircraft) {
    priorityVerticalState = vertical_state::updateState(
        priorityVerticalState, target->verticalRateFpm);
  } else {
    priorityVerticalState =
        vertical_state::initialState(target->verticalRateFpm);
    if (target->hex[0]) {
      strncpy(priorityVerticalStateHex, target->hex, 6);
      priorityVerticalStateHex[6] = 0;
    } else {
      priorityVerticalStateHex[0] = 0;
    }
    priorityVerticalStateInitialized = true;
  }

  uint8_t red = 220;
  uint8_t green = 235;
  uint8_t blue = 240;
  switch (priorityVerticalState) {
    case vertical_state::State::CLIMBING:
      red = 70;
      green = 225;
      blue = 255;
      break;
    case vertical_state::State::DESCENDING:
      red = 255;
      green = 185;
      blue = 65;
      break;
    case vertical_state::State::LEVEL:
    default:
      break;
  }

  constexpr uint8_t backgroundRed = 10;
  constexpr uint8_t backgroundGreen = 18;
  constexpr uint8_t backgroundBlue = 25;
  const uint8_t* bitmap = verticalStateBitmap(priorityVerticalState);
  for (int pixel = 0;
       pixel < VERTICAL_STATE_ICON_WIDTH * VERTICAL_STATE_ICON_HEIGHT;
       ++pixel) {
    const uint8_t alpha = pgm_read_byte(bitmap + pixel);
    const uint16_t inverseAlpha = 255U - alpha;
    const uint8_t blendedRed = static_cast<uint8_t>(
        (backgroundRed * inverseAlpha + red * alpha + 127U) / 255U);
    const uint8_t blendedGreen = static_cast<uint8_t>(
        (backgroundGreen * inverseAlpha + green * alpha + 127U) / 255U);
    const uint8_t blendedBlue = static_cast<uint8_t>(
        (backgroundBlue * inverseAlpha + blue * alpha + 127U) / 255U);
    buffer[pixel] = rgb(blendedRed, blendedGreen, blendedBlue);
  }
  lv_obj_invalidate(canvas);

  const int roundedRate =
      vertical_state::roundedRateFpm(target->verticalRateFpm);
  char stateText[40];
  if (roundedRate > 0) {
    snprintf(stateText, sizeof(stateText), "%s\n+%d FT/MIN",
             vertical_state::stateName(priorityVerticalState), roundedRate);
  } else {
    snprintf(stateText, sizeof(stateText), "%s\n%d FT/MIN",
             vertical_state::stateName(priorityVerticalState), roundedRate);
  }
  lv_label_set_text(label, stateText);
  lv_obj_set_style_text_color(label, rgb(red, green, blue), 0);
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
}

void updateRadarSummary(aircraft::Target* workTargets, uint8_t count,
                        const app_state::Snapshot& snapshot,
                        const char* selectedHex) {
  char text[128];
  lv_label_set_text_fmt(radarView.countLabel, "%u", count);

  const aircraft::Target* nearestTarget = count > 0 ? &workTargets[0] : nullptr;
  if (radarView.leftNearestHex) radarView.leftNearestHex[0] = 0;
  if (nearestTarget && nearestTarget->hex[0] && radarView.leftNearestHex) {
    strncpy(radarView.leftNearestHex, nearestTarget->hex, 6);
    radarView.leftNearestHex[6] = 0;
  }
  if (radarView.leftNearestModeLabel) {
    lv_label_set_text(radarView.leftNearestModeLabel, "NEAREST");
    lv_obj_set_style_text_color(radarView.leftNearestModeLabel,
                                rgb(100, 170, 180), 0);
  }
  if (nearestTarget) {
    if (radarView.leftNearestCallsignLabel) {
      lv_label_set_text(radarView.leftNearestCallsignLabel,
                        aircraft::primaryIdentifier(*nearestTarget));
      lv_obj_set_style_text_color(radarView.leftNearestCallsignLabel,
                                  rgb(63, 255, 155), 0);
    }
    if (radarView.leftNearestSummaryLabel) {
      snprintf(text, sizeof(text),
               "%s %s\n%.1f mi %s\n%.0f ft\n%.0f MPH",
               aircraft::kindName(*nearestTarget),
               nearestTarget->typeCode, nearestTarget->distanceMiles,
               aircraft::compassDirection(nearestTarget->bearing),
               nearestTarget->altitudeFt,
               nearestTarget->speedKt * 1.15078f);
      lv_label_set_text(radarView.leftNearestSummaryLabel, text);
    }
  } else {
    if (radarView.leftNearestCallsignLabel) {
      lv_label_set_text(radarView.leftNearestCallsignLabel, "--");
    }
    if (radarView.leftNearestSummaryLabel) {
      lv_label_set_text(radarView.leftNearestSummaryLabel,
                        "Waiting for aircraft");
    }
  }

  const aircraft::Target* primaryTarget = nullptr;
  bool priorityAircraft = snapshot.manualTracking;

  if (snapshot.manualTracking) {
    for (uint8_t i = 0; i < count; ++i) {
      if (!app_state::isManuallyTracked(workTargets[i], snapshot)) continue;
      primaryTarget = &workTargets[i];
      break;
    }
    lv_label_set_text(radarView.aircraftModeLabel, "TRACKED AIRCRAFT");
    lv_obj_set_style_text_color(radarView.aircraftModeLabel,
                                rgb(255, 120, 100), 0);
    lv_obj_set_style_text_color(radarView.nearestCallsignLabel,
                                rgb(255, 150, 130), 0);
    lv_obj_set_style_line_color(radarView.headingArrow,
                                rgb(255, 120, 100), 0);
    lv_obj_set_style_text_color(radarView.headingLabel,
                                rgb(255, 150, 130), 0);
  } else if (selectedHex && selectedHex[0]) {
    for (uint8_t i = 0; i < count; ++i) {
      if (workTargets[i].hex[0] &&
          strcmp(workTargets[i].hex, selectedHex) == 0) {
        primaryTarget = &workTargets[i];
        break;
      }
    }
    priorityAircraft = primaryTarget != nullptr;
    if (primaryTarget) {
      lv_label_set_text(radarView.aircraftModeLabel, "SELECTED AIRCRAFT");
      lv_obj_set_style_text_color(radarView.aircraftModeLabel,
                                  rgb(255, 190, 70), 0);
      lv_obj_set_style_text_color(radarView.nearestCallsignLabel,
                                  rgb(255, 205, 90), 0);
      lv_obj_set_style_line_color(radarView.headingArrow,
                                  rgb(255, 190, 70), 0);
      lv_obj_set_style_text_color(radarView.headingLabel,
                                  rgb(255, 205, 90), 0);
    }
  }

  if (!priorityAircraft) {
    lv_label_set_text(radarView.aircraftModeLabel, "NEAREST 5 AIRCRAFT");
    lv_obj_set_style_text_color(radarView.aircraftModeLabel,
                                rgb(110, 220, 255), 0);
  }

  lv_obj_t* leftNearestLabels[] = {
    radarView.leftNearestModeLabel,
    radarView.leftNearestCallsignLabel,
    radarView.leftNearestSummaryLabel
  };
  for (lv_obj_t* label : leftNearestLabels) {
    if (!label) continue;
    if (priorityAircraft) lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
  updateSideIcon(radarView.leftNearestIcon,
                 radarView.leftNearestIconBuffer, nearestTarget,
                 !priorityAircraft && nearestTarget);
  if (radarView.leftNearestHeadingLabel) {
    if (priorityAircraft) {
      lv_obj_add_flag(radarView.leftNearestHeadingLabel,
                      LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(radarView.leftNearestHeadingLabel,
                        LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (priorityAircraft) {
    if (radarView.leftNearestHeadingArrow) {
      lv_obj_add_flag(radarView.leftNearestHeadingArrow,
                      LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    updateHeadingDisplay(radarView.leftNearestHeadingArrow,
                         radarView.leftNearestHeadingLabel,
                         leftNearestHeadingPoints, nearestTarget);
  }

  if (radarView.leftOtherModeLabel) {
    lv_obj_add_flag(radarView.leftOtherModeLabel, LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < PRIORITY_OTHER_COUNT; ++i) {
    if (radarView.leftOtherHexes[i]) radarView.leftOtherHexes[i][0] = 0;
    if (radarView.leftOtherLabels[i]) {
      lv_label_set_text(radarView.leftOtherLabels[i], "");
      lv_obj_add_flag(radarView.leftOtherLabels[i], LV_OBJ_FLAG_HIDDEN);
    }
    updateSideIcon(radarView.leftOtherIcons[i],
                   radarView.leftOtherIconBuffers[i], nullptr, false);
  }
  if (priorityAircraft) {
    uint8_t otherIndex = 0;
    for (uint8_t targetIndex = 0;
         targetIndex < count && otherIndex < PRIORITY_OTHER_COUNT;
         ++targetIndex) {
      const aircraft::Target& target = workTargets[targetIndex];
      if (primaryTarget && target.hex[0] && primaryTarget->hex[0] &&
          strcmp(target.hex, primaryTarget->hex) == 0) {
        continue;
      }
      if (radarView.leftOtherHexes[otherIndex] && target.hex[0]) {
        strncpy(radarView.leftOtherHexes[otherIndex], target.hex, 6);
        radarView.leftOtherHexes[otherIndex][6] = 0;
      }
      snprintf(text, sizeof(text), "%s\n%.1f MI %s",
               aircraft::primaryIdentifier(target), target.distanceMiles,
               aircraft::compassDirection(target.bearing));
      if (radarView.leftOtherLabels[otherIndex]) {
        lv_label_set_text(radarView.leftOtherLabels[otherIndex], text);
        lv_obj_clear_flag(radarView.leftOtherLabels[otherIndex],
                          LV_OBJ_FLAG_HIDDEN);
      }
      updateSideIcon(radarView.leftOtherIcons[otherIndex],
                     radarView.leftOtherIconBuffers[otherIndex], &target, true);
      ++otherIndex;
    }
    if (radarView.leftOtherModeLabel) {
      if (otherIndex > 0) {
        lv_label_set_text_fmt(radarView.leftOtherModeLabel, "NEAREST %u",
                              (unsigned)otherIndex);
      } else {
        lv_label_set_text(radarView.leftOtherModeLabel, "NO OTHER");
      }
      lv_obj_clear_flag(radarView.leftOtherModeLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }

  for (int i = 0; i < 5; ++i) {
    if (radarView.listLabels[i]) {
      if (priorityAircraft) {
        lv_obj_add_flag(radarView.listLabels[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(radarView.listLabels[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (priorityAircraft && radarView.listIcons[i]) {
      lv_obj_add_flag(radarView.listIcons[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (radarView.nearestCallsignLabel) {
    if (priorityAircraft) {
      lv_obj_clear_flag(radarView.nearestCallsignLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(radarView.nearestCallsignLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (radarView.nearestSummaryLabel) {
    if (priorityAircraft) {
      lv_obj_clear_flag(radarView.nearestSummaryLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(radarView.nearestSummaryLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (radarView.headingLabel) {
    if (priorityAircraft) {
      lv_obj_clear_flag(radarView.headingLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(radarView.headingLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!priorityAircraft && radarView.headingArrow) {
    lv_obj_add_flag(radarView.headingArrow, LV_OBJ_FLAG_HIDDEN);
  }
  if (!priorityAircraft) {
    updateVerticalStateDisplay(radarView.verticalStateIcon,
                               radarView.verticalStateIconBuffer,
                               radarView.verticalStateLabel, nullptr);
  }

  if (priorityAircraft && primaryTarget) {
    lv_label_set_text(radarView.nearestCallsignLabel,
                      aircraft::primaryIdentifier(*primaryTarget));
    const bool distinctRegistration =
        strcmp(primaryTarget->registration, "Unknown") != 0 &&
        strcmp(primaryTarget->registration, primaryTarget->id) != 0;
    if (distinctRegistration) {
      snprintf(text, sizeof(text),
               "%s  %s\n%s\n%.1f mi %s\n%.0f ft | %.0f MPH",
               aircraft::kindName(*primaryTarget),
               primaryTarget->typeCode, primaryTarget->registration,
               primaryTarget->distanceMiles,
               aircraft::compassDirection(primaryTarget->bearing),
               primaryTarget->altitudeFt,
               primaryTarget->speedKt * 1.15078f);
    } else {
      snprintf(text, sizeof(text),
               "%s  %s\n%.1f mi %s\n%.0f ft | %.0f MPH",
               aircraft::kindName(*primaryTarget),
               primaryTarget->typeCode, primaryTarget->distanceMiles,
               aircraft::compassDirection(primaryTarget->bearing),
               primaryTarget->altitudeFt,
               primaryTarget->speedKt * 1.15078f);
    }
    lv_label_set_text(radarView.nearestSummaryLabel, text);
    updateHeadingDisplay(radarView.headingArrow, radarView.headingLabel,
                         priorityHeadingPoints, primaryTarget);
    updateVerticalStateDisplay(radarView.verticalStateIcon,
                               radarView.verticalStateIconBuffer,
                               radarView.verticalStateLabel, primaryTarget);
  } else if (snapshot.manualTracking) {
    lv_label_set_text(radarView.nearestCallsignLabel, "--");
    lv_label_set_text(radarView.nearestSummaryLabel,
                      "TRACK SIGNAL LOST\nChecking next update");
    updateHeadingDisplay(radarView.headingArrow, radarView.headingLabel,
                         priorityHeadingPoints, nullptr);
    updateVerticalStateDisplay(radarView.verticalStateIcon,
                               radarView.verticalStateIconBuffer,
                               radarView.verticalStateLabel, nullptr);
  }
  updateSideIcon(radarView.priorityIcon, radarView.priorityIconBuffer,
                 primaryTarget, priorityAircraft && primaryTarget);

  for (int i = 0; i < 5; ++i) {
    if (radarView.listHexes[i]) radarView.listHexes[i][0] = 0;
    if (i < count) {
      if (workTargets[i].hex[0] && radarView.listHexes[i]) {
        strncpy(radarView.listHexes[i], workTargets[i].hex, 6);
        radarView.listHexes[i][6] = 0;
      }
      char altitude[16];
      aircraft::formatWholeNumber(workTargets[i].altitudeFt, altitude,
                                  sizeof(altitude));
      const bool distinctRegistration =
          strcmp(workTargets[i].registration, "Unknown") != 0 &&
          strcmp(workTargets[i].registration, workTargets[i].id) != 0;
      if (distinctRegistration) {
        snprintf(text, sizeof(text),
                 "%s  %s\n%s | %.1f mi %s\n%s ft | %.0f MPH",
                 workTargets[i].id, workTargets[i].typeCode,
                 workTargets[i].registration, workTargets[i].distanceMiles,
                 aircraft::compassDirection(workTargets[i].bearing), altitude,
                 workTargets[i].speedKt * 1.15078f);
      } else {
        snprintf(text, sizeof(text),
                 "%s  %s\n%.1f mi %s\n%s ft | %.0f MPH",
                 workTargets[i].id, workTargets[i].typeCode,
                 workTargets[i].distanceMiles,
                 aircraft::compassDirection(workTargets[i].bearing), altitude,
                 workTargets[i].speedKt * 1.15078f);
      }
      lv_label_set_text(radarView.listLabels[i], text);
      updateSideIcon(radarView.listIcons[i],
                     radarView.listIconBuffers[i], &workTargets[i],
                     !priorityAircraft);
    } else {
      lv_label_set_text(radarView.listLabels[i], "");
      updateSideIcon(radarView.listIcons[i],
                     radarView.listIconBuffers[i], nullptr, false);
    }
  }
}

}  // namespace
bool render(aircraft::Target* workTargets, const char* selectedHex) {
  if (!radarView.buffer || !radarView.canvas || !workTargets ||
      !renderedHits || !renderedContacts || !renderedLabelBoxes ||
      !contactFrame.contacts) {
    return false;
  }

  const uint32_t frameStartedMs = millis();
  const uint32_t frameStartedUs = micros();
  if (lastRenderStartedMs != 0) {
    const uint32_t frameGapMs = frameStartedMs - lastRenderStartedMs;
    if (frameGapMs <= ACTIVE_FRAME_GAP_LIMIT_MS) {
      performanceStats.lastFrameGapMs = frameGapMs;
      performanceStats.maximumFrameGapMs =
          max(performanceStats.maximumFrameGapMs, frameGapMs);
    } else {
      performanceStats.lastFrameGapMs = 0;
    }
  }
  lastRenderStartedMs = frameStartedMs;

  app_state::Snapshot snapshot;
  app_state::copySnapshot(workTargets, snapshot);
  const uint8_t count = snapshot.count;
  const uint32_t publishedAt = snapshot.lastUpdateMs;
  const uint32_t contactAgeMs =
      publishedAt ? frameStartedMs - publishedAt : 0;
  const float projectionSeconds =
      min(contactAgeMs, adsb::FETCH_INTERVAL_MS * 2UL) / 1000.0f;
  const float rangeMiles = snapshot.rangeMiles;
  renderedTwentyMileRange = rangeMiles <= 20.1f;
  bool selectedAvailable = !selectedHex || !selectedHex[0];
  if (!snapshot.manualTracking && selectedHex && selectedHex[0]) {
    selectedAvailable = false;
    for (uint8_t i = 0; i < count; ++i) {
      if (workTargets[i].hex[0] &&
          strcmp(workTargets[i].hex, selectedHex) == 0) {
        selectedAvailable = true;
        break;
      }
    }
  }

  // Expire a temporary airport focus even while the cached base is otherwise
  // unchanged. Focus changes invalidate both the cache and label statistics.
  airportFocusActive();

  if (radarBaseBuffer) {
    const bool rebuilt =
        !radarBaseValid ||
        radarBaseRangeGeneration != snapshot.rangeGeneration;
    if (rebuilt) {
      rebuildStaticRadarLayer(rangeMiles, snapshot.rangeGeneration);
      copyStaticRadarLayerToDisplay();
    } else if (contactFrame.count <= DIRTY_RESTORE_CONTACT_LIMIT) {
      restorePreviousDynamicLayer(sweepDegrees);
    } else {
      // Dense 40/80-mile frames are faster as one bounded PSRAM copy than as
      // thousands of tiny row restores. The static airport work still remains
      // cached and is not recalculated.
      copyStaticRadarLayerToDisplay();
    }
  } else {
    drawRadarBase();
    prepareAirportFrame(rangeMiles);
    drawAirportLabels(rangeMiles);
    drawAirportSymbols(rangeMiles);
  }

  advanceSweep(frameStartedMs);
  drawRadarSweep(sweepDegrees);
  drawContacts(workTargets, count, projectionSeconds, rangeMiles, snapshot,
               selectedHex, contactFrame);
  drawContactLabels(workTargets, rangeMiles, snapshot, contactFrame);
  lv_obj_invalidate(radarView.canvas);

  static uint32_t lastSummaryTargetVersion = UINT32_MAX;
  static uint32_t lastSummaryRangeGeneration = UINT32_MAX;
  static uint32_t lastSummaryTrackingVersion = UINT32_MAX;
  static char lastSummarySelectedHex[7]{};
  if (snapshot.targetVersion != lastSummaryTargetVersion ||
      snapshot.rangeGeneration != lastSummaryRangeGeneration ||
      snapshot.trackingVersion != lastSummaryTrackingVersion ||
      strncmp(lastSummarySelectedHex, selectedHex ? selectedHex : "",
              sizeof(lastSummarySelectedHex)) != 0) {
    lastSummaryTargetVersion = snapshot.targetVersion;
    lastSummaryRangeGeneration = snapshot.rangeGeneration;
    lastSummaryTrackingVersion = snapshot.trackingVersion;
    strncpy(lastSummarySelectedHex, selectedHex ? selectedHex : "",
            sizeof(lastSummarySelectedHex) - 1);
    lastSummarySelectedHex[sizeof(lastSummarySelectedHex) - 1] = 0;
    updateRadarSummary(workTargets, count, snapshot, selectedHex);
  }

  const uint32_t renderUs = micros() - frameStartedUs;
  performanceStats.lastRenderUs = renderUs;
  performanceStats.maximumRenderUs =
      max(performanceStats.maximumRenderUs, renderUs);
  return selectedAvailable;
}

void copyPerformanceStats(PerformanceStats& stats) {
  stats = performanceStats;
}

bool hitTest(int canvasX, int canvasY, HitResult& result) {
  result = HitResult{};
  if (!renderedHits) return false;

  auto copyResult = [&](int index) {
    if (index < 0) return false;
    const HitRegion& hit = renderedHits[index];
    strncpy(result.hex, hit.hex, sizeof(result.hex) - 1);
    result.tracked = hit.tracked;
    result.selected = hit.selected;
    return result.hex[0] != 0;
  };

  auto priorityFor = [](const HitRegion& hit) {
    return hit.tracked ? 2 : (hit.selected ? 1 : 0);
  };

  auto squaredDistanceToTagCenter = [&](const HitRegion& hit) {
    const int centerX = (hit.tag.x1 + hit.tag.x2) / 2;
    const int centerY = (hit.tag.y1 + hit.tag.y2) / 2;
    const int deltaX = canvasX - centerX;
    const int deltaY = canvasY - centerY;
    return deltaX * deltaX + deltaY * deltaY;
  };

  auto squaredDistanceToTag = [&](const HitRegion& hit) {
    const int nearestX =
        canvasX < hit.tag.x1 ? hit.tag.x1
                             : (canvasX > hit.tag.x2 ? hit.tag.x2 : canvasX);
    const int nearestY =
        canvasY < hit.tag.y1 ? hit.tag.y1
                             : (canvasY > hit.tag.y2 ? hit.tag.y2 : canvasY);
    const int deltaX = canvasX - nearestX;
    const int deltaY = canvasY - nearestY;
    return deltaX * deltaX + deltaY * deltaY;
  };

  auto betterCandidate = [&](uint8_t index, int primaryDistance,
                             int secondaryDistance, int bestIndex,
                             int bestPrimaryDistance,
                             int bestSecondaryDistance) {
    if (bestIndex < 0) return true;
    const HitRegion& candidate = renderedHits[index];
    const HitRegion& best = renderedHits[bestIndex];
    const int candidatePriority = priorityFor(candidate);
    const int bestPriority = priorityFor(best);
    if (candidatePriority != bestPriority) {
      return candidatePriority > bestPriority;
    }
    if (primaryDistance != bestPrimaryDistance) {
      return primaryDistance < bestPrimaryDistance;
    }
    if (secondaryDistance != bestSecondaryDistance) {
      return secondaryDistance < bestSecondaryDistance;
    }
    return strcmp(candidate.hex, best.hex) < 0;
  };

  if (renderedTwentyMileRange) {
    // A direct press on a visible tag is authoritative. Tag rectangles are
    // collision-placed, but priority and center distance keep any edge case
    // deterministic without allowing a nearby aircraft icon to steal the press.
    int bestIndex = -1;
    int bestCenterDistance = INT_MAX;
    for (uint8_t i = 0; i < renderedHitCount; ++i) {
      const HitRegion& hit = renderedHits[i];
      if (!hit.hasTag || canvasX < hit.tag.x1 || canvasX > hit.tag.x2 ||
          canvasY < hit.tag.y1 || canvasY > hit.tag.y2) {
        continue;
      }
      const int centerDistance = squaredDistanceToTagCenter(hit);
      if (betterCandidate(i, centerDistance, 0, bestIndex,
                          bestCenterDistance, 0)) {
        bestIndex = i;
        bestCenterDistance = centerDistance;
      }
    }
    if (copyResult(bestIndex)) return true;

    // A small invisible pad catches edge touches. When neighboring pads overlap,
    // choose the tag whose visible rectangle is actually nearest, then its center.
    constexpr int TAG_TOUCH_PADDING = 4;
    bestIndex = -1;
    int bestRectangleDistance = INT_MAX;
    bestCenterDistance = INT_MAX;
    for (uint8_t i = 0; i < renderedHitCount; ++i) {
      const HitRegion& hit = renderedHits[i];
      if (!hit.hasTag ||
          canvasX < hit.tag.x1 - TAG_TOUCH_PADDING ||
          canvasX > hit.tag.x2 + TAG_TOUCH_PADDING ||
          canvasY < hit.tag.y1 - TAG_TOUCH_PADDING ||
          canvasY > hit.tag.y2 + TAG_TOUCH_PADDING) {
        continue;
      }
      const int rectangleDistance = squaredDistanceToTag(hit);
      const int centerDistance = squaredDistanceToTagCenter(hit);
      if (betterCandidate(i, rectangleDistance, centerDistance, bestIndex,
                          bestRectangleDistance, bestCenterDistance)) {
        bestIndex = i;
        bestRectangleDistance = rectangleDistance;
        bestCenterDistance = centerDistance;
      }
    }
    if (copyResult(bestIndex)) return true;

    constexpr int CONTACT_TOUCH_RADIUS = 18;
    constexpr int CONTACT_TOUCH_RADIUS_SQUARED =
        CONTACT_TOUCH_RADIUS * CONTACT_TOUCH_RADIUS;
    bestIndex = -1;
    int bestContactDistance = INT_MAX;
    for (uint8_t i = 0; i < renderedHitCount; ++i) {
      const HitRegion& hit = renderedHits[i];
      const int deltaX = canvasX - hit.contactX;
      const int deltaY = canvasY - hit.contactY;
      const int distanceSquared = deltaX * deltaX + deltaY * deltaY;
      if (distanceSquared > CONTACT_TOUCH_RADIUS_SQUARED) continue;
      if (betterCandidate(i, distanceSquared, 0, bestIndex,
                          bestContactDistance, 0)) {
        bestIndex = i;
        bestContactDistance = distanceSquared;
      }
    }
    return copyResult(bestIndex);
  }

  // Preserve the established 40/80-mile behavior: selected/tracked tags and
  // compact contact dots participate in the same priority-and-distance contest.
  int bestIndex = -1;
  int bestPriority = -1;
  int bestDistanceSquared = INT_MAX;
  auto consider = [&](uint8_t index) {
    const HitRegion& hit = renderedHits[index];
    const int priority = priorityFor(hit);
    const int deltaX = canvasX - hit.contactX;
    const int deltaY = canvasY - hit.contactY;
    const int distanceSquared = deltaX * deltaX + deltaY * deltaY;
    if (priority > bestPriority ||
        (priority == bestPriority && distanceSquared < bestDistanceSquared)) {
      bestIndex = index;
      bestPriority = priority;
      bestDistanceSquared = distanceSquared;
    }
  };

  constexpr int TAG_TOUCH_EXPANSION = 6;
  for (uint8_t i = 0; i < renderedHitCount; ++i) {
    const HitRegion& hit = renderedHits[i];
    if (!hit.hasTag) continue;
    if (canvasX >= hit.tag.x1 - TAG_TOUCH_EXPANSION &&
        canvasX <= hit.tag.x2 + TAG_TOUCH_EXPANSION &&
        canvasY >= hit.tag.y1 - TAG_TOUCH_EXPANSION &&
        canvasY <= hit.tag.y2 + TAG_TOUCH_EXPANSION) {
      consider(i);
    }
  }

  constexpr int CONTACT_TOUCH_RADIUS = 18;
  constexpr int CONTACT_TOUCH_RADIUS_SQUARED =
      CONTACT_TOUCH_RADIUS * CONTACT_TOUCH_RADIUS;
  for (uint8_t i = 0; i < renderedHitCount; ++i) {
    const HitRegion& hit = renderedHits[i];
    const int deltaX = canvasX - hit.contactX;
    const int deltaY = canvasY - hit.contactY;
    if (deltaX * deltaX + deltaY * deltaY <=
        CONTACT_TOUCH_RADIUS_SQUARED) {
      consider(i);
    }
  }

  return copyResult(bestIndex);
}

}  // namespace radar
