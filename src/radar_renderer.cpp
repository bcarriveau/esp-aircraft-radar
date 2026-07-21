#include "radar_renderer.h"

#include <Arduino.h>
#include <math.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_bitmaps.h"

namespace radar {
namespace {

constexpr int CENTER_X = WIDTH / 2;
constexpr int CENTER_Y = HEIGHT / 2;
constexpr int RADIUS = 168;

View radarView;
float sweepDegrees = 0;

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
  int16_t x;
  int16_t y;
  bool tracked;
};

struct LabelBox { int16_t x1, y1, x2, y2; };

bool drawPlacedLabel(int dotX, int dotY, const char* text, lv_color_t color,
                     int maxWidth, LabelBox* labelBoxes,
                     uint8_t& labelBoxCount) {
  if (!text || !text[0]) return false;
  int lineCount = 1;
  int longestLine = 0;
  int currentLine = 0;
  for (const char* ch = text; *ch; ++ch) {
    if (*ch == '\n') {
      longestLine = max(longestLine, currentLine);
      currentLine = 0;
      ++lineCount;
    } else {
      ++currentLine;
    }
  }
  longestLine = max(longestLine, currentLine);
  const int labelHeight = lineCount * 15 + 1;
  int labelWidth = constrain(longestLine * 7 + 6, 44, maxWidth);
  int candidateX[6] = {
    dotX + 7, dotX - labelWidth - 7, dotX - labelWidth / 2,
    dotX - labelWidth / 2, dotX + 7, dotX - labelWidth - 7
  };
  int candidateY[6] = {
    dotY - labelHeight / 2, dotY - labelHeight / 2,
    dotY - labelHeight - 7, dotY + 7, dotY + 7, dotY + 7
  };
  for (int candidate = 0; candidate < 6; ++candidate) {
    int labelX = constrain(candidateX[candidate], 2, WIDTH - labelWidth - 2);
    int labelY = constrain(candidateY[candidate], 2, HEIGHT - labelHeight - 2);
    int x2 = labelX + labelWidth;
    int y2 = labelY + labelHeight;
    bool overlaps = false;
    for (uint8_t box = 0; box < labelBoxCount; ++box) {
      const LabelBox& used = labelBoxes[box];
      if (!(x2 + 3 < used.x1 || labelX - 3 > used.x2 ||
            y2 + 2 < used.y1 || labelY - 2 > used.y2)) {
        overlaps = true;
        break;
      }
    }
    if (overlaps) continue;
    lv_draw_label_dsc_t labelDescription;
    lv_draw_label_dsc_init(&labelDescription);
    labelDescription.color = color;
    labelDescription.font = &lv_font_montserrat_12;
    lv_canvas_draw_text(radarView.canvas, labelX, labelY, labelWidth,
                        &labelDescription, text);
    if (labelBoxCount < aircraft::MAX_TARGETS + 2) {
      labelBoxes[labelBoxCount++] = {
        (int16_t)labelX, (int16_t)labelY, (int16_t)x2, (int16_t)y2
      };
    }
    return true;
  }
  return false;
}

}  // namespace

void configure(const View& view) { radarView = view; }

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

  const uint16_t* sprite = aircraftBitmap(aircraft::bitmapForType(target.typeCode));
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

struct ContactFrame {
  ScreenContact contacts[aircraft::MAX_TARGETS];
  uint8_t count = 0;
  bool trackedVisible = false;
  int trackedX = 0;
  int trackedY = 0;
  int trackedTargetIndex = -1;
};

void drawRadarBackground() {
  const lv_color_t background = rgb(2, 8, 12);
  const lv_color_t grid = rgb(20, 58, 55);
  const lv_color_t green = rgb(50, 255, 135);
  for (int i = 0; i < WIDTH * HEIGHT; ++i) radarView.buffer[i] = background;
  drawCircle(CENTER_X, CENTER_Y, RADIUS, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS * 3 / 4, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS / 2, grid);
  drawCircle(CENTER_X, CENTER_Y, RADIUS / 4, grid);
  drawLine(CENTER_X - RADIUS, CENTER_Y, CENTER_X + RADIUS, CENTER_Y, grid);
  drawLine(CENTER_X, CENTER_Y - RADIUS, CENTER_X, CENTER_Y + RADIUS, grid);

  for (int tail = 18; tail >= 0; --tail) {
    float angle = (sweepDegrees - tail * 1.6f) * M_PI / 180.0f;
    int endX = CENTER_X + (int)(sin(angle) * RADIUS);
    int endY = CENTER_Y - (int)(cos(angle) * RADIUS);
    uint8_t intensity = 28 + (18 - tail) * 7;
    drawLine(CENTER_X, CENTER_Y, endX, endY,
             rgb(0, intensity, intensity / 2));
  }
  float angle = sweepDegrees * M_PI / 180.0f;
  drawLine(CENTER_X, CENTER_Y,
           CENTER_X + (int)(sin(angle) * RADIUS),
           CENTER_Y - (int)(cos(angle) * RADIUS), green);
  fillCircle(CENTER_X, CENTER_Y, 3, green);
}

void drawContacts(aircraft::Target* workTargets, uint8_t count,
                  float projectionSeconds, float rangeMiles,
                  const app_state::Snapshot& snapshot, ContactFrame& frame) {
  const lv_color_t bright = rgb(255, 220, 80);
  const lv_color_t cyan = rgb(80, 210, 255);
  frame.count = 0;
  frame.trackedVisible = false;
  frame.trackedX = 0;
  frame.trackedY = 0;
  frame.trackedTargetIndex = -1;

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
    float behind = fmodf(sweepDegrees - projectedBearing + 360.0f, 360.0f);
    bool contactIsTracked =
        app_state::isManuallyTracked(workTargets[i], snapshot);
    fillCircle(x, y, contactIsTracked ? 4 : 2,
               contactIsTracked ? rgb(255, 80, 80)
                                : (behind < 24.0f ? bright : cyan));
    if (frame.count < aircraft::MAX_TARGETS) {
      frame.contacts[frame.count++] = {
        i, (int16_t)x, (int16_t)y, contactIsTracked
      };
    }
    if (contactIsTracked) {
      drawCircle(x, y, 10, rgb(255, 80, 80));
      frame.trackedVisible = true;
      frame.trackedX = x;
      frame.trackedY = y;
      frame.trackedTargetIndex = i;
    }
  }
}

void drawContactLabels(aircraft::Target* workTargets, float rangeMiles,
                       const app_state::Snapshot& snapshot,
                       const ContactFrame& frame) {
  static LabelBox labelBoxes[aircraft::MAX_TARGETS + 2];
  uint8_t labelBoxCount = 0;
  // The compact live-status overlay occupies the radar's upper-left corner.
  labelBoxes[labelBoxCount++] = {2, 2, 286, 31};
  if (snapshot.manualTracking) {
    labelBoxes[labelBoxCount++] = {
      (int16_t)(WIDTH - 124), (int16_t)(HEIGHT - 46),
      (int16_t)(WIDTH - 2), (int16_t)(HEIGHT - 2)
    };
  }

  if (frame.trackedVisible && frame.trackedTargetIndex >= 0) {
    const aircraft::Target& trackedTarget = workTargets[frame.trackedTargetIndex];
    const char* identifier = aircraft::primaryIdentifier(trackedTarget);
    char trackedLabel[40];
    snprintf(trackedLabel, sizeof(trackedLabel), "TRACKED %s\n%.0f MPH",
             identifier, trackedTarget.speedKt * 1.15078f);
    drawPlacedLabel(frame.trackedX, frame.trackedY, trackedLabel, rgb(255, 120, 100),
                    154, labelBoxes, labelBoxCount);
  }

  if (rangeMiles <= 20.1f) {
    for (uint8_t contact = 0; contact < frame.count; ++contact) {
      const ScreenContact& screen = frame.contacts[contact];
      if (screen.tracked) continue;
      const aircraft::Target& target = workTargets[screen.targetIndex];
      const char* identifier = aircraft::primaryIdentifier(target);
      drawPlacedLabel(screen.x, screen.y, identifier, rgb(135, 225, 255),
                      86, labelBoxes, labelBoxCount);
    }
  }
}

void updateRadarSummary(aircraft::Target* workTargets, uint8_t count) {
  char text[96];
  lv_label_set_text_fmt(radarView.countLabel, "%u", count);
  if (count > 0) {
    lv_label_set_text(radarView.nearestCallsignLabel,
                      aircraft::primaryIdentifier(workTargets[0]));
    snprintf(text, sizeof(text), "%s %s\n%.1f mi %s\n%.0f ft\n%.0f MPH",
             aircraft::kindName(workTargets[0].typeCode), workTargets[0].typeCode,
             workTargets[0].distanceMiles,
             aircraft::compassDirection(workTargets[0].bearing),
             workTargets[0].altitudeFt, workTargets[0].speedKt * 1.15078f);
    lv_label_set_text(radarView.nearestSummaryLabel, text);
  } else {
    lv_label_set_text(radarView.nearestCallsignLabel, "--");
    lv_label_set_text(radarView.nearestSummaryLabel, "Waiting for aircraft");
  }
  for (int i = 0; i < 5; ++i) {
    if (i < count) {
      char altitude[16];
      aircraft::formatWholeNumber(workTargets[i].altitudeFt, altitude,
                                  sizeof(altitude));
      bool distinctRegistration =
          strcmp(workTargets[i].registration, "Unknown") != 0 &&
          strcmp(workTargets[i].registration, workTargets[i].id) != 0;
      if (distinctRegistration) {
        snprintf(text, sizeof(text), "%s  %s\n%s | %.1f mi %s\n%s ft | %.0f MPH",
                 workTargets[i].id, workTargets[i].typeCode,
                 workTargets[i].registration, workTargets[i].distanceMiles,
                 aircraft::compassDirection(workTargets[i].bearing), altitude,
                 workTargets[i].speedKt * 1.15078f);
      } else {
        snprintf(text, sizeof(text), "%s  %s\n%.1f mi %s\n%s ft | %.0f MPH",
                 workTargets[i].id, workTargets[i].typeCode,
                 workTargets[i].distanceMiles,
                 aircraft::compassDirection(workTargets[i].bearing), altitude,
                 workTargets[i].speedKt * 1.15078f);
      }
      lv_label_set_text(radarView.listLabels[i], text);
    } else {
      lv_label_set_text(radarView.listLabels[i], "");
    }
  }
}

}  // namespace

void render(aircraft::Target* workTargets) {
  sweepDegrees = fmodf(sweepDegrees + 2.2f, 360.0f);
  if (!radarView.buffer || !radarView.canvas || !workTargets) return;
  drawRadarBackground();

  app_state::Snapshot snapshot;
  app_state::copySnapshot(workTargets, snapshot);
  const uint8_t count = snapshot.count;
  const uint32_t publishedAt = snapshot.lastUpdateMs;
  const uint32_t contactAgeMs = publishedAt ? millis() - publishedAt : 0;
  const float projectionSeconds =
      min(contactAgeMs, adsb::FETCH_INTERVAL_MS * 2UL) / 1000.0f;
  const float rangeMiles = snapshot.rangeMiles;

  static ContactFrame frame;
  drawContacts(workTargets, count, projectionSeconds, rangeMiles, snapshot,
               frame);
  drawContactLabels(workTargets, rangeMiles, snapshot, frame);
  lv_obj_invalidate(radarView.canvas);

  static uint32_t lastSummaryTargetVersion = UINT32_MAX;
  static uint32_t lastSummaryRangeGeneration = UINT32_MAX;
  if (snapshot.targetVersion != lastSummaryTargetVersion ||
      snapshot.rangeGeneration != lastSummaryRangeGeneration) {
    lastSummaryTargetVersion = snapshot.targetVersion;
    lastSummaryRangeGeneration = snapshot.rangeGeneration;
    updateRadarSummary(workTargets, count);
  }
}

}  // namespace radar
