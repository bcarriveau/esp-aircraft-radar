from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class RemoteGithubOtaTests(unittest.TestCase):
    def test_package_header_contract_matches_existing_radarota(self) -> None:
        source = read("src/github_ota_installer.cpp")
        self.assertIn('PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA"', source)
        self.assertIn('PACKAGE_HARDWARE_ID[32] = "WAVESHARE-ESP32-S3-LCD-7"', source)
        self.assertIn("PACKAGE_FORMAT_VERSION = 1", source)
        self.assertIn("PACKAGE_HEADER_SIZE = 512", source)
        self.assertIn("static_assert(sizeof(PackageHeader) == PACKAGE_HEADER_SIZE", source)
        fields = (
            "char magic[16]",
            "uint16_t formatVersion",
            "uint16_t headerSize",
            "char hardwareId[32]",
            "char buildId[96]",
            "uint32_t firmwareSize",
            "uint8_t firmwareSha256[32]",
            "uint8_t reserved[328]",
        )
        for field in fields:
            self.assertIn(field, source)

    def test_partition_is_selected_only_after_all_verification(self) -> None:
        source = read("src/github_ota_installer.cpp")
        finish = source[source.index("bool finishPackage") : source.index("void setRestartState")]
        package_hash = finish.index("actualPackageSha")
        firmware_hash = finish.index("actualFirmwareSha")
        end = finish.index("esp_ota_end")
        boot = finish.index("esp_ota_set_boot_partition")
        self.assertLess(package_hash, end)
        self.assertLess(firmware_hash, end)
        self.assertLess(end, boot)
        self.assertIn("workspace.buildIdSeen", finish)
        self.assertIn("packageReceived != release.packageSize", finish)
        self.assertIn("payloadWritten != release.firmwareSize", finish)

    def test_download_is_streamed_and_deterministically_bounded(self) -> None:
        source = read("src/github_ota_installer.cpp")
        self.assertIn("uint8_t downloadBuffer[DOWNLOAD_BUFFER_BYTES]", source)
        self.assertIn("DOWNLOAD_BUFFER_BYTES = 4096U", source)
        self.assertIn("INTERNAL_FLASH_WRITE_BUFFER_BYTES = 1024U", source)
        self.assertIn("MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT", source)
        self.assertIn("workspace.flashWriteBuffer", source)
        self.assertIn(
            "esp_ota_write(\n        workspace.otaHandle, "
            "workspace.flashWriteBuffer, chunk)",
            source,
        )
        self.assertNotIn("esp_ota_write(workspace.otaHandle, data, length)", source)
        self.assertIn("esp_ota_handle_t startedHandle = 0", source)
        self.assertIn("while (workspace->packageReceived < release.packageSize)", source)
        self.assertIn("std::min<size_t>(remaining, DOWNLOAD_BUFFER_BYTES)", source)
        self.assertIn("MAXIMUM_PACKAGE_BYTES", read("include/update_policy.h"))
        for forbidden in (
            "malloc(release.packageSize",
            "heap_caps_malloc(release.packageSize",
            "readString",
            "getString",
        ):
            self.assertNotIn(forbidden, source)

    def test_restart_handoff_runs_from_core_one_service(self) -> None:
        source = read("src/github_ota_installer.cpp")
        main = read("src/main.cpp")
        self.assertIn("RESTART_TASK_CORE = 0", source)
        self.assertIn("RESTART_LOOP_CORE = 1", source)
        self.assertIn("parkCoreOneForRestart", source)
        self.assertIn("xTaskCreatePinnedToCoreWithCaps", source)
        self.assertIn("heap_caps_check_integrity_all", source)
        self.assertIn("update_manager::service();", main)

    def test_second_confirmation_is_ui_local_and_later_is_network_free(self) -> None:
        ui = read("src/update_ui.cpp")
        self.assertIn("installConfirmUntilMs = now + 15000U", ui)
        self.assertNotIn("!confirmationChanged && !installConfirmUntilMs", ui)
        self.assertIn('"CONFIRM INSTALL"', ui)
        self.assertIn("update_manager::requestInstall();", ui)
        later = re.search(r"void laterEvent\([^)]*\) \{(?P<body>.*?)\n\}", ui, re.S)
        self.assertIsNotNone(later)
        assert later
        for forbidden in ("requestInstall", "requestManualCheck", "WiFi", "http"):
            self.assertNotIn(forbidden, later.group("body"))


if __name__ == "__main__":
    unittest.main()
