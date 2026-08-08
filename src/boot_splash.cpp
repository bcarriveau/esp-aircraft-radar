#include "boot_splash.h"

#include <lvgl.h>
#include <math.h>
#include <stdint.h>

namespace boot_splash {
namespace {

constexpr uint32_t MIN_VISIBLE_MS = 3800;
constexpr uint32_t DISMISS_FADE_MS = 260;
constexpr uint32_t SERVICE_PERIOD_MS = 40;
constexpr int RADAR_GROUP_SIZE = 250;
constexpr int RADAR_CENTER = RADAR_GROUP_SIZE / 2;
constexpr int SWEEP_RADIUS = 94;
constexpr float PI_F = 3.14159265358979323846f;

lv_obj_t* splashRoot = nullptr;
lv_obj_t* introLine = nullptr;
lv_obj_t* radarGroup = nullptr;
lv_obj_t* sweepLine = nullptr;
lv_obj_t* titleGroup = nullptr;
lv_obj_t* subtitleLabel = nullptr;
lv_obj_t* designerLabel = nullptr;
lv_obj_t* statusGroup = nullptr;
lv_obj_t* systemStatusLabel = nullptr;
lv_obj_t* systemStatusDot = nullptr;
lv_obj_t* contacts[4]{};
lv_timer_t* lifecycleTimer = nullptr;
lv_point_t sweepPoints[2]{};
uint32_t shownAtMs = 0;
uint32_t dismissStartedAtMs = 0;
bool startupReady = false;
bool dismissStarted = false;

inline lv_color_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return lv_color_make(red, green, blue);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                    const lv_font_t* font, lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  if (!label) return nullptr;
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

void setObjectOpacity(lv_obj_t* object, lv_opa_t opacity) {
  if (object) lv_obj_set_style_opa(object, opacity, 0);
}

void opacityAnimation(void* variable, int32_t value) {
  lv_obj_t* object = static_cast<lv_obj_t*>(variable);
  if (!object) return;
  lv_obj_set_style_opa(object, static_cast<lv_opa_t>(value), 0);
}

void introLineAnimation(void* variable, int32_t value) {
  lv_obj_t* object = static_cast<lv_obj_t*>(variable);
  if (!object) return;
  lv_obj_set_width(object, static_cast<lv_coord_t>(value));
  lv_obj_align(object, LV_ALIGN_CENTER, 0, 0);
}

void sweepAnimation(void*, int32_t value) {
  if (!sweepLine) return;
  const float angle = (static_cast<float>(value) - 90.0f) * PI_F / 180.0f;
  sweepPoints[0].x = RADAR_CENTER;
  sweepPoints[0].y = RADAR_CENTER;
  sweepPoints[1].x = static_cast<lv_coord_t>(
      lroundf(RADAR_CENTER + cosf(angle) * SWEEP_RADIUS));
  sweepPoints[1].y = static_cast<lv_coord_t>(
      lroundf(RADAR_CENTER + sinf(angle) * SWEEP_RADIUS));
  lv_line_set_points(sweepLine, sweepPoints, 2);
}

void startOpacityAnimation(lv_obj_t* object, int32_t from, int32_t to,
                           uint32_t delayMs, uint32_t durationMs) {
  if (!object) return;
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, object);
  lv_anim_set_values(&animation, from, to);
  lv_anim_set_time(&animation, durationMs);
  lv_anim_set_delay(&animation, delayMs);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&animation, opacityAnimation);
  lv_anim_start(&animation);
}

void makeRing(lv_obj_t* parent, int size, lv_color_t color, int borderWidth,
              lv_opa_t opacity) {
  lv_obj_t* ring = lv_obj_create(parent);
  if (!ring) return;
  lv_obj_set_size(ring, size, size);
  lv_obj_center(ring);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ring, color, 0);
  lv_obj_set_style_border_width(ring, borderWidth, 0);
  lv_obj_set_style_border_opa(ring, opacity, 0);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(ring, 0, 0);
  lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
}

void makeCrosshair(lv_obj_t* parent) {
  lv_obj_t* horizontal = lv_obj_create(parent);
  if (horizontal) {
    lv_obj_set_size(horizontal, 204, 1);
    lv_obj_center(horizontal);
    lv_obj_set_style_bg_color(horizontal, rgb(45, 128, 143), 0);
    lv_obj_set_style_bg_opa(horizontal, LV_OPA_60, 0);
    lv_obj_set_style_border_width(horizontal, 0, 0);
    lv_obj_set_style_pad_all(horizontal, 0, 0);
    lv_obj_clear_flag(horizontal, LV_OBJ_FLAG_SCROLLABLE);
  }

  lv_obj_t* vertical = lv_obj_create(parent);
  if (vertical) {
    lv_obj_set_size(vertical, 1, 204);
    lv_obj_center(vertical);
    lv_obj_set_style_bg_color(vertical, rgb(45, 128, 143), 0);
    lv_obj_set_style_bg_opa(vertical, LV_OPA_60, 0);
    lv_obj_set_style_border_width(vertical, 0, 0);
    lv_obj_set_style_pad_all(vertical, 0, 0);
    lv_obj_clear_flag(vertical, LV_OBJ_FLAG_SCROLLABLE);
  }
}

void makeAircraftSilhouette(lv_obj_t* parent) {
  static lv_point_t aircraftPoints[] = {
      {125, 48}, {131, 102}, {159, 118}, {159, 128}, {132, 119},
      {132, 151}, {142, 159}, {142, 166}, {125, 158}, {108, 166},
      {108, 159}, {118, 151}, {118, 119}, {91, 128}, {91, 118},
      {119, 102}, {125, 48}};

  lv_obj_t* aircraft = lv_line_create(parent);
  if (!aircraft) return;
  lv_obj_set_size(aircraft, RADAR_GROUP_SIZE, RADAR_GROUP_SIZE);
  lv_obj_set_pos(aircraft, 0, 0);
  lv_line_set_points(aircraft, aircraftPoints,
                     sizeof(aircraftPoints) / sizeof(aircraftPoints[0]));
  lv_obj_set_style_line_color(aircraft, rgb(46, 205, 235), 0);
  lv_obj_set_style_line_width(aircraft, 2, 0);
  lv_obj_set_style_line_rounded(aircraft, true, 0);
}

void makeCardinalLabel(lv_obj_t* parent, const char* text, int x, int y,
                       int width) {
  lv_obj_t* label = makeLabel(parent, text, &lv_font_montserrat_12,
                              rgb(89, 190, 208));
  if (!label) return;
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

void makeContact(uint8_t index, int x, int y) {
  if (!radarGroup || index >= 4) return;
  contacts[index] = lv_obj_create(radarGroup);
  if (!contacts[index]) return;
  lv_obj_set_size(contacts[index], 7, 7);
  lv_obj_set_pos(contacts[index], x, y);
  lv_obj_set_style_radius(contacts[index], LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(contacts[index], rgb(126, 244, 110), 0);
  lv_obj_set_style_bg_opa(contacts[index], LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(contacts[index], 0, 0);
  lv_obj_set_style_shadow_width(contacts[index], 8, 0);
  lv_obj_set_style_shadow_color(contacts[index], rgb(90, 230, 100), 0);
  lv_obj_set_style_shadow_opa(contacts[index], LV_OPA_50, 0);
  lv_obj_set_style_pad_all(contacts[index], 0, 0);
  lv_obj_clear_flag(contacts[index], LV_OBJ_FLAG_SCROLLABLE);
  setObjectOpacity(contacts[index], LV_OPA_TRANSP);
}

lv_obj_t* makeStatusDot(lv_obj_t* parent, int x) {
  lv_obj_t* dot = lv_obj_create(parent);
  if (!dot) return nullptr;
  lv_obj_set_size(dot, 7, 7);
  lv_obj_set_pos(dot, x, 8);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, rgb(93, 240, 115), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  return dot;
}

void setStatusLabel(lv_obj_t* parent, const char* text, int x, int width) {
  lv_obj_t* label = makeLabel(parent, text, &lv_font_montserrat_12,
                              rgb(198, 215, 220));
  if (!label) return;
  lv_obj_set_pos(label, x, 2);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

void clearPointers() {
  splashRoot = nullptr;
  introLine = nullptr;
  radarGroup = nullptr;
  sweepLine = nullptr;
  titleGroup = nullptr;
  subtitleLabel = nullptr;
  designerLabel = nullptr;
  statusGroup = nullptr;
  systemStatusLabel = nullptr;
  systemStatusDot = nullptr;
  for (lv_obj_t*& contact : contacts) contact = nullptr;
}

void deleteSplashNow() {
  if (lifecycleTimer) {
    lv_timer_del(lifecycleTimer);
    lifecycleTimer = nullptr;
  }
  if (splashRoot) {
    lv_obj_del(splashRoot);
  }
  clearPointers();
  shownAtMs = 0;
  dismissStartedAtMs = 0;
  startupReady = false;
  dismissStarted = false;
}

void lifecycleTimerCallback(lv_timer_t*) {
  if (!splashRoot) {
    if (lifecycleTimer) {
      lv_timer_del(lifecycleTimer);
      lifecycleTimer = nullptr;
    }
    return;
  }

  const uint32_t now = lv_tick_get();
  const uint32_t elapsed = now - shownAtMs;
  if (!startupReady || elapsed < MIN_VISIBLE_MS) return;

  if (!dismissStarted) {
    dismissStarted = true;
    dismissStartedAtMs = now;
    startOpacityAnimation(splashRoot, LV_OPA_COVER, LV_OPA_TRANSP,
                          0, DISMISS_FADE_MS);
    return;
  }

  if (now - dismissStartedAtMs >= DISMISS_FADE_MS + SERVICE_PERIOD_MS) {
    deleteSplashNow();
  }
}

bool buildRadarLogo() {
  radarGroup = lv_obj_create(splashRoot);
  if (!radarGroup) return false;
  lv_obj_set_size(radarGroup, RADAR_GROUP_SIZE, RADAR_GROUP_SIZE);
  lv_obj_set_pos(radarGroup, 275, 12);
  lv_obj_set_style_bg_opa(radarGroup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(radarGroup, 0, 0);
  lv_obj_set_style_pad_all(radarGroup, 0, 0);
  lv_obj_clear_flag(radarGroup, LV_OBJ_FLAG_SCROLLABLE);
  setObjectOpacity(radarGroup, LV_OPA_TRANSP);

  makeRing(radarGroup, 202, rgb(42, 211, 244), 2, LV_OPA_COVER);
  makeRing(radarGroup, 136, rgb(34, 151, 170), 1, LV_OPA_70);
  makeRing(radarGroup, 72, rgb(27, 92, 108), 1, LV_OPA_60);
  makeCrosshair(radarGroup);
  // Keep the emblem intentionally uncluttered: north is the only compass mark.
  makeCardinalLabel(radarGroup, "N", 111, 10, 28);
  makeAircraftSilhouette(radarGroup);

  sweepLine = lv_line_create(radarGroup);
  if (!sweepLine) return false;
  lv_obj_set_size(sweepLine, RADAR_GROUP_SIZE, RADAR_GROUP_SIZE);
  lv_obj_set_pos(sweepLine, 0, 0);
  lv_obj_set_style_line_color(sweepLine, rgb(34, 210, 240), 0);
  lv_obj_set_style_line_width(sweepLine, 3, 0);
  lv_obj_set_style_line_opa(sweepLine, LV_OPA_70, 0);
  sweepAnimation(nullptr, 0);

  makeContact(0, 73, 78);
  makeContact(1, 165, 87);
  makeContact(2, 76, 174);
  makeContact(3, 174, 165);
  return true;
}

const lv_font_t* titleFontFor(const char* text) {
  const char* titleText = text && text[0] ? text : "BILLS AIRCRAFT RADAR";
  const lv_font_t* fonts[] = {
      &lv_font_montserrat_32,
      &lv_font_montserrat_28,
      &lv_font_montserrat_24,
      &lv_font_montserrat_20,
      &lv_font_montserrat_18,
  };
  for (const lv_font_t* font : fonts) {
    lv_point_t size{};
    lv_txt_get_size(&size, titleText, font, 0, 0, LV_COORD_MAX,
                    LV_TEXT_FLAG_NONE);
    if (size.x <= 760) return font;
  }
  return &lv_font_montserrat_16;
}

bool buildTitleAndStatus(const char* deviceTitle) {
  const char* titleText =
      deviceTitle && deviceTitle[0] ? deviceTitle : "BILLS AIRCRAFT RADAR";
  titleGroup = lv_obj_create(splashRoot);
  if (!titleGroup) return false;
  lv_obj_set_size(titleGroup, 800, 44);
  lv_obj_set_pos(titleGroup, 0, 270);
  lv_obj_set_style_bg_opa(titleGroup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(titleGroup, 0, 0);
  lv_obj_set_style_pad_all(titleGroup, 0, 0);
  lv_obj_clear_flag(titleGroup, LV_OBJ_FLAG_SCROLLABLE);
  setObjectOpacity(titleGroup, LV_OPA_TRANSP);

  lv_obj_t* title = makeLabel(titleGroup, titleText,
                              titleFontFor(titleText),
                              rgb(224, 245, 250));
  if (!title) return false;
  lv_obj_set_pos(title, 0, 2);
  lv_obj_set_width(title, 800);
  lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  subtitleLabel = makeLabel(splashRoot, "ESP32-S3 ADS-B DISPLAY",
                            &lv_font_montserrat_16,
                            rgb(52, 211, 234));
  if (!subtitleLabel) return false;
  lv_obj_set_pos(subtitleLabel, 0, 326);
  lv_obj_set_width(subtitleLabel, 800);
  lv_obj_set_style_text_align(subtitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  setObjectOpacity(subtitleLabel, LV_OPA_TRANSP);

  designerLabel = makeLabel(splashRoot, "Designed by Bill Carriveau",
                            &lv_font_montserrat_14,
                            rgb(62, 198, 205));
  if (!designerLabel) return false;
  lv_obj_set_pos(designerLabel, 0, 363);
  lv_obj_set_width(designerLabel, 800);
  lv_obj_set_style_text_align(designerLabel, LV_TEXT_ALIGN_CENTER, 0);
  setObjectOpacity(designerLabel, LV_OPA_TRANSP);

  statusGroup = lv_obj_create(splashRoot);
  if (!statusGroup) return false;
  lv_obj_set_size(statusGroup, 690, 28);
  lv_obj_set_pos(statusGroup, 55, 420);
  lv_obj_set_style_bg_opa(statusGroup, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(statusGroup, 0, 0);
  lv_obj_set_style_pad_all(statusGroup, 0, 0);
  lv_obj_clear_flag(statusGroup, LV_OBJ_FLAG_SCROLLABLE);
  setObjectOpacity(statusGroup, LV_OPA_TRANSP);

  makeStatusDot(statusGroup, 22);
  setStatusLabel(statusGroup, "DISPLAY READY", 38, 174);
  makeStatusDot(statusGroup, 252);
  setStatusLabel(statusGroup, "TOUCH READY", 268, 164);
  systemStatusDot = makeStatusDot(statusGroup, 482);
  if (systemStatusDot) {
    lv_obj_set_style_bg_color(systemStatusDot, rgb(238, 176, 67), 0);
  }
  systemStatusLabel = makeLabel(statusGroup, "SYSTEM STARTING",
                                &lv_font_montserrat_12,
                                rgb(198, 215, 220));
  if (!systemStatusLabel) return false;
  lv_obj_set_pos(systemStatusLabel, 498, 2);
  lv_obj_set_width(systemStatusLabel, 184);
  return true;
}

void startAnimations() {
  lv_anim_t lineAnimation;
  lv_anim_init(&lineAnimation);
  lv_anim_set_var(&lineAnimation, introLine);
  lv_anim_set_values(&lineAnimation, 8, 510);
  lv_anim_set_time(&lineAnimation, 300);
  lv_anim_set_path_cb(&lineAnimation, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&lineAnimation, introLineAnimation);
  lv_anim_start(&lineAnimation);
  startOpacityAnimation(introLine, LV_OPA_COVER, LV_OPA_TRANSP,
                        500, 260);

  startOpacityAnimation(radarGroup, LV_OPA_TRANSP, LV_OPA_COVER,
                        300, 500);

  lv_anim_t sweep;
  lv_anim_init(&sweep);
  lv_anim_set_var(&sweep, sweepLine);
  lv_anim_set_values(&sweep, 0, 359);
  lv_anim_set_time(&sweep, 1600);
  lv_anim_set_delay(&sweep, 800);
  lv_anim_set_path_cb(&sweep, lv_anim_path_linear);
  lv_anim_set_exec_cb(&sweep, sweepAnimation);
  lv_anim_start(&sweep);

  startOpacityAnimation(contacts[0], LV_OPA_TRANSP, LV_OPA_COVER,
                        1040, 140);
  startOpacityAnimation(contacts[1], LV_OPA_TRANSP, LV_OPA_COVER,
                        1360, 140);
  startOpacityAnimation(contacts[2], LV_OPA_TRANSP, LV_OPA_COVER,
                        1680, 140);
  startOpacityAnimation(contacts[3], LV_OPA_TRANSP, LV_OPA_COVER,
                        2000, 140);

  startOpacityAnimation(titleGroup, LV_OPA_TRANSP, LV_OPA_COVER,
                        2400, 320);
  startOpacityAnimation(subtitleLabel, LV_OPA_TRANSP, LV_OPA_COVER,
                        2560, 320);
  startOpacityAnimation(designerLabel, LV_OPA_TRANSP, LV_OPA_COVER,
                        2700, 320);
  startOpacityAnimation(statusGroup, LV_OPA_TRANSP, LV_OPA_COVER,
                        3000, 320);
}

}  // namespace

bool show(const char* deviceTitle) {
  if (splashRoot) return true;

  lv_obj_t* screen = lv_scr_act();
  if (!screen) return false;

  splashRoot = lv_obj_create(screen);
  if (!splashRoot) return false;
  lv_obj_set_size(splashRoot, 800, 480);
  lv_obj_set_pos(splashRoot, 0, 0);
  lv_obj_set_style_bg_color(splashRoot, rgb(3, 10, 20), 0);
  lv_obj_set_style_bg_opa(splashRoot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(splashRoot, 0, 0);
  lv_obj_set_style_radius(splashRoot, 0, 0);
  lv_obj_set_style_pad_all(splashRoot, 0, 0);
  lv_obj_clear_flag(splashRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(splashRoot, LV_OBJ_FLAG_CLICKABLE);

  introLine = lv_obj_create(splashRoot);
  if (!introLine) {
    deleteSplashNow();
    return false;
  }
  lv_obj_set_size(introLine, 8, 2);
  lv_obj_align(introLine, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(introLine, rgb(35, 213, 248), 0);
  lv_obj_set_style_bg_opa(introLine, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(introLine, 0, 0);
  lv_obj_set_style_shadow_width(introLine, 12, 0);
  lv_obj_set_style_shadow_color(introLine, rgb(25, 190, 235), 0);
  lv_obj_set_style_shadow_opa(introLine, LV_OPA_50, 0);
  lv_obj_set_style_pad_all(introLine, 0, 0);
  lv_obj_clear_flag(introLine, LV_OBJ_FLAG_SCROLLABLE);

  if (!buildRadarLogo() || !buildTitleAndStatus(deviceTitle)) {
    deleteSplashNow();
    return false;
  }

  startupReady = false;
  dismissStarted = false;
  shownAtMs = lv_tick_get();

  lifecycleTimer = lv_timer_create(lifecycleTimerCallback,
                                   SERVICE_PERIOD_MS, nullptr);
  if (!lifecycleTimer) {
    deleteSplashNow();
    return false;
  }

  startAnimations();
  lv_obj_move_foreground(splashRoot);
  return true;
}

void markStartupComplete() {
  startupReady = true;
  if (systemStatusLabel) {
    lv_label_set_text(systemStatusLabel, "SYSTEM READY");
    lv_obj_set_style_text_color(systemStatusLabel, rgb(119, 241, 151), 0);
  }
  if (systemStatusDot) {
    lv_obj_set_style_bg_color(systemStatusDot, rgb(93, 240, 115), 0);
    lv_obj_set_style_shadow_width(systemStatusDot, 7, 0);
    lv_obj_set_style_shadow_color(systemStatusDot, rgb(93, 240, 115), 0);
    lv_obj_set_style_shadow_opa(systemStatusDot, LV_OPA_40, 0);
  }
}

void cancel() {
  deleteSplashNow();
}

}  // namespace boot_splash
