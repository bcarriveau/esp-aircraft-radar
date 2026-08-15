from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
ADSB = (ROOT / "src" / "adsb_fetch.cpp").read_text(encoding="utf-8")
UI = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
POLICY = (ROOT / "include" / "adsb_transport_policy.h").read_text(encoding="utf-8")


class Product95AdsbUiFixTests(unittest.TestCase):
    def test_native_retryable_read_keeps_connection_within_existing_deadlines(self):
        self.assertIn("bytesRead == -ESP_ERR_HTTP_EAGAIN", ADSB)
        self.assertIn("retryBudget >= policy::MIN_BLOCKING_CALL_BUDGET_MS", ADSB)
        self.assertIn("stalledForMs < policy::IDLE_TIMEOUT_MS", ADSB)
        self.assertIn("delay(2);\n          continue;", ADSB)

    def test_body_read_call_is_bounded_by_remaining_idle_window(self):
        self.assertIn(
            "const uint32_t idleRemainingMs = policy::IDLE_TIMEOUT_MS - stalledForMs;",
            ADSB,
        )
        self.assertIn(
            "esp_http_client_set_timeout_ms(client, bodyReadTimeoutMs);",
            ADSB,
        )
        self.assertIn("policy::BODY_READ_TIMEOUT_MS", ADSB)

    def test_transport_budgets_and_cadence_are_unchanged(self):
        self.assertIn("FETCH_TOTAL_BUDGET_MS = 12000", POLICY)
        self.assertIn("JSON_RESERVE_MS = 1500", POLICY)
        self.assertIn("BODY_READ_TIMEOUT_MS = 3000", POLICY)
        self.assertIn("IDLE_TIMEOUT_MS = 3000", POLICY)
        self.assertIn('static_assert(FETCH_TOTAL_BUDGET_MS < 15000', POLICY)

    def test_successful_device_settings_message_expires(self):
        self.assertIn("SETTINGS_SAVED_STATUS_MS = 4000", UI)
        self.assertIn("void setTemporarySettingsSuccess(const char* text)", UI)
        self.assertIn("settingsStatusClearDeadline = millis() + SETTINGS_SAVED_STATUS_MS;", UI)
        self.assertIn("setTemporarySettingsSuccess(savedStatus);", UI)
        self.assertIn('setLabelTextIfChanged(settingsStatusLabel, "");', UI)

    def test_other_settings_statuses_cancel_pending_success_clear(self):
        self.assertIn(
            "void setSettingsStatus(const char* text, lv_color_t color) {\n"
            "  settingsStatusClearDeadline = 0;",
            UI,
        )


if __name__ == "__main__":
    unittest.main()
