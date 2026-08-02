from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


class OtaPrepareIdempotencyTests(unittest.TestCase):
    def test_product_marker(self):
        self.assertIn(
            "7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT", BUILD
        )

    def test_duplicate_preparing_request_is_successful_and_side_effect_free(self):
        prepare = OTA[OTA.index("void handlePrepare()") : OTA.index("void handleStatus()")]
        preparing = re.search(
            r"if \(currentState == State::PREPARING\) \{(.*?)\n  \}",
            prepare,
            re.S,
        )
        self.assertIsNotNone(preparing)
        block = preparing.group(1)
        self.assertIn("sendJson(202, statusMessage);", block)
        self.assertIn("return;", block)
        self.assertNotIn("resetUploadSession", block)
        self.assertNotIn("requestMaintenanceHold", block)
        self.assertNotIn("prepareDeadlineMs", block)

    def test_duplicate_ready_request_is_successful_and_side_effect_free(self):
        prepare = OTA[OTA.index("void handlePrepare()") : OTA.index("void handleStatus()")]
        ready = re.search(
            r"if \(currentState == State::READY\) \{(.*?)\n  \}",
            prepare,
            re.S,
        )
        self.assertIsNotNone(ready)
        block = ready.group(1)
        self.assertIn("sendJson(200, statusMessage);", block)
        self.assertIn("return;", block)
        self.assertNotIn("resetUploadSession", block)
        self.assertNotIn("requestMaintenanceHold", block)
        self.assertNotIn("prepareDeadlineMs", block)

    def test_idempotent_states_are_checked_before_rejection_or_reset(self):
        prepare = OTA[OTA.index("void handlePrepare()") : OTA.index("void handleStatus()")]
        self.assertLess(
            prepare.index("currentState == State::PREPARING"),
            prepare.index("OTA service is not available for preparation"),
        )
        self.assertLess(
            prepare.index("currentState == State::READY"),
            prepare.index("OTA service is not available for preparation"),
        )
        self.assertLess(
            prepare.index("OTA service is not available for preparation"),
            prepare.index("resetUploadSession();"),
        )

    def test_new_preparation_still_has_one_bounded_deadline(self):
        prepare = OTA[OTA.index("void handlePrepare()") : OTA.index("void handleStatus()")]
        self.assertEqual(prepare.count("prepareDeadlineMs = millis() + PREPARE_TIMEOUT_MS;"), 1)
        self.assertEqual(prepare.count("currentState = State::PREPARING;"), 1)
        self.assertEqual(prepare.count("mqtt_service::requestMaintenanceHold();"), 1)

    def test_browser_recovers_when_prepare_response_is_lost(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        self.assertIn("async function prepareUpload()", page)
        self.assertIn("for(let attempt=0;attempt<2;attempt++)", page)
        self.assertIn("await call('/status')", page)
        self.assertIn("j.state==='PREPARING'||j.state==='READY'", page)
        self.assertIn("await prepareUpload();await waitReady();", page)

    def test_product57_multipart_upload_transport_remains_unchanged(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        routes = OTA[OTA.index("void configureRoutes()") : OTA.index("void stopServer()")]
        self.assertIn("x.open('POST','/upload')", page)
        self.assertIn("new FormData()", page)
        self.assertIn("f.append('firmware',file,file.name)", page)
        self.assertIn('server.on("/upload", HTTP_POST', routes)
        self.assertNotIn("/upload-raw", OTA)
        self.assertNotIn("/upload-chunk", OTA)


if __name__ == "__main__":
    unittest.main()
