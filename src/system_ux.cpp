#include "system_ux.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <src/extra/libs/qrcode/qrcodegen.h>
#include <esp_wifi.h>

#include <cstring>

#include "adsb_network.h"
#include "mqtt_service.h"
#include "ota_update.h"
#include "update_manager.h"

namespace system_ux {
namespace {

constexpr uint32_t SYSTEM_UX_INTERVAL_MS = 80U;
constexpr uint8_t WIFI_SCAN_RESULT_CAPACITY = 12;
constexpr uint32_t WIFI_SCAN_HOLD_TIMEOUT_MS = 5000U;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 12000U;
constexpr uint32_t WIFI_SCAN_MAX_MS_PER_CHANNEL = 120U;
constexpr lv_coord_t WIFI_SSID_FIELD_WIDTH = 216;
constexpr lv_coord_t OTA_QR_OUTER_SIZE = 146;
constexpr lv_coord_t OTA_QR_SIZE = 128;
constexpr uint8_t OTA_QR_MAX_VERSION = 5;
constexpr uint8_t OTA_QR_QUIET_MODULES = 4;
constexpr size_t OTA_QR_PALETTE_BYTES = 8U;
constexpr size_t OTA_QR_ROW_BYTES =
    (static_cast<size_t>(OTA_QR_SIZE) + 7U) / 8U;
constexpr size_t OTA_QR_CANVAS_BUFFER_BYTES =
    LV_IMG_BUF_SIZE_INDEXED_1BIT(OTA_QR_SIZE, OTA_QR_SIZE);
constexpr size_t OTA_QR_ENCODE_BUFFER_BYTES =
    (((OTA_QR_MAX_VERSION * 4U + 17U) *
      (OTA_QR_MAX_VERSION * 4U + 17U) + 7U) / 8U) + 1U;
static_assert(OTA_QR_ENCODE_BUFFER_BYTES == 173U,
              "Unexpected Product 100 QR encoder buffer size");

struct WifiScanResult {
  char ssid[33]{};
  int32_t rssi = -127;
  bool secure = false;
};

enum class WifiScanState : uint8_t {
  IDLE = 0,
  WAITING_FOR_HOLDS,
  SCANNING
};

WifiScanResult wifiResults[WIFI_SCAN_RESULT_CAPACITY]{};
uint8_t wifiResultCount = 0;
WifiScanState wifiScanState = WifiScanState::IDLE;
uint32_t wifiScanDeadlineMs = 0;
bool wifiScanOwnsHolds = false;

lv_obj_t* deviceNetworkCard = nullptr;
lv_obj_t* ssidField = nullptr;
lv_obj_t* settingsStatusLabel = nullptr;
lv_obj_t* wifiScanButton = nullptr;
lv_obj_t* wifiScanButtonLabel = nullptr;
lv_obj_t* wifiOverlay = nullptr;
lv_obj_t* wifiList = nullptr;
lv_obj_t* wifiListMessage = nullptr;
lv_obj_t* wifiOverlayCloseButton = nullptr;

lv_obj_t* otaPanel = nullptr;
lv_obj_t* otaAddressLabel = nullptr;
lv_obj_t* otaProgressBar = nullptr;
lv_obj_t* otaProgressLabel = nullptr;
lv_obj_t* otaQrCard = nullptr;
lv_obj_t* otaQrCode = nullptr;
lv_obj_t* otaQrHint = nullptr;
uint8_t otaQrCanvasBuffer[OTA_QR_CANVAS_BUFFER_BYTES]{};
uint8_t otaQrTempBuffer[OTA_QR_ENCODE_BUFFER_BYTES]{};
uint8_t otaQrEncodedBuffer[OTA_QR_ENCODE_BUFFER_BYTES]{};
char otaQrUrl[64]{};
uint32_t lastSystemUxUpdateMs = 0;

inline lv_color_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return lv_color_make(red, green, blue);
}

bool isLabel(const lv_obj_t* object) {
  return object && lv_obj_check_type(object, &lv_label_class);
}

bool isTextArea(const lv_obj_t* object) {
  return object && lv_obj_check_type(object, &lv_textarea_class);
}

lv_obj_t* findLabelRecursive(lv_obj_t* parent, const char* exactText) {
  if (!parent || !exactText) return nullptr;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t i = 0; i < childCount; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!child) continue;
    if (isLabel(child)) {
      const char* text = lv_label_get_text(child);
      if (text && strcmp(text, exactText) == 0) return child;
    }
    if (lv_obj_t* nested = findLabelRecursive(child, exactText)) {
      return nested;
    }
  }
  return nullptr;
}

lv_obj_t* findNthDirectTextArea(lv_obj_t* parent, uint8_t wantedIndex) {
  if (!parent) return nullptr;
  uint8_t index = 0;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t i = 0; i < childCount; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!isTextArea(child)) continue;
    if (index == wantedIndex) return child;
    ++index;
  }
  return nullptr;
}

lv_obj_t* findLastDirectLabel(lv_obj_t* parent) {
  if (!parent) return nullptr;
  lv_obj_t* found = nullptr;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t i = 0; i < childCount; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (isLabel(child)) found = child;
  }
  return found;
}

lv_obj_t* findNthDirectLabel(lv_obj_t* parent, uint8_t wantedIndex) {
  if (!parent) return nullptr;
  uint8_t index = 0;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t i = 0; i < childCount; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!isLabel(child)) continue;
    if (index == wantedIndex) return child;
    ++index;
  }
  return nullptr;
}

lv_obj_t* findFirstDirectBar(lv_obj_t* parent) {
  if (!parent) return nullptr;
  const uint32_t childCount = lv_obj_get_child_cnt(parent);
  for (uint32_t i = 0; i < childCount; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (child && lv_obj_check_type(child, &lv_bar_class)) return child;
  }
  return nullptr;
}

void setStatus(const char* text, lv_color_t color) {
  if (!settingsStatusLabel) return;
  lv_label_set_text(settingsStatusLabel, text ? text : "");
  lv_obj_set_style_text_color(settingsStatusLabel, color, 0);
}

void setScanButtonText(const char* text) {
  if (!wifiScanButtonLabel) return;
  lv_label_set_text(wifiScanButtonLabel, text ? text : "SCAN");
  lv_obj_center(wifiScanButtonLabel);
}

void setScanButtonEnabled(bool enabled) {
  if (!wifiScanButton) return;
  if (enabled) lv_obj_clear_state(wifiScanButton, LV_STATE_DISABLED);
  else lv_obj_add_state(wifiScanButton, LV_STATE_DISABLED);
  lv_obj_set_style_opa(wifiScanButton, enabled ? LV_OPA_COVER : LV_OPA_50, 0);
}

void releaseScanHolds() {
  if (!wifiScanOwnsHolds) return;
  mqtt_service::releaseMaintenanceHold();
  adsb::releaseMaintenanceHold();
  wifiScanOwnsHolds = false;
}

void resetScanState() {
  wifiScanState = WifiScanState::IDLE;
  wifiScanDeadlineMs = 0;
  setScanButtonText("SCAN");
  setScanButtonEnabled(true);
}

void failScan(const char* message) {
  if (wifiScanState == WifiScanState::SCANNING) {
    (void)esp_wifi_scan_stop();
  }
  WiFi.scanDelete();
  releaseScanHolds();
  resetScanState();
  if (wifiList) lv_obj_add_flag(wifiList, LV_OBJ_FLAG_HIDDEN);
  if (wifiListMessage) {
    lv_label_set_text(wifiListMessage, message ? message : "Wi-Fi scan failed");
    lv_obj_clear_flag(wifiListMessage, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifiOverlayCloseButton) {
    lv_obj_clear_state(wifiOverlayCloseButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(wifiOverlayCloseButton, LV_OPA_COVER, 0);
  }
  setStatus(message, rgb(255, 170, 95));
}

void hideWifiOverlay() {
  if (wifiOverlay) lv_obj_add_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
}

bool sameSsid(const WifiScanResult& result, const char* ssid) {
  return ssid && strcmp(result.ssid, ssid) == 0;
}

void rememberWifiResult(const char* ssid, int32_t rssi, bool secure) {
  if (!ssid || !ssid[0]) return;

  for (uint8_t i = 0; i < wifiResultCount; ++i) {
    if (!sameSsid(wifiResults[i], ssid)) continue;
    if (rssi > wifiResults[i].rssi) {
      wifiResults[i].rssi = rssi;
      wifiResults[i].secure = secure;
    }
    return;
  }

  WifiScanResult candidate;
  snprintf(candidate.ssid, sizeof(candidate.ssid), "%s", ssid);
  candidate.rssi = rssi;
  candidate.secure = secure;

  if (wifiResultCount < WIFI_SCAN_RESULT_CAPACITY) {
    wifiResults[wifiResultCount++] = candidate;
    return;
  }

  uint8_t weakest = 0;
  for (uint8_t i = 1; i < wifiResultCount; ++i) {
    if (wifiResults[i].rssi < wifiResults[weakest].rssi) weakest = i;
  }
  if (candidate.rssi > wifiResults[weakest].rssi) {
    wifiResults[weakest] = candidate;
  }
}

void sortWifiResults() {
  for (uint8_t i = 1; i < wifiResultCount; ++i) {
    WifiScanResult value = wifiResults[i];
    uint8_t position = i;
    while (position > 0 && wifiResults[position - 1].rssi < value.rssi) {
      wifiResults[position] = wifiResults[position - 1];
      --position;
    }
    wifiResults[position] = value;
  }
}

void wifiNetworkSelected(lv_event_t* event) {
  const uintptr_t rawIndex = reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(event));
  if (rawIndex >= wifiResultCount || !ssidField) return;
  const uint8_t index = static_cast<uint8_t>(rawIndex);
  lv_textarea_set_text(ssidField, wifiResults[index].ssid);
  hideWifiOverlay();
  setStatus("SSID selected; enter password and SAVE SETTINGS",
            rgb(120, 240, 155));
}

void wifiOverlayClose(lv_event_t*) {
  if (wifiScanState != WifiScanState::IDLE) return;
  if (wifiList) lv_obj_clean(wifiList);
  wifiResultCount = 0;
  hideWifiOverlay();
}

void populateWifiList() {
  if (!wifiList || !wifiListMessage) return;
  lv_obj_clean(wifiList);

  if (wifiResultCount == 0) {
    lv_label_set_text(wifiListMessage,
                      "No named Wi-Fi networks found.\nManual SSID entry remains available.");
    lv_obj_add_flag(wifiList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifiListMessage, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(wifiList, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(wifiListMessage, LV_OBJ_FLAG_HIDDEN);
  for (uint8_t i = 0; i < wifiResultCount; ++i) {
    lv_obj_t* row = lv_btn_create(wifiList);
    lv_obj_set_size(row, 492, 38);
    lv_obj_set_pos(row, 4, static_cast<lv_coord_t>(i * 42));
    lv_obj_set_style_bg_color(row, rgb(12, 38, 49), 0);
    lv_obj_set_style_bg_color(row, rgb(18, 92, 60), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(row, rgb(35, 88, 98), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_add_event_cb(row, wifiNetworkSelected, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

    lv_obj_t* label = lv_label_create(row);
    char text[72];
    snprintf(text, sizeof(text), "%s   %ld dBm   %s",
             wifiResults[i].ssid, static_cast<long>(wifiResults[i].rssi),
             wifiResults[i].secure ? "SECURE" : "OPEN");
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, rgb(225, 235, 240), 0);
    lv_obj_set_width(label, 468);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_center(label);
  }
  lv_obj_scroll_to_y(wifiList, 0, LV_ANIM_OFF);
}

void finishWifiScan(int16_t foundCount) {
  wifiResultCount = 0;
  // Arduino-ESP32 exposes scan-result accessors with an 8-bit item index.
  // Bound the library-facing iteration explicitly even if scanComplete() ever
  // reports more entries than those accessors can address.
  const int16_t readableCount = foundCount > 255 ? 255 : foundCount;
  for (int16_t i = 0; i < readableCount; ++i) {
    const uint8_t item = static_cast<uint8_t>(i);
    const String ssid = WiFi.SSID(item);
    if (ssid.isEmpty()) continue;
    rememberWifiResult(ssid.c_str(), WiFi.RSSI(item),
                       WiFi.encryptionType(item) != WIFI_AUTH_OPEN);
  }
  sortWifiResults();
  WiFi.scanDelete();
  releaseScanHolds();
  resetScanState();
  populateWifiList();
  if (wifiOverlayCloseButton) {
    lv_obj_clear_state(wifiOverlayCloseButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(wifiOverlayCloseButton, LV_OPA_COVER, 0);
  }

  if (deviceNetworkCard &&
      !lv_obj_has_flag(deviceNetworkCard, LV_OBJ_FLAG_HIDDEN) && wifiOverlay) {
    lv_obj_move_foreground(wifiOverlay);
    lv_obj_clear_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
  }

  char status[64];
  snprintf(status, sizeof(status), "%u Wi-Fi network%s found",
           static_cast<unsigned>(wifiResultCount),
           wifiResultCount == 1 ? "" : "s");
  setStatus(status, rgb(110, 220, 255));
}

void beginRadioScan() {
  WiFi.setScanTimeout(WIFI_SCAN_TIMEOUT_MS);
  WiFi.scanDelete();
  const int16_t result = WiFi.scanNetworks(
      true, false, false, WIFI_SCAN_MAX_MS_PER_CHANNEL);
  if (result == WIFI_SCAN_FAILED) {
    failScan("Wi-Fi scan could not start; try again");
    return;
  }
  if (result >= 0) {
    finishWifiScan(result);
    return;
  }
  wifiScanState = WifiScanState::SCANNING;
  wifiScanDeadlineMs = 0;
  setScanButtonText("SCANNING");
  setStatus("Scanning nearby Wi-Fi networks...", rgb(110, 220, 255));
}

void wifiScanRequested(lv_event_t*) {
  if (wifiScanState != WifiScanState::IDLE) return;

  ota_update::Status otaStatus;
  ota_update::copyStatus(otaStatus);
  if (otaStatus.serverRunning || ota_update::busy()) {
    setStatus("Disable firmware OTA before scanning Wi-Fi",
              rgb(255, 190, 95));
    return;
  }
  if (update_manager::networkCheckInProgress() ||
      adsb::wifiOperationInProgress() || adsb::maintenanceHoldActive() ||
      mqtt_service::maintenanceHoldActive()) {
    setStatus("Network maintenance is busy; try Wi-Fi scan again",
              rgb(255, 190, 95));
    return;
  }

  if (!adsb::requestMaintenanceHold()) {
    setStatus("Wi-Fi recovery is busy; try scan again",
              rgb(255, 190, 95));
    return;
  }
  mqtt_service::requestMaintenanceHold();
  wifiScanOwnsHolds = true;
  wifiScanState = WifiScanState::WAITING_FOR_HOLDS;
  wifiScanDeadlineMs = millis() + WIFI_SCAN_HOLD_TIMEOUT_MS;
  setScanButtonEnabled(false);
  setScanButtonText("WAIT");
  if (wifiList) {
    lv_obj_clean(wifiList);
    lv_obj_add_flag(wifiList, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifiListMessage) {
    lv_label_set_text(wifiListMessage,
                      "Pausing network activity for Wi-Fi scan...");
    lv_obj_clear_flag(wifiListMessage, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifiOverlayCloseButton) {
    lv_obj_add_state(wifiOverlayCloseButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(wifiOverlayCloseButton, LV_OPA_50, 0);
  }
  if (wifiOverlay) {
    lv_obj_move_foreground(wifiOverlay);
    lv_obj_clear_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
  }
  setStatus("Pausing network activity for Wi-Fi scan...",
            rgb(110, 220, 255));
}

bool buildWifiPicker() {
  lv_obj_t* title = findLabelRecursive(lv_scr_act(), "DEVICE & NETWORK");
  if (!title) return false;
  deviceNetworkCard = lv_obj_get_parent(title);
  if (!deviceNetworkCard) return false;

  // The device card contains five direct textareas in stable form order:
  // display name, SSID, password, latitude, longitude. SSID is index 1.
  ssidField = findNthDirectTextArea(deviceNetworkCard, 1);
  settingsStatusLabel = findLastDirectLabel(deviceNetworkCard);
  if (!ssidField || !settingsStatusLabel) return false;

  lv_obj_set_width(ssidField, WIFI_SSID_FIELD_WIDTH);

  wifiScanButton = lv_btn_create(deviceNetworkCard);
  lv_obj_set_size(wifiScanButton, 88, 28);
  lv_obj_set_pos(wifiScanButton, 352, 55);
  lv_obj_set_style_bg_color(wifiScanButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_bg_color(wifiScanButton, rgb(18, 92, 60), LV_STATE_PRESSED);
  lv_obj_set_style_radius(wifiScanButton, 5, 0);
  lv_obj_set_style_shadow_width(wifiScanButton, 0, 0);
  lv_obj_add_event_cb(wifiScanButton, wifiScanRequested,
                      LV_EVENT_CLICKED, nullptr);
  wifiScanButtonLabel = lv_label_create(wifiScanButton);
  lv_label_set_text(wifiScanButtonLabel, "SCAN");
  lv_obj_set_style_text_font(wifiScanButtonLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(wifiScanButtonLabel);

  wifiOverlay = lv_obj_create(lv_scr_act());
  lv_obj_set_size(wifiOverlay, 800, 480);
  lv_obj_set_pos(wifiOverlay, 0, 0);
  lv_obj_set_style_bg_color(wifiOverlay, rgb(0, 0, 0), 0);
  lv_obj_set_style_bg_opa(wifiOverlay, LV_OPA_70, 0);
  lv_obj_set_style_border_width(wifiOverlay, 0, 0);
  lv_obj_set_style_pad_all(wifiOverlay, 0, 0);
  lv_obj_clear_flag(wifiOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* panel = lv_obj_create(wifiOverlay);
  lv_obj_set_size(panel, 560, 330);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, rgb(7, 20, 28), 0);
  lv_obj_set_style_border_color(panel, rgb(35, 105, 115), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* popupTitle = lv_label_create(panel);
  lv_label_set_text(popupTitle, "SELECT WI-FI NETWORK");
  lv_obj_set_style_text_font(popupTitle, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(popupTitle, rgb(63, 255, 155), 0);
  lv_obj_set_pos(popupTitle, 16, 14);

  wifiOverlayCloseButton = lv_btn_create(panel);
  lv_obj_set_size(wifiOverlayCloseButton, 92, 32);
  lv_obj_set_pos(wifiOverlayCloseButton, 450, 10);
  lv_obj_set_style_bg_color(wifiOverlayCloseButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(wifiOverlayCloseButton, 5, 0);
  lv_obj_add_event_cb(wifiOverlayCloseButton, wifiOverlayClose,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* closeLabel = lv_label_create(wifiOverlayCloseButton);
  lv_label_set_text(closeLabel, "CLOSE");
  lv_obj_set_style_text_font(closeLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(closeLabel);

  wifiListMessage = lv_label_create(panel);
  lv_label_set_text(wifiListMessage, "Scanning...");
  lv_obj_set_style_text_font(wifiListMessage, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifiListMessage, rgb(180, 210, 215), 0);
  lv_obj_set_width(wifiListMessage, 510);
  lv_obj_set_pos(wifiListMessage, 18, 54);
  lv_label_set_long_mode(wifiListMessage, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(wifiListMessage, LV_OBJ_FLAG_HIDDEN);

  wifiList = lv_obj_create(panel);
  lv_obj_set_size(wifiList, 522, 258);
  lv_obj_set_pos(wifiList, 18, 56);
  lv_obj_set_style_bg_color(wifiList, rgb(5, 16, 23), 0);
  lv_obj_set_style_border_color(wifiList, rgb(28, 70, 80), 0);
  lv_obj_set_style_border_width(wifiList, 1, 0);
  lv_obj_set_style_radius(wifiList, 5, 0);
  lv_obj_set_style_pad_all(wifiList, 6, 0);
  lv_obj_set_scroll_dir(wifiList, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(wifiList, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_add_flag(wifiOverlay, LV_OBJ_FLAG_HIDDEN);
  return true;
}

void clearOtaQrCanvas() {
  // Palette bytes are owned by LVGL. The indexed pixel payload follows them.
  // 1 selects the white palette entry; black modules clear individual bits.
  memset(otaQrCanvasBuffer + OTA_QR_PALETTE_BYTES, 0xFF,
         OTA_QR_ROW_BYTES * static_cast<size_t>(OTA_QR_SIZE));
}

bool renderOtaQr(const char* url) {
  if (!otaQrCode || !url || !url[0]) return false;

  memset(otaQrTempBuffer, 0, sizeof(otaQrTempBuffer));
  memset(otaQrEncodedBuffer, 0, sizeof(otaQrEncodedBuffer));
  if (!qrcodegen_encodeText(
          url, otaQrTempBuffer, otaQrEncodedBuffer,
          qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
          OTA_QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) {
    return false;
  }

  const int moduleCount = qrcodegen_getSize(otaQrEncodedBuffer);
  if (moduleCount <= 0) return false;

  const int totalModules =
      moduleCount + static_cast<int>(OTA_QR_QUIET_MODULES) * 2;
  const int scale = static_cast<int>(OTA_QR_SIZE) / totalModules;
  if (scale <= 0) return false;

  clearOtaQrCanvas();

  const int renderedSize = totalModules * scale;
  const int quietPixels = static_cast<int>(OTA_QR_QUIET_MODULES) * scale;
  const int origin =
      (static_cast<int>(OTA_QR_SIZE) - renderedSize) / 2 + quietPixels;

  for (int moduleY = 0; moduleY < moduleCount; ++moduleY) {
    for (int moduleX = 0; moduleX < moduleCount; ++moduleX) {
      if (!qrcodegen_getModule(
              otaQrEncodedBuffer, moduleX, moduleY)) {
        continue;
      }

      const int startX = origin + moduleX * scale;
      const int startY = origin + moduleY * scale;
      for (int pixelY = 0; pixelY < scale; ++pixelY) {
        const size_t row =
            static_cast<size_t>(startY + pixelY) * OTA_QR_ROW_BYTES;
        for (int pixelX = 0; pixelX < scale; ++pixelX) {
          const int x = startX + pixelX;
          uint8_t& byte =
              otaQrCanvasBuffer[OTA_QR_PALETTE_BYTES + row +
                                static_cast<size_t>(x >> 3)];
          byte = static_cast<uint8_t>(
              byte & ~(1U << (7 - (x & 0x7))));
        }
      }
    }
  }

  lv_img_cache_invalidate_src(lv_canvas_get_img(otaQrCode));
  lv_obj_invalidate(otaQrCode);
  return true;
}

bool buildOtaQr() {
  lv_obj_t* title = findLabelRecursive(lv_scr_act(), "FIRMWARE UPDATE");
  if (!title) return false;
  otaPanel = lv_obj_get_parent(title);
  if (!otaPanel) return false;

  // ui::buildUi() calls updateOtaPanel() before this module attaches, so the
  // address/progress label text is already live state and cannot be located by
  // its construction-time placeholder. Resolve the stable direct-child layout
  // instead: title, build, state, address, access code, progress, message.
  otaAddressLabel = findNthDirectLabel(otaPanel, 3);
  otaProgressLabel = findNthDirectLabel(otaPanel, 5);
  otaProgressBar = findFirstDirectBar(otaPanel);
  if (!otaAddressLabel || !otaProgressLabel || !otaProgressBar) {
    Serial.printf(
        "Product 100 OTA QR attachment detail: address=%s progress=%s bar=%s\n",
        otaAddressLabel ? "yes" : "no",
        otaProgressLabel ? "yes" : "no",
        otaProgressBar ? "yes" : "no");
    return false;
  }

  lv_obj_set_width(otaAddressLabel, 540);
  lv_obj_set_width(otaProgressBar, 540);
  lv_obj_set_pos(otaProgressLabel, 390, 178);
  lv_obj_set_width(otaProgressLabel, 160);

  otaQrHint = lv_label_create(otaPanel);
  lv_label_set_text(otaQrHint, "SCAN TO OPEN");
  lv_obj_set_style_text_font(otaQrHint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(otaQrHint, rgb(110, 220, 255), 0);
  lv_obj_set_width(otaQrHint, OTA_QR_OUTER_SIZE);
  lv_obj_set_style_text_align(otaQrHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(otaQrHint, 576, 54);

  otaQrCard = lv_obj_create(otaPanel);
  lv_obj_set_size(otaQrCard, OTA_QR_OUTER_SIZE, OTA_QR_OUTER_SIZE);
  lv_obj_set_pos(otaQrCard, 576, 72);
  lv_obj_set_style_bg_color(otaQrCard, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(otaQrCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(otaQrCard, 0, 0);
  lv_obj_set_style_radius(otaQrCard, 0, 0);
  lv_obj_set_style_pad_all(otaQrCard, 0, 0);
  lv_obj_clear_flag(otaQrCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(otaQrCard, LV_OBJ_FLAG_CLICKABLE);

  otaQrCode = lv_canvas_create(otaQrCard);
  if (!otaQrCode) return false;
  lv_canvas_set_buffer(otaQrCode, otaQrCanvasBuffer,
                       OTA_QR_SIZE, OTA_QR_SIZE,
                       LV_IMG_CF_INDEXED_1BIT);
  lv_canvas_set_palette(otaQrCode, 0, lv_color_black());
  lv_canvas_set_palette(otaQrCode, 1, lv_color_white());
  clearOtaQrCanvas();
  lv_obj_set_size(otaQrCode, OTA_QR_SIZE, OTA_QR_SIZE);
  lv_obj_center(otaQrCode);

  lv_obj_add_flag(otaQrHint, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(otaQrCard, LV_OBJ_FLAG_HIDDEN);
  Serial.println("Product 100 OTA QR UI attached");
  return true;
}

void updateWifiScan(uint32_t now) {
  if (wifiOverlay && deviceNetworkCard &&
      lv_obj_has_flag(deviceNetworkCard, LV_OBJ_FLAG_HIDDEN)) {
    hideWifiOverlay();
  }

  if (wifiScanState == WifiScanState::IDLE) return;

  if (wifiScanState == WifiScanState::WAITING_FOR_HOLDS) {
    if ((int32_t)(now - wifiScanDeadlineMs) >= 0) {
      failScan("Network did not become idle; Wi-Fi scan cancelled");
      return;
    }
    if (adsb::maintenanceHoldActive() &&
        mqtt_service::maintenanceHoldActive()) {
      beginRadioScan();
    }
    return;
  }

  const int16_t result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) return;
  if (result == WIFI_SCAN_FAILED) {
    failScan("Wi-Fi scan failed; try again");
    return;
  }
  if (result >= 0) finishWifiScan(result);
}

void updateOtaQr() {
  if (!otaQrCard || !otaQrCode || !otaQrHint) return;
  ota_update::Status status;
  ota_update::copyStatus(status);

  const bool showQr = status.serverRunning && status.ipAddress[0];
  if (!showQr) {
    otaQrUrl[0] = 0;
    lv_obj_add_flag(otaQrCard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(otaQrHint, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (strcmp(otaQrUrl, status.ipAddress) != 0) {
    if (!renderOtaQr(status.ipAddress)) {
      otaQrUrl[0] = 0;
      lv_obj_add_flag(otaQrCard, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(otaQrHint, "QR ERROR - USE URL");
      lv_obj_set_style_text_color(otaQrHint, rgb(255, 190, 95), 0);
      lv_obj_clear_flag(otaQrHint, LV_OBJ_FLAG_HIDDEN);
      Serial.printf("Product 100 OTA QR render failed for URL length %u\n",
                    static_cast<unsigned>(strlen(status.ipAddress)));
      return;
    }
    snprintf(otaQrUrl, sizeof(otaQrUrl), "%s", status.ipAddress);
    lv_label_set_text(otaQrHint, "SCAN TO OPEN");
    lv_obj_set_style_text_color(otaQrHint, rgb(110, 220, 255), 0);
  }

  lv_obj_clear_flag(otaQrCard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(otaQrHint, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

bool build() {
  const bool wifiReady = buildWifiPicker();
  const bool qrReady = buildOtaQr();
  if (!wifiReady) {
    Serial.println("Product 100 Wi-Fi picker UI attachment failed");
  }
  if (!qrReady) {
    Serial.println("Product 100 OTA QR UI attachment failed");
  }
  return wifiReady && qrReady;
}

void update(uint32_t now) {
  if (now - lastSystemUxUpdateMs < SYSTEM_UX_INTERVAL_MS) return;
  lastSystemUxUpdateMs = now;
  updateWifiScan(now);
  updateOtaQr();
}

}  // namespace system_ux
