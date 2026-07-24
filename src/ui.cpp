#include "ui.h"

#include <WiFi.h>
#include <math.h>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_data.h"
#include "build_info.h"
#include "config.h"
#include "radar_renderer.h"
#include "settings.h"

namespace ui {
namespace {

constexpr uint32_t FRAME_INTERVAL_MS = 80;
constexpr uint8_t PAGE_COUNT = 5;
constexpr uint8_t NEAREST_LIST_COUNT = 5;
constexpr uint8_t RANGE_OPTION_COUNT = 3;
constexpr float RADAR_RANGES[RANGE_OPTION_COUNT] = {
  20.0f, 40.0f, 80.0f
};
constexpr const char* RADAR_RANGE_NAMES[RANGE_OPTION_COUNT] = {
  "20 MI", "40 MI", "80 MI"
};
constexpr float TRACK_AUTO_ZOOM_EDGE_RATIO = 0.92f;
constexpr float TRACK_AUTO_ZOOM_LOOKAHEAD_SECONDS = 30.0f;
constexpr uint32_t SELECTED_AIRCRAFT_TIMEOUT_MS = 30000;
constexpr uint8_t RADAR_SIDE_ICON_COUNT = NEAREST_LIST_COUNT + 2;
constexpr uint8_t LEFT_NEAREST_ICON_INDEX = 0;
constexpr uint8_t PRIORITY_ICON_INDEX = 1;
constexpr uint8_t LIST_ICON_BASE_INDEX = 2;
static_assert(LIST_ICON_BASE_INDEX + NEAREST_LIST_COUNT ==
                  RADAR_SIDE_ICON_COUNT,
              "Radar side-icon indexes do not match allocation");

aircraft::Target* uiTargets = nullptr;
lv_color_t* radarSideIconBuffers = nullptr;

lv_obj_t* radarCanvas = nullptr;
lv_color_t* radarBuffer = nullptr;
lv_obj_t* wifiLabel = nullptr;
lv_obj_t* clockLabel = nullptr;
lv_obj_t* countLabel = nullptr;
lv_obj_t* rangeLabel = nullptr;
lv_obj_t* leftNearestModeLabel = nullptr;
lv_obj_t* leftNearestCallsignLabel = nullptr;
lv_obj_t* leftNearestSummaryLabel = nullptr;
lv_obj_t* leftNearestIcon = nullptr;
lv_obj_t* leftNearestHeadingArrow = nullptr;
lv_obj_t* leftNearestHeadingLabel = nullptr;
lv_obj_t* aircraftModeLabel = nullptr;
lv_obj_t* nearestCallsignLabel = nullptr;
lv_obj_t* nearestSummaryLabel = nullptr;
lv_obj_t* priorityAircraftIcon = nullptr;
lv_obj_t* headingArrow = nullptr;
lv_obj_t* headingLabel = nullptr;
lv_obj_t* listLabels[NEAREST_LIST_COUNT]{};
lv_obj_t* listIcons[NEAREST_LIST_COUNT]{};
char leftNearestHex[7]{};
char nearestListHex[NEAREST_LIST_COUNT][7]{};
lv_obj_t* statusLabel = nullptr;
lv_obj_t* radarUntrackButton = nullptr;
lv_obj_t* radarRangeControl = nullptr;
lv_obj_t* radarRangeButtons[RANGE_OPTION_COUNT]{};
lv_obj_t* radarRangeButtonLabels[RANGE_OPTION_COUNT]{};
lv_obj_t* selectedInfoButton = nullptr;
lv_obj_t* selectedTrackButton = nullptr;
lv_obj_t* selectedClearButton = nullptr;
lv_obj_t* radarPanels[3]{};
lv_obj_t* tabButtons[PAGE_COUNT]{};
lv_obj_t* pagePanel = nullptr;
lv_obj_t* pageTitle = nullptr;
lv_obj_t* pageBody = nullptr;
lv_obj_t* headerTitle = nullptr;
lv_obj_t* settingsKeyboard = nullptr;
lv_obj_t* titleField = nullptr;
lv_obj_t* ssidField = nullptr;
lv_obj_t* passwordField = nullptr;
lv_obj_t* latitudeField = nullptr;
lv_obj_t* longitudeField = nullptr;
lv_obj_t* settingsFormLabels[5]{};
lv_obj_t* settingsStatusLabel = nullptr;
lv_obj_t* saveSettingsButton = nullptr;
lv_obj_t* resetSettingsButton = nullptr;
lv_obj_t* resetSettingsLabel = nullptr;
lv_obj_t* reconnectButton = nullptr;
lv_obj_t* retryButton = nullptr;
lv_obj_t* showPasswordButton = nullptr;
lv_obj_t* showPasswordLabel = nullptr;
lv_obj_t* setupRangeTitle = nullptr;
lv_obj_t* setupRangeButtons[RANGE_OPTION_COUNT]{};
lv_obj_t* tracksTable = nullptr;
lv_obj_t* detailPanel = nullptr;
lv_obj_t* detailTitle = nullptr;
lv_obj_t* detailBody = nullptr;
lv_obj_t* detailTrackButton = nullptr;
lv_obj_t* detailTrackLabel = nullptr;
lv_obj_t* detailPlaneCanvas = nullptr;
lv_color_t* detailPlaneBuffer = nullptr;

uint8_t currentPage = 0;
uint32_t lastTracksVersion = UINT32_MAX;
uint32_t lastTracksRangeGeneration = UINT32_MAX;
uint32_t lastAirspaceVersion = UINT32_MAX;
uint32_t lastAirspaceRangeGeneration = UINT32_MAX;
aircraft::Target detailTarget;
bool detailTargetValid = false;
bool passwordVisible = false;
bool resetConfirmationPending = false;
uint32_t resetConfirmationDeadline = 0;
uint32_t lastFrame = 0;
uint32_t lastHeaderUpdate = 0;
char selectedHex[7]{};
uint32_t selectedAtMs = 0;

enum class DetailOrigin : uint8_t {
  RADAR,
  TRACKS
};

DetailOrigin detailOrigin = DetailOrigin::TRACKS;

inline lv_color_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return lv_color_make(red, green, blue);
}

void stylePanel(lv_obj_t* object) {
  lv_obj_set_style_bg_color(object, rgb(10, 18, 25), 0);
  lv_obj_set_style_border_color(object, rgb(35, 76, 87), 0);
  lv_obj_set_style_border_width(object, 1, 0);
  lv_obj_set_style_radius(object, 8, 0);
  lv_obj_set_style_pad_all(object, 10, 0);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                    const lv_font_t* font, lv_color_t color, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_pos(label, x, y);
  return label;
}

lv_color_t* radarSideIconBuffer(uint8_t index) {
  if (!radarSideIconBuffers || index >= RADAR_SIDE_ICON_COUNT) return nullptr;
  return radarSideIconBuffers +
      index * radar::SIDE_ICON_WIDTH * radar::SIDE_ICON_HEIGHT;
}

lv_obj_t* makeRadarSideIcon(lv_obj_t* parent, uint8_t index, int x, int y) {
  lv_color_t* buffer = radarSideIconBuffer(index);
  if (!buffer) return nullptr;
  lv_obj_t* canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buffer, radar::SIDE_ICON_WIDTH,
                       radar::SIDE_ICON_HEIGHT, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(canvas, x, y);
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  return canvas;
}

void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  if (!label || !text) return;
  const char* current = lv_label_get_text(label);
  if (!current || strcmp(current, text) != 0) lv_label_set_text(label, text);
}

void setLabelTextFmtIfChanged(lv_obj_t* label, const char* format, ...) {
  char text[192];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  setLabelTextIfChanged(label, text);
}

void configurePageBody(const lv_font_t* font, int x, int y, int width) {
  lv_obj_set_style_text_font(pageBody, font, 0);
  lv_obj_set_pos(pageBody, x, y);
  lv_obj_set_width(pageBody, width);
}

void setVisible(lv_obj_t* object, bool visible) {
  if (!object) return;
  if (visible) lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void setTracksVisible(bool visible) {
  setVisible(tracksTable, visible);
}

void setSettingsFormVisible(bool visible) {
  setVisible(titleField, visible);
  setVisible(ssidField, visible);
  setVisible(passwordField, visible);
  setVisible(latitudeField, visible);
  setVisible(longitudeField, visible);
  setVisible(saveSettingsButton, visible);
  setVisible(resetSettingsButton, visible);
  setVisible(settingsStatusLabel, visible);
  for (lv_obj_t* label : settingsFormLabels) setVisible(label, visible);
  if (!visible) {
    lv_obj_add_flag(settingsKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void populateSettingsForm() {
  if (titleField) lv_textarea_set_text(titleField, settings::deviceTitle().c_str());
  if (ssidField) lv_textarea_set_text(ssidField, settings::wifiSsid().c_str());
  if (passwordField) lv_textarea_set_text(passwordField, settings::wifiPassword().c_str());
  if (latitudeField) {
    char latitudeText[32];
    snprintf(latitudeText, sizeof(latitudeText), "%.6f", settings::homeLatitude());
    lv_textarea_set_text(latitudeField, latitudeText);
  }
  if (longitudeField) {
    char longitudeText[32];
    snprintf(longitudeText, sizeof(longitudeText), "%.6f", settings::homeLongitude());
    lv_textarea_set_text(longitudeField, longitudeText);
  }
}

void setSettingsStatus(const char* text, lv_color_t color) {
  if (!settingsStatusLabel) return;
  lv_label_set_text(settingsStatusLabel, text);
  lv_obj_set_style_text_color(settingsStatusLabel, color, 0);
}

void closeSettingsKeyboard() {
  if (!settingsKeyboard) return;
  lv_obj_t* field = lv_keyboard_get_textarea(settingsKeyboard);
  lv_keyboard_set_textarea(settingsKeyboard, nullptr);
  lv_obj_add_flag(settingsKeyboard, LV_OBJ_FLAG_HIDDEN);
  if (field) lv_obj_clear_state(field, LV_STATE_FOCUSED);
}

void settingsKeyboardEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    closeSettingsKeyboard();
  }
}

void settingsFieldEvent(lv_event_t* event) {
  if (!settingsKeyboard) return;
  if (lv_event_get_code(event) == LV_EVENT_FOCUSED) {
    lv_obj_t* field = lv_event_get_target(event);
    lv_keyboard_set_textarea(settingsKeyboard, field);
    lv_keyboard_set_mode(
        settingsKeyboard,
        (field == latitudeField || field == longitudeField)
            ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_move_foreground(settingsKeyboard);
    lv_obj_clear_flag(settingsKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

bool parseCoordinate(const char* text, float& value) {
  if (!text || !text[0]) return false;
  char* end = nullptr;
  value = strtof(text, &end);
  if (end == text) return false;
  while (*end == ' ' || *end == '\t') ++end;
  return *end == 0 && isfinite(value);
}

void saveSettingsEvent(lv_event_t*) {
  const char* titleValue = lv_textarea_get_text(titleField);
  const char* ssidValue = lv_textarea_get_text(ssidField);
  const char* passwordValue = lv_textarea_get_text(passwordField);
  const char* latitudeValue = lv_textarea_get_text(latitudeField);
  const char* longitudeValue = lv_textarea_get_text(longitudeField);
  float latitude = 0;
  float longitude = 0;
  if (!parseCoordinate(latitudeValue, latitude) ||
      !parseCoordinate(longitudeValue, longitude) ||
      !settings::coordinatesValid(latitude, longitude)) {
    setSettingsStatus("Invalid latitude or longitude", rgb(255, 120, 110));
    return;
  }
  String previousSsid = settings::wifiSsid();
  String previousPassword = settings::wifiPassword();
  const float previousLatitude = settings::homeLatitude();
  const float previousLongitude = settings::homeLongitude();
  if (!settings::saveSettings(titleValue, ssidValue, passwordValue,
                              latitude, longitude)) {
    setSettingsStatus("Display name and SSID are required",
                      rgb(255, 120, 110));
    return;
  }
  const bool wifiChanged = previousSsid != settings::wifiSsid() ||
                           previousPassword != settings::wifiPassword();
  const bool locationChanged = fabsf(previousLatitude - latitude) > 0.00001f ||
                               fabsf(previousLongitude - longitude) > 0.00001f;
  if (locationChanged) app_state::invalidateRequests();
  setSettingsStatus(wifiChanged ? "Saved; reconnecting to new WiFi"
                                : "Settings saved",
                    rgb(120, 240, 155));
  setLabelTextIfChanged(headerTitle, settings::deviceTitle().c_str());
  if (wifiChanged) adsb::requestWifiReconnect();
  else adsb::requestRefresh();
}

void resetSettingsEvent(lv_event_t*) {
  const uint32_t now = millis();
  if (!resetConfirmationPending ||
      (int32_t)(now - resetConfirmationDeadline) >= 0) {
    resetConfirmationPending = true;
    resetConfirmationDeadline = now + 5000;
    setLabelTextIfChanged(resetSettingsLabel, "CONFIRM RESET");
    lv_obj_center(resetSettingsLabel);
    setSettingsStatus("Tap CONFIRM RESET within 5 seconds",
                      rgb(255, 220, 120));
    return;
  }
  resetConfirmationPending = false;
  setLabelTextIfChanged(resetSettingsLabel, "RESET DEFAULTS");
  lv_obj_center(resetSettingsLabel);
  settings::resetToDefaults();
  app_state::invalidateRequests();
  populateSettingsForm();
  setSettingsStatus("Defaults restored", rgb(255, 220, 120));
  setLabelTextIfChanged(headerTitle, settings::deviceTitle().c_str());
  adsb::requestWifiReconnect();
}

void updatePageContent();
void showTargetDetails(const aircraft::Target& target, DetailOrigin origin);

void selectPage(uint8_t page) {
  if (detailTargetValid) {
    if (detailOrigin == DetailOrigin::RADAR && selectedHex[0]) {
      selectedAtMs = millis();
    }
    detailTargetValid = false;
    if (detailPanel) lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  }
  currentPage = page < PAGE_COUNT ? page : 0;
  if (currentPage == 1) {
    lastTracksVersion = UINT32_MAX;
    lastTracksRangeGeneration = UINT32_MAX;
  }
  if (currentPage == 2) {
    lastAirspaceVersion = UINT32_MAX;
    lastAirspaceRangeGeneration = UINT32_MAX;
  }
  if (currentPage != 4 && resetConfirmationPending) {
    resetConfirmationPending = false;
    setLabelTextIfChanged(resetSettingsLabel, "RESET DEFAULTS");
    lv_obj_center(resetSettingsLabel);
  }
  for (int i = 0; i < PAGE_COUNT; ++i) {
    lv_obj_set_style_bg_color(
        tabButtons[i],
        i == currentPage ? rgb(24, 128, 84) : rgb(20, 38, 48), 0);
  }
  for (lv_obj_t* panel : radarPanels) {
    setVisible(panel, currentPage == 0);
  }
  setVisible(pagePanel, currentPage != 0);
  setSettingsFormVisible(currentPage == 4);
  setVisible(reconnectButton, currentPage == 4);
  setVisible(retryButton, currentPage == 4);
  setVisible(showPasswordButton, currentPage == 4);
  setVisible(setupRangeTitle, currentPage == 4);
  for (lv_obj_t* button : setupRangeButtons) {
    setVisible(button, currentPage == 4);
  }
  updatePageContent();
}

void tabEvent(lv_event_t* event) {
  selectPage((uint8_t)(uintptr_t)lv_event_get_user_data(event));
}

void reconnectEvent(lv_event_t*) {
  adsb::requestWifiReconnect();
  setSettingsStatus("WiFi reconnect queued", rgb(120, 220, 255));
  updatePageContent();
}

void retryEvent(lv_event_t*) {
  adsb::requestRefresh();
  setSettingsStatus("ADSB refresh queued", rgb(120, 220, 255));
}

void showPasswordEvent(lv_event_t*) {
  passwordVisible = !passwordVisible;
  lv_textarea_set_password_mode(passwordField, !passwordVisible);
  setLabelTextIfChanged(showPasswordLabel, passwordVisible ? "HIDE" : "SHOW");
  lv_obj_center(showPasswordLabel);
}

bool hasSelectedAircraft() { return selectedHex[0] != 0; }

void updateSelectedActions() {
  const bool tracking = app_state::hasManualTracking();
  const bool selected = hasSelectedAircraft() && !tracking;
  setVisible(selectedInfoButton, selected || tracking);
  setVisible(selectedTrackButton, selected);
  setVisible(selectedClearButton, selected);
  setVisible(radarUntrackButton, tracking);
}

void clearSelectedAircraft(bool announce = false) {
  if (!hasSelectedAircraft()) return;
  if (announce) {
    Serial.printf("Radar selection cleared: %s\n", selectedHex);
  }
  selectedHex[0] = 0;
  selectedAtMs = 0;
  updateSelectedActions();
}

void selectAircraftHex(const char* hex) {
  if (!hex || !hex[0] || app_state::hasManualTracking()) return;
  strncpy(selectedHex, hex, sizeof(selectedHex) - 1);
  selectedHex[sizeof(selectedHex) - 1] = 0;
  selectedAtMs = millis();
  updateSelectedActions();
  Serial.printf("Radar aircraft selected: %s\n", selectedHex);
}

bool copyVisibleTargetByHex(const char* hex, aircraft::Target& target) {
  if (!hex || !hex[0]) return false;
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (uiTargets[i].hex[0] && strcmp(uiTargets[i].hex, hex) == 0) {
      target = uiTargets[i];
      return true;
    }
  }
  return false;
}

bool copyTrackedTarget(aircraft::Target& target) {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  if (!snapshot.manualTracking) return false;
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (!app_state::isManuallyTracked(uiTargets[i], snapshot)) continue;
    target = uiTargets[i];
    return true;
  }
  return false;
}

void syncRadarRangeControlPosition() {
  if (!radarRangeControl) return;
  lv_obj_align(radarRangeControl, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
}

void syncRangeControls(float rangeMiles) {
  if (rangeLabel) {
    lv_label_set_text_fmt(rangeLabel, "RANGE // %d MILES",
                          (int)lroundf(rangeMiles));
  }
  for (int i = 0; i < RANGE_OPTION_COUNT; ++i) {
    const bool active = fabsf(rangeMiles - RADAR_RANGES[i]) < 1.0f;
    if (setupRangeButtons[i]) {
      lv_obj_set_style_bg_color(
          setupRangeButtons[i],
          active ? rgb(24, 128, 84) : rgb(20, 68, 82), 0);
    }
    if (radarRangeButtons[i]) {
      lv_obj_set_style_bg_color(
          radarRangeButtons[i],
          active ? rgb(24, 128, 84) : rgb(10, 38, 48), 0);
      lv_obj_set_style_border_width(radarRangeButtons[i], active ? 1 : 0, 0);
      lv_obj_set_style_border_color(radarRangeButtons[i],
                                    rgb(63, 255, 155), 0);
    }
    if (radarRangeButtonLabels[i]) {
      lv_obj_set_style_text_color(radarRangeButtonLabels[i],
                                  active ? rgb(240, 255, 245)
                                         : rgb(110, 220, 255), 0);
    }
  }
  syncRadarRangeControlPosition();
}

void rangeEvent(lv_event_t* event) {
  int index = (int)(intptr_t)lv_event_get_user_data(event);
  index = constrain(index, 0, RANGE_OPTION_COUNT - 1);
  if (!app_state::setRadarRangeMiles(RADAR_RANGES[index])) return;
  const float rangeMiles = app_state::radarRangeMiles();
  syncRangeControls(rangeMiles);
  Serial.printf("Radar range changed to %.0f miles\n", rangeMiles);
  adsb::requestRefresh();
}

float projectedTrackedDistance(const aircraft::Target& target,
                               float seconds) {
  if (!target.hasTrack || target.speedKt <= 5.0f) {
    return target.distanceMiles;
  }
  const float bearingRadians = target.bearing * M_PI / 180.0f;
  float northMiles = cosf(bearingRadians) * target.distanceMiles;
  float eastMiles = sinf(bearingRadians) * target.distanceMiles;
  const float travelMiles = target.speedKt * 1.15078f * seconds / 3600.0f;
  const float trackRadians = target.track * M_PI / 180.0f;
  northMiles += cosf(trackRadians) * travelMiles;
  eastMiles += sinf(trackRadians) * travelMiles;
  return sqrtf(northMiles * northMiles + eastMiles * eastMiles);
}

void autoExpandTrackedRange() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  if (!snapshot.manualTracking || snapshot.rangeMiles >= 79.9f) return;

  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (!app_state::isManuallyTracked(uiTargets[i], snapshot)) continue;
    const float projectedDistance = projectedTrackedDistance(
        uiTargets[i], TRACK_AUTO_ZOOM_LOOKAHEAD_SECONDS);
    if (projectedDistance <
        snapshot.rangeMiles * TRACK_AUTO_ZOOM_EDGE_RATIO) return;

    const float expandedRange = snapshot.rangeMiles < 39.9f ? 40.0f : 80.0f;
    if (!app_state::setRadarRangeMiles(expandedRange)) return;
    syncRangeControls(expandedRange);
    Serial.printf(
        "Tracked aircraft %s nearing %.0f-mile edge (projected %.1f mi); "
        "auto zooming to %.0f miles\n",
        aircraft::primaryIdentifier(uiTargets[i]), snapshot.rangeMiles,
        projectedDistance, expandedRange);
    adsb::requestRefresh();
    return;
  }
}

void showTargetDetails(const aircraft::Target& target,
                       DetailOrigin origin) {
  detailOrigin = origin;
  if (origin == DetailOrigin::RADAR) {
    for (lv_obj_t* panel : radarPanels) setVisible(panel, false);
    setVisible(pagePanel, true);
    setTracksVisible(false);
    if (pageBody) lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
    setSettingsFormVisible(false);
    setVisible(reconnectButton, false);
    setVisible(retryButton, false);
    setVisible(showPasswordButton, false);
    setVisible(setupRangeTitle, false);
    for (lv_obj_t* button : setupRangeButtons) setVisible(button, false);
  }
  detailTarget = target;
  detailTargetValid = true;
  lv_label_set_text_fmt(detailTitle, "%s // AIRCRAFT PROFILE", target.id);
  char details[768];
  snprintf(details, sizeof(details),
    "TYPE: %s  %s\nRegistration: %s\nICAO: %s\nOperator: %s\nDescription: %s\n\n"
    "Distance: %.1f miles\nBearing: %.0f deg %s\nAltitude: %.0f feet\n"
    "Speed: %.0f MPH\nHeading: %.0f deg\nVertical rate: %+.0f ft/min",
    aircraft::kindName(target.typeCode), target.typeCode, target.registration,
    target.hex[0] ? target.hex : "Unknown", target.operatorName,
    target.description, target.distanceMiles, target.bearing,
    aircraft::compassDirection(target.bearing), target.altitudeFt,
    target.speedKt * 1.15078f, target.track, target.verticalRateFpm);
  lv_label_set_text(detailBody, details);
  radar::drawAircraftPreview(detailPlaneCanvas, detailPlaneBuffer, target);
  lv_label_set_text(detailTrackLabel,
                    app_state::isManuallyTracked(target)
                        ? "STOP TRACKING" : "TRACK ON RADAR");
  lv_obj_move_foreground(detailPanel);
  lv_obj_clear_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
}

void tracksTableEvent(lv_event_t*) {
  uint16_t row = 0, column = 0;
  lv_table_get_selected_cell(tracksTable, &row, &column);
  if (row == LV_TABLE_CELL_NONE || row == 0) return;
  uint8_t count = 0;
  app_state::copyVisibleTargets(uiTargets, count);
  uint16_t index = row - 1;
  if (index < count) {
    showTargetDetails(uiTargets[index], DetailOrigin::TRACKS);
  }
}

void tracksTableDrawEvent(lv_event_t* event) {
  lv_obj_draw_part_dsc_t* part = lv_event_get_draw_part_dsc(event);
  if (!part || part->part != LV_PART_ITEMS || !part->draw_area) return;
  uint16_t columns = lv_table_get_col_cnt(tracksTable);
  if (columns == 0) return;
  uint16_t row = part->id / columns;
  uint16_t column = part->id % columns;
  if (row == 0 || column != 2 || row - 1 >= aircraft::MAX_TARGETS) return;
  const aircraft::Target& target = uiTargets[row - 1];
  if (!target.valid) return;
  int centerX = (part->draw_area->x1 + part->draw_area->x2) / 2;
  int centerY = (part->draw_area->y1 + part->draw_area->y2) / 2;
  radar::drawTrackBitmapIcon(part->draw_ctx, centerX, centerY,
                             aircraft::bitmapForType(target.typeCode));
}

void nearestTargetEvent(lv_event_t* event) {
  const uint8_t index =
      (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= NEAREST_LIST_COUNT || !nearestListHex[index][0]) return;
  selectAircraftHex(nearestListHex[index]);
}

void primaryRadarTargetEvent(lv_event_t*) {
  if (app_state::hasManualTracking() || !leftNearestHex[0]) return;
  selectAircraftHex(leftNearestHex);
}

void selectedInfoEvent(lv_event_t*) {
  aircraft::Target target;
  const bool found = app_state::hasManualTracking()
      ? copyTrackedTarget(target)
      : copyVisibleTargetByHex(selectedHex, target);
  if (!found) {
    if (!app_state::hasManualTracking()) clearSelectedAircraft(true);
    return;
  }
  if (!app_state::hasManualTracking()) selectedAtMs = millis();
  showTargetDetails(target, DetailOrigin::RADAR);
}

void selectedTrackEvent(lv_event_t*) {
  aircraft::Target target;
  if (!copyVisibleTargetByHex(selectedHex, target)) {
    clearSelectedAircraft(true);
    return;
  }
  app_state::selectManualTracking(target);
  Serial.printf("Tracking selected aircraft %s (%s)\n", target.id,
                target.hex);
  clearSelectedAircraft();
  syncRadarRangeControlPosition();
  selectPage(0);
}

void selectedClearEvent(lv_event_t*) {
  clearSelectedAircraft(true);
}

void radarCanvasEvent(lv_event_t*) {
  lv_indev_t* input = lv_indev_get_act();
  if (!input || !radarCanvas) return;
  lv_point_t point{};
  lv_indev_get_point(input, &point);
  lv_area_t canvasArea{};
  lv_obj_get_coords(radarCanvas, &canvasArea);
  const int canvasX = point.x - canvasArea.x1;
  const int canvasY = point.y - canvasArea.y1;
  radar::HitResult hit;
  const bool tracking = app_state::hasManualTracking();
  if (!radar::hitTest(canvasX, canvasY, hit)) {
    if (!tracking) clearSelectedAircraft(true);
    return;
  }
  if (!tracking) {
    selectAircraftHex(hit.hex);
    return;
  }
  aircraft::Target target;
  if (!copyVisibleTargetByHex(hit.hex, target)) return;
  showTargetDetails(target, DetailOrigin::RADAR);
}

void detailBackEvent(lv_event_t*) {
  const DetailOrigin returnOrigin = detailOrigin;
  detailTargetValid = false;
  lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  if (returnOrigin == DetailOrigin::RADAR) {
    if (hasSelectedAircraft()) selectedAtMs = millis();
    setVisible(pagePanel, false);
    for (lv_obj_t* panel : radarPanels) setVisible(panel, true);
    updateSelectedActions();
  } else {
    updatePageContent();
  }
}

void stopManualTracking() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  char previouslyTrackedHex[7]{};
  if (snapshot.manualTracking && snapshot.trackedHex[0]) {
    strncpy(previouslyTrackedHex, snapshot.trackedHex,
            sizeof(previouslyTrackedHex) - 1);
  }

  app_state::clearManualTracking();
  Serial.println("Manual aircraft tracking stopped");
  if (detailTrackLabel) lv_label_set_text(detailTrackLabel, "TRACK ON RADAR");

  if (previouslyTrackedHex[0]) {
    selectAircraftHex(previouslyTrackedHex);
  } else {
    updateSelectedActions();
  }
  syncRadarRangeControlPosition();
}

void radarUntrackEvent(lv_event_t*) {
  if (!app_state::hasManualTracking()) return;
  stopManualTracking();
}

void detailTrackEvent(lv_event_t*) {
  if (!detailTargetValid) return;
  if (app_state::isManuallyTracked(detailTarget)) {
    stopManualTracking();
  } else {
    app_state::selectManualTracking(detailTarget);
    Serial.printf("Tracking aircraft %s (%s)\n", detailTarget.id,
                  detailTarget.hex);
    clearSelectedAircraft();
  }
  syncRadarRangeControlPosition();
  detailTargetValid = false;
  lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  selectPage(0);
}

void renderRadarPage() {
  const bool selectedAvailable = radar::render(uiTargets, selectedHex);
  if (hasSelectedAircraft() && !selectedAvailable) {
    clearSelectedAircraft(true);
  }
  updateSelectedActions();
}

void renderTracksPage() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  const uint8_t count = snapshot.count;
  setLabelTextIfChanged(pageTitle, "TRACKS // NEAREST AIRCRAFT");
  lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setTracksVisible(true);
  if (lastTracksVersion == snapshot.targetVersion &&
      lastTracksRangeGeneration == snapshot.rangeGeneration) return;
  lastTracksVersion = snapshot.targetVersion;
  lastTracksRangeGeneration = snapshot.rangeGeneration;
  const char* headers[] = {
    "FLIGHT", "AIRCRAFT", "", "DIST", "DIR", "ALTITUDE", "SPEED"
  };
  lv_table_set_row_cnt(tracksTable, count + 1);
  for (int column = 0; column < 7; ++column) {
    lv_table_set_cell_value(tracksTable, 0, column, headers[column]);
  }
  char value[32];
  for (uint16_t row = 0; row < count; ++row) {
    lv_table_set_cell_value(tracksTable, row + 1, 0,
                            aircraft::primaryIdentifier(uiTargets[row]));
    snprintf(value, sizeof(value), "%s  %s",
             aircraft::kindName(uiTargets[row].typeCode),
             uiTargets[row].typeCode);
    lv_table_set_cell_value(tracksTable, row + 1, 1, value);
    lv_table_set_cell_value(tracksTable, row + 1, 2, "");
    snprintf(value, sizeof(value), "%.1f mi", uiTargets[row].distanceMiles);
    lv_table_set_cell_value(tracksTable, row + 1, 3, value);
    snprintf(value, sizeof(value), "%.0f %s", uiTargets[row].bearing,
             aircraft::compassDirection(uiTargets[row].bearing));
    lv_table_set_cell_value(tracksTable, row + 1, 4, value);
    snprintf(value, sizeof(value), "%.0f ft", uiTargets[row].altitudeFt);
    lv_table_set_cell_value(tracksTable, row + 1, 5, value);
    snprintf(value, sizeof(value), "%.0f MPH",
             uiTargets[row].speedKt * 1.15078f);
    lv_table_set_cell_value(tracksTable, row + 1, 6, value);
  }
}

void renderAirspacePage() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  const uint8_t count = snapshot.count;
  setTracksVisible(false);
  lv_obj_clear_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setLabelTextIfChanged(pageTitle, "AIRSPACE // LIVE SUMMARY");
  configurePageBody(&lv_font_montserrat_18, 14, 62, 735);
  if (lastAirspaceVersion == snapshot.targetVersion &&
      lastAirspaceRangeGeneration == snapshot.rangeGeneration) return;
  lastAirspaceVersion = snapshot.targetVersion;
  lastAirspaceRangeGeneration = snapshot.rangeGeneration;
  uint16_t airliners = 0, businessJets = 0, military = 0;
  uint16_t turboprops = 0, pistons = 0, helicopters = 0, unknown = 0;
  uint16_t inside20 = 0, inside40 = 0;
  float fastestMph = 0;
  float lowestAltitude = 0;
  for (uint8_t i = 0; i < count; ++i) {
    switch (aircraft::categoryForType(uiTargets[i].typeCode)) {
      case aircraft::Category::AIRLINER: ++airliners; break;
      case aircraft::Category::BUSINESS_JET: ++businessJets; break;
      case aircraft::Category::MILITARY_HEAVY: ++military; break;
      case aircraft::Category::TURBOPROP: ++turboprops; break;
      case aircraft::Category::PISTON: ++pistons; break;
      case aircraft::Category::HELICOPTER: ++helicopters; break;
      default: ++unknown; break;
    }
    if (uiTargets[i].distanceMiles <= 20.0f) ++inside20;
    if (uiTargets[i].distanceMiles <= 40.0f) ++inside40;
    fastestMph = max(fastestMph, uiTargets[i].speedKt * 1.15078f);
    if (uiTargets[i].altitudeFt > 0 &&
        (lowestAltitude == 0 || uiTargets[i].altitudeFt < lowestAltitude)) {
      lowestAltitude = uiTargets[i].altitudeFt;
    }
  }
  char body[900]{};
  snprintf(body, sizeof(body),
      "Aircraft in current %.0f-mile range: %u\n\n"
      "Airliners: %u    Business jets: %u    Military/heavy: %u\n"
      "Turboprops: %u    Piston: %u    Helicopters: %u    Unknown: %u\n"
      "Within 20 miles: %u    Within 40 miles: %u\n\n"
      "Nearest: %s at %.1f miles %s\n"
      "Fastest observed: %.0f MPH\nLowest airborne altitude: %.0f feet",
      snapshot.rangeMiles, count, airliners, businessJets, military,
      turboprops, pistons, helicopters, unknown, inside20, inside40,
      count ? aircraft::primaryIdentifier(uiTargets[0]) : "--",
      count ? uiTargets[0].distanceMiles : 0,
      count ? aircraft::compassDirection(uiTargets[0].bearing) : "--",
      fastestMph, lowestAltitude);
  setLabelTextIfChanged(pageBody, body);
}

void renderSystemPage() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);
  setTracksVisible(false);
  lv_obj_clear_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setLabelTextIfChanged(pageTitle, "SYSTEM // ESP32-S3");
  configurePageBody(&lv_font_montserrat_16, 14, 58, 735);
  const wl_status_t wifiStatus = app_state::wifiStatus();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;
  const uint32_t dataAgeSeconds = snapshot.lastUpdateMs
      ? (millis() - snapshot.lastUpdateMs) / 1000 : 0;
  char body[900]{};
  snprintf(body, sizeof(body),
      "Build: %s\nUptime: %lu sec    WiFi: %s    RSSI: %d dBm    IP: %s\n"
      "Aircraft: API %u / eligible %u / stored %u / visible %u\n"
      "Capacity: %u max / %u dropped    Data age: %lu sec\n"
      "Last fetch: %lu ms / %lu bytes    Failures: %u (%s)\n"
      "Recovery cycles: %lu    Discarded old-range replies: %lu\n"
      "Heap: %u current / %u minimum    PSRAM: %u current / %u minimum / %u total",
      BUILD_ID, (unsigned long)(millis() / 1000),
      adsb::wifiStatusName(wifiStatus),
      wifiConnected ? WiFi.RSSI() : 0,
      wifiConnected ? WiFi.localIP().toString().c_str() : "--",
      (unsigned)diagnostics.lastReceivedCount,
      (unsigned)diagnostics.lastEligibleCount,
      (unsigned)diagnostics.lastAcceptedCount, (unsigned)snapshot.count,
      (unsigned)aircraft::MAX_TARGETS,
      (unsigned)diagnostics.lastCapacityDroppedCount,
      (unsigned long)dataAgeSeconds,
      (unsigned long)diagnostics.lastDurationMs,
      (unsigned long)diagnostics.lastResponseBytes,
      diagnostics.consecutiveFailures,
      app_state::failureStageName(diagnostics.lastFailureStage),
      (unsigned long)diagnostics.networkRecoveries,
      (unsigned long)diagnostics.discardedResponses,
      ESP.getFreeHeap(), diagnostics.minimumFreeHeap, ESP.getFreePsram(),
      diagnostics.minimumFreePsram, ESP.getPsramSize());
  setLabelTextIfChanged(pageBody, body);
}

void renderSetupPage() {
  setTracksVisible(false);
  lv_obj_clear_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setLabelTextIfChanged(pageTitle, "SETUP // RADAR & NETWORK");
  configurePageBody(&lv_font_montserrat_14, 14, 58, 245);
  const wl_status_t wifiStatus = app_state::wifiStatus();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);
  const uint32_t updatedAt = app_state::lastUpdateMs();
  const uint32_t ageSeconds = updatedAt ? (millis() - updatedAt) / 1000 : 0;
  char body[900]{};
  snprintf(body, sizeof(body),
      "WiFi: %s\nSSID: %s\nIP: %s\nSignal: %d dBm\n"
      "Last WiFi error: %d\nData age: %lu sec\nFailures: %u (%s)",
      adsb::wifiStatusName(wifiStatus), settings::wifiSsid().c_str(),
      wifiConnected ? WiFi.localIP().toString().c_str() : "--",
      wifiConnected ? WiFi.RSSI() : 0,
      app_state::lastDisconnectReason(), (unsigned long)ageSeconds,
      diagnostics.consecutiveFailures,
      app_state::failureStageName(diagnostics.lastFailureStage));
  setLabelTextIfChanged(pageBody, body);
}

void updatePageContent() {
  if (!pagePanel || currentPage == 0) return;
  if (detailPanel && !lv_obj_has_flag(detailPanel, LV_OBJ_FLAG_HIDDEN)) return;
  switch (currentPage) {
    case 1: renderTracksPage(); break;
    case 2: renderAirspacePage(); break;
    case 3: renderSystemPage(); break;
    default: renderSetupPage(); break;
  }
}

void updateHeader() {
  const wl_status_t wifiStatus = app_state::wifiStatus();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;
  setLabelTextFmtIfChanged(wifiLabel, "WiFi: %s  RSSI %d",
                           wifiConnected ? "online" : "offline",
                           wifiConnected ? WiFi.RSSI() : 0);
  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 10)) {
    char buffer[16];
    strftime(buffer, sizeof(buffer), "%I:%M:%S", &timeInfo);
    setLabelTextIfChanged(clockLabel, buffer);
  }
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);
  const uint32_t publishedAt = snapshot.lastUpdateMs;
  const uint32_t ageSeconds = publishedAt ? (millis() - publishedAt) / 1000 : 0;
  const char* stateName = "LIVE";
  char stateDetail[64]{};
  lv_color_t stateColor = rgb(80, 235, 145);
  if (!wifiConnected) {
    stateName = "OFFLINE";
    snprintf(stateDetail, sizeof(stateDetail), "OFFLINE\nWiFi");
    stateColor = rgb(255, 120, 110);
  } else if (publishedAt == 0) {
    stateName = app_state::fetchInProgress() ? "UPDATING" : "OFFLINE";
    snprintf(stateDetail, sizeof(stateDetail), "%s\n%s",
             stateName, app_state::failureStageName(
                 diagnostics.lastFailureStage));
    stateColor = app_state::fetchInProgress() ? rgb(255, 220, 100)
                                              : rgb(255, 120, 110);
  } else if (ageSeconds >= 60 && diagnostics.consecutiveFailures >= 3) {
    stateName = "OFFLINE";
    snprintf(stateDetail, sizeof(stateDetail), "OFFLINE\n%s x%u",
             app_state::failureStageName(diagnostics.lastFailureStage),
             diagnostics.consecutiveFailures);
    stateColor = rgb(255, 120, 110);
  } else if (ageSeconds >= 60) {
    stateName = "STALE";
    snprintf(stateDetail, sizeof(stateDetail), "STALE\n%lu min old",
             (unsigned long)(ageSeconds / 60));
    stateColor = rgb(255, 175, 90);
  } else if (app_state::fetchInProgress()) {
    stateName = "UPDATING";
    snprintf(stateDetail, sizeof(stateDetail), "UPDATING\n%lu sec",
             (unsigned long)ageSeconds);
    stateColor = rgb(255, 220, 100);
  } else {
    snprintf(stateDetail, sizeof(stateDetail), "LIVE\n%lu sec",
             (unsigned long)ageSeconds);
  }
  setLabelTextIfChanged(statusLabel, stateDetail);
  lv_obj_set_style_text_color(statusLabel, stateColor, 0);

  updatePageContent();
}

// UI construction is grouped by stable visual regions.
void buildHeader(lv_obj_t* root) {
  headerTitle = makeLabel(root, settings::deviceTitle().c_str(), &lv_font_montserrat_24,
            rgb(63, 255, 155), 18, 10);
  wifiLabel = makeLabel(root, "WiFi: connecting", &lv_font_montserrat_16,
                        rgb(110, 220, 255), 470, 14);
  clockLabel = makeLabel(root, "--:--:--", &lv_font_montserrat_18,
                         rgb(230, 240, 245), 680, 12);
}

bool buildRadarPanels(lv_obj_t* root) {
  if (!radarSideIconBuffers) {
    radarSideIconBuffers = static_cast<lv_color_t*>(heap_caps_calloc(
        RADAR_SIDE_ICON_COUNT * radar::SIDE_ICON_WIDTH *
            radar::SIDE_ICON_HEIGHT,
        sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!radarSideIconBuffers) {
    Serial.println("FATAL: radar side-icon PSRAM allocation failed");
    return false;
  }
  Serial.printf("Radar side-icon buffers in PSRAM: %u bytes\n",
                (unsigned)(RADAR_SIDE_ICON_COUNT * radar::SIDE_ICON_WIDTH *
                           radar::SIDE_ICON_HEIGHT * sizeof(lv_color_t)));

  lv_obj_t* left = lv_obj_create(root);
  radarPanels[0] = left;
  lv_obj_set_size(left, 128, 365);
  lv_obj_set_pos(left, 10, 52);
  stylePanel(left);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(left, "AIRCRAFT", &lv_font_montserrat_14,
            rgb(100, 170, 180), 4, 4);
  countLabel = makeLabel(left, "0", &lv_font_montserrat_32,
                         rgb(255, 214, 80), 4, 26);

  leftNearestModeLabel = makeLabel(left, "NEAREST",
                                   &lv_font_montserrat_14,
                                   rgb(100, 170, 180), 4, 88);
  leftNearestIcon = makeRadarSideIcon(
      left, LEFT_NEAREST_ICON_INDEX, 76, 86);
  if (!leftNearestIcon) return false;
  lv_obj_add_flag(leftNearestIcon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(leftNearestIcon, 5);
  lv_obj_add_event_cb(leftNearestIcon, primaryRadarTargetEvent,
                      LV_EVENT_CLICKED, nullptr);
  leftNearestCallsignLabel = makeLabel(left, "--",
                                       &lv_font_montserrat_20,
                                       rgb(63, 255, 155), 4, 112);
  lv_obj_set_width(leftNearestCallsignLabel, 104);
  leftNearestSummaryLabel = makeLabel(left, "Waiting for aircraft",
                                      &lv_font_montserrat_14,
                                      rgb(225, 235, 240), 4, 140);
  lv_obj_set_width(leftNearestSummaryLabel, 104);
  lv_label_set_long_mode(leftNearestSummaryLabel, LV_LABEL_LONG_WRAP);

  leftNearestHeadingArrow = lv_line_create(left);
  lv_obj_set_size(leftNearestHeadingArrow, 44, 44);
  lv_obj_set_pos(leftNearestHeadingArrow, 4, 220);
  lv_obj_set_style_line_width(leftNearestHeadingArrow, 3, 0);
  lv_obj_set_style_line_color(leftNearestHeadingArrow,
                              rgb(63, 255, 155), 0);
  lv_obj_set_style_line_rounded(leftNearestHeadingArrow, true, 0);
  lv_obj_add_flag(leftNearestHeadingArrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(leftNearestHeadingArrow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(leftNearestHeadingArrow, 5);
  lv_obj_add_event_cb(leftNearestHeadingArrow, primaryRadarTargetEvent,
                      LV_EVENT_CLICKED, nullptr);
  leftNearestHeadingLabel = makeLabel(left, "HDG\n--",
                                      &lv_font_montserrat_12,
                                      rgb(63, 255, 155), 50, 225);
  lv_obj_set_width(leftNearestHeadingLabel, 68);
  lv_label_set_long_mode(leftNearestHeadingLabel, LV_LABEL_LONG_CLIP);
  lv_obj_add_flag(leftNearestHeadingLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(leftNearestHeadingLabel, 5);
  lv_obj_add_event_cb(leftNearestHeadingLabel, primaryRadarTargetEvent,
                      LV_EVENT_CLICKED, nullptr);

  lv_obj_t* leftNearestTargets[] = {
    leftNearestModeLabel, leftNearestCallsignLabel, leftNearestSummaryLabel
  };
  for (lv_obj_t* targetLabel : leftNearestTargets) {
    lv_obj_add_flag(targetLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(targetLabel, 4);
    lv_obj_add_event_cb(targetLabel, primaryRadarTargetEvent,
                        LV_EVENT_CLICKED, nullptr);
  }

  makeLabel(left, "DATA STATUS", &lv_font_montserrat_12,
            rgb(100, 170, 180), 4, 286);
  statusLabel = makeLabel(left, "Starting...", &lv_font_montserrat_14,
                          rgb(150, 170, 180), 4, 302);
  lv_obj_set_width(statusLabel, 104);
  lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_WRAP);

  lv_obj_t* radarPanel = lv_obj_create(root);
  radarPanels[1] = radarPanel;
  lv_obj_set_size(radarPanel, radar::WIDTH + 4, radar::HEIGHT + 4);
  lv_obj_set_pos(radarPanel, 146, 52);
  lv_obj_set_style_pad_all(radarPanel, 0, 0);
  lv_obj_set_style_bg_color(radarPanel, rgb(2, 8, 12), 0);
  lv_obj_set_style_border_color(radarPanel, rgb(28, 84, 70), 0);
  lv_obj_set_style_border_width(radarPanel, 1, 0);

  radarCanvas = lv_canvas_create(radarPanel);
  radarBuffer = static_cast<lv_color_t*>(heap_caps_malloc(
      radar::WIDTH * radar::HEIGHT * sizeof(lv_color_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!radarBuffer) {
    Serial.println("FATAL: radar buffer allocation failed");
    return false;
  }
  memset(radarBuffer, 0, radar::WIDTH * radar::HEIGHT * sizeof(lv_color_t));
  lv_canvas_set_buffer(radarCanvas, radarBuffer, radar::WIDTH, radar::HEIGHT,
                       LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(radarCanvas, 1, 1);
  lv_obj_add_flag(radarCanvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(radarCanvas, radarCanvasEvent, LV_EVENT_CLICKED, nullptr);

  radarRangeControl = lv_obj_create(radarPanel);
  lv_obj_set_size(radarRangeControl, 114, 32);
  lv_obj_align(radarRangeControl, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_clear_flag(radarRangeControl, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(radarRangeControl, 0, 0);
  lv_obj_set_style_bg_color(radarRangeControl, rgb(5, 18, 25), 0);
  lv_obj_set_style_border_color(radarRangeControl, rgb(35, 105, 108), 0);
  lv_obj_set_style_border_width(radarRangeControl, 1, 0);
  lv_obj_set_style_radius(radarRangeControl, 5, 0);
  for (int i = 0; i < RANGE_OPTION_COUNT; ++i) {
    radarRangeButtons[i] = lv_btn_create(radarRangeControl);
    lv_obj_set_size(radarRangeButtons[i], 36, 28);
    lv_obj_set_pos(radarRangeButtons[i], 2 + i * 36, 2);
    lv_obj_set_style_radius(radarRangeButtons[i], 3, 0);
    lv_obj_set_style_shadow_width(radarRangeButtons[i], 0, 0);
    lv_obj_set_ext_click_area(radarRangeButtons[i], 3);
    lv_obj_add_event_cb(radarRangeButtons[i], rangeEvent, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);
    radarRangeButtonLabels[i] = lv_label_create(radarRangeButtons[i]);
    char rangeText[4];
    snprintf(rangeText, sizeof(rangeText), "%d",
             (int)lroundf(RADAR_RANGES[i]));
    lv_label_set_text(radarRangeButtonLabels[i], rangeText);
    lv_obj_set_style_text_font(radarRangeButtonLabels[i],
                               &lv_font_montserrat_12, 0);
    lv_obj_center(radarRangeButtonLabels[i]);
  }

  lv_obj_t* right = lv_obj_create(root);
  radarPanels[2] = right;
  lv_obj_set_size(right, 205, 365);
  lv_obj_set_pos(right, 585, 52);
  stylePanel(right);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  aircraftModeLabel = makeLabel(right, "NEAREST 5 AIRCRAFT",
                                &lv_font_montserrat_16,
                                rgb(110, 220, 255), 3, 3);
  priorityAircraftIcon = makeRadarSideIcon(
      right, PRIORITY_ICON_INDEX, 148, 38);
  if (!priorityAircraftIcon) return false;
  nearestCallsignLabel = makeLabel(right, "--", &lv_font_montserrat_20,
                                   rgb(255, 205, 90), 4, 40);
  lv_obj_set_width(nearestCallsignLabel, 136);
  nearestSummaryLabel = makeLabel(right, "", &lv_font_montserrat_14,
                                  rgb(225, 235, 240), 4, 72);
  lv_obj_set_width(nearestSummaryLabel, 175);
  lv_label_set_long_mode(nearestSummaryLabel, LV_LABEL_LONG_WRAP);

  headingArrow = lv_line_create(right);
  lv_obj_set_size(headingArrow, 44, 44);
  lv_obj_set_pos(headingArrow, 4, 185);
  lv_obj_set_style_line_width(headingArrow, 3, 0);
  lv_obj_set_style_line_color(headingArrow, rgb(255, 214, 80), 0);
  lv_obj_set_style_line_rounded(headingArrow, true, 0);
  headingLabel = makeLabel(right, "HDG\n--", &lv_font_montserrat_12,
                           rgb(255, 214, 80), 50, 190);
  lv_obj_set_width(headingLabel, 124);
  lv_label_set_long_mode(headingLabel, LV_LABEL_LONG_CLIP);

  selectedInfoButton = lv_btn_create(right);
  lv_obj_set_size(selectedInfoButton, 82, 34);
  lv_obj_set_pos(selectedInfoButton, 4, 248);
  lv_obj_set_style_bg_color(selectedInfoButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_border_color(selectedInfoButton, rgb(80, 180, 190), 0);
  lv_obj_set_style_border_width(selectedInfoButton, 1, 0);
  lv_obj_set_style_radius(selectedInfoButton, 5, 0);
  lv_obj_add_event_cb(selectedInfoButton, selectedInfoEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* selectedInfoLabel = lv_label_create(selectedInfoButton);
  lv_label_set_text(selectedInfoLabel, "INFO");
  lv_obj_set_style_text_font(selectedInfoLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(selectedInfoLabel, rgb(150, 230, 255), 0);
  lv_obj_center(selectedInfoLabel);
  lv_obj_add_flag(selectedInfoButton, LV_OBJ_FLAG_HIDDEN);

  selectedTrackButton = lv_btn_create(right);
  lv_obj_set_size(selectedTrackButton, 82, 34);
  lv_obj_set_pos(selectedTrackButton, 92, 248);
  lv_obj_set_style_bg_color(selectedTrackButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_border_color(selectedTrackButton, rgb(63, 255, 155), 0);
  lv_obj_set_style_border_width(selectedTrackButton, 1, 0);
  lv_obj_set_style_radius(selectedTrackButton, 5, 0);
  lv_obj_add_event_cb(selectedTrackButton, selectedTrackEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* selectedTrackLabel = lv_label_create(selectedTrackButton);
  lv_label_set_text(selectedTrackLabel, "TRACK");
  lv_obj_set_style_text_font(selectedTrackLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(selectedTrackLabel, rgb(240, 255, 245), 0);
  lv_obj_center(selectedTrackLabel);
  lv_obj_add_flag(selectedTrackButton, LV_OBJ_FLAG_HIDDEN);

  radarUntrackButton = lv_btn_create(right);
  lv_obj_set_size(radarUntrackButton, 82, 34);
  lv_obj_set_pos(radarUntrackButton, 92, 248);
  lv_obj_set_style_bg_color(radarUntrackButton, rgb(125, 28, 35), 0);
  lv_obj_set_style_border_color(radarUntrackButton, rgb(255, 105, 105), 0);
  lv_obj_set_style_border_width(radarUntrackButton, 1, 0);
  lv_obj_set_style_radius(radarUntrackButton, 5, 0);
  lv_obj_add_event_cb(radarUntrackButton, radarUntrackEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* radarUntrackLabel = lv_label_create(radarUntrackButton);
  lv_label_set_text(radarUntrackLabel, "STOP TRACK");
  lv_obj_set_style_text_font(radarUntrackLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(radarUntrackLabel, rgb(255, 235, 235), 0);
  lv_obj_center(radarUntrackLabel);
  lv_obj_add_flag(radarUntrackButton, LV_OBJ_FLAG_HIDDEN);

  selectedClearButton = lv_btn_create(right);
  lv_obj_set_size(selectedClearButton, 170, 32);
  lv_obj_set_pos(selectedClearButton, 4, 290);
  lv_obj_set_style_bg_color(selectedClearButton, rgb(35, 48, 58), 0);
  lv_obj_set_style_border_color(selectedClearButton, rgb(100, 145, 155), 0);
  lv_obj_set_style_border_width(selectedClearButton, 1, 0);
  lv_obj_set_style_radius(selectedClearButton, 5, 0);
  lv_obj_add_event_cb(selectedClearButton, selectedClearEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* selectedClearLabel = lv_label_create(selectedClearButton);
  lv_label_set_text(selectedClearLabel, "CLEAR SELECTION");
  lv_obj_set_style_text_font(selectedClearLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(selectedClearLabel, rgb(190, 210, 215), 0);
  lv_obj_center(selectedClearLabel);
  lv_obj_add_flag(selectedClearButton, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < NEAREST_LIST_COUNT; ++i) {
    listIcons[i] = makeRadarSideIcon(
        right, LIST_ICON_BASE_INDEX + i, 4, 40 + i * 56);
    if (!listIcons[i]) return false;
    lv_obj_add_flag(listIcons[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(listIcons[i], 6);
    lv_obj_add_event_cb(listIcons[i], nearestTargetEvent, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)i);

    listLabels[i] = makeLabel(right, "", &lv_font_montserrat_12,
                              rgb(225, 235, 240), 38, 36 + i * 56);
    lv_obj_set_width(listLabels[i], 143);
    lv_obj_add_flag(listLabels[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(listLabels[i], 8);
    lv_obj_add_event_cb(listLabels[i], nearestTargetEvent, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)i);
  }

  radar::View view;
  view.canvas = radarCanvas;
  view.buffer = radarBuffer;
  view.countLabel = countLabel;
  view.leftNearestModeLabel = leftNearestModeLabel;
  view.leftNearestCallsignLabel = leftNearestCallsignLabel;
  view.leftNearestSummaryLabel = leftNearestSummaryLabel;
  view.leftNearestIcon = leftNearestIcon;
  view.leftNearestIconBuffer =
      radarSideIconBuffer(LEFT_NEAREST_ICON_INDEX);
  view.leftNearestHeadingArrow = leftNearestHeadingArrow;
  view.leftNearestHeadingLabel = leftNearestHeadingLabel;
  view.aircraftModeLabel = aircraftModeLabel;
  view.nearestCallsignLabel = nearestCallsignLabel;
  view.nearestSummaryLabel = nearestSummaryLabel;
  view.priorityIcon = priorityAircraftIcon;
  view.priorityIconBuffer = radarSideIconBuffer(PRIORITY_ICON_INDEX);
  view.headingArrow = headingArrow;
  view.headingLabel = headingLabel;
  view.leftNearestHex = leftNearestHex;
  for (int i = 0; i < NEAREST_LIST_COUNT; ++i) {
    view.listLabels[i] = listLabels[i];
    view.listIcons[i] = listIcons[i];
    view.listIconBuffers[i] =
        radarSideIconBuffer(LIST_ICON_BASE_INDEX + i);
    view.listHexes[i] = nearestListHex[i];
  }
  radar::configure(view);

  syncRangeControls(app_state::radarRangeMiles());
  updateSelectedActions();
  return true;
}

void buildNavigation(lv_obj_t* root) {
  const char* tabs[] = {"RADAR", "TRACKS", "AIRSPACE", "SYSTEM", "SETUP"};
  for (int i = 0; i < PAGE_COUNT; ++i) {
    lv_obj_t* button = lv_btn_create(root);
    tabButtons[i] = button;
    lv_obj_set_size(button, 146, 46);
    lv_obj_set_pos(button, 12 + i * 157, 426);
    lv_obj_set_style_bg_color(
        button, i == 0 ? rgb(24, 128, 84) : rgb(20, 38, 48), 0);
    lv_obj_set_style_radius(button, 7, 0);
    lv_obj_add_event_cb(button, tabEvent, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)i);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, tabs[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
  }
}

void buildPageShell(lv_obj_t* root) {
  pagePanel = lv_obj_create(root);
  lv_obj_set_size(pagePanel, 780, 365);
  lv_obj_set_pos(pagePanel, 10, 52);
  stylePanel(pagePanel);
  pageTitle = makeLabel(pagePanel, "", &lv_font_montserrat_28,
                        rgb(63, 255, 155), 14, 10);
  pageBody = makeLabel(pagePanel, "", &lv_font_montserrat_18,
                       rgb(225, 235, 240), 14, 62);
  lv_obj_set_width(pageBody, 735);
  lv_label_set_long_mode(pageBody, LV_LABEL_LONG_WRAP);

  tracksTable = lv_table_create(pagePanel);
  lv_obj_set_size(tracksTable, 742, 270);
  lv_obj_set_pos(tracksTable, 8, 58);
  lv_table_set_col_cnt(tracksTable, 7);
  lv_table_set_col_width(tracksTable, 0, 110);
  lv_table_set_col_width(tracksTable, 1, 140);
  lv_table_set_col_width(tracksTable, 2, 42);
  lv_table_set_col_width(tracksTable, 3, 90);
  lv_table_set_col_width(tracksTable, 4, 95);
  lv_table_set_col_width(tracksTable, 5, 125);
  lv_table_set_col_width(tracksTable, 6, 105);
  lv_obj_set_style_text_font(tracksTable, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(tracksTable, rgb(8, 18, 26), LV_PART_MAIN);
  lv_obj_set_style_bg_color(tracksTable, rgb(12, 28, 38), LV_PART_ITEMS);
  lv_obj_set_style_border_color(tracksTable, rgb(35, 76, 87), LV_PART_ITEMS);
  lv_obj_set_style_text_color(tracksTable, rgb(225, 235, 240), LV_PART_ITEMS);
  lv_obj_add_event_cb(tracksTable, tracksTableEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(tracksTable, tracksTableDrawEvent,
                      LV_EVENT_DRAW_PART_END, nullptr);
  setTracksVisible(false);

  reconnectButton = lv_btn_create(pagePanel);
  lv_obj_set_size(reconnectButton, 220, 52);
  lv_obj_set_pos(reconnectButton, 14, 274);
  lv_obj_set_style_bg_color(reconnectButton, rgb(24, 128, 84), 0);
  lv_obj_add_event_cb(reconnectButton, reconnectEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* reconnectLabel = lv_label_create(reconnectButton);
  lv_label_set_text(reconnectLabel, "RECONNECT WIFI");
  lv_obj_set_style_text_font(reconnectLabel, &lv_font_montserrat_16, 0);
  lv_obj_center(reconnectLabel);

  retryButton = lv_btn_create(pagePanel);
  lv_obj_set_size(retryButton, 220, 42);
  lv_obj_set_pos(retryButton, 14, 222);
  lv_obj_set_style_bg_color(retryButton, rgb(20, 68, 82), 0);
  lv_obj_add_event_cb(retryButton, retryEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* retryLabel = lv_label_create(retryButton);
  lv_label_set_text(retryLabel, "RETRY ADSB NOW");
  lv_obj_set_style_text_font(retryLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(retryLabel);

  settingsKeyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(settingsKeyboard, 800, 250);
  lv_obj_align(settingsKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(settingsKeyboard, rgb(8, 18, 26), 0);
  lv_obj_set_style_border_color(settingsKeyboard, rgb(63, 255, 155), 0);
  lv_obj_set_style_border_width(settingsKeyboard, 2, 0);
  lv_obj_set_style_shadow_width(settingsKeyboard, 28, 0);
  lv_obj_set_style_shadow_color(settingsKeyboard, rgb(0, 0, 0), 0);
  lv_obj_set_style_shadow_opa(settingsKeyboard, LV_OPA_70, 0);
  lv_keyboard_set_popovers(settingsKeyboard, true);
  lv_obj_add_event_cb(settingsKeyboard, settingsKeyboardEvent,
                      LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(settingsKeyboard, settingsKeyboardEvent,
                      LV_EVENT_CANCEL, nullptr);
  lv_obj_add_flag(settingsKeyboard, LV_OBJ_FLAG_HIDDEN);

  settingsStatusLabel = makeLabel(pagePanel, "", &lv_font_montserrat_14,
                                  rgb(120, 240, 155), 280, 320);
  lv_obj_set_width(settingsStatusLabel, 465);

  titleField = lv_textarea_create(pagePanel);
  lv_obj_set_size(titleField, 355, 34);
  lv_obj_set_pos(titleField, 390, 52);
  lv_textarea_set_placeholder_text(titleField, "Display name");
  lv_obj_add_event_cb(titleField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(titleField, true);

  ssidField = lv_textarea_create(pagePanel);
  lv_obj_set_size(ssidField, 355, 34);
  lv_obj_set_pos(ssidField, 390, 92);
  lv_textarea_set_placeholder_text(ssidField, "Wi-Fi SSID");
  lv_obj_add_event_cb(ssidField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(ssidField, true);

  passwordField = lv_textarea_create(pagePanel);
  lv_obj_set_size(passwordField, 265, 34);
  lv_obj_set_pos(passwordField, 390, 132);
  lv_textarea_set_placeholder_text(passwordField, "Wi-Fi password");
  lv_obj_add_event_cb(passwordField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(passwordField, true);
  lv_textarea_set_password_mode(passwordField, true);

  showPasswordButton = lv_btn_create(pagePanel);
  lv_obj_set_size(showPasswordButton, 80, 34);
  lv_obj_set_pos(showPasswordButton, 665, 132);
  lv_obj_set_style_bg_color(showPasswordButton, rgb(20, 68, 82), 0);
  lv_obj_add_event_cb(showPasswordButton, showPasswordEvent,
                      LV_EVENT_CLICKED, nullptr);
  showPasswordLabel = lv_label_create(showPasswordButton);
  lv_label_set_text(showPasswordLabel, "SHOW");
  lv_obj_set_style_text_font(showPasswordLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(showPasswordLabel);

  latitudeField = lv_textarea_create(pagePanel);
  lv_obj_set_size(latitudeField, 140, 34);
  lv_obj_set_pos(latitudeField, 370, 174);
  lv_textarea_set_placeholder_text(latitudeField, "Latitude");
  lv_obj_add_event_cb(latitudeField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(latitudeField, true);

  longitudeField = lv_textarea_create(pagePanel);
  lv_obj_set_size(longitudeField, 140, 34);
  lv_obj_set_pos(longitudeField, 605, 174);
  lv_textarea_set_placeholder_text(longitudeField, "Longitude");
  lv_obj_add_event_cb(longitudeField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(longitudeField, true);

  settingsFormLabels[0] = makeLabel(pagePanel, "DISPLAY NAME",
      &lv_font_montserrat_14, rgb(110, 220, 255), 280, 58);
  settingsFormLabels[1] = makeLabel(pagePanel, "WI-FI SSID",
      &lv_font_montserrat_14, rgb(110, 220, 255), 280, 98);
  settingsFormLabels[2] = makeLabel(pagePanel, "PASSWORD",
      &lv_font_montserrat_14, rgb(110, 220, 255), 280, 138);
  settingsFormLabels[3] = makeLabel(pagePanel, "LATITUDE",
      &lv_font_montserrat_14, rgb(110, 220, 255), 280, 180);
  settingsFormLabels[4] = makeLabel(pagePanel, "LONGITUDE",
      &lv_font_montserrat_14, rgb(110, 220, 255), 515, 180);

  saveSettingsButton = lv_btn_create(pagePanel);
  lv_obj_set_size(saveSettingsButton, 150, 42);
  lv_obj_set_pos(saveSettingsButton, 280, 220);
  lv_obj_set_style_bg_color(saveSettingsButton, rgb(24, 128, 84), 0);
  lv_obj_add_event_cb(saveSettingsButton, saveSettingsEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* saveLabel = lv_label_create(saveSettingsButton);
  lv_label_set_text(saveLabel, "SAVE SETTINGS");
  lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(saveLabel);

  resetSettingsButton = lv_btn_create(pagePanel);
  lv_obj_set_size(resetSettingsButton, 155, 42);
  lv_obj_set_pos(resetSettingsButton, 440, 220);
  lv_obj_set_style_bg_color(resetSettingsButton, rgb(20, 68, 82), 0);
  lv_obj_add_event_cb(resetSettingsButton, resetSettingsEvent, LV_EVENT_CLICKED, nullptr);
  resetSettingsLabel = lv_label_create(resetSettingsButton);
  lv_label_set_text(resetSettingsLabel, "RESET DEFAULTS");
  lv_obj_set_style_text_font(resetSettingsLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(resetSettingsLabel);

  populateSettingsForm();
  setSettingsFormVisible(false);

  setupRangeTitle = makeLabel(pagePanel, "RANGE // 80 MILES",
                              &lv_font_montserrat_16, rgb(110, 220, 255),
                              280, 278);
  rangeLabel = setupRangeTitle;
  int activeRange = 2;
  const float rangeMiles = app_state::radarRangeMiles();
  for (int i = 0; i < RANGE_OPTION_COUNT; ++i) {
    if (fabsf(rangeMiles - RADAR_RANGES[i]) < 1.0f) {
      activeRange = i;
    }
    setupRangeButtons[i] = lv_btn_create(pagePanel);
    lv_obj_set_size(setupRangeButtons[i], 68, 38);
    lv_obj_set_pos(setupRangeButtons[i], 470 + i * 70, 270);
    lv_obj_set_style_bg_color(
        setupRangeButtons[i],
        i == activeRange ? rgb(24, 128, 84) : rgb(20, 68, 82), 0);
    lv_obj_set_style_radius(setupRangeButtons[i], 6, 0);
    lv_obj_add_event_cb(setupRangeButtons[i], rangeEvent, LV_EVENT_CLICKED,
                        (void*)(intptr_t)i);
    lv_obj_t* label = lv_label_create(setupRangeButtons[i]);
    lv_label_set_text(label, RADAR_RANGE_NAMES[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    lv_obj_add_flag(setupRangeButtons[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text_fmt(rangeLabel, "RANGE // %d MILES",
                        (int)lroundf(rangeMiles));
  lv_obj_add_flag(setupRangeTitle, LV_OBJ_FLAG_HIDDEN);
}

void buildDetailPanel() {
  detailPanel = lv_obj_create(pagePanel);
  lv_obj_set_size(detailPanel, 752, 337);
  lv_obj_set_pos(detailPanel, 3, 3);
  stylePanel(detailPanel);
  lv_obj_set_style_bg_color(detailPanel, rgb(7, 16, 23), 0);
  detailTitle = makeLabel(detailPanel, "AIRCRAFT DETAILS",
                          &lv_font_montserrat_28, rgb(63, 255, 155), 10, 6);
  detailBody = makeLabel(detailPanel, "", &lv_font_montserrat_16,
                         rgb(225, 235, 240), 10, 50);
  lv_obj_set_width(detailBody, 480);

  detailPlaneCanvas = lv_canvas_create(detailPanel);
  detailPlaneBuffer = static_cast<lv_color_t*>(heap_caps_malloc(
      radar::PREVIEW_WIDTH * radar::PREVIEW_HEIGHT * sizeof(lv_color_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (detailPlaneBuffer) {
    memset(detailPlaneBuffer, 0,
           radar::PREVIEW_WIDTH * radar::PREVIEW_HEIGHT * sizeof(lv_color_t));
    lv_canvas_set_buffer(detailPlaneCanvas, detailPlaneBuffer,
                         radar::PREVIEW_WIDTH, radar::PREVIEW_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(detailPlaneCanvas, 510, 58);
  } else {
    Serial.println("Aircraft preview buffer allocation failed");
    lv_obj_add_flag(detailPlaneCanvas, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_t* backButton = lv_btn_create(detailPanel);
  lv_obj_set_size(backButton, 150, 46);
  lv_obj_set_pos(backButton, 410, 270);
  lv_obj_set_style_bg_color(backButton, rgb(20, 68, 82), 0);
  lv_obj_add_event_cb(backButton, detailBackEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* backLabel = lv_label_create(backButton);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_16, 0);
  lv_obj_center(backLabel);

  detailTrackButton = lv_btn_create(detailPanel);
  lv_obj_set_size(detailTrackButton, 170, 46);
  lv_obj_set_pos(detailTrackButton, 570, 270);
  lv_obj_set_style_bg_color(detailTrackButton, rgb(24, 128, 84), 0);
  lv_obj_add_event_cb(detailTrackButton, detailTrackEvent,
                      LV_EVENT_CLICKED, nullptr);
  detailTrackLabel = lv_label_create(detailTrackButton);
  lv_label_set_text(detailTrackLabel, "TRACK ON RADAR");
  lv_obj_set_style_text_font(detailTrackLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(detailTrackLabel);
  lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pagePanel, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

bool allocateTargetBuffer() {
  if (!app_state::targetStorageReady()) {
    Serial.println("FATAL: App-state target storage is unavailable");
    return false;
  }

  uiTargets = static_cast<aircraft::Target*>(heap_caps_calloc(
      aircraft::MAX_TARGETS, sizeof(aircraft::Target),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!uiTargets) {
    Serial.println("FATAL: UI target-buffer PSRAM allocation failed");
    return false;
  }
  if (!radar::allocateWorkingBuffers()) {
    free(uiTargets);
    uiTargets = nullptr;
    return false;
  }
  Serial.printf("UI target buffer in PSRAM: %u bytes\n",
                (unsigned)(aircraft::MAX_TARGETS *
                           sizeof(aircraft::Target)));
  return true;
}

void buildUi() {
  lv_obj_t* root = lv_scr_act();
  lv_obj_set_style_bg_color(root, rgb(4, 10, 15), 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  buildHeader(root);
  if (!buildRadarPanels(root)) return;
  buildNavigation(root);
  buildPageShell(root);
  buildDetailPanel();
  populateSettingsForm();
  setSettingsFormVisible(false);
}

void update(uint32_t now) {
  if (resetConfirmationPending &&
      (int32_t)(now - resetConfirmationDeadline) >= 0) {
    resetConfirmationPending = false;
    setLabelTextIfChanged(resetSettingsLabel, "RESET DEFAULTS");
    lv_obj_center(resetSettingsLabel);
    setSettingsStatus("Reset confirmation expired", rgb(150, 170, 180));
  }
  if (hasSelectedAircraft() && !detailTargetValid &&
      (int32_t)(now - selectedAtMs) >=
          (int32_t)SELECTED_AIRCRAFT_TIMEOUT_MS) {
    clearSelectedAircraft(true);
  }
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  lastFrame = now;
  if (currentPage == 0 && !detailTargetValid) {
    autoExpandTrackedRange();
    renderRadarPage();
  }
  if (now - lastHeaderUpdate >= 500) {
    lastHeaderUpdate = now;
    updateHeader();
  }
}

}  // namespace ui
