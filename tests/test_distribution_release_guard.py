from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "build_radar_ota.py"
SPEC = importlib.util.spec_from_file_location("build_radar_ota_guard", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FakeEnv:
    def __init__(self, build_flags: str):
        self.build_flags = build_flags

    def get(self, name: str, default=None):
        if name == "BUILD_FLAGS":
            return self.build_flags.split()
        return default

    def subst(self, name: str) -> str:
        if name == "$BUILD_FLAGS":
            return self.build_flags
        raise KeyError(name)


class DistributionReleaseGuardTests(unittest.TestCase):
    @staticmethod
    def firmware(build_id: str, distribution: bool) -> bytes:
        image = bytearray(96 * 1024)
        image[0] = MODULE.ESP_IMAGE_MAGIC
        struct.pack_into("<H", image, 12, MODULE.ESP32_S3_CHIP_ID)
        build = build_id.encode("ascii")
        image[4096 : 4096 + len(build)] = build
        if distribution:
            marker = MODULE.DISTRIBUTION_FIRMWARE_MARKER
            image[8192 : 8192 + len(marker)] = marker
        return bytes(image)

    def test_platformio_guard_rejects_private_flags(self) -> None:
        self.assertFalse(
            MODULE._platformio_distribution_enabled(
                FakeEnv("-DBOARD_HAS_PSRAM -DLV_CONF_INCLUDE_SIMPLE")
            )
        )

    def test_platformio_guard_accepts_distribution_flag(self) -> None:
        self.assertTrue(
            MODULE._platformio_distribution_enabled(
                FakeEnv(
                    "-DBOARD_HAS_PSRAM -DRADAR_DISTRIBUTION_BUILD=1 "
                    "-DLV_CONF_INCLUDE_SIMPLE"
                )
            )
        )

    def test_private_firmware_cannot_be_written_as_public_package(self) -> None:
        build_id = "7IN-TEST-PRIVATE-REFUSED"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware_path = root / "firmware.bin"
            build_info_path = root / "build_info.h"
            output_path = root / "firmware.radarota"
            firmware_path.write_bytes(self.firmware(build_id, distribution=False))
            build_info_path.write_text(
                '#pragma once\nconstexpr const char* BUILD_ID =\n'
                f'    "{build_id}";\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "RADAR_DISTRIBUTION_BUILD"):
                MODULE.write_distribution_package(
                    firmware_path, build_info_path, output_path
                )
            self.assertFalse(output_path.exists())

    def test_distribution_firmware_can_be_written(self) -> None:
        build_id = "7IN-TEST-DISTRIBUTION-ACCEPTED"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            firmware_path = root / "firmware.bin"
            build_info_path = root / "build_info.h"
            output_path = root / "firmware.radarota"
            firmware_path.write_bytes(self.firmware(build_id, distribution=True))
            build_info_path.write_text(
                '#pragma once\nconstexpr const char* BUILD_ID =\n'
                f'    "{build_id}";\n',
                encoding="utf-8",
            )
            size, _ = MODULE.write_distribution_package(
                firmware_path, build_info_path, output_path
            )
            self.assertEqual(size, output_path.stat().st_size)
            self.assertEqual(MODULE.validate_distribution_package(
                output_path.read_bytes()), build_id)

    def test_release_assets_reject_low_level_private_package(self) -> None:
        identity = MODULE.read_build_identity(ROOT / "include" / "build_info.h")
        private_firmware = self.firmware(identity.build_id, distribution=False)
        package = MODULE.create_package(private_firmware, identity.build_id)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package_path = root / "private.radarota"
            package_path.write_bytes(package)
            with self.assertRaisesRegex(ValueError, "RADAR_DISTRIBUTION_BUILD"):
                MODULE.write_release_assets(
                    package_path,
                    ROOT / "include" / "build_info.h",
                    root / "release",
                )
            self.assertFalse((root / "release").exists())


if __name__ == "__main__":
    unittest.main()
