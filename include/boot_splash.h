#pragma once

namespace boot_splash {

// Creates a full-screen LVGL startup overlay on the active screen. The overlay
// remains visible for at least the programmed intro duration and will not dismiss
// until markStartupComplete() confirms required startup finished. deviceTitle is
// copied into the LVGL title label when the splash is created.
bool show(const char* deviceTitle);

// Marks required startup complete. The splash changes its final status to
// SYSTEM READY and dismisses once the minimum intro duration has elapsed.
void markStartupComplete();

// Removes the startup overlay immediately. Use before replacing the active screen
// with a fatal startup status.
void cancel();

}  // namespace boot_splash
