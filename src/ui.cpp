#include "ui.h"

#include <WiFi.h>
#include <math.h>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_data.h"
#include "airport_data.h"
#include "airport_status_bitmaps.h"
#include "build_info.h"
#include "mqtt_service.h"
#include "ota_update.h"
#include "config.h"
#include "radar_control.h"
#include "radar_renderer.h"
#include "settings.h"

namespace ui {
namespace {

constexpr uint32_t FRAME_INTERVAL_MS = 80;
constexpr uint8_t PAGE_COUNT = 5;
constexpr uint8_t NEAREST_LIST_COUNT = 5;
constexpr uint8_t PRIORITY_OTHER_COUNT = 3;
constexpr uint8_t AIRSPACE_CATEGORY_COUNT = 6;
constexpr uint8_t AIRSPACE_METRIC_COUNT = 4;
constexpr uint8_t AIRSPACE_HIGHLIGHT_COUNT = 4;
constexpr uint8_t AIRPORT_CATEGORY_COUNT = airport_data::CATEGORY_COUNT;
constexpr uint8_t AIRPORT_RANGE_COUNT = airport_data::RANGE_COUNT;
constexpr uint8_t AIRPORT_CONFIG_MODE_COUNT = 2;
constexpr uint16_t AIRPORT_DIRECTORY_CAPACITY = 64;
constexpr int AIRPORT_TABLE_TAP_MOVE_LIMIT = 10;
constexpr uint32_t AIRPORT_TABLE_TAP_MAX_MS = 900;
constexpr uint32_t AIRPORT_FOCUS_DURATION_MS = 15000;
static_assert(AIRPORT_RANGE_COUNT == settings::AIRPORT_RANGE_COUNT,
              "Airport range settings must stay synchronized");
constexpr uint8_t RANGE_OPTION_COUNT = 3;
constexpr float RADAR_RANGES[RANGE_OPTION_COUNT] = {
  20.0f, 40.0f, 80.0f
};
constexpr float TRACK_AUTO_ZOOM_EDGE_RATIO = 0.92f;
constexpr float TRACK_AUTO_ZOOM_LOOKAHEAD_SECONDS = 30.0f;
constexpr uint32_t SELECTED_AIRCRAFT_TIMEOUT_MS = 30000;
constexpr uint8_t LEFT_NEAREST_ICON_INDEX = 0;
constexpr uint8_t PRIORITY_ICON_INDEX = 1;
constexpr uint8_t LIST_ICON_BASE_INDEX = 2;
constexpr uint8_t PRIORITY_OTHER_ICON_BASE_INDEX =
    LIST_ICON_BASE_INDEX + NEAREST_LIST_COUNT;
constexpr uint8_t AIRSPACE_ICON_BASE_INDEX =
    PRIORITY_OTHER_ICON_BASE_INDEX + PRIORITY_OTHER_COUNT;
constexpr uint8_t RADAR_SIDE_ICON_COUNT =
    AIRSPACE_ICON_BASE_INDEX + AIRSPACE_CATEGORY_COUNT;
static_assert(RADAR_SIDE_ICON_COUNT == 16,
              "Radar side-icon indexes do not match allocation");

constexpr AircraftBitmapId AIRSPACE_CATEGORY_BITMAPS[AIRSPACE_CATEGORY_COUNT] = {
  AircraftBitmapId::AIRLINER,
  AircraftBitmapId::BUSINESS_JET,
  AircraftBitmapId::TURBOPROP,
  AircraftBitmapId::PISTON,
  AircraftBitmapId::HELICOPTER,
  AircraftBitmapId::UNKNOWN
};
constexpr const char* AIRSPACE_CATEGORY_NAMES[AIRSPACE_CATEGORY_COUNT] = {
  "AIRLINERS", "BUSINESS JETS", "TURBOPROPS",
  "PISTON", "HELICOPTERS", "MILITARY / OTHER"
};
constexpr const char* AIRSPACE_HIGHLIGHT_NAMES[AIRSPACE_HIGHLIGHT_COUNT] = {
  "NEAREST", "FASTEST", "LOWEST AIRBORNE", "HIGHEST AIRBORNE"
};

aircraft::Target* uiTargets = nullptr;
lv_color_t* radarSideIconBuffers = nullptr;
lv_color_t* verticalStateIconBuffer = nullptr;

lv_obj_t* radarCanvas = nullptr;
lv_color_t* radarBuffer = nullptr;
lv_obj_t* wifiLabel = nullptr;
lv_obj_t* clockLabel = nullptr;
lv_obj_t* countLabel = nullptr;
lv_obj_t* leftNearestModeLabel = nullptr;
lv_obj_t* leftNearestCallsignLabel = nullptr;
lv_obj_t* leftNearestSummaryLabel = nullptr;
lv_obj_t* leftNearestIcon = nullptr;
lv_obj_t* leftNearestHeadingArrow = nullptr;
lv_obj_t* leftNearestHeadingLabel = nullptr;
lv_obj_t* leftOtherModeLabel = nullptr;
lv_obj_t* leftOtherLabels[PRIORITY_OTHER_COUNT]{};
lv_obj_t* leftOtherIcons[PRIORITY_OTHER_COUNT]{};
char leftOtherHex[PRIORITY_OTHER_COUNT][7]{};
lv_obj_t* aircraftModeLabel = nullptr;
lv_obj_t* nearestCallsignLabel = nullptr;
lv_obj_t* nearestSummaryLabel = nullptr;
lv_obj_t* priorityAircraftIcon = nullptr;
lv_obj_t* headingArrow = nullptr;
lv_obj_t* headingLabel = nullptr;
lv_obj_t* verticalStateIcon = nullptr;
lv_obj_t* verticalStateLabel = nullptr;
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
lv_obj_t* tracksTable = nullptr;
lv_obj_t* airspaceDashboard = nullptr;
lv_obj_t* airspaceMetricValueLabels[AIRSPACE_METRIC_COUNT]{};
lv_obj_t* airspaceCategoryCountLabels[AIRSPACE_CATEGORY_COUNT]{};
lv_obj_t* airspaceCategoryPercentLabels[AIRSPACE_CATEGORY_COUNT]{};
lv_obj_t* airspaceCategoryIcons[AIRSPACE_CATEGORY_COUNT]{};
lv_obj_t* airspaceHighlightsViewport = nullptr;
lv_obj_t* airspaceHighlightRows[AIRSPACE_HIGHLIGHT_COUNT]{};
lv_obj_t* airspaceHighlightValueLabels[AIRSPACE_HIGHLIGHT_COUNT]{};
char airspaceHighlightHex[AIRSPACE_HIGHLIGHT_COUNT][7]{};
lv_obj_t* airportDashboard = nullptr;
lv_obj_t* airportDirectoryView = nullptr;
lv_obj_t* airportOptionsView = nullptr;
lv_obj_t* airportDetailView = nullptr;
lv_obj_t* airportDirectorySummaryLabel = nullptr;
lv_obj_t* airportDirectoryTable = nullptr;
lv_obj_t* airportOptionsButton = nullptr;
lv_obj_t* airportLabelEditButton = nullptr;
lv_obj_t* airportLabelEditLabel = nullptr;
lv_obj_t* airportBackButton = nullptr;
lv_obj_t* airportDetailBackButton = nullptr;
lv_obj_t* airportDetailShowButton = nullptr;
lv_obj_t* airportDetailShowLabel = nullptr;
lv_obj_t* airportDetailIdentLabel = nullptr;
lv_obj_t* airportDetailNameLabel = nullptr;
lv_obj_t* airportDetailLeftLabel = nullptr;
lv_obj_t* airportDetailRightLabel = nullptr;
lv_obj_t* airportEnabledButton = nullptr;
lv_obj_t* airportEnabledLabel = nullptr;
lv_obj_t* airportModeButtons[AIRPORT_CONFIG_MODE_COUNT]{};
lv_obj_t* airportModeLabels[AIRPORT_CONFIG_MODE_COUNT]{};
lv_obj_t* airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]{};
lv_obj_t* airportToggleLabels[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]{};
lv_obj_t* airportSaveButton = nullptr;
lv_obj_t* airportStatusLabel = nullptr;
lv_obj_t* systemCreditPanel = nullptr;
lv_obj_t* systemStatusCard = nullptr;
lv_obj_t* systemStatusLabel = nullptr;
lv_obj_t* systemBuildLabel = nullptr;
lv_obj_t* systemFirmwareButton = nullptr;
lv_obj_t* systemMqttButton = nullptr;
lv_obj_t* systemMqttLabel = nullptr;
lv_obj_t* deviceNetworkCard = nullptr;
lv_obj_t* maintenanceCard = nullptr;
lv_obj_t* otaPanel = nullptr;
lv_obj_t* mqttPanel = nullptr;
lv_obj_t* mqttStateLabel = nullptr;
lv_obj_t* mqttDeviceLabel = nullptr;
lv_obj_t* mqttMessageLabel = nullptr;
lv_obj_t* mqttToggleButton = nullptr;
lv_obj_t* mqttToggleLabel = nullptr;
lv_obj_t* mqttCloseButton = nullptr;
lv_obj_t* otaStateLabel = nullptr;
lv_obj_t* otaAddressLabel = nullptr;
lv_obj_t* otaCodeLabel = nullptr;
lv_obj_t* otaProgressBar = nullptr;
lv_obj_t* otaProgressLabel = nullptr;
lv_obj_t* otaMessageLabel = nullptr;
lv_obj_t* otaEnableButton = nullptr;
lv_obj_t* otaEnableLabel = nullptr;
lv_obj_t* otaCloseButton = nullptr;
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
uint32_t lastSyncedRangeGeneration = UINT32_MAX;
bool pendingAirportEnabled = true;
uint8_t pendingAirportSymbolMasks[AIRPORT_RANGE_COUNT]{};
uint8_t pendingAirportLabelMasks[AIRPORT_RANGE_COUNT]{};
uint8_t airportConfigMode = 0;
bool airportOptionsLoaded = false;
bool airportOptionsDirty = false;
airport_data::NearbyAirport airportDirectoryEntries[AIRPORT_DIRECTORY_CAPACITY]{};
bool airportDirectoryLabelVisible[AIRPORT_DIRECTORY_CAPACITY]{};
bool airportDirectoryLabelVisibilityCurrent = false;
uint16_t airportDirectoryCount = 0;
airport_data::NearbyAirport airportDetailAirport{};
bool airportDetailValid = false;
bool airportDirectoryUpdating = false;
bool airportLabelEditMode = false;
bool airportTablePressActive = false;
bool airportTablePressMoved = false;
lv_point_t airportTablePressPoint{};
uint32_t airportTablePressStartedMs = 0;

enum class AirportView : uint8_t {
  DIRECTORY,
  OPTIONS,
  DETAIL
};

AirportView airportView = AirportView::DIRECTORY;
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

void styleDashboardCard(lv_obj_t* object) {
  lv_obj_set_style_bg_color(object, rgb(7, 20, 28), 0);
  lv_obj_set_style_border_color(object, rgb(32, 88, 96), 0);
  lv_obj_set_style_border_width(object, 1, 0);
  lv_obj_set_style_radius(object, 6, 0);
  lv_obj_set_style_pad_all(object, 6, 0);
  lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void styleSettingsField(lv_obj_t* field) {
  if (!field) return;
  lv_obj_set_style_bg_color(field, rgb(12, 28, 38), 0);
  lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(field, rgb(38, 102, 112), 0);
  lv_obj_set_style_border_width(field, 1, 0);
  lv_obj_set_style_radius(field, 6, 0);
  lv_obj_set_style_pad_all(field, 8, 0);
  lv_obj_set_style_text_color(field, rgb(225, 235, 240), 0);
  lv_obj_set_style_border_color(field, rgb(63, 255, 155), LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(field, 2, LV_STATE_FOCUSED);
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

void restoreTracksScrollPosition(lv_coord_t previousScrollY) {
  if (!tracksTable) return;

  // LVGL 8.3 keeps the old table scroll offset when the row count shrinks.
  // Normalize at the top first, measure the new valid range, then restore the
  // prior position only as far as the shortened table can still scroll.
  lv_obj_scroll_to_y(tracksTable, 0, LV_ANIM_OFF);
  lv_obj_update_layout(tracksTable);

  const lv_coord_t maxScrollY = lv_obj_get_scroll_bottom(tracksTable);
  const lv_coord_t restoredScrollY =
      previousScrollY <= 0 ? 0
                           : (previousScrollY < maxScrollY
                                  ? previousScrollY
                                  : maxScrollY);
  if (restoredScrollY > 0) {
    lv_obj_scroll_to_y(tracksTable, restoredScrollY, LV_ANIM_OFF);
  }

  if (previousScrollY > maxScrollY) {
    Serial.printf("Tracks scroll clamped: %d -> %d\n",
                  (int)previousScrollY, (int)restoredScrollY);
  }
}

void setAirspaceVisible(bool visible) {
  setVisible(airspaceDashboard, visible);
}

void setAirportVisible(bool visible) {
  setVisible(airportDashboard, visible);
}

void setSystemCreditVisible(bool visible) {
  setVisible(systemCreditPanel, visible);
}

void setSettingsFormVisible(bool visible) {
  setVisible(systemStatusCard, visible);
  setVisible(deviceNetworkCard, visible);
  setVisible(maintenanceCard, visible);
  setVisible(titleField, visible);
  setVisible(ssidField, visible);
  setVisible(passwordField, visible);
  setVisible(latitudeField, visible);
  setVisible(longitudeField, visible);
  setVisible(saveSettingsButton, visible);
  setVisible(resetSettingsButton, visible);
  setVisible(settingsStatusLabel, visible);
  for (lv_obj_t* label : settingsFormLabels) setVisible(label, visible);
  if (!visible && settingsKeyboard) {
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

void syncSettingsStorageState() {
  const bool ready = settings::storageAvailable();
  lv_obj_t* storageButtons[] = {saveSettingsButton, resetSettingsButton,
                                    airportSaveButton};
  for (lv_obj_t* button : storageButtons) {
    if (!button) continue;
    if (ready) lv_obj_clear_state(button, LV_STATE_DISABLED);
    else lv_obj_add_state(button, LV_STATE_DISABLED);
    lv_obj_set_style_opa(button, ready ? LV_OPA_COVER : LV_OPA_50, 0);
  }
  if (!ready) {
    setSettingsStatus("NVS ERROR: saving disabled", rgb(255, 120, 110));
  }
}

void setAirportStatus(const char* text, lv_color_t color) {
  if (!airportStatusLabel) return;
  setLabelTextIfChanged(airportStatusLabel, text ? text : "");
  lv_obj_set_style_text_color(airportStatusLabel, color, 0);
}

void selectPage(uint8_t page);
void syncRangeControls(float rangeMiles);

void syncAirportLabelEditControl() {
  if (!airportLabelEditButton || !airportLabelEditLabel) return;
  lv_obj_set_style_bg_color(
      airportLabelEditButton,
      airportLabelEditMode ? rgb(118, 75, 18) : rgb(20, 68, 82), 0);
  lv_obj_set_style_border_color(
      airportLabelEditButton,
      airportLabelEditMode ? rgb(255, 195, 80) : rgb(35, 105, 108), 0);
  lv_obj_set_style_border_width(airportLabelEditButton,
                                airportLabelEditMode ? 1 : 0, 0);
  setLabelTextIfChanged(airportLabelEditLabel,
                        airportLabelEditMode ? "DONE" : "EDIT");
  lv_obj_set_style_text_color(
      airportLabelEditLabel,
      airportLabelEditMode ? rgb(255, 232, 160) : rgb(160, 220, 230), 0);
  lv_obj_center(airportLabelEditLabel);
}

void setAirportLabelEditMode(bool enabled) {
  airportLabelEditMode = enabled;
  airportTablePressActive = false;
  airportTablePressMoved = false;
  syncAirportLabelEditControl();
}

float airportFocusRangeMiles(float distanceMiles) {
  if (!isfinite(distanceMiles) || distanceMiles < 0.0f ||
      distanceMiles > 80.0f) {
    return 0.0f;
  }
  if (distanceMiles <= 18.0f) return 20.0f;
  if (distanceMiles <= 36.0f) return 40.0f;
  return 80.0f;
}

void syncAirportDetailShowButton() {
  if (!airportDetailShowButton || !airportDetailShowLabel) return;
  const float focusRange = airportDetailValid
      ? airportFocusRangeMiles(airportDetailAirport.distanceMiles) : 0.0f;
  const bool available = focusRange > 0.0f;
  if (available) {
    lv_obj_clear_state(airportDetailShowButton, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(airportDetailShowButton, rgb(118, 75, 18), 0);
    lv_obj_set_style_border_color(airportDetailShowButton,
                                  rgb(255, 195, 80), 0);
    lv_obj_set_style_border_width(airportDetailShowButton, 1, 0);
    setLabelTextIfChanged(airportDetailShowLabel, "SHOW ON RADAR");
    lv_obj_set_style_text_color(airportDetailShowLabel,
                                rgb(255, 232, 160), 0);
  } else {
    lv_obj_add_state(airportDetailShowButton, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(airportDetailShowButton, rgb(45, 52, 58), 0);
    lv_obj_set_style_border_width(airportDetailShowButton, 0, 0);
    setLabelTextIfChanged(airportDetailShowLabel, "OUTSIDE 80 MI");
    lv_obj_set_style_text_color(airportDetailShowLabel,
                                rgb(130, 145, 150), 0);
  }
  lv_obj_center(airportDetailShowLabel);
}

void loadAirportOptions() {
  pendingAirportEnabled = settings::airportsEnabled();
  for (uint8_t range = 0; range < AIRPORT_RANGE_COUNT; ++range) {
    pendingAirportSymbolMasks[range] = settings::airportSymbolMask(range);
    pendingAirportLabelMasks[range] = settings::airportLabelMask(range);
  }
  airportOptionsLoaded = true;
}

void syncAirportControls() {
  if (!airportOptionsLoaded) loadAirportOptions();
  if (airportEnabledButton) {
    lv_obj_set_style_bg_color(
        airportEnabledButton,
        pendingAirportEnabled ? rgb(24, 128, 84) : rgb(45, 52, 58), 0);
  }
  if (airportEnabledLabel) {
    setLabelTextIfChanged(airportEnabledLabel,
                          pendingAirportEnabled ? "OVERLAY ON" : "OVERLAY OFF");
    lv_obj_center(airportEnabledLabel);
  }

  for (uint8_t mode = 0; mode < AIRPORT_CONFIG_MODE_COUNT; ++mode) {
    const bool active = airportConfigMode == mode;
    if (airportModeButtons[mode]) {
      lv_obj_set_style_bg_color(
          airportModeButtons[mode],
          active ? rgb(20, 92, 102) : rgb(18, 38, 48), 0);
      lv_obj_set_style_border_width(airportModeButtons[mode], active ? 1 : 0, 0);
      lv_obj_set_style_border_color(airportModeButtons[mode],
                                    rgb(110, 220, 255), 0);
    }
    if (airportModeLabels[mode]) {
      lv_obj_set_style_text_color(
          airportModeLabels[mode],
          active ? rgb(170, 240, 255) : rgb(100, 170, 180), 0);
    }
  }

  const uint8_t* activeMasks = airportConfigMode == 0
      ? pendingAirportSymbolMasks : pendingAirportLabelMasks;
  for (uint8_t category = 0; category < AIRPORT_CATEGORY_COUNT; ++category) {
    const uint8_t categoryBit = static_cast<uint8_t>(1U << category);
    for (uint8_t range = 0; range < AIRPORT_RANGE_COUNT; ++range) {
      const bool enabled = (activeMasks[range] & categoryBit) != 0;
      if (airportToggleButtons[category][range]) {
        lv_obj_set_style_bg_color(
            airportToggleButtons[category][range],
            enabled ? rgb(24, 128, 84) : rgb(35, 48, 58), 0);
        lv_obj_set_style_border_color(
            airportToggleButtons[category][range],
            enabled ? rgb(63, 255, 155) : rgb(70, 100, 108), 0);
        lv_obj_set_style_border_width(airportToggleButtons[category][range], 1,
                                      0);
      }
      if (airportToggleLabels[category][range]) {
        setLabelTextIfChanged(airportToggleLabels[category][range],
                              enabled ? "ON" : "OFF");
        lv_obj_center(airportToggleLabels[category][range]);
        lv_obj_set_style_text_color(
            airportToggleLabels[category][range],
            enabled ? rgb(240, 255, 245) : rgb(145, 160, 165), 0);
      }
    }
  }
}

void setAirportDirectoryCell(uint16_t row, uint8_t column,
                             const char* text) {
  if (!airportDirectoryTable || column >= 6) return;
  lv_table_set_cell_value(airportDirectoryTable, row, column,
                          text ? text : "");
  lv_table_add_cell_ctrl(airportDirectoryTable, row, column,
                         LV_TABLE_CELL_CTRL_TEXT_CROP);
}

void updateAirportDirectory() {
  if (!airportDirectorySummaryLabel || !airportDirectoryTable) return;
  airportDirectoryUpdating = true;
  lv_obj_set_style_text_color(airportDirectorySummaryLabel,
                              rgb(180, 210, 215), 0);

  airport_data::Status status;
  airport_data::copyStatus(status);
  const float rangeMiles = app_state::radarRangeMiles();
  const uint8_t range = airport_data::rangeIndex(rangeMiles);
  const bool overlayEnabled = settings::airportsEnabled();
  const uint16_t inRangeCount = overlayEnabled
      ? airport_data::visibleCount(rangeMiles,
                                   airport_data::CATEGORY_MASK_ALL)
      : 0;
  const uint8_t labelMask = overlayEnabled
      ? settings::airportLabelMask(range) : 0;
  uint16_t renderedLabelCount = 0;
  const bool labelCountCurrent = radar::airportLabelCount(
      range, labelMask, renderedLabelCount);

  airportDirectoryCount = overlayEnabled
      ? airport_data::copyNearby(airportDirectoryEntries,
                                 AIRPORT_DIRECTORY_CAPACITY,
                                 rangeMiles,
                                 airport_data::CATEGORY_MASK_ALL)
      : 0;

  memset(airportDirectoryLabelVisible, 0,
         sizeof(airportDirectoryLabelVisible));
  airportDirectoryLabelVisibilityCurrent = labelCountCurrent;
  if (airportDirectoryLabelVisibilityCurrent) {
    for (uint16_t row = 0; row < airportDirectoryCount; ++row) {
      bool visible = false;
      if (!radar::airportLabelVisible(
              range, labelMask, airportDirectoryEntries[row].ident,
              visible)) {
        airportDirectoryLabelVisibilityCurrent = false;
        memset(airportDirectoryLabelVisible, 0,
               sizeof(airportDirectoryLabelVisible));
        break;
      }
      airportDirectoryLabelVisible[row] = visible;
    }
  }

  uint16_t manualShowCount = 0;
  uint16_t manualHideCount = 0;
  for (uint16_t row = 0; row < airportDirectoryCount; ++row) {
    const settings::AirportLabelMode mode =
        settings::airportLabelMode(airportDirectoryEntries[row].ident);
    if (mode == settings::AirportLabelMode::SHOW) {
      ++manualShowCount;
    } else if (mode == settings::AirportLabelMode::HIDE) {
      ++manualHideCount;
    }
  }

  char summary[192];
  if (labelCountCurrent) {
    snprintf(summary, sizeof(summary),
             "%s  |  %.0f MI  |  %u RANGE  |  %u LABELS  |  SHOW %u  HIDE %u  |  DB %s",
             overlayEnabled ? "ON" : "OFF", rangeMiles,
             (unsigned)inRangeCount, (unsigned)renderedLabelCount,
             (unsigned)manualShowCount, (unsigned)manualHideCount,
             status.ready ? airport_data::databaseDate() : "OFFLINE");
  } else {
    snprintf(summary, sizeof(summary),
             "%s  |  %.0f MI  |  %u RANGE  |  LABELS --  |  SHOW %u  HIDE %u  |  DB %s",
             overlayEnabled ? "ON" : "OFF", rangeMiles,
             (unsigned)inRangeCount, (unsigned)manualShowCount,
             (unsigned)manualHideCount,
             status.ready ? airport_data::databaseDate() : "OFFLINE");
  }
  setLabelTextIfChanged(airportDirectorySummaryLabel, summary);

  lv_table_set_row_cnt(airportDirectoryTable,
                       airportDirectoryCount ? airportDirectoryCount : 1);

  if (airportDirectoryCount == 0) {
    setAirportDirectoryCell(0, 0, "");
    setAirportDirectoryCell(
        0, 1, status.ready ? "No airports in current range"
                           : "Airport database unavailable");
    for (uint8_t column = 2; column < 6; ++column) {
      setAirportDirectoryCell(0, column, "");
    }
    airportDirectoryUpdating = false;
    return;
  }

  char value[48];
  for (uint16_t row = 0; row < airportDirectoryCount; ++row) {
    const airport_data::NearbyAirport& airport = airportDirectoryEntries[row];
    setAirportDirectoryCell(row, 0, airport.ident);
    setAirportDirectoryCell(row, 1, airport.name);
    setAirportDirectoryCell(
        row, 2, airport_data::categoryName(airport.category));
    snprintf(value, sizeof(value), "%.1f mi", airport.distanceMiles);
    setAirportDirectoryCell(row, 3, value);
    if (airport.runwayLengthFt) {
      snprintf(value, sizeof(value), "%u ft",
               (unsigned)airport.runwayLengthFt);
    } else {
      snprintf(value, sizeof(value), "--");
    }
    setAirportDirectoryCell(row, 4, value);
    setAirportDirectoryCell(
        row, 5,
        settings::airportLabelModeName(
            settings::airportLabelMode(airport.ident)));
  }
  airportDirectoryUpdating = false;
}

void showAirportDirectory() {
  setAirportLabelEditMode(false);
  airportView = AirportView::DIRECTORY;
  airportOptionsDirty = false;
  airportDetailValid = false;
  setVisible(airportDirectoryView, true);
  setVisible(airportOptionsView, false);
  setVisible(airportDetailView, false);
  setLabelTextIfChanged(pageTitle, "AIRPORTS // NEARBY");
  updateAirportDirectory();
}

void showAirportOptions() {
  setAirportLabelEditMode(false);
  airportView = AirportView::OPTIONS;
  airportOptionsLoaded = false;
  airportOptionsDirty = false;
  loadAirportOptions();
  syncAirportControls();
  setAirportStatus("", rgb(120, 240, 155));
  setVisible(airportDirectoryView, false);
  setVisible(airportOptionsView, true);
  setVisible(airportDetailView, false);
  setLabelTextIfChanged(pageTitle, "AIRPORTS // DISPLAY OPTIONS");
}

void showAirportDetail(const airport_data::NearbyAirport& airport) {
  setAirportLabelEditMode(false);
  airportDetailAirport = airport;
  airportDetailValid = true;
  airportView = AirportView::DETAIL;
  setVisible(airportDirectoryView, false);
  setVisible(airportOptionsView, false);
  setVisible(airportDetailView, true);
  setLabelTextIfChanged(pageTitle, "AIRPORTS // AIRPORT PROFILE");
  setLabelTextIfChanged(airportDetailIdentLabel, airport.ident);
  setLabelTextIfChanged(airportDetailNameLabel, airport.name);

  char elevation[24];
  if (airport.elevationFt != 0) {
    snprintf(elevation, sizeof(elevation), "%d ft", (int)airport.elevationFt);
  } else {
    snprintf(elevation, sizeof(elevation), "--");
  }
  char runwayHeading[24];
  if (airport.runwayHeadingDegrees) {
    snprintf(runwayHeading, sizeof(runwayHeading), "%03u deg",
             (unsigned)airport.runwayHeadingDegrees);
  } else {
    snprintf(runwayHeading, sizeof(runwayHeading), "--");
  }
  char runwayLength[24];
  if (airport.runwayLengthFt) {
    snprintf(runwayLength, sizeof(runwayLength), "%u ft",
             (unsigned)airport.runwayLengthFt);
  } else {
    snprintf(runwayLength, sizeof(runwayLength), "--");
  }

  char left[256];
  snprintf(left, sizeof(left),
           "TYPE\n%s\n\nDISTANCE\n%.1f miles\n\nBEARING\n%03.0f deg  %s",
           airport_data::categoryName(airport.category),
           airport.distanceMiles, airport.bearingDegrees,
           aircraft::compassDirection(airport.bearingDegrees));
  setLabelTextIfChanged(airportDetailLeftLabel, left);

  char right[256];
  snprintf(right, sizeof(right),
           "LONGEST RUNWAY\n%s\n\nRUNWAY HEADING\n%s\n\nELEVATION\n%s",
           runwayLength, runwayHeading, elevation);
  setLabelTextIfChanged(airportDetailRightLabel, right);
  syncAirportDetailShowButton();
}

void handleAirportDirectoryTap() {
  if (airportDirectoryUpdating) return;
  uint16_t row = 0;
  uint16_t column = 0;
  lv_table_get_selected_cell(airportDirectoryTable, &row, &column);
  if (row == LV_TABLE_CELL_NONE || row >= airportDirectoryCount) return;

  if (column != 5 || !airportLabelEditMode) {
    showAirportDetail(airportDirectoryEntries[row]);
    return;
  }

  const char* ident = airportDirectoryEntries[row].ident;
  const settings::AirportLabelMode current = settings::airportLabelMode(ident);
  settings::AirportLabelMode next = settings::AirportLabelMode::SHOW;
  if (current == settings::AirportLabelMode::SHOW) {
    next = settings::AirportLabelMode::HIDE;
  } else if (current == settings::AirportLabelMode::HIDE) {
    next = settings::AirportLabelMode::AUTO;
  }
  if (!settings::setAirportLabelMode(ident, next)) {
    setLabelTextIfChanged(airportDirectorySummaryLabel,
                          "NVS ERROR: airport label preference not saved");
    lv_obj_set_style_text_color(airportDirectorySummaryLabel,
                                rgb(255, 120, 110), 0);
    return;
  }
  lv_obj_set_style_text_color(airportDirectorySummaryLabel,
                              rgb(180, 210, 215), 0);
  radar::invalidateAirportLabelCount();
  updateAirportDirectory();
}

void airportDirectoryTableEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED) {
    airportTablePressActive = true;
    airportTablePressMoved = false;
    airportTablePressStartedMs = millis();
    lv_indev_t* input = lv_indev_get_act();
    if (input) {
      lv_indev_get_point(input, &airportTablePressPoint);
    } else {
      airportTablePressPoint = {0, 0};
    }
    return;
  }

  if (code == LV_EVENT_PRESSING && airportTablePressActive) {
    lv_indev_t* input = lv_indev_get_act();
    if (!input) return;
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    if (abs(point.x - airportTablePressPoint.x) > AIRPORT_TABLE_TAP_MOVE_LIMIT ||
        abs(point.y - airportTablePressPoint.y) > AIRPORT_TABLE_TAP_MOVE_LIMIT) {
      airportTablePressMoved = true;
    }
    return;
  }

  if (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_PRESS_LOST) {
    airportTablePressMoved = true;
    airportTablePressActive = false;
    return;
  }

  if (code != LV_EVENT_VALUE_CHANGED) return;
  bool accepted = airportTablePressActive && !airportTablePressMoved &&
      millis() - airportTablePressStartedMs <= AIRPORT_TABLE_TAP_MAX_MS;
  lv_indev_t* input = lv_indev_get_act();
  if (accepted && input) {
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    accepted = abs(point.x - airportTablePressPoint.x) <=
                   AIRPORT_TABLE_TAP_MOVE_LIMIT &&
               abs(point.y - airportTablePressPoint.y) <=
                   AIRPORT_TABLE_TAP_MOVE_LIMIT;
  }
  airportTablePressActive = false;
  airportTablePressMoved = false;
  if (accepted) handleAirportDirectoryTap();
}

void airportDirectoryTableDrawEvent(lv_event_t* event) {
  lv_obj_draw_part_dsc_t* part = lv_event_get_draw_part_dsc(event);
  if (!part || part->part != LV_PART_ITEMS || !part->label_dsc ||
      !part->rect_dsc) {
    return;
  }
  const uint16_t columns = lv_table_get_col_cnt(airportDirectoryTable);
  if (columns == 0) return;
  const uint16_t row = static_cast<uint16_t>(part->id / columns);
  const uint16_t column = static_cast<uint16_t>(part->id % columns);
  if (column != 5 || row >= airportDirectoryCount) return;

  part->label_dsc->align = LV_TEXT_ALIGN_CENTER;
  const settings::AirportLabelMode mode =
      settings::airportLabelMode(airportDirectoryEntries[row].ident);
  if (mode == settings::AirportLabelMode::SHOW) {
    part->label_dsc->color = rgb(120, 240, 155);
    part->rect_dsc->bg_color = rgb(14, 54, 42);
    part->rect_dsc->bg_opa = LV_OPA_COVER;
  } else if (mode == settings::AirportLabelMode::HIDE) {
    part->label_dsc->color = rgb(255, 190, 105);
    part->rect_dsc->bg_color = rgb(58, 36, 24);
    part->rect_dsc->bg_opa = LV_OPA_COVER;
  } else {
    part->label_dsc->color = rgb(110, 220, 255);
  }
}

void airportDirectoryTableEyeDrawEvent(lv_event_t* event) {
  lv_obj_draw_part_dsc_t* part = lv_event_get_draw_part_dsc(event);
  if (!part || part->part != LV_PART_ITEMS || !part->draw_ctx ||
      !part->draw_area) {
    return;
  }
  const uint16_t columns = lv_table_get_col_cnt(airportDirectoryTable);
  if (columns == 0) return;
  const uint16_t row = static_cast<uint16_t>(part->id / columns);
  const uint16_t column = static_cast<uint16_t>(part->id % columns);
  if (column != 5 || row >= airportDirectoryCount ||
      !airportDirectoryLabelVisibilityCurrent ||
      !airportDirectoryLabelVisible[row]) {
    return;
  }

  const settings::AirportLabelMode mode =
      settings::airportLabelMode(airportDirectoryEntries[row].ident);
  if (mode == settings::AirportLabelMode::HIDE) return;

  const lv_coord_t cellHeight =
      part->draw_area->y2 - part->draw_area->y1 + 1;
  lv_area_t eyeArea{
      static_cast<lv_coord_t>(part->draw_area->x2 -
                              AIRPORT_LABEL_EYE_W - 6),
      static_cast<lv_coord_t>(part->draw_area->y1 +
                              (cellHeight - AIRPORT_LABEL_EYE_H) / 2),
      static_cast<lv_coord_t>(part->draw_area->x2 - 7),
      0
  };
  eyeArea.y2 = static_cast<lv_coord_t>(
      eyeArea.y1 + AIRPORT_LABEL_EYE_H - 1);

  lv_draw_img_dsc_t image;
  lv_draw_img_dsc_init(&image);
  image.recolor = mode == settings::AirportLabelMode::SHOW
      ? rgb(120, 240, 155) : rgb(110, 220, 255);
  image.recolor_opa = LV_OPA_COVER;
  image.opa = LV_OPA_COVER;
  lv_draw_img_decoded(part->draw_ctx, &image, &eyeArea,
                      AIRPORT_LABEL_EYE_ALPHA, LV_IMG_CF_ALPHA_8BIT);
}

void airportLabelEditEvent(lv_event_t*) {
  setAirportLabelEditMode(!airportLabelEditMode);
}

void airportDetailShowOnRadarEvent(lv_event_t*) {
  if (!airportDetailValid) return;
  const float focusRange = airportFocusRangeMiles(
      airportDetailAirport.distanceMiles);
  if (focusRange <= 0.0f) return;

  radar::focusAirport(airportDetailAirport.ident,
                      AIRPORT_FOCUS_DURATION_MS);
  const bool rangeChanged = app_state::setRadarRangeMiles(focusRange);
  syncRangeControls(app_state::radarRangeMiles());
  if (rangeChanged) {
    Serial.printf("Airport %s focused; radar range set to %.0f miles\n",
                  airportDetailAirport.ident, focusRange);
    adsb::requestRefresh();
  } else {
    Serial.printf("Airport %s focused on %.0f-mile radar\n",
                  airportDetailAirport.ident, focusRange);
  }
  selectPage(0);
}

void airportDetailBackEvent(lv_event_t*) {
  showAirportDirectory();
}

void airportOptionsEvent(lv_event_t*) {
  showAirportOptions();
}

void airportBackEvent(lv_event_t*) {
  if (airportOptionsDirty) {
    airportOptionsLoaded = false;
    loadAirportOptions();
  }
  showAirportDirectory();
}

void airportEnabledEvent(lv_event_t*) {
  if (!airportOptionsLoaded) loadAirportOptions();
  pendingAirportEnabled = !pendingAirportEnabled;
  airportOptionsDirty = true;
  syncAirportControls();
  setAirportStatus("Unsaved airport display change", rgb(255, 220, 100));
}

void airportModeEvent(lv_event_t* event) {
  airportConfigMode = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (airportConfigMode >= AIRPORT_CONFIG_MODE_COUNT) airportConfigMode = 0;
  syncAirportControls();
  setAirportStatus(airportConfigMode == 0
                       ? "Editing runway symbols by range"
                       : "Editing airport labels by range",
                   rgb(110, 220, 255));
}

void airportToggleEvent(lv_event_t* event) {
  if (!airportOptionsLoaded) loadAirportOptions();
  const uintptr_t packed =
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  const uint8_t category = static_cast<uint8_t>((packed >> 4) & 0x0F);
  const uint8_t range = static_cast<uint8_t>(packed & 0x0F);
  if (category >= AIRPORT_CATEGORY_COUNT || range >= AIRPORT_RANGE_COUNT) return;
  uint8_t* activeMasks = airportConfigMode == 0
      ? pendingAirportSymbolMasks : pendingAirportLabelMasks;
  activeMasks[range] ^= static_cast<uint8_t>(1U << category);
  airportOptionsDirty = true;
  syncAirportControls();
  setAirportStatus("Unsaved airport range change", rgb(255, 220, 100));
}

void airportSaveEvent(lv_event_t*) {
  if (!settings::storageAvailable()) {
    syncSettingsStorageState();
    setAirportStatus("NVS ERROR: airport settings not saved",
                     rgb(255, 120, 110));
    return;
  }
  if (!settings::saveAirportSettings(
          pendingAirportEnabled, pendingAirportSymbolMasks,
          pendingAirportLabelMasks)) {
    syncSettingsStorageState();
    loadAirportOptions();
    syncAirportControls();
    setAirportStatus("Airport settings write failed", rgb(255, 120, 110));
    return;
  }
  airportOptionsDirty = false;
  radar::invalidateAirportLabelCount();
  setAirportStatus("Airport settings saved", rgb(120, 240, 155));
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
  if (!settings::storageAvailable()) {
    syncSettingsStorageState();
    setSettingsStatus("NVS ERROR: settings cannot be saved",
                      rgb(255, 120, 110));
    return;
  }

  const char* titleValue = lv_textarea_get_text(titleField);
  const char* ssidValue = lv_textarea_get_text(ssidField);
  const char* passwordValue = lv_textarea_get_text(passwordField);
  const char* latitudeValue = lv_textarea_get_text(latitudeField);
  const char* longitudeValue = lv_textarea_get_text(longitudeField);
  String cleanedTitle = titleValue;
  String cleanedSsid = ssidValue;
  cleanedTitle.trim();
  cleanedSsid.trim();
  if (!cleanedTitle.length() || !cleanedSsid.length()) {
    setSettingsStatus("Display name and SSID are required",
                      rgb(255, 120, 110));
    return;
  }

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
  if (!settings::saveSettings(cleanedTitle, cleanedSsid, passwordValue,
                              latitude, longitude)) {
    populateSettingsForm();
    syncSettingsStorageState();
    setSettingsStatus("NVS write failed; saving disabled",
                      rgb(255, 120, 110));
    return;
  }
  const bool wifiChanged = previousSsid != settings::wifiSsid() ||
                           previousPassword != settings::wifiPassword();
  const bool locationChanged = fabsf(previousLatitude - latitude) > 0.00001f ||
                               fabsf(previousLongitude - longitude) > 0.00001f;
  if (locationChanged) {
    app_state::invalidateLocation();
    radar::invalidateAirportLabelCount();
    if (!airport_data::rebuild(latitude, longitude)) {
      Serial.println("Airport cache rebuild failed after location change");
    }
  }
  const char* savedStatus = nullptr;
  if (wifiChanged) {
    savedStatus = locationChanged ? "Location changed; reconnecting to WiFi"
                                  : "Saved; reconnecting to new WiFi";
  } else {
    savedStatus = locationChanged ? "Location changed; updating aircraft"
                                  : "Settings saved";
  }
  setSettingsStatus(savedStatus, rgb(120, 240, 155));
  setLabelTextIfChanged(headerTitle, settings::deviceTitle().c_str());
  mqtt_service::requestDiscoveryRefresh();
  if (wifiChanged) adsb::requestWifiReconnect();
  else adsb::requestRefresh();
}

void resetSettingsEvent(lv_event_t*) {
  if (!settings::storageAvailable()) {
    syncSettingsStorageState();
    setSettingsStatus("NVS ERROR: defaults cannot be saved",
                      rgb(255, 120, 110));
    return;
  }

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
  const float previousLatitude = settings::homeLatitude();
  const float previousLongitude = settings::homeLongitude();
  if (!settings::resetToDefaults()) {
    populateSettingsForm();
    syncSettingsStorageState();
    setSettingsStatus("NVS reset failed; saving disabled",
                      rgb(255, 120, 110));
    return;
  }
  const bool locationChanged =
      fabsf(previousLatitude - settings::homeLatitude()) > 0.00001f ||
      fabsf(previousLongitude - settings::homeLongitude()) > 0.00001f;
  if (locationChanged) {
    app_state::invalidateLocation();
    radar::invalidateAirportLabelCount();
    if (!airport_data::rebuild(settings::homeLatitude(),
                               settings::homeLongitude())) {
      Serial.println("Airport cache rebuild failed after defaults reset");
    }
  }
  airportOptionsLoaded = false;
  loadAirportOptions();
  syncAirportControls();
  populateSettingsForm();
  setSettingsStatus(locationChanged ? "Defaults restored; updating aircraft"
                                    : "Defaults restored",
                    rgb(255, 220, 120));
  setLabelTextIfChanged(headerTitle, settings::deviceTitle().c_str());
  mqtt_service::setEnabled(settings::mqttEnabled());
  mqtt_service::requestDiscoveryRefresh();
  adsb::requestWifiReconnect();
}

void updatePageContent();
void updateOtaPanel();
void updateMqttPanel();
void showTargetDetails(const aircraft::Target& target, DetailOrigin origin);

void selectPage(uint8_t page) {
  const uint8_t nextPage = page < PAGE_COUNT ? page : 0;
  if (otaPanel && !lv_obj_has_flag(otaPanel, LV_OBJ_FLAG_HIDDEN)) {
    if (ota_update::busy()) return;
    lv_obj_add_flag(otaPanel, LV_OBJ_FLAG_HIDDEN);
  }
  if (mqttPanel && !lv_obj_has_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN);
  }
  if (currentPage == 0 && nextPage != 0) {
    radar::clearAirportFocus();
  }
  if (currentPage == 3 && nextPage != 3) {
    setAirportLabelEditMode(false);
  }
  if (detailTargetValid) {
    if (detailOrigin == DetailOrigin::RADAR && selectedHex[0]) {
      selectedAtMs = millis();
    }
    detailTargetValid = false;
    if (detailPanel) lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  }
  currentPage = nextPage;
  if (currentPage == 1) {
    lastTracksVersion = UINT32_MAX;
    lastTracksRangeGeneration = UINT32_MAX;
  }
  if (currentPage == 2) {
    lastAirspaceVersion = UINT32_MAX;
    lastAirspaceRangeGeneration = UINT32_MAX;
  }
  if (currentPage == 3) {
    setAirportLabelEditMode(false);
    airportView = AirportView::DIRECTORY;
    airportOptionsDirty = false;
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
  setTracksVisible(false);
  setAirspaceVisible(false);
  setAirportVisible(false);
  setSystemCreditVisible(false);
  setSettingsFormVisible(false);
  setVisible(reconnectButton, false);
  setVisible(retryButton, false);
  setVisible(showPasswordButton, false);
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

void otaOpenEvent(lv_event_t*) {
  if (!otaPanel) return;
  closeSettingsKeyboard();
  updateOtaPanel();
  lv_obj_move_foreground(otaPanel);
  lv_obj_clear_flag(otaPanel, LV_OBJ_FLAG_HIDDEN);
}

void otaEnableEvent(lv_event_t*) {
  ota_update::Status status;
  ota_update::copyStatus(status);
  if (ota_update::busy()) return;
  if (status.serverRunning) ota_update::disable();
  else ota_update::enable();
  updateOtaPanel();
}

void otaCloseEvent(lv_event_t*) {
  if (!otaPanel || ota_update::busy()) return;
  lv_obj_add_flag(otaPanel, LV_OBJ_FLAG_HIDDEN);
  updatePageContent();
}

void mqttOpenEvent(lv_event_t*) {
  if (!mqttPanel) return;
  closeSettingsKeyboard();
  updateMqttPanel();
  lv_obj_move_foreground(mqttPanel);
  lv_obj_clear_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN);
}

void mqttToggleEvent(lv_event_t*) {
  mqtt_service::Status status;
  mqtt_service::copyStatus(status);
  if (!status.configured && !status.enabled) return;
  if (!mqtt_service::setEnabled(!status.enabled)) {
    setSettingsStatus("NVS ERROR: MQTT setting not saved",
                      rgb(255, 120, 110));
  }
  updateMqttPanel();
}

void mqttCloseEvent(lv_event_t*) {
  if (!mqttPanel) return;
  lv_obj_add_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN);
  updatePageContent();
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
  lv_obj_align(radarRangeControl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
}

void syncRangeControls(float rangeMiles) {
  for (int i = 0; i < RANGE_OPTION_COUNT; ++i) {
    const bool active = fabsf(rangeMiles - RADAR_RANGES[i]) < 1.0f;
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

bool applyManualRangeIndex(uint8_t index) {
  if (index >= RANGE_OPTION_COUNT) return false;
  if (!radar_control::setManualRangeMiles(RADAR_RANGES[index])) return false;
  const float rangeMiles = app_state::radarRangeMiles();
  syncRangeControls(rangeMiles);
  lastSyncedRangeGeneration = app_state::rangeGeneration();
  Serial.printf("Radar range changed to %.0f miles\n", rangeMiles);
  if (currentPage == 2) updatePageContent();
  return true;
}

void rangeEvent(lv_event_t* event) {
  int index = (int)(intptr_t)lv_event_get_user_data(event);
  index = constrain(index, 0, RANGE_OPTION_COUNT - 1);
  applyManualRangeIndex((uint8_t)index);
}

void airspaceRangeEvent(lv_event_t*) {
  const float rangeMiles = app_state::radarRangeMiles();
  uint8_t currentIndex = 0;
  float closestDifference = fabsf(rangeMiles - RADAR_RANGES[0]);
  for (uint8_t i = 1; i < RANGE_OPTION_COUNT; ++i) {
    const float difference = fabsf(rangeMiles - RADAR_RANGES[i]);
    if (difference < closestDifference) {
      closestDifference = difference;
      currentIndex = i;
    }
  }
  const uint8_t nextIndex = (currentIndex + 1) % RANGE_OPTION_COUNT;
  applyManualRangeIndex(nextIndex);
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
    radar::clearAirportFocus();
    for (lv_obj_t* panel : radarPanels) setVisible(panel, false);
    setVisible(pagePanel, true);
    setTracksVisible(false);
    if (pageBody) lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
    setSettingsFormVisible(false);
    setAirspaceVisible(false);
    setSystemCreditVisible(false);
    setVisible(reconnectButton, false);
    setVisible(retryButton, false);
    setVisible(showPasswordButton, false);
  }
  detailTarget = target;
  detailTargetValid = true;
  lv_label_set_text_fmt(detailTitle, "%s // AIRCRAFT PROFILE", target.id);
  char details[768];
  snprintf(details, sizeof(details),
    "TYPE: %s  %s\nRegistration: %s\nICAO: %s\nOperator: %s\nDescription: %s\n\n"
    "Distance: %.1f miles\nBearing: %.0f deg %s\nAltitude: %.0f feet\n"
    "Speed: %.0f MPH\nHeading: %.0f deg\nVertical rate: %+.0f ft/min",
    aircraft::kindName(target), target.typeCode, target.registration,
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
                             aircraft::bitmapForTarget(target));
}

void nearestTargetEvent(lv_event_t* event) {
  const uint8_t index =
      (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= NEAREST_LIST_COUNT || !nearestListHex[index][0]) return;
  selectAircraftHex(nearestListHex[index]);
}

void priorityOtherTargetEvent(lv_event_t* event) {
  const uint8_t index =
      (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= PRIORITY_OTHER_COUNT || !leftOtherHex[index][0]) return;
  if (!app_state::hasManualTracking()) {
    selectAircraftHex(leftOtherHex[index]);
    return;
  }
  aircraft::Target target;
  if (copyVisibleTargetByHex(leftOtherHex[index], target)) {
    showTargetDetails(target, DetailOrigin::RADAR);
  }
}

void primaryRadarTargetEvent(lv_event_t*) {
  if (app_state::hasManualTracking() || !leftNearestHex[0]) return;
  selectAircraftHex(leftNearestHex);
}

void airspaceHighlightEvent(lv_event_t* event) {
  const uint8_t index =
      (uint8_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= AIRSPACE_HIGHLIGHT_COUNT ||
      !airspaceHighlightHex[index][0] || app_state::hasManualTracking()) {
    return;
  }

  aircraft::Target target;
  if (!copyVisibleTargetByHex(airspaceHighlightHex[index], target)) return;
  selectAircraftHex(target.hex);
  if (!hasSelectedAircraft()) return;
  selectPage(0);
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

  // Any deliberate radar-canvas tap dismisses a temporary Airport Profile
  // focus before normal aircraft hit testing. Range controls are separate LVGL
  // objects and already clear focus through rangeEvent().
  radar::clearAirportFocus();

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
  setAirspaceVisible(false);
  setSystemCreditVisible(false);
  if (lastTracksVersion == snapshot.targetVersion &&
      lastTracksRangeGeneration == snapshot.rangeGeneration) return;
  lastTracksVersion = snapshot.targetVersion;
  lastTracksRangeGeneration = snapshot.rangeGeneration;
  const char* headers[] = {
    "FLIGHT", "AIRCRAFT", "", "DIST", "DIR", "ALTITUDE", "SPEED"
  };
  const lv_coord_t previousScrollY = lv_obj_get_scroll_y(tracksTable);
  lv_table_set_row_cnt(tracksTable, count + 1);
  for (int column = 0; column < 7; ++column) {
    lv_table_set_cell_value(tracksTable, 0, column, headers[column]);
  }
  char value[32];
  for (uint16_t row = 0; row < count; ++row) {
    lv_table_set_cell_value(tracksTable, row + 1, 0,
                            aircraft::primaryIdentifier(uiTargets[row]));
    snprintf(value, sizeof(value), "%s  %s",
             aircraft::kindName(uiTargets[row]),
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
  restoreTracksScrollPosition(previousScrollY);
}

void setAirspaceHighlight(uint8_t index, const aircraft::Target* target,
                          const char* value) {
  if (index >= AIRSPACE_HIGHLIGHT_COUNT) return;
  setLabelTextIfChanged(airspaceHighlightValueLabels[index],
                        value && value[0] ? value : "--");
  airspaceHighlightHex[index][0] = 0;
  if (target && target->hex[0]) {
    strncpy(airspaceHighlightHex[index], target->hex,
            sizeof(airspaceHighlightHex[index]) - 1);
    airspaceHighlightHex[index][sizeof(airspaceHighlightHex[index]) - 1] = 0;
  }
  if (airspaceHighlightRows[index]) {
    if (airspaceHighlightHex[index][0]) {
      lv_obj_clear_state(airspaceHighlightRows[index], LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(airspaceHighlightRows[index], LV_STATE_DISABLED);
    }
  }
}

void renderAirspacePage() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  const uint8_t count = snapshot.count;
  setTracksVisible(false);
  setSystemCreditVisible(false);
  setAirspaceVisible(true);
  lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setLabelTextIfChanged(pageTitle, "AIRSPACE // LIVE SUMMARY");
  if (lastAirspaceVersion == snapshot.targetVersion &&
      lastAirspaceRangeGeneration == snapshot.rangeGeneration) return;
  lastAirspaceVersion = snapshot.targetVersion;
  lastAirspaceRangeGeneration = snapshot.rangeGeneration;

  uint16_t categoryCounts[AIRSPACE_CATEGORY_COUNT]{};
  uint16_t inside20 = 0;
  uint16_t inside40 = 0;
  const aircraft::Target* nearestTarget = nullptr;
  const aircraft::Target* fastestTarget = nullptr;
  const aircraft::Target* lowestTarget = nullptr;
  const aircraft::Target* highestTarget = nullptr;
  for (uint8_t i = 0; i < count; ++i) {
    const aircraft::Target& target = uiTargets[i];
    uint8_t categoryIndex = AIRSPACE_CATEGORY_COUNT - 1;
    switch (aircraft::categoryForTarget(target)) {
      case aircraft::Category::AIRLINER: categoryIndex = 0; break;
      case aircraft::Category::BUSINESS_JET: categoryIndex = 1; break;
      case aircraft::Category::TURBOPROP: categoryIndex = 2; break;
      case aircraft::Category::PISTON: categoryIndex = 3; break;
      case aircraft::Category::HELICOPTER: categoryIndex = 4; break;
      case aircraft::Category::MILITARY_HEAVY:
      default: categoryIndex = 5; break;
    }
    ++categoryCounts[categoryIndex];
    if (target.distanceMiles <= 20.0f) ++inside20;
    if (target.distanceMiles <= 40.0f) ++inside40;
    if (!nearestTarget || target.distanceMiles < nearestTarget->distanceMiles) {
      nearestTarget = &target;
    }
    if (!fastestTarget || target.speedKt > fastestTarget->speedKt) {
      fastestTarget = &target;
    }
    if (target.altitudeFt > 0 &&
        (!lowestTarget || target.altitudeFt < lowestTarget->altitudeFt)) {
      lowestTarget = &target;
    }
    if (target.altitudeFt > 0 &&
        (!highestTarget || target.altitudeFt > highestTarget->altitudeFt)) {
      highestTarget = &target;
    }
  }

  setLabelTextFmtIfChanged(airspaceMetricValueLabels[0], "%u", count);
  setLabelTextFmtIfChanged(airspaceMetricValueLabels[1], "%u", inside20);
  setLabelTextFmtIfChanged(airspaceMetricValueLabels[2], "%u", inside40);
  setLabelTextFmtIfChanged(airspaceMetricValueLabels[3], "%.0f MI",
                           snapshot.rangeMiles);

  for (uint8_t i = 0; i < AIRSPACE_CATEGORY_COUNT; ++i) {
    const unsigned percentage = count
        ? (unsigned)lroundf(categoryCounts[i] * 100.0f / count)
        : 0;
    setLabelTextFmtIfChanged(airspaceCategoryCountLabels[i], "%u",
                             categoryCounts[i]);
    setLabelTextFmtIfChanged(airspaceCategoryPercentLabels[i], "%u%%",
                             percentage);
  }

  char value[96];
  if (nearestTarget) {
    snprintf(value, sizeof(value), "%s  %.1f MI %s",
             aircraft::primaryIdentifier(*nearestTarget),
             nearestTarget->distanceMiles,
             aircraft::compassDirection(nearestTarget->bearing));
  } else {
    snprintf(value, sizeof(value), "--");
  }
  setAirspaceHighlight(0, nearestTarget, value);

  if (fastestTarget) {
    snprintf(value, sizeof(value), "%s  %.0f MPH",
             aircraft::primaryIdentifier(*fastestTarget),
             fastestTarget->speedKt * 1.15078f);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  setAirspaceHighlight(1, fastestTarget, value);

  char altitude[20] = "--";
  if (lowestTarget) {
    aircraft::formatWholeNumber(lowestTarget->altitudeFt, altitude,
                                sizeof(altitude));
    snprintf(value, sizeof(value), "%s  %s FT",
             aircraft::primaryIdentifier(*lowestTarget), altitude);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  setAirspaceHighlight(2, lowestTarget, value);

  altitude[0] = 0;
  if (highestTarget) {
    aircraft::formatWholeNumber(highestTarget->altitudeFt, altitude,
                                sizeof(altitude));
    snprintf(value, sizeof(value), "%s  %s FT",
             aircraft::primaryIdentifier(*highestTarget), altitude);
  } else {
    snprintf(value, sizeof(value), "--");
  }
  setAirspaceHighlight(3, highestTarget, value);
}

void renderAirportsPage() {
  setTracksVisible(false);
  setAirspaceVisible(false);
  setSystemCreditVisible(false);
  setSettingsFormVisible(false);
  setVisible(reconnectButton, false);
  setVisible(retryButton, false);
  setVisible(showPasswordButton, false);
  setAirportVisible(true);
  lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  syncSettingsStorageState();

  if (airportView == AirportView::OPTIONS) {
    setVisible(airportDirectoryView, false);
    setVisible(airportOptionsView, true);
    setVisible(airportDetailView, false);
    setLabelTextIfChanged(pageTitle, "AIRPORTS // DISPLAY OPTIONS");
    syncAirportControls();
    if (!settings::storageAvailable()) {
      setAirportStatus("NVS ERROR: saving disabled", rgb(255, 120, 110));
    }
  } else if (airportView == AirportView::DETAIL && airportDetailValid) {
    setVisible(airportDirectoryView, false);
    setVisible(airportOptionsView, false);
    setVisible(airportDetailView, true);
    setLabelTextIfChanged(pageTitle, "AIRPORTS // AIRPORT PROFILE");
    syncAirportDetailShowButton();
  } else {
    setVisible(airportDirectoryView, true);
    setVisible(airportOptionsView, false);
    setVisible(airportDetailView, false);
    setLabelTextIfChanged(pageTitle, "AIRPORTS // NEARBY");
    updateAirportDirectory();
  }
}

void renderSystemPage() {
  app_state::Snapshot snapshot;
  app_state::copySnapshot(uiTargets, snapshot);
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);
  airport_data::Status airportStatus;
  airport_data::copyStatus(airportStatus);

  setTracksVisible(false);
  setAirspaceVisible(false);
  setAirportVisible(false);
  setSystemCreditVisible(false);
  setSettingsFormVisible(true);
  setVisible(reconnectButton, true);
  setVisible(retryButton, true);
  setVisible(showPasswordButton, true);
  lv_obj_add_flag(pageBody, LV_OBJ_FLAG_HIDDEN);
  setLabelTextIfChanged(pageTitle, "SYSTEM // STATUS & SETTINGS");

  const wl_status_t wifiStatus = app_state::wifiStatus();
  const bool wifiConnected = wifiStatus == WL_CONNECTED;
  const uint32_t dataAgeSeconds = snapshot.lastUpdateMs
      ? (millis() - snapshot.lastUpdateMs) / 1000 : 0;
  char body[840]{};
  snprintf(body, sizeof(body),
      "NVS        %s\n"
      "WI-FI      %s  %d dBm\n"
      "IP         %s\n"
      "AIRCRAFT   %u / %u\n"
      "DATA AGE   %lu sec\n"
      "FETCH      %lu ms / %lu B\n"
      "FAILURES   %u  %s\n"
      "HEAP       %u KB  MIN %u KB\n"
      "BLOCK      %u KB  MIN %u KB\n"
      "PSRAM      %u KB  MIN %u KB\n"
      "AIRPORTS   %s  %u CACHED",
      settings::storageAvailable() ? "READY" : "ERROR",
      wifiConnected ? "ONLINE" : adsb::wifiStatusName(wifiStatus),
      wifiConnected ? WiFi.RSSI() : 0,
      wifiConnected ? WiFi.localIP().toString().c_str() : "--",
      (unsigned)snapshot.count, (unsigned)aircraft::MAX_TARGETS,
      (unsigned long)dataAgeSeconds,
      (unsigned long)diagnostics.lastDurationMs,
      (unsigned long)diagnostics.lastResponseBytes,
      diagnostics.consecutiveFailures,
      app_state::failureStageName(diagnostics.lastFailureStage),
      (unsigned)(ESP.getFreeHeap() / 1024U),
      (unsigned)(diagnostics.minimumFreeHeap / 1024U),
      (unsigned)(heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024U),
      (unsigned)(diagnostics.minimumLargestInternalBlock / 1024U),
      (unsigned)(ESP.getFreePsram() / 1024U),
      (unsigned)(diagnostics.minimumFreePsram / 1024U),
      airportStatus.ready ? "READY" : "OFF",
      (unsigned)airportStatus.cachedCount);
  setLabelTextIfChanged(systemStatusLabel, body);
  ota_update::Status otaStatus;
  ota_update::copyStatus(otaStatus);
  char firmwareText[64];
  snprintf(firmwareText, sizeof(firmwareText), "FIRMWARE / OTA  %s  >",
           ota_update::stateName(otaStatus.state));
  setLabelTextIfChanged(systemBuildLabel, firmwareText);
  if (systemFirmwareButton) {
    lv_obj_set_style_bg_color(
        systemFirmwareButton,
        otaStatus.state == ota_update::State::ERROR
            ? rgb(126, 48, 44)
            : (otaStatus.serverRunning ? rgb(24, 128, 84)
                                       : rgb(20, 68, 82)),
        0);
  }
  mqtt_service::Status mqttStatus;
  mqtt_service::copyStatus(mqttStatus);
  const char* mqttSummary = mqttStatus.connected
      ? "ONLINE"
      : (mqttStatus.state == mqtt_service::State::INACTIVE
             ? "OFF"
             : (mqttStatus.state == mqtt_service::State::NOT_CONFIGURED
                    ? "SETUP"
                    : (mqttStatus.state == mqtt_service::State::ERROR
                           ? "ERROR" : "CONNECTING")));
  char mqttText[32];
  snprintf(mqttText, sizeof(mqttText), "HA MQTT: %s", mqttSummary);
  setLabelTextIfChanged(systemMqttLabel, mqttText);
  if (systemMqttLabel) lv_obj_center(systemMqttLabel);
  if (systemMqttButton) {
    const bool mqttError = mqttStatus.state == mqtt_service::State::ERROR ||
                           mqttStatus.state == mqtt_service::State::NOT_CONFIGURED;
    lv_obj_set_style_bg_color(
        systemMqttButton,
        mqttError ? rgb(126, 48, 44)
                  : (mqttStatus.connected ? rgb(24, 128, 84)
                                          : rgb(20, 68, 82)),
        0);
  }
  syncSettingsStorageState();
}

void updatePageContent() {
  if (!pagePanel || currentPage == 0) return;
  if (otaPanel && !lv_obj_has_flag(otaPanel, LV_OBJ_FLAG_HIDDEN)) return;
  if (mqttPanel && !lv_obj_has_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN)) return;
  if (detailPanel && !lv_obj_has_flag(detailPanel, LV_OBJ_FLAG_HIDDEN)) return;
  switch (currentPage) {
    case 1: renderTracksPage(); break;
    case 2: renderAirspacePage(); break;
    case 3: renderAirportsPage(); break;
    default: renderSystemPage(); break;
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
  if (snapshot.locationUpdatePending) {
    stateName = "UPDATING";
    snprintf(stateDetail, sizeof(stateDetail),
             "LOCATION CHANGED\nUPDATING");
    stateColor = rgb(255, 220, 100);
  } else if (!wifiConnected) {
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

  if (!verticalStateIconBuffer) {
    verticalStateIconBuffer = static_cast<lv_color_t*>(heap_caps_calloc(
        radar::VERTICAL_STATE_ICON_WIDTH * radar::VERTICAL_STATE_ICON_HEIGHT,
        sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!verticalStateIconBuffer) {
    Serial.println("FATAL: vertical-state icon PSRAM allocation failed");
    return false;
  }
  Serial.printf("Vertical-state icon buffer in PSRAM: %u bytes\n",
                (unsigned)(radar::VERTICAL_STATE_ICON_WIDTH *
                           radar::VERTICAL_STATE_ICON_HEIGHT *
                           sizeof(lv_color_t)));

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

  leftOtherModeLabel = makeLabel(left, "NEAREST 3",
                                 &lv_font_montserrat_16,
                                 rgb(110, 220, 255), 4, 86);
  lv_obj_set_width(leftOtherModeLabel, 112);
  lv_label_set_long_mode(leftOtherModeLabel, LV_LABEL_LONG_CLIP);
  lv_obj_add_flag(leftOtherModeLabel, LV_OBJ_FLAG_HIDDEN);
  for (uint8_t i = 0; i < PRIORITY_OTHER_COUNT; ++i) {
    leftOtherIcons[i] = makeRadarSideIcon(
        left, PRIORITY_OTHER_ICON_BASE_INDEX + i, 4, 112 + i * 54);
    if (!leftOtherIcons[i]) return false;
    lv_obj_add_flag(leftOtherIcons[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(leftOtherIcons[i], 6);
    lv_obj_add_event_cb(leftOtherIcons[i], priorityOtherTargetEvent,
                        LV_EVENT_CLICKED, (void*)(uintptr_t)i);

    leftOtherLabels[i] = makeLabel(left, "", &lv_font_montserrat_12,
                                   rgb(225, 235, 240), 36, 108 + i * 54);
    lv_obj_set_width(leftOtherLabels[i], 78);
    lv_label_set_long_mode(leftOtherLabels[i], LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(leftOtherLabels[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(leftOtherLabels[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_ext_click_area(leftOtherLabels[i], 7);
    lv_obj_add_event_cb(leftOtherLabels[i], priorityOtherTargetEvent,
                        LV_EVENT_CLICKED, (void*)(uintptr_t)i);
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
  lv_obj_align(radarRangeControl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
  lv_obj_clear_flag(radarRangeControl, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(radarRangeControl, 0, 0);
  lv_obj_set_style_bg_color(radarRangeControl, rgb(5, 18, 25), 0);
  lv_obj_set_style_border_color(radarRangeControl, rgb(35, 105, 108), 0);
  lv_obj_set_style_border_width(radarRangeControl, 1, 0);
  lv_obj_set_style_radius(radarRangeControl, 5, 0);
  lv_obj_t* milesLabel = makeLabel(radarPanel, "MILES",
                                   &lv_font_montserrat_12,
                                   rgb(100, 170, 180),
                                   radar::WIDTH - 40, radar::HEIGHT - 49);
  lv_obj_set_width(milesLabel, 38);
  lv_obj_set_style_text_align(milesLabel, LV_TEXT_ALIGN_RIGHT, 0);
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
  lv_obj_set_width(headingLabel, 46);
  lv_label_set_long_mode(headingLabel, LV_LABEL_LONG_CLIP);

  verticalStateIcon = lv_canvas_create(right);
  lv_canvas_set_buffer(verticalStateIcon, verticalStateIconBuffer,
                       radar::VERTICAL_STATE_ICON_WIDTH,
                       radar::VERTICAL_STATE_ICON_HEIGHT,
                       LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(verticalStateIcon, 96, 174);
  lv_obj_add_flag(verticalStateIcon, LV_OBJ_FLAG_HIDDEN);

  verticalStateLabel = makeLabel(right, "", &lv_font_montserrat_12,
                                 rgb(110, 220, 255), 96, 211);
  lv_obj_set_width(verticalStateLabel, 80);
  lv_obj_set_style_text_align(verticalStateLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(verticalStateLabel, LV_LABEL_LONG_CLIP);
  lv_obj_add_flag(verticalStateLabel, LV_OBJ_FLAG_HIDDEN);

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
  view.leftOtherModeLabel = leftOtherModeLabel;
  for (uint8_t i = 0; i < PRIORITY_OTHER_COUNT; ++i) {
    view.leftOtherLabels[i] = leftOtherLabels[i];
    view.leftOtherIcons[i] = leftOtherIcons[i];
    view.leftOtherIconBuffers[i] =
        radarSideIconBuffer(PRIORITY_OTHER_ICON_BASE_INDEX + i);
    view.leftOtherHexes[i] = leftOtherHex[i];
  }
  view.aircraftModeLabel = aircraftModeLabel;
  view.nearestCallsignLabel = nearestCallsignLabel;
  view.nearestSummaryLabel = nearestSummaryLabel;
  view.priorityIcon = priorityAircraftIcon;
  view.priorityIconBuffer = radarSideIconBuffer(PRIORITY_ICON_INDEX);
  view.headingArrow = headingArrow;
  view.headingLabel = headingLabel;
  view.verticalStateIcon = verticalStateIcon;
  view.verticalStateIconBuffer = verticalStateIconBuffer;
  view.verticalStateLabel = verticalStateLabel;
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
  const char* tabs[] = {"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"};
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

  airspaceDashboard = lv_obj_create(pagePanel);
  lv_obj_set_size(airspaceDashboard, 742, 280);
  lv_obj_set_pos(airspaceDashboard, 8, 58);
  lv_obj_set_style_bg_opa(airspaceDashboard, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airspaceDashboard, 0, 0);
  lv_obj_set_style_pad_all(airspaceDashboard, 0, 0);
  lv_obj_clear_flag(airspaceDashboard, LV_OBJ_FLAG_SCROLLABLE);

  const char* metricTitles[AIRSPACE_METRIC_COUNT] = {
    "TOTAL", "WITHIN 20 MI", "WITHIN 40 MI", "CURRENT RANGE"
  };
  for (uint8_t i = 0; i < AIRSPACE_METRIC_COUNT; ++i) {
    lv_obj_t* card = lv_obj_create(airspaceDashboard);
    lv_obj_set_size(card, 176, 58);
    lv_obj_set_pos(card, i * 186, 0);
    styleDashboardCard(card);
    const bool rangeCard = i == AIRSPACE_METRIC_COUNT - 1;
    if (rangeCard) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_ext_click_area(card, 4);
      lv_obj_set_style_bg_color(card, rgb(18, 92, 60), 0);
      lv_obj_set_style_border_color(card, rgb(63, 255, 155), 0);
      lv_obj_set_style_border_width(card, 1, 0);
      lv_obj_set_style_bg_color(card, rgb(12, 62, 43), LV_STATE_PRESSED);
      lv_obj_add_event_cb(card, airspaceRangeEvent, LV_EVENT_CLICKED, nullptr);
    }
    makeLabel(card, metricTitles[i], &lv_font_montserrat_12,
              rangeCard ? rgb(190, 255, 210) : rgb(100, 170, 180), 5, 2);
    airspaceMetricValueLabels[i] = makeLabel(
        card, "0", &lv_font_montserrat_24,
        rangeCard ? rgb(240, 255, 245) : rgb(63, 255, 155), 5, 20);
  }

  for (uint8_t i = 0; i < AIRSPACE_CATEGORY_COUNT; ++i) {
    const uint8_t column = i % 3;
    const uint8_t row = i / 3;
    lv_obj_t* card = lv_obj_create(airspaceDashboard);
    lv_obj_set_size(card, 165, 84);
    lv_obj_set_pos(card, column * 172, 68 + row * 92);
    styleDashboardCard(card);

    airspaceCategoryIcons[i] = makeRadarSideIcon(
        card, AIRSPACE_ICON_BASE_INDEX + i, 6, 14);
    if (airspaceCategoryIcons[i]) {
      radar::drawSideBitmapIcon(
          airspaceCategoryIcons[i],
          radarSideIconBuffer(AIRSPACE_ICON_BASE_INDEX + i),
          AIRSPACE_CATEGORY_BITMAPS[i]);
      lv_obj_clear_flag(airspaceCategoryIcons[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t* title = makeLabel(card, AIRSPACE_CATEGORY_NAMES[i],
                                &lv_font_montserrat_12,
                                rgb(110, 220, 255), 42, 5);
    lv_obj_set_width(title, 110);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    airspaceCategoryCountLabels[i] = makeLabel(
        card, "0", &lv_font_montserrat_24, rgb(255, 214, 80), 42, 32);
    airspaceCategoryPercentLabels[i] = makeLabel(
        card, "0%", &lv_font_montserrat_12, rgb(100, 170, 180), 105, 43);
  }

  lv_obj_t* highlightsCard = lv_obj_create(airspaceDashboard);
  lv_obj_set_size(highlightsCard, 224, 210);
  lv_obj_set_pos(highlightsCard, 518, 68);
  styleDashboardCard(highlightsCard);
  makeLabel(highlightsCard, "LIVE HIGHLIGHTS", &lv_font_montserrat_14,
            rgb(110, 220, 255), 6, 4);
  airspaceHighlightsViewport = lv_obj_create(highlightsCard);
  lv_obj_set_size(airspaceHighlightsViewport, 200, 170);
  lv_obj_set_pos(airspaceHighlightsViewport, 6, 28);
  lv_obj_set_style_bg_opa(airspaceHighlightsViewport, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airspaceHighlightsViewport, 0, 0);
  lv_obj_set_style_pad_all(airspaceHighlightsViewport, 0, 0);
  lv_obj_clear_flag(airspaceHighlightsViewport, LV_OBJ_FLAG_SCROLLABLE);
  for (uint8_t i = 0; i < AIRSPACE_HIGHLIGHT_COUNT; ++i) {
    airspaceHighlightRows[i] = lv_btn_create(airspaceHighlightsViewport);
    lv_obj_set_size(airspaceHighlightRows[i], 198, 39);
    lv_obj_set_pos(airspaceHighlightRows[i], 0, i * 42);
    lv_obj_set_style_bg_color(airspaceHighlightRows[i], rgb(12, 32, 42), 0);
    lv_obj_set_style_bg_color(airspaceHighlightRows[i], rgb(18, 92, 60),
                               LV_STATE_PRESSED);
    lv_obj_set_style_border_color(airspaceHighlightRows[i],
                                  rgb(35, 76, 87), 0);
    lv_obj_set_style_border_width(airspaceHighlightRows[i], 1, 0);
    lv_obj_set_style_radius(airspaceHighlightRows[i], 4, 0);
    lv_obj_set_style_shadow_width(airspaceHighlightRows[i], 0, 0);
    lv_obj_set_style_pad_all(airspaceHighlightRows[i], 0, 0);
    lv_obj_set_style_opa(airspaceHighlightRows[i], LV_OPA_50,
                         LV_STATE_DISABLED);
    lv_obj_add_event_cb(airspaceHighlightRows[i], airspaceHighlightEvent,
                        LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    makeLabel(airspaceHighlightRows[i], AIRSPACE_HIGHLIGHT_NAMES[i],
              &lv_font_montserrat_12, rgb(100, 170, 180), 5, 2);
    airspaceHighlightValueLabels[i] = makeLabel(
        airspaceHighlightRows[i], "--", &lv_font_montserrat_12,
        rgb(225, 235, 240), 5, 20);
    lv_obj_set_width(airspaceHighlightValueLabels[i], 188);
    lv_label_set_long_mode(airspaceHighlightValueLabels[i],
                           LV_LABEL_LONG_CLIP);
    lv_obj_add_state(airspaceHighlightRows[i], LV_STATE_DISABLED);
  }
  setAirspaceVisible(false);

  airportDashboard = lv_obj_create(pagePanel);
  lv_obj_set_size(airportDashboard, 742, 280);
  lv_obj_set_pos(airportDashboard, 8, 58);
  lv_obj_set_style_bg_opa(airportDashboard, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airportDashboard, 0, 0);
  lv_obj_set_style_pad_all(airportDashboard, 0, 0);
  lv_obj_clear_flag(airportDashboard, LV_OBJ_FLAG_SCROLLABLE);

  airportDirectoryView = lv_obj_create(airportDashboard);
  lv_obj_set_size(airportDirectoryView, 742, 280);
  lv_obj_set_pos(airportDirectoryView, 0, 0);
  lv_obj_set_style_bg_opa(airportDirectoryView, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airportDirectoryView, 0, 0);
  lv_obj_set_style_pad_all(airportDirectoryView, 0, 0);
  lv_obj_clear_flag(airportDirectoryView, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* airportDirectoryHeader = lv_obj_create(airportDirectoryView);
  lv_obj_set_size(airportDirectoryHeader, 742, 46);
  lv_obj_set_pos(airportDirectoryHeader, 0, 0);
  styleDashboardCard(airportDirectoryHeader);
  airportDirectorySummaryLabel = makeLabel(
      airportDirectoryHeader, "Airport database starting",
      &lv_font_montserrat_12, rgb(180, 210, 215), 7, 3);
  lv_obj_set_width(airportDirectorySummaryLabel, 548);
  lv_label_set_long_mode(airportDirectorySummaryLabel, LV_LABEL_LONG_CLIP);

  airportOptionsButton = lv_btn_create(airportDirectoryHeader);
  lv_obj_set_size(airportOptionsButton, 166, 32);
  lv_obj_set_pos(airportOptionsButton, 558, 1);
  lv_obj_set_style_bg_color(airportOptionsButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(airportOptionsButton, 5, 0);
  lv_obj_add_event_cb(airportOptionsButton, airportOptionsEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* airportOptionsLabel = lv_label_create(airportOptionsButton);
  lv_label_set_text(airportOptionsLabel, "DISPLAY SETTINGS");
  lv_obj_set_style_text_font(airportOptionsLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(airportOptionsLabel);

  lv_obj_t* airportTableHeader = lv_obj_create(airportDirectoryView);
  lv_obj_set_size(airportTableHeader, 742, 26);
  lv_obj_set_pos(airportTableHeader, 0, 50);
  lv_obj_set_style_bg_color(airportTableHeader, rgb(12, 34, 44), 0);
  lv_obj_set_style_border_color(airportTableHeader, rgb(35, 76, 87), 0);
  lv_obj_set_style_border_width(airportTableHeader, 1, 0);
  lv_obj_set_style_radius(airportTableHeader, 4, 0);
  lv_obj_set_style_pad_all(airportTableHeader, 0, 0);
  lv_obj_clear_flag(airportTableHeader, LV_OBJ_FLAG_SCROLLABLE);
  const char* airportHeaders[] = {
    "ID", "AIRPORT", "TYPE", "DIST", "RUNWAY"
  };
  const int airportHeaderX[] = {6, 68, 372, 452, 524};
  for (uint8_t column = 0; column < 5; ++column) {
    makeLabel(airportTableHeader, airportHeaders[column],
              &lv_font_montserrat_12, rgb(110, 220, 255),
              airportHeaderX[column], 5);
  }
  makeLabel(airportTableHeader, "LABEL", &lv_font_montserrat_12,
            rgb(110, 220, 255), 624, 5);
  airportLabelEditButton = lv_btn_create(airportTableHeader);
  lv_obj_set_size(airportLabelEditButton, 52, 22);
  lv_obj_set_pos(airportLabelEditButton, 682, 2);
  lv_obj_set_style_radius(airportLabelEditButton, 4, 0);
  lv_obj_set_style_shadow_width(airportLabelEditButton, 0, 0);
  lv_obj_add_event_cb(airportLabelEditButton, airportLabelEditEvent,
                      LV_EVENT_CLICKED, nullptr);
  airportLabelEditLabel = lv_label_create(airportLabelEditButton);
  lv_obj_set_style_text_font(airportLabelEditLabel,
                             &lv_font_montserrat_12, 0);
  syncAirportLabelEditControl();

  airportDirectoryTable = lv_table_create(airportDirectoryView);
  lv_obj_set_size(airportDirectoryTable, 742, 200);
  lv_obj_set_pos(airportDirectoryTable, 0, 80);
  lv_table_set_col_cnt(airportDirectoryTable, 6);
  lv_table_set_col_width(airportDirectoryTable, 0, 62);
  lv_table_set_col_width(airportDirectoryTable, 1, 304);
  lv_table_set_col_width(airportDirectoryTable, 2, 80);
  lv_table_set_col_width(airportDirectoryTable, 3, 72);
  lv_table_set_col_width(airportDirectoryTable, 4, 104);
  lv_table_set_col_width(airportDirectoryTable, 5, 92);
  lv_obj_set_style_text_font(airportDirectoryTable, &lv_font_montserrat_12,
                             LV_PART_ITEMS);
  lv_obj_set_style_bg_color(airportDirectoryTable, rgb(8, 18, 26),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(airportDirectoryTable, rgb(10, 26, 35),
                            LV_PART_ITEMS);
  lv_obj_set_style_border_color(airportDirectoryTable, rgb(35, 76, 87),
                                LV_PART_ITEMS);
  lv_obj_set_style_text_color(airportDirectoryTable, rgb(225, 235, 240),
                              LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(airportDirectoryTable, 6, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(airportDirectoryTable, 7, LV_PART_ITEMS);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableEvent,
                      LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableEvent,
                      LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableEvent,
                      LV_EVENT_SCROLL_BEGIN, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableEvent,
                      LV_EVENT_PRESS_LOST, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable, airportDirectoryTableDrawEvent,
                      LV_EVENT_DRAW_PART_BEGIN, nullptr);
  lv_obj_add_event_cb(airportDirectoryTable,
                      airportDirectoryTableEyeDrawEvent,
                      LV_EVENT_DRAW_PART_END, nullptr);

  airportOptionsView = lv_obj_create(airportDashboard);
  lv_obj_set_size(airportOptionsView, 742, 280);
  lv_obj_set_pos(airportOptionsView, 0, 0);
  lv_obj_set_style_bg_opa(airportOptionsView, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airportOptionsView, 0, 0);
  lv_obj_set_style_pad_all(airportOptionsView, 0, 0);
  lv_obj_clear_flag(airportOptionsView, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* airportOptionsHeader = lv_obj_create(airportOptionsView);
  lv_obj_set_size(airportOptionsHeader, 742, 42);
  lv_obj_set_pos(airportOptionsHeader, 0, 0);
  styleDashboardCard(airportOptionsHeader);
  lv_obj_t* airportOptionsHelp = makeLabel(
      airportOptionsHeader,
      "Choose which airport symbols and labels appear at each range",
      &lv_font_montserrat_12, rgb(180, 210, 215), 7, 8);
  lv_obj_set_width(airportOptionsHelp, 540);
  lv_label_set_long_mode(airportOptionsHelp, LV_LABEL_LONG_CLIP);
  airportBackButton = lv_btn_create(airportOptionsHeader);
  lv_obj_set_size(airportBackButton, 166, 30);
  lv_obj_set_pos(airportBackButton, 558, 0);
  lv_obj_set_style_bg_color(airportBackButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(airportBackButton, 5, 0);
  lv_obj_add_event_cb(airportBackButton, airportBackEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* airportBackLabel = lv_label_create(airportBackButton);
  lv_label_set_text(airportBackLabel, "BACK TO AIRPORTS");
  lv_obj_set_style_text_font(airportBackLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(airportBackLabel);

  lv_obj_t* airportOptionsCard = lv_obj_create(airportOptionsView);
  lv_obj_set_size(airportOptionsCard, 742, 230);
  lv_obj_set_pos(airportOptionsCard, 0, 50);
  styleDashboardCard(airportOptionsCard);

  airportEnabledButton = lv_btn_create(airportOptionsCard);
  lv_obj_set_size(airportEnabledButton, 150, 34);
  lv_obj_set_pos(airportEnabledButton, 8, 8);
  lv_obj_set_style_radius(airportEnabledButton, 5, 0);
  lv_obj_add_event_cb(airportEnabledButton, airportEnabledEvent,
                      LV_EVENT_CLICKED, nullptr);
  airportEnabledLabel = lv_label_create(airportEnabledButton);
  lv_obj_set_style_text_font(airportEnabledLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(airportEnabledLabel);

  const char* airportModeNames[AIRPORT_CONFIG_MODE_COUNT] = {
    "SYMBOLS", "LABELS"
  };
  for (uint8_t mode = 0; mode < AIRPORT_CONFIG_MODE_COUNT; ++mode) {
    airportModeButtons[mode] = lv_btn_create(airportOptionsCard);
    lv_obj_set_size(airportModeButtons[mode], 108, 34);
    lv_obj_set_pos(airportModeButtons[mode], 170 + mode * 116, 8);
    lv_obj_set_style_radius(airportModeButtons[mode], 5, 0);
    lv_obj_add_event_cb(airportModeButtons[mode], airportModeEvent,
                        LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(mode)));
    airportModeLabels[mode] = lv_label_create(airportModeButtons[mode]);
    lv_label_set_text(airportModeLabels[mode], airportModeNames[mode]);
    lv_obj_set_style_text_font(airportModeLabels[mode],
                               &lv_font_montserrat_12, 0);
    lv_obj_center(airportModeLabels[mode]);
  }

  airportSaveButton = lv_btn_create(airportOptionsCard);
  lv_obj_set_size(airportSaveButton, 160, 34);
  lv_obj_set_pos(airportSaveButton, 556, 8);
  lv_obj_set_style_bg_color(airportSaveButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(airportSaveButton, 5, 0);
  lv_obj_set_style_pad_hor(airportSaveButton, 16, 0);
  lv_obj_set_style_pad_ver(airportSaveButton, 7, 0);
  lv_obj_add_event_cb(airportSaveButton, airportSaveEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* airportSaveLabel = lv_label_create(airportSaveButton);
  lv_label_set_text(airportSaveLabel, "SAVE SETTINGS");
  lv_obj_set_style_text_font(airportSaveLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(airportSaveLabel);

  const char* airportRangeNames[AIRPORT_RANGE_COUNT] = {
    "20 MI", "40 MI", "80 MI"
  };
  for (uint8_t range = 0; range < AIRPORT_RANGE_COUNT; ++range) {
    lv_obj_t* rangeLabel = makeLabel(
        airportOptionsCard, airportRangeNames[range], &lv_font_montserrat_12,
        rgb(100, 170, 180), 427 + range * 92, 52);
    lv_obj_set_width(rangeLabel, 72);
    lv_obj_set_style_text_align(rangeLabel, LV_TEXT_ALIGN_CENTER, 0);
  }

  const airport_data::Category airportCategories[AIRPORT_CATEGORY_COUNT] = {
    airport_data::Category::MAJOR,
    airport_data::Category::PUBLIC,
    airport_data::Category::PRIVATE_FIELD,
    airport_data::Category::HELIPORT
  };
  const char* airportCategoryNotes[AIRPORT_CATEGORY_COUNT] = {
    "Large and commercial airports",
    "Public regional and local fields",
    "Private-use landing fields",
    "Helicopter landing facilities"
  };
  for (uint8_t category = 0; category < AIRPORT_CATEGORY_COUNT; ++category) {
    const int rowY = 73 + category * 34;
    makeLabel(airportOptionsCard,
              airport_data::categoryName(airportCategories[category]),
              &lv_font_montserrat_14, rgb(225, 235, 240), 10, rowY + 4);
    makeLabel(airportOptionsCard, airportCategoryNotes[category],
              &lv_font_montserrat_12, rgb(100, 170, 180), 105, rowY + 6);
    for (uint8_t range = 0; range < AIRPORT_RANGE_COUNT; ++range) {
      airportToggleButtons[category][range] = lv_btn_create(airportOptionsCard);
      lv_obj_set_size(airportToggleButtons[category][range], 72, 28);
      lv_obj_set_pos(airportToggleButtons[category][range],
                     427 + range * 92, rowY);
      lv_obj_set_style_radius(airportToggleButtons[category][range], 4, 0);
      const uintptr_t packed =
          (static_cast<uintptr_t>(category) << 4) | range;
      lv_obj_add_event_cb(
          airportToggleButtons[category][range], airportToggleEvent,
          LV_EVENT_CLICKED, reinterpret_cast<void*>(packed));
      airportToggleLabels[category][range] =
          lv_label_create(airportToggleButtons[category][range]);
      lv_obj_set_style_text_font(airportToggleLabels[category][range],
                                 &lv_font_montserrat_12, 0);
      lv_obj_center(airportToggleLabels[category][range]);
    }
  }

  airportStatusLabel = makeLabel(
      airportOptionsCard, "", &lv_font_montserrat_12,
      rgb(120, 240, 155), 10, 208);
  lv_obj_set_width(airportStatusLabel, 706);
  lv_obj_set_style_text_align(airportStatusLabel, LV_TEXT_ALIGN_CENTER, 0);

  airportDetailView = lv_obj_create(airportDashboard);
  lv_obj_set_size(airportDetailView, 742, 280);
  lv_obj_set_pos(airportDetailView, 0, 0);
  lv_obj_set_style_bg_opa(airportDetailView, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(airportDetailView, 0, 0);
  lv_obj_set_style_pad_all(airportDetailView, 0, 0);
  lv_obj_clear_flag(airportDetailView, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* airportDetailCard = lv_obj_create(airportDetailView);
  lv_obj_set_size(airportDetailCard, 742, 276);
  lv_obj_set_pos(airportDetailCard, 0, 0);
  styleDashboardCard(airportDetailCard);
  airportDetailIdentLabel = makeLabel(
      airportDetailCard, "----", &lv_font_montserrat_28,
      rgb(63, 255, 155), 14, 8);
  airportDetailNameLabel = makeLabel(
      airportDetailCard, "Airport", &lv_font_montserrat_18,
      rgb(110, 220, 255), 14, 45);
  lv_obj_set_width(airportDetailNameLabel, 520);
  lv_label_set_long_mode(airportDetailNameLabel, LV_LABEL_LONG_CLIP);

  airportDetailBackButton = lv_btn_create(airportDetailCard);
  lv_obj_set_size(airportDetailBackButton, 166, 34);
  lv_obj_set_pos(airportDetailBackButton, 558, 8);
  lv_obj_set_style_bg_color(airportDetailBackButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(airportDetailBackButton, 5, 0);
  lv_obj_add_event_cb(airportDetailBackButton, airportDetailBackEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* airportDetailBackLabel = lv_label_create(airportDetailBackButton);
  lv_label_set_text(airportDetailBackLabel, "BACK TO AIRPORTS");
  lv_obj_set_style_text_font(airportDetailBackLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(airportDetailBackLabel);

  airportDetailShowButton = lv_btn_create(airportDetailCard);
  lv_obj_set_size(airportDetailShowButton, 166, 30);
  lv_obj_set_pos(airportDetailShowButton, 558, 46);
  lv_obj_set_style_radius(airportDetailShowButton, 5, 0);
  lv_obj_set_style_shadow_width(airportDetailShowButton, 0, 0);
  lv_obj_add_event_cb(airportDetailShowButton,
                      airportDetailShowOnRadarEvent,
                      LV_EVENT_CLICKED, nullptr);
  airportDetailShowLabel = lv_label_create(airportDetailShowButton);
  lv_obj_set_style_text_font(airportDetailShowLabel,
                             &lv_font_montserrat_12, 0);
  syncAirportDetailShowButton();

  lv_obj_t* airportDetailDivider = lv_obj_create(airportDetailCard);
  lv_obj_set_size(airportDetailDivider, 1, 176);
  lv_obj_set_pos(airportDetailDivider, 356, 84);
  lv_obj_set_style_bg_color(airportDetailDivider, rgb(35, 76, 87), 0);
  lv_obj_set_style_bg_opa(airportDetailDivider, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(airportDetailDivider, 0, 0);
  lv_obj_clear_flag(airportDetailDivider, LV_OBJ_FLAG_SCROLLABLE);

  airportDetailLeftLabel = makeLabel(
      airportDetailCard, "", &lv_font_montserrat_14,
      rgb(225, 235, 240), 14, 88);
  lv_obj_set_width(airportDetailLeftLabel, 320);
  lv_label_set_long_mode(airportDetailLeftLabel, LV_LABEL_LONG_WRAP);
  airportDetailRightLabel = makeLabel(
      airportDetailCard, "", &lv_font_montserrat_14,
      rgb(225, 235, 240), 380, 88);
  lv_obj_set_width(airportDetailRightLabel, 330);
  lv_label_set_long_mode(airportDetailRightLabel, LV_LABEL_LONG_WRAP);
  setVisible(airportOptionsView, false);
  setVisible(airportDetailView, false);
  setAirportVisible(false);

  systemStatusCard = lv_obj_create(pagePanel);
  lv_obj_set_size(systemStatusCard, 270, 281);
  lv_obj_set_pos(systemStatusCard, 8, 58);
  styleDashboardCard(systemStatusCard);
  makeLabel(systemStatusCard, "SYSTEM STATUS", &lv_font_montserrat_14,
            rgb(110, 220, 255), 7, 4);
  systemStatusLabel = makeLabel(
      systemStatusCard, "", &lv_font_montserrat_12,
      rgb(225, 235, 240), 7, 27);
  lv_obj_set_width(systemStatusLabel, 250);
  lv_label_set_long_mode(systemStatusLabel, LV_LABEL_LONG_CLIP);
  systemFirmwareButton = lv_btn_create(systemStatusCard);
  lv_obj_set_size(systemFirmwareButton, 250, 27);
  lv_obj_set_pos(systemFirmwareButton, 3, 248);
  lv_obj_set_style_bg_color(systemFirmwareButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(systemFirmwareButton, 5, 0);
  lv_obj_set_style_pad_all(systemFirmwareButton, 0, 0);
  lv_obj_add_event_cb(systemFirmwareButton, otaOpenEvent,
                      LV_EVENT_CLICKED, nullptr);
  systemBuildLabel = lv_label_create(systemFirmwareButton);
  lv_label_set_text(systemBuildLabel, "FIRMWARE / OTA  DISABLED  >");
  lv_obj_set_style_text_font(systemBuildLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(systemBuildLabel);

  deviceNetworkCard = lv_obj_create(pagePanel);
  lv_obj_set_size(deviceNetworkCard, 456, 205);
  lv_obj_set_pos(deviceNetworkCard, 286, 58);
  styleDashboardCard(deviceNetworkCard);
  makeLabel(deviceNetworkCard, "DEVICE & NETWORK", &lv_font_montserrat_14,
            rgb(110, 220, 255), 7, 4);

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

  titleField = lv_textarea_create(deviceNetworkCard);
  lv_obj_set_size(titleField, 312, 28);
  lv_obj_set_pos(titleField, 128, 25);
  lv_textarea_set_placeholder_text(titleField, "Display name");
  lv_obj_add_event_cb(titleField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(titleField, true);
  styleSettingsField(titleField);

  ssidField = lv_textarea_create(deviceNetworkCard);
  lv_obj_set_size(ssidField, 312, 28);
  lv_obj_set_pos(ssidField, 128, 56);
  lv_textarea_set_placeholder_text(ssidField, "Wi-Fi SSID");
  lv_obj_add_event_cb(ssidField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(ssidField, true);
  styleSettingsField(ssidField);

  passwordField = lv_textarea_create(deviceNetworkCard);
  lv_obj_set_size(passwordField, 216, 28);
  lv_obj_set_pos(passwordField, 128, 87);
  lv_textarea_set_placeholder_text(passwordField, "Wi-Fi password");
  lv_obj_add_event_cb(passwordField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(passwordField, true);
  lv_textarea_set_password_mode(passwordField, true);
  styleSettingsField(passwordField);

  showPasswordButton = lv_btn_create(deviceNetworkCard);
  lv_obj_set_size(showPasswordButton, 88, 28);
  lv_obj_set_pos(showPasswordButton, 352, 87);
  lv_obj_set_style_bg_color(showPasswordButton, rgb(20, 68, 82), 0);
  lv_obj_add_event_cb(showPasswordButton, showPasswordEvent,
                      LV_EVENT_CLICKED, nullptr);
  showPasswordLabel = lv_label_create(showPasswordButton);
  lv_label_set_text(showPasswordLabel, "SHOW");
  lv_obj_set_style_text_font(showPasswordLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(showPasswordLabel);

  latitudeField = lv_textarea_create(deviceNetworkCard);
  lv_obj_set_size(latitudeField, 140, 28);
  lv_obj_set_pos(latitudeField, 72, 118);
  lv_textarea_set_placeholder_text(latitudeField, "Latitude");
  lv_obj_add_event_cb(latitudeField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(latitudeField, true);
  styleSettingsField(latitudeField);

  longitudeField = lv_textarea_create(deviceNetworkCard);
  lv_obj_set_size(longitudeField, 140, 28);
  lv_obj_set_pos(longitudeField, 300, 118);
  lv_textarea_set_placeholder_text(longitudeField, "Longitude");
  lv_obj_add_event_cb(longitudeField, settingsFieldEvent, LV_EVENT_FOCUSED, nullptr);
  lv_textarea_set_one_line(longitudeField, true);
  styleSettingsField(longitudeField);

  settingsFormLabels[0] = makeLabel(deviceNetworkCard, "DISPLAY NAME",
      &lv_font_montserrat_12, rgb(110, 220, 255), 8, 31);
  settingsFormLabels[1] = makeLabel(deviceNetworkCard, "WI-FI SSID",
      &lv_font_montserrat_12, rgb(110, 220, 255), 8, 62);
  settingsFormLabels[2] = makeLabel(deviceNetworkCard, "PASSWORD",
      &lv_font_montserrat_12, rgb(110, 220, 255), 8, 93);
  settingsFormLabels[3] = makeLabel(deviceNetworkCard, "LAT",
      &lv_font_montserrat_12, rgb(110, 220, 255), 8, 124);
  settingsFormLabels[4] = makeLabel(deviceNetworkCard, "LONGITUDE",
      &lv_font_montserrat_12, rgb(110, 220, 255), 220, 124);

  saveSettingsButton = lv_btn_create(deviceNetworkCard);
  lv_obj_set_size(saveSettingsButton, 160, 27);
  lv_obj_set_pos(saveSettingsButton, 128, 149);
  lv_obj_set_style_bg_color(saveSettingsButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(saveSettingsButton, 5, 0);
  lv_obj_set_style_pad_hor(saveSettingsButton, 14, 0);
  lv_obj_set_style_pad_ver(saveSettingsButton, 5, 0);
  lv_obj_add_event_cb(saveSettingsButton, saveSettingsEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* saveLabel = lv_label_create(saveSettingsButton);
  lv_label_set_text(saveLabel, "SAVE SETTINGS");
  lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(saveLabel);

  settingsStatusLabel = makeLabel(
      deviceNetworkCard, "", &lv_font_montserrat_12,
      rgb(120, 240, 155), 8, 181);
  lv_obj_set_width(settingsStatusLabel, 432);
  lv_label_set_long_mode(settingsStatusLabel, LV_LABEL_LONG_CLIP);

  maintenanceCard = lv_obj_create(pagePanel);
  lv_obj_set_size(maintenanceCard, 456, 68);
  lv_obj_set_pos(maintenanceCard, 286, 271);
  styleDashboardCard(maintenanceCard);
  makeLabel(maintenanceCard, "MAINTENANCE", &lv_font_montserrat_12,
            rgb(100, 170, 180), 7, 0);

  retryButton = lv_btn_create(maintenanceCard);
  lv_obj_set_size(retryButton, 205, 23);
  lv_obj_set_pos(retryButton, 10, 15);
  lv_obj_set_style_bg_color(retryButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(retryButton, 5, 0);
  lv_obj_add_event_cb(retryButton, retryEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* retryLabel = lv_label_create(retryButton);
  lv_label_set_text(retryLabel, "RETRY ADS-B");
  lv_obj_set_style_text_font(retryLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(retryLabel);

  reconnectButton = lv_btn_create(maintenanceCard);
  lv_obj_set_size(reconnectButton, 205, 23);
  lv_obj_set_pos(reconnectButton, 231, 15);
  lv_obj_set_style_bg_color(reconnectButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(reconnectButton, 5, 0);
  lv_obj_add_event_cb(reconnectButton, reconnectEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* reconnectLabel = lv_label_create(reconnectButton);
  lv_label_set_text(reconnectLabel, "RECONNECT WI-FI");
  lv_obj_set_style_text_font(reconnectLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(reconnectLabel);

  systemMqttButton = lv_btn_create(maintenanceCard);
  lv_obj_set_size(systemMqttButton, 205, 23);
  lv_obj_set_pos(systemMqttButton, 10, 40);
  lv_obj_set_style_bg_color(systemMqttButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(systemMqttButton, 5, 0);
  lv_obj_add_event_cb(systemMqttButton, mqttOpenEvent,
                      LV_EVENT_CLICKED, nullptr);
  systemMqttLabel = lv_label_create(systemMqttButton);
  lv_label_set_text(systemMqttLabel, "HA MQTT: OFF");
  lv_obj_set_style_text_font(systemMqttLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(systemMqttLabel);

  resetSettingsButton = lv_btn_create(maintenanceCard);
  lv_obj_set_size(resetSettingsButton, 205, 23);
  lv_obj_set_pos(resetSettingsButton, 231, 40);
  lv_obj_set_style_bg_color(resetSettingsButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(resetSettingsButton, 5, 0);
  lv_obj_add_event_cb(resetSettingsButton, resetSettingsEvent,
                      LV_EVENT_CLICKED, nullptr);
  resetSettingsLabel = lv_label_create(resetSettingsButton);
  lv_label_set_text(resetSettingsLabel, "RESET DEFAULTS");
  lv_obj_set_style_text_font(resetSettingsLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(resetSettingsLabel);

  populateSettingsForm();
  syncSettingsStorageState();
  setSettingsFormVisible(false);
}

void buildOtaPanel() {
  otaPanel = lv_obj_create(pagePanel);
  lv_obj_set_size(otaPanel, 752, 337);
  lv_obj_set_pos(otaPanel, 3, 3);
  stylePanel(otaPanel);
  lv_obj_set_style_bg_color(otaPanel, rgb(7, 16, 23), 0);
  lv_obj_clear_flag(otaPanel, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(otaPanel, "FIRMWARE UPDATE", &lv_font_montserrat_28,
            rgb(63, 255, 155), 12, 7);
  lv_obj_t* installed = makeLabel(
      otaPanel, BUILD_ID, &lv_font_montserrat_12,
      rgb(100, 170, 180), 12, 45);
  lv_obj_set_width(installed, 710);
  lv_label_set_long_mode(installed, LV_LABEL_LONG_CLIP);

  otaStateLabel = makeLabel(otaPanel, "LOCAL OTA: DISABLED",
                            &lv_font_montserrat_18,
                            rgb(110, 220, 255), 12, 70);
  otaAddressLabel = makeLabel(
      otaPanel, "Enable local OTA to open the browser update page.",
      &lv_font_montserrat_14, rgb(225, 235, 240), 12, 102);
  lv_obj_set_width(otaAddressLabel, 710);
  lv_label_set_long_mode(otaAddressLabel, LV_LABEL_LONG_WRAP);
  otaCodeLabel = makeLabel(otaPanel, "ACCESS CODE  ------",
                           &lv_font_montserrat_24,
                           rgb(255, 214, 80), 12, 166);
  otaProgressLabel = makeLabel(otaPanel, "0%  0 / 0 KB",
                               &lv_font_montserrat_14,
                               rgb(120, 240, 155), 500, 178);
  lv_obj_set_width(otaProgressLabel, 220);
  lv_obj_set_style_text_align(otaProgressLabel, LV_TEXT_ALIGN_RIGHT, 0);

  otaProgressBar = lv_bar_create(otaPanel);
  lv_obj_set_size(otaProgressBar, 710, 18);
  lv_obj_set_pos(otaProgressBar, 12, 210);
  lv_bar_set_range(otaProgressBar, 0, 100);
  lv_bar_set_value(otaProgressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(otaProgressBar, rgb(20, 38, 48), LV_PART_MAIN);
  lv_obj_set_style_bg_color(otaProgressBar, rgb(24, 128, 84),
                            LV_PART_INDICATOR);

  otaMessageLabel = makeLabel(
      otaPanel, "Local OTA is disabled", &lv_font_montserrat_14,
      rgb(180, 210, 215), 12, 238);
  lv_obj_set_width(otaMessageLabel, 710);
  lv_label_set_long_mode(otaMessageLabel, LV_LABEL_LONG_WRAP);

  otaEnableButton = lv_btn_create(otaPanel);
  lv_obj_set_size(otaEnableButton, 250, 40);
  lv_obj_set_pos(otaEnableButton, 300, 280);
  lv_obj_set_style_bg_color(otaEnableButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(otaEnableButton, 6, 0);
  lv_obj_add_event_cb(otaEnableButton, otaEnableEvent,
                      LV_EVENT_CLICKED, nullptr);
  otaEnableLabel = lv_label_create(otaEnableButton);
  lv_label_set_text(otaEnableLabel, "ENABLE FOR 5 MIN");
  lv_obj_set_style_text_font(otaEnableLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(otaEnableLabel);

  otaCloseButton = lv_btn_create(otaPanel);
  lv_obj_set_size(otaCloseButton, 150, 40);
  lv_obj_set_pos(otaCloseButton, 572, 280);
  lv_obj_set_style_bg_color(otaCloseButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(otaCloseButton, 6, 0);
  lv_obj_add_event_cb(otaCloseButton, otaCloseEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* closeLabel = lv_label_create(otaCloseButton);
  lv_label_set_text(closeLabel, "CLOSE");
  lv_obj_set_style_text_font(closeLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(closeLabel);

  updateOtaPanel();
  lv_obj_add_flag(otaPanel, LV_OBJ_FLAG_HIDDEN);
}

void buildMqttPanel() {
  mqttPanel = lv_obj_create(pagePanel);
  lv_obj_set_size(mqttPanel, 752, 337);
  lv_obj_set_pos(mqttPanel, 3, 3);
  stylePanel(mqttPanel);
  lv_obj_set_style_bg_color(mqttPanel, rgb(7, 16, 23), 0);
  lv_obj_clear_flag(mqttPanel, LV_OBJ_FLAG_SCROLLABLE);

  makeLabel(mqttPanel, "HOME ASSISTANT MQTT", &lv_font_montserrat_28,
            rgb(63, 255, 155), 12, 7);
  lv_obj_t* installed = makeLabel(
      mqttPanel, BUILD_ID, &lv_font_montserrat_12,
      rgb(100, 170, 180), 12, 45);
  lv_obj_set_width(installed, 710);
  lv_label_set_long_mode(installed, LV_LABEL_LONG_CLIP);

  mqttStateLabel = makeLabel(mqttPanel, "MQTT: DISABLED",
                             &lv_font_montserrat_20,
                             rgb(110, 220, 255), 12, 74);
  mqttDeviceLabel = makeLabel(
      mqttPanel, "DEVICE  --", &lv_font_montserrat_14,
      rgb(225, 235, 240), 12, 112);
  lv_obj_set_width(mqttDeviceLabel, 710);
  lv_label_set_long_mode(mqttDeviceLabel, LV_LABEL_LONG_WRAP);

  mqttMessageLabel = makeLabel(
      mqttPanel, "MQTT is disabled. No task or PSRAM buffers are allocated.",
      &lv_font_montserrat_16, rgb(180, 210, 215), 12, 164);
  lv_obj_set_width(mqttMessageLabel, 710);
  lv_label_set_long_mode(mqttMessageLabel, LV_LABEL_LONG_WRAP);

  makeLabel(mqttPanel,
            "Dashboard file: home-assistant/aircraft-radar-view.yaml",
            &lv_font_montserrat_12, rgb(100, 170, 180), 12, 238);

  mqttToggleButton = lv_btn_create(mqttPanel);
  lv_obj_set_size(mqttToggleButton, 250, 40);
  lv_obj_set_pos(mqttToggleButton, 300, 280);
  lv_obj_set_style_bg_color(mqttToggleButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(mqttToggleButton, 6, 0);
  lv_obj_add_event_cb(mqttToggleButton, mqttToggleEvent,
                      LV_EVENT_CLICKED, nullptr);
  mqttToggleLabel = lv_label_create(mqttToggleButton);
  lv_label_set_text(mqttToggleLabel, "ENABLE MQTT");
  lv_obj_set_style_text_font(mqttToggleLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(mqttToggleLabel);

  mqttCloseButton = lv_btn_create(mqttPanel);
  lv_obj_set_size(mqttCloseButton, 150, 40);
  lv_obj_set_pos(mqttCloseButton, 572, 280);
  lv_obj_set_style_bg_color(mqttCloseButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(mqttCloseButton, 6, 0);
  lv_obj_add_event_cb(mqttCloseButton, mqttCloseEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* closeLabel = lv_label_create(mqttCloseButton);
  lv_label_set_text(closeLabel, "CLOSE");
  lv_obj_set_style_text_font(closeLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(closeLabel);

  updateMqttPanel();
  lv_obj_add_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN);
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

void updateOtaPanel() {
  if (!otaPanel) return;
  ota_update::Status status;
  ota_update::copyStatus(status);

  char stateText[96];
  if (status.serverRunning && status.secondsRemaining) {
    snprintf(stateText, sizeof(stateText), "LOCAL OTA: %s   %02lu:%02lu",
             ota_update::stateName(status.state),
             static_cast<unsigned long>(status.secondsRemaining / 60U),
             static_cast<unsigned long>(status.secondsRemaining % 60U));
  } else {
    snprintf(stateText, sizeof(stateText), "LOCAL OTA: %s",
             ota_update::stateName(status.state));
  }
  setLabelTextIfChanged(otaStateLabel, stateText);
  lv_obj_set_style_text_color(
      otaStateLabel,
      status.state == ota_update::State::ERROR
          ? rgb(255, 120, 110)
          : (status.serverRunning ? rgb(120, 240, 155)
                                  : rgb(110, 220, 255)),
      0);

  char addressText[220];
  if (status.serverRunning) {
    snprintf(addressText, sizeof(addressText),
             "OPEN ON A DEVICE ON THIS NETWORK\n%s%s%s",
             status.ipAddress,
             status.mdnsAddress[0] ? "\n" : "",
             status.mdnsAddress);
  } else {
    snprintf(addressText, sizeof(addressText),
             "Enable local OTA to open a temporary browser update page.\n"
             "Only firmware.radarota packages are accepted.");
  }
  setLabelTextIfChanged(otaAddressLabel, addressText);

  char codeText[64];
  snprintf(codeText, sizeof(codeText), "ACCESS CODE  %s",
           status.accessCode[0] ? status.accessCode : "------");
  setLabelTextIfChanged(otaCodeLabel, codeText);

  if (otaProgressBar) {
    lv_bar_set_value(otaProgressBar, status.progressPercent, LV_ANIM_OFF);
  }
  char progressText[96];
  snprintf(progressText, sizeof(progressText), "%u%%  %lu / %lu KB",
           static_cast<unsigned>(status.progressPercent),
           static_cast<unsigned long>(status.writtenBytes / 1024U),
           static_cast<unsigned long>(status.firmwareBytes / 1024U));
  setLabelTextIfChanged(otaProgressLabel, progressText);
  setLabelTextIfChanged(otaMessageLabel, status.message);

  const bool isBusy = ota_update::busy();
  if (otaEnableButton) {
    if (!status.available || isBusy) lv_obj_add_state(otaEnableButton, LV_STATE_DISABLED);
    else lv_obj_clear_state(otaEnableButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(otaEnableButton,
                         (!status.available || isBusy) ? LV_OPA_50
                                                       : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        otaEnableButton, status.serverRunning ? rgb(126, 48, 44)
                                              : rgb(24, 128, 84), 0);
  }
  setLabelTextIfChanged(
      otaEnableLabel,
      isBusy ? "UPDATE IN PROGRESS"
             : (status.serverRunning ? "DISABLE OTA" : "ENABLE FOR 5 MIN"));
  if (otaEnableLabel) lv_obj_center(otaEnableLabel);

  if (otaCloseButton) {
    if (isBusy) lv_obj_add_state(otaCloseButton, LV_STATE_DISABLED);
    else lv_obj_clear_state(otaCloseButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(otaCloseButton, isBusy ? LV_OPA_50 : LV_OPA_COVER, 0);
  }
}


void updateMqttPanel() {
  if (!mqttPanel) return;
  mqtt_service::Status status;
  mqtt_service::copyStatus(status);

  char stateText[96];
  snprintf(stateText, sizeof(stateText), "MQTT: %s",
           mqtt_service::stateName(status.state));
  setLabelTextIfChanged(mqttStateLabel, stateText);
  lv_obj_set_style_text_color(
      mqttStateLabel,
      status.state == mqtt_service::State::ERROR ||
              status.state == mqtt_service::State::NOT_CONFIGURED
          ? rgb(255, 120, 110)
          : (status.connected ? rgb(120, 240, 155)
                              : rgb(110, 220, 255)),
      0);

  char deviceText[220];
  snprintf(deviceText, sizeof(deviceText),
           "DEVICE  %s\nBROKER  %s\nRUNTIME %s",
           status.deviceId[0] ? status.deviceId : "--",
           status.configured ? "CONFIGURED IN PRIVATE config.h"
                             : "NOT CONFIGURED",
           status.clientRunning
               ? (status.connected ? "CONNECTED" : "CLIENT ACTIVE")
               : "NO MQTT TASK OR PSRAM BUFFERS");
  setLabelTextIfChanged(mqttDeviceLabel, deviceText);
  setLabelTextIfChanged(mqttMessageLabel, status.message);

  const bool canChange = settings::storageAvailable() &&
                         (status.configured || status.enabled) &&
                         !status.maintenanceActive;
  if (mqttToggleButton) {
    if (canChange) lv_obj_clear_state(mqttToggleButton, LV_STATE_DISABLED);
    else lv_obj_add_state(mqttToggleButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(mqttToggleButton,
                         canChange ? LV_OPA_COVER : LV_OPA_50, 0);
    lv_obj_set_style_bg_color(
        mqttToggleButton,
        status.enabled ? rgb(126, 48, 44) : rgb(24, 128, 84), 0);
  }
  setLabelTextIfChanged(mqttToggleLabel,
                        status.enabled ? "DISABLE MQTT" : "ENABLE MQTT");
  if (mqttToggleLabel) lv_obj_center(mqttToggleLabel);
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

bool buildUi() {
  lv_obj_t* root = lv_scr_act();
  lv_obj_set_style_bg_color(root, rgb(4, 10, 15), 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  buildHeader(root);
  if (!buildRadarPanels(root)) return false;
  buildNavigation(root);
  buildPageShell(root);
  loadAirportOptions();
  syncAirportControls();
  buildOtaPanel();
  buildMqttPanel();
  buildDetailPanel();
  populateSettingsForm();
  setSettingsFormVisible(false);
  lastSyncedRangeGeneration = app_state::rangeGeneration();
  return true;
}

void showFatalStatus(const char* message) {
  lv_obj_t* root = lv_scr_act();
  lv_obj_clean(root);
  lv_obj_set_style_bg_color(root, rgb(4, 10, 15), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* panel = lv_obj_create(root);
  lv_obj_set_size(panel, 700, 270);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, rgb(10, 18, 25), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(panel, rgb(180, 52, 52), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = makeLabel(panel, "STARTUP HALTED",
                              &lv_font_montserrat_32,
                              rgb(255, 120, 110), 18, 18);
  lv_obj_set_width(title, 640);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* reason = makeLabel(
      panel, message && message[0] ? message : "INITIALIZATION FAILED",
      &lv_font_montserrat_20, rgb(255, 220, 120), 18, 82);
  lv_obj_set_width(reason, 640);
  lv_label_set_long_mode(reason, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(reason, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* detail = makeLabel(
      panel,
      "The radar did not enter a partial operating state.\n"
      "Check the serial log for the specific failure.",
      &lv_font_montserrat_16, rgb(225, 235, 240), 18, 142);
  lv_obj_set_width(detail, 640);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* build = makeLabel(panel, BUILD_ID, &lv_font_montserrat_12,
                              rgb(100, 170, 180), 18, 224);
  lv_obj_set_width(build, 640);
  lv_obj_set_style_text_align(build, LV_TEXT_ALIGN_CENTER, 0);
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
  const uint32_t rangeGeneration = app_state::rangeGeneration();
  if (rangeGeneration != lastSyncedRangeGeneration) {
    lastSyncedRangeGeneration = rangeGeneration;
    syncRangeControls(app_state::radarRangeMiles());
    if (currentPage == 2) updatePageContent();
  }
  if (otaPanel && !lv_obj_has_flag(otaPanel, LV_OBJ_FLAG_HIDDEN)) {
    updateOtaPanel();
  }
  if (mqttPanel && !lv_obj_has_flag(mqttPanel, LV_OBJ_FLAG_HIDDEN)) {
    updateMqttPanel();
  }
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
