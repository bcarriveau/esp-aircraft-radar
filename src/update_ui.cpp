#include "update_ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstring>
#include <ctime>

#include "build_info.h"
#include "update_manager.h"

namespace update_ui {
namespace {

lv_obj_t* headerButton = nullptr;
lv_obj_t* systemButton = nullptr;
lv_obj_t* navigationButtons[5]{};
lv_obj_t* systemSummaryButton = nullptr;
lv_obj_t* systemSummaryLabel = nullptr;
lv_obj_t* detailPanel = nullptr;
lv_obj_t* installedLabel = nullptr;
lv_obj_t* availableLabel = nullptr;
lv_obj_t* lastCheckLabel = nullptr;
lv_obj_t* messageLabel = nullptr;
lv_obj_t* notesLabel = nullptr;
lv_obj_t* checkNowButton = nullptr;
lv_obj_t* checkNowLabel = nullptr;
lv_obj_t* installButton = nullptr;
lv_obj_t* installLabel = nullptr;
lv_obj_t* laterButton = nullptr;
uint32_t renderedStatusVersion = UINT32_MAX;
uint32_t installConfirmUntilMs = 0;

inline lv_color_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return lv_color_make(red, green, blue);
}

void setLabelTextIfChanged(lv_obj_t* label, const char* text) {
  if (!label || !text) return;
  const char* current = lv_label_get_text(label);
  if (!current || strcmp(current, text) != 0) lv_label_set_text(label, text);
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

lv_obj_t* findLabelRecursive(lv_obj_t* parent, const char* text) {
  if (!parent || !text) return nullptr;
  const uint32_t count = lv_obj_get_child_cnt(parent);
  for (uint32_t index = 0; index < count; ++index) {
    lv_obj_t* child = lv_obj_get_child(parent, index);
    if (!child) continue;
    if (lv_obj_check_type(child, &lv_label_class)) {
      const char* value = lv_label_get_text(child);
      if (value && strcmp(value, text) == 0) return child;
    }
    if (lv_obj_t* found = findLabelRecursive(child, text)) return found;
  }
  return nullptr;
}

lv_obj_t* findPagePanel(lv_obj_t* root) {
  if (!root) return nullptr;
  const uint32_t count = lv_obj_get_child_cnt(root);
  for (uint32_t index = 0; index < count; ++index) {
    lv_obj_t* child = lv_obj_get_child(root, index);
    if (!child) continue;
    if (lv_obj_get_x(child) == 10 && lv_obj_get_y(child) == 52 &&
        lv_obj_get_width(child) == 780 && lv_obj_get_height(child) == 365) {
      return child;
    }
  }
  return nullptr;
}

void formatEpoch(uint32_t epoch, char* output, size_t capacity) {
  if (!output || capacity == 0) return;
  if (!epoch) {
    snprintf(output, capacity, "Never");
    return;
  }
  const time_t value = static_cast<time_t>(epoch);
  struct tm local{};
  if (!localtime_r(&value, &local)) {
    snprintf(output, capacity, "Unavailable");
    return;
  }
  strftime(output, capacity, "%b %d, %Y  %I:%M %p", &local);
}

void formatBytes(uint32_t bytes, char* output, size_t capacity) {
  if (!output || capacity == 0) return;
  if (bytes >= 1024U * 1024U) {
    snprintf(output, capacity, "%.2f MB",
             static_cast<double>(bytes) / (1024.0 * 1024.0));
  } else if (bytes >= 1024U) {
    snprintf(output, capacity, "%.1f KB",
             static_cast<double>(bytes) / 1024.0);
  } else {
    snprintf(output, capacity, "%lu B",
             static_cast<unsigned long>(bytes));
  }
}

void showDetails() {
  if (!detailPanel) return;
  if (systemButton) lv_event_send(systemButton, LV_EVENT_CLICKED, nullptr);
  lv_obj_move_foreground(detailPanel);
  lv_obj_clear_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
}

void headerEvent(lv_event_t*) { showDetails(); }
void summaryEvent(lv_event_t*) { showDetails(); }

void navigationEvent(lv_event_t* event) {
  if (!event) return;
  lv_obj_t* target = lv_event_get_target(event);
  const bool systemSelected = target == systemButton;
  if (systemSummaryButton) {
    if (systemSelected) lv_obj_clear_flag(systemSummaryButton, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(systemSummaryButton, LV_OBJ_FLAG_HIDDEN);
  }
  if (!systemSelected && detailPanel) {
    lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  }
}

void checkNowEvent(lv_event_t*) {
  update_manager::requestManualCheck();
}

void installEvent(lv_event_t*) {
  update_manager::Status status;
  update_manager::copyStatus(status);
  if (status.installQueued || status.installing) {
    if (status.installResult != update_manager::InstallResult::RESTARTING) {
      update_manager::requestInstallAbort();
    }
    installConfirmUntilMs = 0;
    renderedStatusVersion = UINT32_MAX;
    return;
  }
  if (!status.updateAvailable) return;

  const uint32_t now = millis();
  if (!installConfirmUntilMs ||
      static_cast<int32_t>(now - installConfirmUntilMs) >= 0) {
    installConfirmUntilMs = now + 15000U;
    setLabelTextIfChanged(installLabel, "CONFIRM INSTALL");
    lv_obj_center(installLabel);
    return;
  }
  installConfirmUntilMs = 0;
  update_manager::requestInstall();
}

void laterEvent(lv_event_t*) {
  if (detailPanel) lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
}

void stylePanel(lv_obj_t* object) {
  lv_obj_set_style_bg_color(object, rgb(7, 16, 23), 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(object, rgb(35, 76, 87), 0);
  lv_obj_set_style_border_width(object, 1, 0);
  lv_obj_set_style_radius(object, 8, 0);
  lv_obj_set_style_pad_all(object, 10, 0);
  lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

bool build() {
  lv_obj_t* root = lv_scr_act();
  const char* navigationNames[5] = {
      "RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"};
  for (uint8_t index = 0; index < 5; ++index) {
    lv_obj_t* label = findLabelRecursive(root, navigationNames[index]);
    navigationButtons[index] = label ? lv_obj_get_parent(label) : nullptr;
  }
  systemButton = navigationButtons[4];
  lv_obj_t* pagePanel = findPagePanel(root);
  if (!systemButton || !pagePanel) {
    Serial.println("Update UI: System navigation or page panel not found");
    return false;
  }

  headerButton = lv_btn_create(root);
  lv_obj_set_size(headerButton, 27, 30);
  lv_obj_set_pos(headerButton, 649, 7);
  lv_obj_set_style_bg_color(headerButton, rgb(18, 88, 58), 0);
  lv_obj_set_style_border_color(headerButton, rgb(63, 255, 155), 0);
  lv_obj_set_style_border_width(headerButton, 1, 0);
  lv_obj_set_style_radius(headerButton, 6, 0);
  lv_obj_set_style_shadow_width(headerButton, 0, 0);
  lv_obj_set_style_pad_all(headerButton, 0, 0);
  lv_obj_add_event_cb(headerButton, headerEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* icon = lv_label_create(headerButton);
  lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(icon, rgb(180, 255, 205), 0);
  lv_obj_center(icon);
  lv_obj_add_flag(headerButton, LV_OBJ_FLAG_HIDDEN);

  systemSummaryButton = lv_btn_create(pagePanel);
  lv_obj_set_size(systemSummaryButton, 100, 34);
  lv_obj_set_pos(systemSummaryButton, 440, 8);
  lv_obj_set_style_bg_color(systemSummaryButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(systemSummaryButton, 5, 0);
  lv_obj_set_style_shadow_width(systemSummaryButton, 0, 0);
  lv_obj_set_style_pad_all(systemSummaryButton, 0, 0);
  lv_obj_add_event_cb(systemSummaryButton, summaryEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(systemSummaryButton, LV_OBJ_FLAG_HIDDEN);
  for (lv_obj_t* button : navigationButtons) {
    if (button) {
      lv_obj_add_event_cb(button, navigationEvent, LV_EVENT_CLICKED, nullptr);
    }
  }
  systemSummaryLabel = lv_label_create(systemSummaryButton);
  lv_label_set_text(systemSummaryLabel, "UPDATE CHECK");
  lv_obj_set_style_text_font(systemSummaryLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(systemSummaryLabel);

  detailPanel = lv_obj_create(pagePanel);
  lv_obj_set_size(detailPanel, 752, 337);
  lv_obj_set_pos(detailPanel, 3, 3);
  stylePanel(detailPanel);
  makeLabel(detailPanel, "SOFTWARE UPDATE", &lv_font_montserrat_28,
            rgb(63, 255, 155), 12, 7);
  makeLabel(detailPanel,
            "Verified GitHub release — download and install on this radar",
            &lv_font_montserrat_12, rgb(100, 170, 180), 12, 45);

  installedLabel = makeLabel(detailPanel, "INSTALLED", &lv_font_montserrat_16,
                             rgb(225, 235, 240), 12, 76);
  availableLabel = makeLabel(detailPanel, "AVAILABLE", &lv_font_montserrat_16,
                             rgb(120, 240, 155), 12, 105);
  lastCheckLabel = makeLabel(detailPanel, "LAST CHECK", &lv_font_montserrat_14,
                             rgb(110, 220, 255), 12, 136);
  messageLabel = makeLabel(detailPanel, "", &lv_font_montserrat_14,
                           rgb(255, 214, 80), 12, 165);
  lv_obj_set_width(messageLabel, 710);
  lv_label_set_long_mode(messageLabel, LV_LABEL_LONG_WRAP);
  notesLabel = makeLabel(detailPanel, "", &lv_font_montserrat_14,
                         rgb(180, 210, 215), 12, 205);
  lv_obj_set_width(notesLabel, 710);
  lv_label_set_long_mode(notesLabel, LV_LABEL_LONG_WRAP);

  checkNowButton = lv_btn_create(detailPanel);
  lv_obj_set_size(checkNowButton, 160, 40);
  lv_obj_set_pos(checkNowButton, 12, 280);
  lv_obj_set_style_bg_color(checkNowButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(checkNowButton, 6, 0);
  lv_obj_add_event_cb(checkNowButton, checkNowEvent,
                      LV_EVENT_CLICKED, nullptr);
  checkNowLabel = lv_label_create(checkNowButton);
  lv_label_set_text(checkNowLabel, "CHECK NOW");
  lv_obj_set_style_text_font(checkNowLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(checkNowLabel);

  installButton = lv_btn_create(detailPanel);
  lv_obj_set_size(installButton, 210, 40);
  lv_obj_set_pos(installButton, 190, 280);
  lv_obj_set_style_bg_color(installButton, rgb(24, 128, 84), 0);
  lv_obj_set_style_radius(installButton, 6, 0);
  lv_obj_add_event_cb(installButton, installEvent,
                      LV_EVENT_CLICKED, nullptr);
  installLabel = lv_label_create(installButton);
  lv_label_set_text(installLabel, "DOWNLOAD & INSTALL");
  lv_obj_set_style_text_font(installLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(installLabel);

  laterButton = lv_btn_create(detailPanel);
  lv_obj_set_size(laterButton, 150, 40);
  lv_obj_set_pos(laterButton, 572, 280);
  lv_obj_set_style_bg_color(laterButton, rgb(20, 68, 82), 0);
  lv_obj_set_style_radius(laterButton, 6, 0);
  lv_obj_add_event_cb(laterButton, laterEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* laterLabel = lv_label_create(laterButton);
  lv_label_set_text(laterLabel, "LATER");
  lv_obj_set_style_text_font(laterLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(laterLabel);

  lv_obj_add_flag(detailPanel, LV_OBJ_FLAG_HIDDEN);
  update(millis());
  return true;
}

void update(uint32_t now) {
  if (!headerButton || !systemSummaryButton || !detailPanel) return;
  update_manager::Status status;
  update_manager::copyStatus(status);

  bool confirmationChanged = false;
  if (installConfirmUntilMs &&
      (static_cast<int32_t>(now - installConfirmUntilMs) >= 0 ||
       !status.updateAvailable || status.installQueued || status.installing)) {
    installConfirmUntilMs = 0;
    confirmationChanged = true;
  }
  if (status.statusVersion == renderedStatusVersion &&
      !confirmationChanged) {
    return;
  }
  renderedStatusVersion = status.statusVersion;

  if (status.updateAvailable || status.installing || status.installQueued) {
    lv_obj_clear_flag(headerButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(headerButton, LV_OBJ_FLAG_HIDDEN);
  }

  const bool failed = status.lastResult == update_manager::CheckResult::FAILED;
  const bool aborted =
      status.lastResult == update_manager::CheckResult::ABORTED;
  const bool queued = status.manualQueued ||
      status.lastResult == update_manager::CheckResult::QUEUED;
  const bool installFailed =
      status.installResult == update_manager::InstallResult::FAILED ||
      status.installResult == update_manager::InstallResult::RESTART_FAILED;
  const bool installActive = status.installQueued || status.installing;

  lv_obj_set_style_bg_color(
      systemSummaryButton,
      installFailed ? rgb(126, 76, 34)
                    : (installActive || status.updateAvailable
                           ? rgb(24, 128, 84)
                           : (failed || aborted ? rgb(126, 76, 34)
                                               : rgb(20, 68, 82))),
      0);
  char summary[32];
  if (status.installing &&
      status.installResult == update_manager::InstallResult::DOWNLOADING) {
    snprintf(summary, sizeof(summary), "INSTALL %u%%",
             static_cast<unsigned>(status.installProgressPercent));
  } else {
    snprintf(summary, sizeof(summary), "%s",
             status.installing ? "INSTALLING..."
             : (status.installQueued ? "INSTALL QUEUED"
             : (status.checking ? "CHECKING..."
             : (queued ? "CHECK QUEUED"
             : (status.updateAvailable ? "UPDATE READY"
             : (failed ? "CHECK FAILED" : "UPDATE CHECK"))))));
  }
  setLabelTextIfChanged(systemSummaryLabel, summary);
  lv_obj_center(systemSummaryLabel);

  if (checkNowButton && checkNowLabel) {
    const bool checkButtonDisabled =
        status.checking || queued || status.installing || status.installQueued;
    if (checkButtonDisabled) lv_obj_add_state(checkNowButton, LV_STATE_DISABLED);
    else lv_obj_clear_state(checkNowButton, LV_STATE_DISABLED);
    lv_obj_set_style_opa(checkNowButton,
                         checkButtonDisabled ? LV_OPA_60 : LV_OPA_COVER, 0);
    setLabelTextIfChanged(checkNowLabel,
                          status.checking ? "CHECKING..."
                          : (queued ? "QUEUED..." : "CHECK NOW"));
    lv_obj_center(checkNowLabel);
  }

  if (installButton && installLabel) {
    const bool restarting =
        status.installResult == update_manager::InstallResult::RESTARTING;
    const bool installEnabled =
        status.updateAvailable || status.installQueued || status.installing;
    if (!installEnabled || restarting) {
      lv_obj_add_state(installButton, LV_STATE_DISABLED);
    } else {
      lv_obj_clear_state(installButton, LV_STATE_DISABLED);
    }
    lv_obj_set_style_opa(installButton,
                         (!installEnabled || restarting)
                             ? LV_OPA_60 : LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(
        installButton,
        status.installQueued || status.installing
            ? rgb(145, 72, 42) : rgb(24, 128, 84), 0);
    const char* installText = "DOWNLOAD & INSTALL";
    if (restarting) installText = "RESTARTING...";
    else if (status.installQueued || status.installing)
      installText = "CANCEL INSTALL";
    else if (installConfirmUntilMs)
      installText = "CONFIRM INSTALL";
    setLabelTextIfChanged(installLabel, installText);
    lv_obj_center(installLabel);
  }

  char text[256];
  snprintf(text, sizeof(text), "INSTALLED     %s\n%s",
           FIRMWARE_VERSION_LABEL, BUILD_ID);
  setLabelTextIfChanged(installedLabel, text);

  if (status.updateAvailable) {
    snprintf(text, sizeof(text), "AVAILABLE     %s\n%s",
             status.remoteVersionLabel, status.remoteBuildId);
  } else {
    snprintf(text, sizeof(text), "AVAILABLE     None");
  }
  setLabelTextIfChanged(availableLabel, text);

  if (status.installing || status.installQueued ||
      status.installResult == update_manager::InstallResult::FAILED ||
      status.installResult == update_manager::InstallResult::CANCELLED ||
      status.installResult == update_manager::InstallResult::RESTART_FAILED) {
    char received[32];
    char total[32];
    formatBytes(status.installReceivedBytes, received, sizeof(received));
    formatBytes(status.installPackageBytes, total, sizeof(total));
    snprintf(text, sizeof(text), "INSTALL     %s     %u%%     %s / %s",
             update_manager::installResultName(status.installResult),
             static_cast<unsigned>(status.installProgressPercent),
             received, total);
  } else {
    char checked[48];
    formatEpoch(status.lastSuccessEpoch, checked, sizeof(checked));
    snprintf(text, sizeof(text), "LAST SUCCESSFUL CHECK     %s", checked);
  }
  setLabelTextIfChanged(lastCheckLabel, text);

  if (status.installResult != update_manager::InstallResult::IDLE) {
    snprintf(text, sizeof(text), "%s: %s",
             update_manager::installResultName(status.installResult),
             status.message[0] ? status.message : "No installation result");
  } else {
    snprintf(text, sizeof(text), "%s: %s",
             update_manager::checkResultName(status.lastResult),
             status.message[0] ? status.message : "No update-check result");
  }
  setLabelTextIfChanged(messageLabel, text);
  lv_obj_set_style_text_color(
      messageLabel,
      installFailed || failed || aborted
          ? rgb(255, 175, 90)
          : ((status.updateAvailable || status.installing)
                 ? rgb(120, 240, 155) : rgb(110, 220, 255)), 0);

  setLabelTextIfChanged(
      notesLabel,
      status.updateAvailable && status.notes[0]
          ? status.notes
          : "CHECK NOW verifies the latest stable release. DOWNLOAD & INSTALL "
            "requires a second confirmation, checks the manifest again, streams "
            "the exact .radarota package into the inactive partition, validates "
            "both SHA-256 digests and the embedded build ID, then restarts. The "
            "local browser FIRMWARE / OTA path remains available for recovery.");
}

}  // namespace update_ui
