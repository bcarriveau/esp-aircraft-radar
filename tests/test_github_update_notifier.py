from __future__ import annotations

import hashlib
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRODUCT69_MAIN_SHA = "547bc67f23e07c67fb022505001863510cdd604c"
PRODUCT69_NETWORK_SHA = "53da05b49ef5c4b7858d399bb44fc842974f219c"


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def git_blob_sha(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


class GithubUpdateNotifierTests(unittest.TestCase):
    def test_build_identity_is_product_72_and_hardware_specific(self) -> None:
        build = read("include/build_info.h")
        self.assertIn("FIRMWARE_VERSION_CODE = 72", build)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 72"', build)
        self.assertIn('"waveshare-esp32-s3-touch-lcd-7"', build)
        self.assertIn('FIRMWARE_RELEASE_CHANNEL = "stable"', build)
        self.assertIn("7IN-20260803-PRODUCT72-GITHUB-TX-BUFFER-FIX", build)
        self.assertIn("a52ee1cd39f1d39182730418de7192b9779a4307", build)

    def test_check_has_no_task_or_forbidden_transport(self) -> None:
        source = read("src/update_manager.cpp")
        for forbidden in (
            "xTaskCreate",
            "HTTPClient",
            "setInsecure",
            "WiFiClientSecure",
            "String ",
            "esp_ota_begin",
            "esp_ota_write",
        ):
            self.assertNotIn(forbidden, source)
        self.assertIn("esp_http_client_open", source)
        self.assertIn("esp_crt_bundle_attach", source)
        self.assertIn("skip_cert_common_name_check = false", source)
        self.assertIn("MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT", source)
        self.assertIn("MAX_REDIRECTS = 3", source)
        self.assertIn("CHECK_TOTAL_TIMEOUT_MS = 6000UL", source)
        self.assertIn("HTTP_CONNECT_TIMEOUT_MS = 2500UL", source)
        self.assertIn("HTTP_BODY_IDLE_TIMEOUT_MS = 1500UL", source)

    def test_github_headers_and_signed_redirects_are_realistically_bounded(self) -> None:
        source = read("src/update_manager.cpp")
        policy = read("include/update_policy.h")
        self.assertIn("MAX_HTTP_HEADER_BYTES = 16U * 1024U", policy)
        self.assertIn("MAX_REDIRECT_URL_LENGTH = 4095U", policy)
        self.assertIn("MIN_HTTP_TX_BUFFER_BYTES = 1024U", policy)
        self.assertIn("HTTP_TX_HEADROOM_BYTES = 512U", policy)
        self.assertIn("MAX_HTTP_TX_BUFFER_BYTES", policy)
        self.assertIn("httpTransmitBufferBytes", policy)
        self.assertIn("accumulateHeaderBytes", policy)
        self.assertIn("redirectUrlLengthValid", policy)
        self.assertIn("HeaderFailure::TOTAL_BYTES", source)
        self.assertIn("HeaderFailure::LOCATION_TOO_LONG", source)
        self.assertIn("HeaderFailure::CONFLICTING_FRAMING", source)
        self.assertIn("response headers exceeded the 16384-byte limit", source)
        self.assertIn("redirect Location exceeded the 4095-byte limit", source)
        self.assertIn("GitHub header fetch failed: result=%lld errno=%d", source)
        self.assertNotIn(
            'copyText(failure, failureCapacity,\n'
            '                 "GitHub response headers were invalid");',
            source,
        )
        self.assertIn(
            "char currentUrl[update_policy::MAX_REDIRECT_URL_LENGTH + 1U]",
            source,
        )
        self.assertIn(
            "char location[update_policy::MAX_REDIRECT_URL_LENGTH + 1U]",
            source,
        )
        self.assertIn("release-assets.githubusercontent.com", policy)
        self.assertIn("MAX_REDIRECTS = 3", source)
        self.assertIn("httpTransmitBufferBytes(currentUrlLength)", source)
        self.assertIn(
            "config.buffer_size_tx = static_cast<int>(transmitBufferBytes)",
            source,
        )
        self.assertIn("GitHub HTTPS open/send failed", source)
        self.assertIn("(url=%u tx=%u)", source)
        self.assertNotIn("config.buffer_size_tx = 512", source)

    def test_manual_check_is_real_and_bypasses_only_timers(self) -> None:
        header = read("include/update_manager.h")
        source = read("src/update_manager.cpp")
        ui = read("src/update_ui.cpp")
        self.assertIn("bool requestManualCheck();", header)
        self.assertIn("bool requestManualCheck()", source)
        self.assertIn("status.manualQueued = true", source)
        self.assertIn("if (!manual && !automaticCheckDue(status))", source)
        self.assertIn("automaticWaitReason(status)", source)
        self.assertIn("update_manager::requestManualCheck();", ui)
        self.assertIn('lv_label_set_text(checkNowLabel, "CHECK NOW")', ui)
        self.assertIn("successful ADS-B cycle", source)

    def test_failed_product_69_cache_cannot_throttle_product_70(self) -> None:
        source = read("src/update_manager.cpp")
        self.assertIn("STORED_SCHEMA = 2", source)
        self.assertIn("incompatible pre-Product-70 schema", source)
        self.assertNotIn("setAttemptTimestamp(status)", source)
        prepare = source[source.index("bool prepareAfterSuccessfulAdsb") :]
        self.assertNotIn("commitAttemptTimestamp(status)", prepare.split("void performPreparedCheck", 1)[0])
        self.assertGreaterEqual(source.count("commitAttemptTimestamp(status)"), 3)

    def test_abort_and_failure_have_different_throttle_semantics(self) -> None:
        source = read("src/update_manager.cpp")
        aborted = re.search(
            r"void finishAborted\([^)]*\) \{(?P<body>.*?)\n\}", source, re.S
        )
        failed = re.search(
            r"void finishFailure\([^)]*\) \{(?P<body>.*?)\n\}", source, re.S
        )
        self.assertIsNotNone(aborted)
        self.assertIsNotNone(failed)
        assert aborted and failed
        self.assertNotIn("commitAttemptTimestamp", aborted.group("body"))
        self.assertNotIn("persist", aborted.group("body"))
        self.assertIn("commitAttemptTimestamp", failed.group("body"))
        self.assertIn("persist", failed.group("body"))
        self.assertIn("AbortCause::OTA", source)
        self.assertIn("AbortCause::COMMAND", source)
        self.assertIn("status.manualQueued = manual", aborted.group("body"))
        self.assertIn("CheckResult::QUEUED", aborted.group("body"))
        self.assertIn("CHECK NOW requeued", aborted.group("body"))

    def test_check_runs_inside_product_69_serialized_fetch_window(self) -> None:
        network = read("src/adsb_network.cpp")
        publish = network.index("app_state::publishTargets(")
        claim = network.index("update_manager::prepareAfterSuccessfulAdsb(")
        complete = network.index("app_state::recordFetchSuccess(")
        execute = network.index("update_manager::performPreparedCheck()")
        self.assertLess(publish, claim)
        self.assertLess(claim, complete)
        self.assertLess(complete, execute)
        self.assertIn("ADSB_TASK_STACK_BYTES = 12U * 1024U", network)
        self.assertEqual(network.count("xTaskCreatePinnedToCore("), 1)

    def test_product_69_fetch_cancellation_is_preserved(self) -> None:
        network = read("src/adsb_network.cpp")
        self.assertIn("if (result.cancelled)", network)
        self.assertIn("ADSB fetch cancelled for OTA maintenance", network)
        self.assertIn("bool fetchAbortRequested()", network)
        self.assertIn("return isMaintenanceRequested();", network)
        self.assertIn("update_manager::requestAbort();", network)
        maintenance = network[network.index("bool requestMaintenanceHold()") :]
        self.assertIn("update_manager::requestAbort();", maintenance.split("bool accepted", 1)[0])

    def test_adsb_diff_restores_exact_product_69_blob(self) -> None:
        source = read("src/adsb_network.cpp")
        restored = source.replace('#include "update_manager.h"\n', "")
        restored = restored.replace(
            "void queueCommand(uint32_t command) {\n"
            "  update_manager::requestAbort();\n",
            "void queueCommand(uint32_t command) {\n",
        )
        hook = '''        // Claim an optional manual or daily release check while the established
        // ADS-B/MQTT exclusion is still active. The disposable GitHub request
        // runs only after publication and ADS-B diagnostics complete.
        const bool updateCheckPrepared =
            update_manager::prepareAfterSuccessfulAdsb(
                pollStartedAt + FETCH_INTERVAL_MS);
'''
        self.assertIn(hook, restored)
        restored = restored.replace(hook, "")
        restored = restored.replace(
            "        if (updateCheckPrepared) update_manager::performPreparedCheck();\n",
            "",
        )
        maintenance = '''  // A browser OTA request takes priority over the disposable GitHub check.
  update_manager::requestAbort();
'''
        self.assertIn(maintenance, restored)
        restored = restored.replace(maintenance, "")
        self.assertEqual(git_blob_sha(restored.encode()), PRODUCT69_NETWORK_SHA)

    def test_main_diff_restores_exact_product_69_blob(self) -> None:
        source = read("src/main.cpp")
        restored = source.replace('#include "update_manager.h"\n', "")
        restored = restored.replace('#include "update_ui.h"\n', "")
        restored = restored.replace(
            '  if (!update_manager::begin()) {\n'
            '    Serial.println(\n'
            '        "WARNING: Update-check persistence unavailable; using boot-local schedule");\n'
            '  }\n',
            "",
        )
        restored = restored.replace(
            "  const bool uiReady = ui::buildUi();\n"
            "  const bool updateUiReady = uiReady && update_ui::build();\n",
            "  const bool uiReady = ui::buildUi();\n",
        )
        restored = restored.replace(
            '  if (!updateUiReady) {\n'
            '    Serial.println(\n'
            '        "WARNING: GitHub update indicator UI unavailable; radar continuing");\n'
            '  }\n',
            "",
        )
        restored = restored.replace(
            "  if (!update_manager::networkCheckInProgress()) {\n"
            "    mqtt_service::service();\n"
            "  }\n",
            "  mqtt_service::service();\n",
        )
        restored = restored.replace("  update_ui::update(now);\n", "")
        self.assertEqual(git_blob_sha(restored.encode()), PRODUCT69_MAIN_SHA)

    def test_mqtt_is_gated_without_faking_adsb_update_state(self) -> None:
        main = read("src/main.cpp")
        self.assertIn("if (!update_manager::networkCheckInProgress())", main)
        gated = main[main.index("if (!update_manager::networkCheckInProgress())") :]
        self.assertIn("mqtt_service::service();", gated.split("}", 1)[0])
        self.assertNotIn("app_state::setFetchInProgress", main)

    def test_ui_is_static_and_later_is_network_free(self) -> None:
        ui = read("src/update_ui.cpp")
        self.assertIn("LV_SYMBOL_DOWNLOAD", ui)
        self.assertIn("status.statusVersion == renderedStatusVersion", ui)
        self.assertNotIn("LV_ANIM_ON", ui)
        later = re.search(r"void laterEvent\([^)]*\) \{(?P<body>.*?)\n\}", ui, re.S)
        self.assertIsNotNone(later)
        assert later
        self.assertIn("LV_OBJ_FLAG_HIDDEN", later.group("body"))
        for forbidden in ("request", "http", "WiFi", "persist"):
            self.assertNotIn(forbidden, later.group("body"))

    def test_credentials_capacity_and_forbidden_apis_are_untouched(self) -> None:
        changed_production = (
            "include/build_info.h",
            "include/update_manager.h",
            "include/update_policy.h",
            "include/update_ui.h",
            "src/main.cpp",
            "src/adsb_network.cpp",
            "src/update_manager.cpp",
            "src/update_ui.cpp",
            "scripts/build_radar_ota.py",
            "docs/GITHUB_RELEASES.md",
        )
        for path in changed_production:
            text = read(path)
            self.assertNotIn("MAX_TARGETS =", text)
            self.assertNotIn("setInsecure()", text)
            self.assertNotIn("HTTPClient::GET()", text)
            self.assertNotIn("WIFI_PASS", text)
        self.assertFalse((ROOT / "include" / "config.h").exists())


if __name__ == "__main__":
    unittest.main()
