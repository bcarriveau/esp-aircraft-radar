from pathlib import Path
import hashlib
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SPLASH = (ROOT / "src/boot_splash.cpp").read_text(encoding="utf-8")
SPLASH_HEADER = (ROOT / "include/boot_splash.h").read_text(encoding="utf-8")
NORTH = (ROOT / "src/radar_north_marker.cpp").read_text(encoding="utf-8")
NORTH_HEADER = (ROOT / "include/radar_north_marker.h").read_text(encoding="utf-8")
BUILD = (ROOT / "include/build_info.h").read_text(encoding="utf-8")
CHANGELOG = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")

PRODUCT79_MAIN_BLOB_SHA = "579360580044c48e0341a0569fc2e81cf8b485bb"
PRODUCT79_BUILD_BLOB_SHA = "193c92a3957540c0b40e99af0012efa8c1e3c3a5"
PRODUCT79_CHANGELOG_BLOB_SHA = "cc81716d6da3bbd3df1d5aca6fc2d92d2d80617f"


def git_blob_sha(text: str) -> str:
    data = text.encode("utf-8")
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


def product79_changelog_from_product80(text: str) -> str:
    marker = "## Product 79 — 40/80-mile aircraft symbols\n"
    rest = text[text.index(marker):]
    original_top = """# Changelog

All notable **confirmed** changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
Dates follow preserved firmware build markers. Commit links refer to the current
`main` history in `bcarriveau/esp-aircraft-radar` where a standalone commit is
available.

The authoritative numbered history begins with Product 15, the first hardened
version-controlled baseline. Earlier history is intentionally omitted where the
repository does not provide authoritative evidence.

## Current status

- **Current replacement source:** Product 79
- **Current build marker:** `7IN-20260806-PRODUCT79-RANGE-SYMBOLS`
- **Source baseline branch:** `main`
- **Latest inspected baseline commit:** `87e3382d23b68a55f3bc7ec7167641d3a7eceea4`
- **Product 78 source commit retained:** `87e3382d23b68a55f3bc7ec7167641d3a7eceea4`
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262 RGB LCD,
  GT911 touch, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback baseline:** Product 15
- **Recommended rollback tag:** `product-15-hardened`

Product 79 is a focused replacement-source candidate based on committed Product 78
`main` commit `87e3382d23b68a55f3bc7ec7167641d3a7eceea4`. It has focused host
regression coverage but is not claimed as committed, PlatformIO-built, uploaded, or
physically verified here.

Product 78 is committed on `main`. Product 73's remote GitHub installer was
physically proven by installing the versioned Product 74 test release. Later Product
source remains documented without adding unrecorded device-validation claims.

"""
    return original_top + rest


def product79_build_from_product80(text: str) -> str:
    text = text.replace(
        "// Authoritative implementation baseline: committed Product 79 on current main,\n",
        "// Authoritative implementation baseline: Product 78 from current main,\n")
    text = text.replace("FIRMWARE_VERSION_CODE = 80", "FIRMWARE_VERSION_CODE = 79")
    text = text.replace('FIRMWARE_VERSION_LABEL = "Product 80"',
                        'FIRMWARE_VERSION_LABEL = "Product 79"')
    text = text.replace(
        '    "Adds a lightweight avionics boot splash, saved-name branding, and north markers.";',
        '    "Adds stable scaled aircraft symbols at 40 and 80 miles without per-frame allocation or new bitmap assets.";')
    text = text.replace(
        '    "7IN-20260807-PRODUCT80-BOOT-SPLASH";',
        '    "7IN-20260806-PRODUCT79-RANGE-SYMBOLS";')
    return text


def product79_main_from_product80(text: str) -> str:
    text = text.replace('#include "boot_splash.h"\n', '')
    text = text.replace('#include "radar_north_marker.h"\n', '')
    text = text.replace(
        '  const bool radarNorthReady = uiReady && radar_north_marker::attach();\n', '')
    text = text.replace(
        '  const bool splashReady =\n'
        '      uiReady && boot_splash::show(settings::deviceTitle().c_str());\n', '')
    text = text.replace(
        '  if (!radarNorthReady) {\n'
        '    Serial.println(\n'
        '        "WARNING: Live radar north marker unavailable; radar continuing");\n'
        '  }\n', '')
    text = text.replace(
        '  if (!splashReady) {\n'
        '    Serial.println(\n'
        '        "WARNING: Boot splash unavailable; continuing with operational UI");\n'
        '  }\n', '')
    text = text.replace('    boot_splash::cancel();\n', '')
    text = text.replace(
        '  lvgl_port_lock(-1);\n'
        '  boot_splash::markStartupComplete();\n'
        '  lvgl_port_unlock();\n', '')
    return text


class Product80BootSplashTests(unittest.TestCase):
    def test_product_identity_stays_product80(self) -> None:
        self.assertIn("FIRMWARE_VERSION_CODE = 80", BUILD)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 80"', BUILD)
        self.assertIn('7IN-20260807-PRODUCT80-BOOT-SPLASH', BUILD)

    def test_changelog_preserves_complete_product79_history(self) -> None:
        restored = product79_changelog_from_product80(CHANGELOG)
        self.assertEqual(git_blob_sha(restored), PRODUCT79_CHANGELOG_BLOB_SHA)
        self.assertIn("## Product 80 - 2026-08-07", CHANGELOG)

    def test_build_info_is_product79_plus_product80_identity_only(self) -> None:
        restored = product79_build_from_product80(BUILD)
        self.assertEqual(git_blob_sha(restored), PRODUCT79_BUILD_BLOB_SHA)

    def test_main_is_product79_plus_product80_wiring_only(self) -> None:
        restored = product79_main_from_product80(MAIN)
        self.assertEqual(git_blob_sha(restored), PRODUCT79_MAIN_BLOB_SHA)

    def test_splash_is_built_over_completed_operational_ui(self) -> None:
        ui_build = MAIN.index("ui::buildUi()")
        update_ui_build = MAIN.index("update_ui::build()")
        north_attach = MAIN.index("radar_north_marker::attach()")
        splash_show = MAIN.index("boot_splash::show(settings::deviceTitle().c_str())")
        self.assertLess(ui_build, update_ui_build)
        self.assertLess(update_ui_build, north_attach)
        self.assertLess(north_attach, splash_show)

    def test_boot_title_uses_saved_system_display_name(self) -> None:
        self.assertIn("boot_splash::show(settings::deviceTitle().c_str())", MAIN)
        self.assertIn("bool show(const char* deviceTitle);", SPLASH_HEADER)
        self.assertIn("buildTitleAndStatus(deviceTitle)", SPLASH)
        self.assertIn("deviceTitle && deviceTitle[0]", SPLASH)
        self.assertIn('"BILLS AIRCRAFT RADAR"', SPLASH)  # safe fallback only
        for font in (32, 28, 24, 20, 18, 16):
            self.assertIn(f"lv_font_montserrat_{font}", SPLASH)
        self.assertIn("if (size.x <= 760) return font;", SPLASH)
        self.assertIn("LV_LABEL_LONG_CLIP", SPLASH)

    def test_splash_compass_has_north_only(self) -> None:
        calls = re.findall(r'makeCardinalLabel\(radarGroup, "([NSEW])"', SPLASH)
        self.assertEqual(calls, ["N"])

    def test_live_radar_north_marker_is_centered_above_circle(self) -> None:
        self.assertIn('lv_label_set_text(northLabel, "N")', NORTH)
        self.assertIn("lv_obj_get_width(child) == radar::WIDTH", NORTH)
        self.assertIn("lv_obj_get_height(child) == radar::HEIGHT", NORTH)
        self.assertIn("lv_obj_check_type(child, &lv_canvas_class)", NORTH)
        self.assertIn("lv_obj_set_width(northLabel, 20)", NORTH)
        self.assertIn("lv_obj_set_pos(northLabel, (radar::WIDTH - 20) / 2, 0)", NORTH)
        self.assertIn("&lv_font_montserrat_12", NORTH)
        self.assertIn("LV_TEXT_ALIGN_CENTER", NORTH)
        self.assertIn("LV_OBJ_FLAG_CLICKABLE", NORTH)

    def test_live_north_marker_is_small_ui_only_module(self) -> None:
        includes = re.findall(r'^#include\s+[<\"]([^>\"]+)[>\"]', NORTH,
                              flags=re.MULTILINE)
        self.assertEqual(includes,
                         ["radar_north_marker.h", "lvgl.h", "stdint.h",
                          "radar_renderer.h"])
        for token in ("malloc(", "calloc(", "heap_caps_", "WiFi", "HTTP",
                      "TLS", "config.h", "String "):
            self.assertNotIn(token, NORTH)
        self.assertIn("bool attach();", NORTH_HEADER)

    def test_fatal_network_status_replaces_splash(self) -> None:
        fatal_block = MAIN[MAIN.index("if (!adsb::begin())") :]
        self.assertLess(fatal_block.index("boot_splash::cancel()"),
                        fatal_block.index("ui::showFatalStatus"))

    def test_startup_ready_is_signaled_only_after_required_startup(self) -> None:
        signal = MAIN.index("boot_splash::markStartupComplete()")
        self.assertLess(MAIN.index("adsb::begin()"), signal)
        self.assertLess(MAIN.index("ota_update::begin()"), signal)
        self.assertLess(MAIN.index("mqtt_service::begin()"), signal)
        self.assertLess(MAIN.index("startupComplete = true"), signal)

    def test_requested_identity_and_status_text_is_present(self) -> None:
        for text in (
            "ESP32-S3 ADS-B DISPLAY",
            "Designed by Bill Carriveau",
            "DISPLAY READY",
            "TOUCH READY",
            "SYSTEM STARTING",
            "SYSTEM READY",
        ):
            self.assertIn(text, SPLASH)

    def test_minimum_duration_and_doubled_sweep_are_retained(self) -> None:
        self.assertRegex(SPLASH, r"MIN_VISIBLE_MS\s*=\s*3800")
        self.assertIn("if (!startupReady || elapsed < MIN_VISIBLE_MS) return;", SPLASH)
        self.assertRegex(SPLASH, r"DISMISS_FADE_MS\s*=\s*260")
        self.assertIn("lv_anim_set_delay(&sweep, 800)", SPLASH)
        self.assertIn("lv_anim_set_time(&sweep, 1600)", SPLASH)

    def test_transient_primitives_only_no_large_splash_buffer(self) -> None:
        for token in (
            "lv_canvas_create",
            "heap_caps_malloc",
            "heap_caps_calloc",
            "malloc(",
            "calloc(",
            "new ",
            "String ",
            "HTTPClient",
            "WiFiClientSecure",
            "setInsecure",
        ):
            self.assertNotIn(token, SPLASH)

    def test_splash_module_has_no_network_or_display_driver_dependencies(self) -> None:
        includes = re.findall(r'^#include\s+[<\"]([^>\"]+)[>\"]', SPLASH,
                              flags=re.MULTILINE)
        self.assertEqual(includes,
                         ["boot_splash.h", "lvgl.h", "math.h", "stdint.h"])
        self.assertNotIn("config.h", SPLASH)
        self.assertNotIn("config.h", MAIN)


if __name__ == "__main__":
    unittest.main()
