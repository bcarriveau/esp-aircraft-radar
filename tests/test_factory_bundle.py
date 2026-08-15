from __future__ import annotations

import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

OTA_SPEC = importlib.util.spec_from_file_location(
    "build_radar_ota", SCRIPTS / "build_radar_ota.py"
)
assert OTA_SPEC and OTA_SPEC.loader
OTA = importlib.util.module_from_spec(OTA_SPEC)
sys.modules[OTA_SPEC.name] = OTA
OTA_SPEC.loader.exec_module(OTA)

FACTORY_SPEC = importlib.util.spec_from_file_location(
    "build_factory_bundle", SCRIPTS / "build_factory_bundle.py"
)
assert FACTORY_SPEC and FACTORY_SPEC.loader
FACTORY = importlib.util.module_from_spec(FACTORY_SPEC)
sys.modules[FACTORY_SPEC.name] = FACTORY
FACTORY_SPEC.loader.exec_module(FACTORY)


class FactoryBundleTests(unittest.TestCase):
    @staticmethod
    def firmware(build_id: str, distribution: bool) -> bytes:
        image = bytearray(96 * 1024)
        image[0] = OTA.ESP_IMAGE_MAGIC
        struct.pack_into("<H", image, 12, OTA.ESP32_S3_CHIP_ID)
        build = build_id.encode("ascii")
        image[4096 : 4096 + len(build)] = build
        if distribution:
            marker = OTA.DISTRIBUTION_FIRMWARE_MARKER
            image[8192 : 8192 + len(marker)] = marker
        return bytes(image)

    @staticmethod
    def build_info(path: Path, build_id: str) -> None:
        path.write_text(
            "#pragma once\n"
            "#include <stdint.h>\n"
            "constexpr uint32_t FIRMWARE_VERSION_CODE = 94;\n"
            'constexpr const char* FIRMWARE_VERSION_LABEL = "Product 94";\n'
            'constexpr const char* FIRMWARE_HARDWARE_ID = "waveshare-esp32-s3-touch-lcd-7";\n'
            'constexpr const char* FIRMWARE_RELEASE_CHANNEL = "stable";\n'
            "constexpr uint16_t FIRMWARE_MANIFEST_SCHEMA = 1;\n"
            "constexpr uint16_t FIRMWARE_UPDATER_VERSION = 1;\n"
            'constexpr const char* FIRMWARE_RELEASE_NOTES = "Factory test";\n'
            f'constexpr const char* BUILD_ID = "{build_id}";\n',
            encoding="utf-8",
        )

    def test_distribution_bundle_contains_fixed_layout_and_hashes(self) -> None:
        build_id = "7IN-TEST-PRODUCT94-FACTORY"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            release = root / "release"
            build.mkdir()
            for name, _ in FACTORY.FACTORY_FILES:
                if name == "firmware.bin":
                    payload = self.firmware(build_id, distribution=True)
                else:
                    payload = (name.encode("ascii") + b"\0") * 64
                (build / name).write_bytes(payload)

            info = root / "build_info.h"
            self.build_info(info, build_id)
            installer = ROOT / "tools" / "factory" / "FLASH_RADAR_FACTORY.ps1"
            bundle = FACTORY.write_factory_bundle(build, info, installer, release)
            manifest = json.loads((bundle / FACTORY.FACTORY_MANIFEST).read_text())

            self.assertTrue(manifest["destructive_full_erase"])
            self.assertEqual(manifest["chip"], "esp32s3")
            self.assertEqual(manifest["flash_size"], "16MB")
            self.assertEqual(manifest["build_id"], build_id)
            self.assertEqual(
                [(entry["name"], entry["address"]) for entry in manifest["files"]],
                [
                    ("bootloader.bin", "0x00000000"),
                    ("partitions.bin", "0x00008000"),
                    ("boot_app0.bin", "0x0000E000"),
                    ("firmware.bin", "0x00010000"),
                ],
            )
            for entry in manifest["files"]:
                path = bundle / entry["name"]
                self.assertEqual(entry["size"], path.stat().st_size)
                self.assertEqual(entry["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertTrue((bundle / FACTORY.FACTORY_SCRIPT).is_file())

    def test_boot_app0_can_come_from_framework_package(self) -> None:
        build_id = "7IN-TEST-PRODUCT94-FRAMEWORK-BOOTAPP"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            release = root / "release"
            framework_boot = root / "framework" / "tools" / "partitions" / "boot_app0.bin"
            build.mkdir()
            framework_boot.parent.mkdir(parents=True)
            framework_payload = b"framework-boot-app0" * 32
            framework_boot.write_bytes(framework_payload)
            for name, _ in FACTORY.FACTORY_FILES:
                if name == "boot_app0.bin":
                    continue
                payload = (
                    self.firmware(build_id, distribution=True)
                    if name == "firmware.bin"
                    else (name.encode("ascii") + b"\0") * 64
                )
                (build / name).write_bytes(payload)

            info = root / "build_info.h"
            self.build_info(info, build_id)
            installer = ROOT / "tools" / "factory" / "FLASH_RADAR_FACTORY.ps1"
            bundle = FACTORY.write_factory_bundle(
                build, info, installer, release, boot_app0_path=framework_boot
            )
            self.assertEqual((bundle / "boot_app0.bin").read_bytes(), framework_payload)

    def test_private_firmware_is_refused_before_bundle_creation(self) -> None:
        build_id = "7IN-TEST-PRIVATE-FACTORY-REFUSED"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            release = root / "release"
            build.mkdir()
            for name, _ in FACTORY.FACTORY_FILES:
                payload = (
                    self.firmware(build_id, distribution=False)
                    if name == "firmware.bin"
                    else b"placeholder"
                )
                (build / name).write_bytes(payload)
            info = root / "build_info.h"
            self.build_info(info, build_id)
            with self.assertRaisesRegex(ValueError, "RADAR_DISTRIBUTION_BUILD"):
                FACTORY.write_factory_bundle(
                    build,
                    info,
                    ROOT / "tools" / "factory" / "FLASH_RADAR_FACTORY.ps1",
                    release,
                )
            self.assertFalse(release.exists())

    def test_installer_is_full_erase_and_fixed_layout(self) -> None:
        script = (ROOT / "tools" / "factory" / "FLASH_RADAR_FACTORY.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn('"erase_flash"', script)
        self.assertIn('"0x00000000"', script)
        self.assertIn('"0x00008000"', script)
        self.assertIn('"0x0000E000"', script)
        self.assertIn('"0x00010000"', script)
        self.assertIn("RADAR-DISTRIBUTION-BUILD", script)
        self.assertIn("ENTIRE flash chip", script)
        self.assertIn("Flash size:\\s*16MB", script)
        self.assertNotIn("|16MB)", script)
        self.assertLess(script.index('"erase_flash"'), script.index('"write_flash"'))


if __name__ == "__main__":
    unittest.main()
