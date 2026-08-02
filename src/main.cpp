#include <Arduino.h>
#include <Waveshare_ST7262_LVGL.h>

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

namespace {
bool startupComplete = false;
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

  if (!settings::initialize()) {
    Serial.println(
        "WARNING: NVS unavailable or unhealthy; saving disabled and compile-time "
        "defaults used when stored values cannot be read");
  }
  app_state::initialize();
  if (!ui::allocateTargetBuffer()) return;

  lcd_init();
  display_power::initialize();
  lvgl_port_lock(-1);
  const bool uiReady = ui::buildUi();
  if (!uiReady) ui::showFatalStatus("UI INITIALIZATION FAILED");
  lvgl_port_unlock();
  if (!uiReady) {
    Serial.println("FATAL: Required UI construction failed; startup halted");
    return;
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
  mqtt_service::service();
  ota_update::service();
  lvgl_port_lock(-1);
  ui::update(now);
  lvgl_port_unlock();
  delay(5);
}
