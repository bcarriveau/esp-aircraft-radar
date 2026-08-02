from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
ADSB = (ROOT / "src" / "adsb_network.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


class OtaExclusiveWindowTests(unittest.TestCase):
    def test_product_marker(self):
        self.assertIn(
            "7IN-20260802-PRODUCT64-MEMORY-PHASE2", BUILD
        )

    def test_enable_claims_network_before_starting_http_server(self):
        enable = OTA[OTA.index("bool enable()") : OTA.index("void disable()") ]
        adsb_hold = enable.index("if (!adsb::requestMaintenanceHold())")
        mqtt_hold = enable.index("mqtt_service::requestMaintenanceHold();")
        server_start = enable.index("server.begin();")
        self.assertLess(adsb_hold, server_start)
        self.assertLess(mqtt_hold, server_start)
        self.assertIn("OTA exclusive window requested", enable)
        self.assertIn("Wi-Fi recovery in progress; try enabling OTA again", enable)

    def test_hard_recovery_cannot_steal_an_ota_reserved_network(self):
        reserve = ADSB[ADSB.index("bool reserveHardWifiRecovery()") :
                       ADSB.index("bool prepareHardWifiRecovery()") ]
        self.assertIn("!maintenanceRequested && !wifiOperationPending", reserve)
        request = ADSB[ADSB.index("bool requestMaintenanceHold()") :
                       ADSB.index("bool maintenanceHoldActive()") ]
        self.assertIn("else if (!wifiOperationPending)", request)
        self.assertIn("return accepted;", request)

    def test_upload_failure_keeps_the_bounded_exclusive_hold(self):
        failure = OTA[OTA.index("void failUpload(") : OTA.index("void prepareBuildIdentityMatcher") ]
        self.assertNotIn("releaseMaintenanceHold();", failure)
        self.assertIn("exclusive hold remains active", failure)

    def test_prepare_expiry_does_not_resume_background_networking(self):
        service = OTA[OTA.index("void service()") : OTA.index("void copyStatus") ]
        expiry = re.search(
            r"if \(\(currentState == State::PREPARING.*?"
            r"Upload preparation expired.*?\n  }",
            service,
            re.S,
        )
        self.assertIsNotNone(expiry)
        self.assertNotIn("releaseMaintenanceHold();", expiry.group(0))
        self.assertIn("background services remain paused", expiry.group(0))

    def test_browser_cancel_stops_http_before_releasing_holds(self):
        cancel = OTA[OTA.index("void handleCancel()") : OTA.index("void handleUploadComplete") ]
        self.assertNotIn("releaseMaintenanceHold();", cancel)
        self.assertIn("releaseMaintenanceAfterStopPending = true;", cancel)
        service = OTA[OTA.index("void service()") : OTA.index("const uint32_t now = millis();") ]
        stop = service.index("stopServer();")
        release = service.index("releaseMaintenanceHold();")
        self.assertLess(stop, release)

    def test_local_disable_stops_server_before_resuming_services(self):
        disable = OTA[OTA.index("void disable()") : OTA.index("bool busy()") ]
        self.assertLess(disable.index("stopServer();"),
                        disable.index("releaseMaintenanceHold();"))

    def test_inflight_fetch_cannot_enter_recovery_after_ota_request(self):
        self.assertIn("maintenanceRequestedAfterFetch = isMaintenanceRequested()", ADSB)
        self.assertIn("recoveryDeferredForOta", ADSB)
        recovery = ADSB[ADSB.index("const bool recoveryDue") :
                        ADSB.index("nextPollAt = recoveryConnected") ]
        self.assertIn("recoveryDue && !recoveryDeferredForOta", recovery)
        self.assertIn("!maintenanceRequestedAfterFetch", recovery)
        self.assertIn("OTA exclusive hold was requested", recovery)

    def test_maintenance_request_clears_pending_network_commands(self):
        request = ADSB[ADSB.index("bool requestMaintenanceHold()") :
                       ADSB.index("bool maintenanceHoldActive()") ]
        self.assertIn("maintenanceRequested = true;", request)
        self.assertIn("pendingCommands = 0;", request)

    def test_manual_refresh_and_reconnect_are_ignored_during_hold(self):
        refresh = ADSB[ADSB.index("void requestRefresh()") :
                       ADSB.index("bool requestMaintenanceHold()") ]
        self.assertGreaterEqual(refresh.count("isMaintenanceRequested()"), 2)
        self.assertIn("refresh ignored during OTA exclusive hold", refresh)
        self.assertIn("reconnect ignored during OTA exclusive hold", refresh)

    def test_product57_upload_transport_remains_unchanged(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        routes = OTA[OTA.index("void configureRoutes()") : OTA.index("void stopServer()") ]
        self.assertIn("x.open('POST','/upload')", page)
        self.assertIn("new FormData()", page)
        self.assertIn("f.append('firmware',file,file.name)", page)
        self.assertIn('server.on("/upload", HTTP_POST', routes)
        self.assertNotIn("/upload-raw", OTA)
        self.assertNotIn("/upload-chunk", OTA)


if __name__ == "__main__":
    unittest.main()
