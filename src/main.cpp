#include <Arduino.h>
#include <Waveshare_ST7262_LVGL.h>

#include "adsb_network.h"
#include "app_state.h"
#include "aircraft_data.h"
#include "build_info.h"
#include "settings.h"
#include "ui.h"

namespace {
bool startupComplete = false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BILLS Aircraft Radar 7-inch bring-up");
  Serial.printf("Build: %s, max targets=%u\n", BUILD_ID,
                (unsigned)aircraft::MAX_TARGETS);
  Serial.printf("PSRAM: %s, size=%u\n", psramFound() ? "YES" : "NO",
                ESP.getPsramSize());

  settings::initialize();
  app_state::initialize();
  if (!ui::allocateTargetBuffer()) return;

  lcd_init();
  lvgl_port_lock(-1);
  ui::buildUi();
  lvgl_port_unlock();

  adsb::begin();
  startupComplete = true;
}

void loop() {
  if (!startupComplete) {
    delay(1000);
    return;
  }

  uint32_t now = millis();
  adsb::service();
  lvgl_port_lock(-1);
  ui::update(now);
  lvgl_port_unlock();
  delay(5);
}
