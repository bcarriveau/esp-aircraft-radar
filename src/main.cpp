#include <Arduino.h>
#include <Waveshare_ST7262_LVGL.h>

#include <esp_bt.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_data.h"
#include "airport_data.h"
#include "build_info.h"
#include "display_power.h"
#include "mqtt_service.h"
#include "ota_update.h"
#include "settings.h"
#include "ui.h"
#include "update_manager.h"
#include "update_ui.h"

namespace {
bool startupComplete = false;

void releaseUnusedBluetoothControllerMemory() {
  const size_t heapBefore = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestBefore = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  const esp_err_t result =
      esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  const size_t heapAfter = heap_caps_get_free_size(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestAfter = heap_caps_get_largest_free_block(
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (result == ESP_OK) {
    Serial.printf(
        "Unused BLE controller memory released: internal heap +%ld, "
        "largest block %+ld bytes\n",
        static_cast<long>(heapAfter) - static_cast<long>(heapBefore),
        static_cast<long>(largestAfter) -
            static_cast<long>(largestBefore));
  } else {
    Serial.printf("BLE controller memory release skipped: %s (%d)\n",
                  esp_err_to_name(result), static_cast<int>(result));
  }
}
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BILLS Aircraft Radar 7-inch bring-up");
  Serial.printf("Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
  Serial.printf("Build: %s, max targets=%u\n", BUILD_ID,
                (unsigned)aircraft::MAX_TARGETS);
  Serial.printf("PSRAM: %s, size=%u\n", psramFound() ? "YES" : "NO",
                ESP.getPsramSize());
  releaseUnusedBluetoothControllerMemory();

  if (!settings::initialize()) {
    Serial.println(
        "WARNING: NVS unavailable or unhealthy; saving disabled and compile-time "
        "defaults used when stored values cannot be read");
  }
  if (!update_manager::begin()) {
    Serial.println(
        "WARNING: Update-check persistence unavailable; using boot-local schedule");
  }
  app_state::initialize();
  if (!ui::allocateTargetBuffer()) return;

  lcd_init();
  display_power::initialize();
  lvgl_port_lock(-1);
  const bool uiReady = ui::buildUi();
  const bool updateUiReady = uiReady && update_ui::build();
  if (!uiReady) ui::showFatalStatus("UI INITIALIZATION FAILED");
  lvgl_port_unlock();
  if (!uiReady) {
    Serial.println("FATAL: Required UI construction failed; startup halted");
    return;
  }
  if (!updateUiReady) {
    Serial.println(
        "WARNING: GitHub update indicator UI unavailable; radar continuing");
  }

  if (!airport_data::initialize(settings::homeLatitude(),
                                settings::homeLongitude())) {
    Serial.println(
        "WARNING: Airport overlay unavailable; aircraft radar continuing");
  }

  if (!adsb::begin()) {
    Serial.println("FATAL: ADSB networking initialization failed; startup halted");
    lvgl_port_lock(-1);
    ui::showFatalStatus("NETWORK INITIALIZATION FAILED");
    lvgl_port_unlock();
    return;
  }

  if (!ota_update::begin()) {
    Serial.println("WARNING: Local OTA update service is unavailable");
  }
  if (!mqtt_service::begin()) {
    Serial.println("WARNING: MQTT service initialization failed");
  }

  startupComplete = true;
  Serial.println("Startup complete");
}

void loop() {
  if (!startupComplete) {
    delay(1000);
    return;
  }

  uint32_t now = millis();
  adsb::service();
  update_manager::service();
  if (!update_manager::networkCheckInProgress()) {
    mqtt_service::service();
  }
  if (!adsb::wifiOperationInProgress()) ota_update::service();
  lvgl_port_lock(-1);
  ui::update(now);
  update_ui::update(now);
  lvgl_port_unlock();
  delay(5);
}
