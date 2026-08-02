from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


class OtaRestartShutdownTests(unittest.TestCase):
    def test_product_marker(self):
        self.assertIn("7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS", BUILD)

    def test_reset_reason_is_reported_once_at_boot(self):
        self.assertEqual(MAIN.count("esp_reset_reason()"), 1)
        self.assertIn("Reset reason: %d", MAIN)

    def test_product57_browser_upload_path_is_unchanged(self):
        page = OTA[OTA.index("const char UPDATE_PAGE[]") : OTA.index(")HTML\";")]
        routes = OTA[OTA.index("void configureRoutes()") : OTA.index("void stopServer()")]
        self.assertIn("x.open('POST','/upload')", page)
        self.assertIn("new FormData()", page)
        self.assertIn("f.append('firmware',file,file.name)", page)
        self.assertIn('server.on("/upload", HTTP_POST', routes)
        self.assertNotIn("/upload-raw", OTA)
        self.assertNotIn("/upload-chunk", OTA)
        self.assertNotIn("BROWSER_CHUNK_BYTES", OTA)

    def test_success_restart_is_two_phase_and_creates_one_shot_task(self):
        first = re.search(
            r"if \(restartAtMs.*?restartAtMs = 0;.*?stopServer\(\);"
            r".*?restartExecuteAtMs = now \+ RESTART_SETTLE_MS;",
            OTA,
            re.S,
        )
        second = re.search(
            r"if \(restartExecuteAtMs.*?restartExecuteAtMs = 0;"
            r".*?createRestartTaskOnce\(\);",
            OTA,
            re.S,
        )
        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        self.assertIn("constexpr uint32_t RESTART_SETTLE_MS = 500U;", OTA)

    def test_restart_task_is_internal_ram_and_pinned_to_core_zero(self):
        self.assertIn("constexpr uint32_t RESTART_TASK_STACK_BYTES = 4096U;", OTA)
        self.assertIn("constexpr BaseType_t RESTART_TASK_CORE = 0;", OTA)
        creation = re.search(
            r"xTaskCreatePinnedToCoreWithCaps\("
            r".*?RESTART_TASK_STACK_BYTES.*?RESTART_TASK_CORE,"
            r".*?MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT\);",
            OTA,
            re.S,
        )
        self.assertIsNotNone(creation)

    def test_direct_core_one_restart_is_removed(self):
        service = OTA[OTA.index("void service()") : OTA.index("void copyStatus")]
        task = OTA[OTA.index("void restartTask(") : OTA.index("void createRestartTaskOnce")]
        self.assertNotIn("esp_restart();", service)
        self.assertIn("esp_restart();", task)
        self.assertEqual(OTA.count("esp_restart();"), 1)
        self.assertIsNotNone(re.search(
            r"name=%s core=%d stack-hwm-bytes=%u.*?Serial\.flush\(\);"
            r"\s*esp_restart\(\);",
            task,
            re.S,
        ))

    def test_duplicate_creation_is_prevented_and_failure_is_bounded(self):
        creator = OTA[
            OTA.index("void createRestartTaskOnce()") : OTA.index("}  // namespace")
        ]
        self.assertIn("if (restartTaskCreationAttempted) return;", creator)
        attempt = creator.index("restartTaskCreationAttempted = true;")
        creation = creator.index("xTaskCreatePinnedToCoreWithCaps(")
        self.assertLess(attempt, creation)
        failure = creator[creator.index("if (result != pdPASS)") :]
        self.assertIn("Firmware verified; automatic restart failed", failure)
        self.assertIn("automatic retry disabled", failure)
        self.assertNotIn("restartTaskCreationAttempted = false", failure)

    def test_core_one_park_state_is_forced_into_internal_dram(self):
        self.assertIn(
            "DRAM_ATTR uint32_t restartLoopState = RESTART_LOOP_WAITING;", OTA
        )

    def test_core_one_enters_noninlined_iram_and_masks_interrupts_before_ack(self):
        park = OTA[
            OTA.index("__attribute__((noinline)) bool IRAM_ATTR parkCoreOneForRestart()") :
            OTA.index("void restartTask(")
        ]
        interrupts_snapshot = park.index("xthal_get_intenable()")
        interrupts_off = park.index("xt_ints_off(0xFFFFFFFFU)")
        acknowledge = park.index("__atomic_compare_exchange_n(")
        interrupts_restore = park.index("xt_ints_on(enabledInterrupts)")
        loop = park.index("for (;;)")
        nop = park.index('__asm__ __volatile__("nop");')
        self.assertLess(interrupts_snapshot, interrupts_off)
        self.assertLess(interrupts_off, acknowledge)
        self.assertLess(acknowledge, interrupts_restore)
        self.assertLess(interrupts_restore, loop)
        self.assertLess(loop, nop)
        self.assertIn("RESTART_LOOP_QUIESCED", park)
        self.assertIn("__ATOMIC_RELEASE", park)
        self.assertIn("__ATOMIC_ACQUIRE", park)
        self.assertNotIn("Serial.", park)
        self.assertNotIn("vTaskDelay", park)
        self.assertNotIn("taskYIELD", park)
        self.assertNotIn("delay(", park)

    def test_creator_calls_iram_park_only_after_core_zero_task_exists(self):
        creator = OTA[
            OTA.index("void createRestartTaskOnce()") : OTA.index("}  // namespace")
        ]
        core_check = creator.index("xPortGetCoreID() != RESTART_LOOP_CORE")
        creation = creator.index("xTaskCreatePinnedToCoreWithCaps(")
        creation_failure = creator.index("if (result != pdPASS)")
        flush = creator.index("Serial.flush();")
        park = creator.index("parkCoreOneForRestart()")
        self.assertLess(core_check, creation)
        self.assertLess(creation, creation_failure)
        self.assertLess(creation_failure, flush)
        self.assertLess(flush, park)
        self.assertIn("restart task timed out", creator[park:])
        self.assertNotIn('__asm__ __volatile__("nop");', creator)

    def test_core_zero_timeout_cannot_overwrite_quiesced_acknowledgement(self):
        task = OTA[OTA.index("void restartTask(") : OTA.index("void createRestartTaskOnce")]
        timeout = task.index("xTaskGetTickCount() - waitStarted >= waitTicks")
        abort_compare = task.index("__atomic_compare_exchange_n(", timeout)
        high_water = task.index("uxTaskGetStackHighWaterMark(nullptr)")
        restart = task.index("esp_restart();")
        self.assertLess(timeout, abort_compare)
        self.assertLess(abort_compare, high_water)
        self.assertLess(high_water, restart)
        timeout_block = task[timeout:high_water]
        self.assertIn("RESTART_LOOP_WAITING", timeout_block)
        self.assertIn("RESTART_LOOP_ABORTED", timeout_block)
        self.assertIn("continue;", timeout_block)
        self.assertNotIn(
            "__atomic_store_n(\n          &restartLoopState, RESTART_LOOP_ABORTED",
            task,
        )
        self.assertIn("Core-1 loop did not quiesce", timeout_block)
        self.assertIn("remains verified", timeout_block)
        self.assertIn("automatic retry disabled", timeout_block)
        self.assertNotIn("releaseMaintenanceHold();", timeout_block)

    def test_network_owners_stop_before_restart_task_creation(self):
        stop_pos = OTA.index("stopServer();", OTA.index("if (restartAtMs"))
        create_pos = OTA.index("createRestartTaskOnce();", stop_pos)
        self.assertLess(stop_pos, create_pos)
        self.assertIn("server.stop();", OTA)
        self.assertIn("MDNS.end();", OTA)
        shutdown = OTA[stop_pos:create_pos]
        self.assertNotIn("releaseMaintenanceHold();", shutdown)
        self.assertNotIn("WiFi.disconnect", shutdown)

    def test_maintenance_holds_remain_active_until_restart(self):
        service = OTA[OTA.index("void service()") : OTA.index("void copyStatus")]
        shutdown = service[service.index("if (restartAtMs") :]
        task = OTA[OTA.index("void restartTask(") : OTA.index("void createRestartTaskOnce")]
        self.assertNotIn("releaseMaintenanceHold();", shutdown)
        self.assertNotIn("releaseMaintenanceHold();", task)
        self.assertIn("adsb::maintenanceHoldActive()", OTA)
        self.assertIn("mqtt_service::maintenanceHoldActive()", OTA)

    def test_final_resource_diagnostics_are_present(self):
        creator = OTA[
            OTA.index("void createRestartTaskOnce()") : OTA.index("}  // namespace")
        ]
        creation = creator.index("xTaskCreatePinnedToCoreWithCaps(")
        for diagnostic in (
            "heap_caps_check_integrity_all(true)",
            "heap_caps_get_free_size",
            "heap_caps_get_largest_free_block",
            "uxTaskGetStackHighWaterMark(nullptr)",
            "xTaskGetIdleTaskHandleForCPU(0)",
            "xTaskGetIdleTaskHandleForCPU(1)",
            'xTaskGetHandle("esp_timer")',
            "OTA restart resources:",
        ):
            self.assertIn(diagnostic, creator)
            self.assertLess(creator.index(diagnostic), creation)
        integrity_failure = creator[creator.index("if (!heapIntegrityOk)") : creation]
        self.assertIn("remains verified", integrity_failure)
        self.assertIn("automatic retry disabled", integrity_failure)


if __name__ == "__main__":
    unittest.main()
