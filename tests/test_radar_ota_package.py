#!/usr/bin/env python3

import hashlib
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "build_radar_ota.py"
SPEC = importlib.util.spec_from_file_location("build_radar_ota", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class RadarOtaPackageTests(unittest.TestCase):
    @staticmethod
    def firmware(build_id="7IN-20260801-PRODUCT54-LOCAL-WEB-OTA",
                 chip_id=MODULE.ESP32_S3_CHIP_ID, size=70000):
        image = bytearray(size)
        image[0] = MODULE.ESP_IMAGE_MAGIC
        struct.pack_into("<H", image, 12, chip_id)
        for index in range(24, size):
            image[index] = index & 0xFF
        encoded = build_id.encode("ascii")
        image[128:128 + len(encoded)] = encoded
        return bytes(image)

    def test_package_header_and_payload(self):
        firmware = self.firmware()
        build_id = "7IN-20260801-PRODUCT54-LOCAL-WEB-OTA"
        package = MODULE.create_package(firmware, build_id)
        self.assertEqual(len(package), MODULE.HEADER_SIZE + len(firmware))
        fields = MODULE.HEADER_STRUCT.unpack(package[:MODULE.HEADER_SIZE])
        self.assertEqual(fields[0].rstrip(b"\0"), MODULE.PACKAGE_MAGIC)
        self.assertEqual(fields[1], MODULE.FORMAT_VERSION)
        self.assertEqual(fields[2], MODULE.HEADER_SIZE)
        self.assertEqual(fields[3].rstrip(b"\0"), MODULE.HARDWARE_ID)
        self.assertEqual(fields[4].rstrip(b"\0").decode(), build_id)
        self.assertEqual(fields[5], len(firmware))
        self.assertEqual(fields[6], hashlib.sha256(firmware).digest())
        self.assertEqual(package[MODULE.HEADER_SIZE:], firmware)

    def test_validates_complete_package(self):
        firmware = self.firmware()
        build_id = "7IN-20260801-PRODUCT54-LOCAL-WEB-OTA"
        package = MODULE.create_package(firmware, build_id)
        self.assertEqual(MODULE.validate_package(package), build_id)

    def test_rejects_corrupt_payload_digest(self):
        build_id = "7IN-TEST-CORRUPT-DIGEST"
        package = bytearray(MODULE.create_package(
            self.firmware(build_id), build_id
        ))
        package[-1] ^= 0x01
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            MODULE.validate_package(bytes(package))

    def test_rejects_incorrect_package_length(self):
        build_id = "7IN-TEST-TRUNCATED-PACKAGE"
        package = MODULE.create_package(
            self.firmware(build_id), build_id
        )
        with self.assertRaisesRegex(ValueError, "length"):
            MODULE.validate_package(package[:-1])

    def test_rejects_build_id_not_embedded_in_firmware(self):
        with self.assertRaisesRegex(ValueError, "not embedded"):
            MODULE.create_package(
                self.firmware("7IN-OTHER-BUILD"),
                "7IN-DECLARED-BUILD",
            )

    def test_rejects_wrong_chip(self):
        with self.assertRaisesRegex(ValueError, "not ESP32-S3"):
            MODULE.create_package(
                self.firmware("7IN-TEST-WRONG-CHIP", chip_id=0),
                "7IN-TEST-WRONG-CHIP",
            )

    def test_rejects_truncated_image(self):
        with self.assertRaisesRegex(ValueError, "too small"):
            MODULE.create_package(b"\xE9" * 12, "7IN-TEST-TRUNCATED")

    def test_reads_multiline_build_id_and_writes_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            firmware_path = root / "firmware.bin"
            build_info = root / "build_info.h"
            output = root / "firmware.radarota"
            build_id = "7IN-20260801-PRODUCT54-LOCAL-WEB-OTA"
            firmware = self.firmware(build_id)
            firmware_path.write_bytes(firmware)
            build_info.write_text(
                '#pragma once\nconstexpr const char* BUILD_ID =\n'
                f'    "{build_id}";\n',
                encoding="utf-8",
            )
            size, digest = MODULE.write_package(
                firmware_path, build_info, output
            )
            self.assertEqual(size, MODULE.HEADER_SIZE + len(firmware))
            self.assertEqual(digest, hashlib.sha256(output.read_bytes()).hexdigest())

    def test_copies_package_into_project_release_folder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "workspace" / "firmware.radarota"
            package.parent.mkdir()
            package.write_bytes(b"test-radar-ota-package")

            copied = MODULE.copy_project_package(package, root / "project")

            self.assertEqual(
                copied, root / "project" / "release" / "firmware.radarota"
            )
            self.assertEqual(copied.read_bytes(), package.read_bytes())


class RadarOtaIntegrationTests(unittest.TestCase):
    def test_product_54_integration_is_bounded_and_isolated(self):
        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        build_info = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
        ota_source = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
        network_header = (ROOT / "include" / "adsb_network.h").read_text(encoding="utf-8")
        network_source = (ROOT / "src" / "adsb_network.cpp").read_text(encoding="utf-8")
        main_source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        mqtt_header = (ROOT / "include" / "mqtt_service.h").read_text(encoding="utf-8")
        mqtt_source = (ROOT / "src" / "mqtt_service.cpp").read_text(encoding="utf-8")

        self.assertIn("extra_scripts = post:scripts/build_radar_ota.py", platformio)
        package_script = SCRIPT.read_text(encoding="utf-8")
        self.assertIn('Path("release") / "firmware.radarota"', package_script)
        self.assertIn("board_build.partitions = default_16MB.csv", platformio)
        self.assertIn("platformio/framework-arduinoespressif32-libs@", platformio)
        self.assertIn("7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS", build_info)
        for api in ("esp_ota_begin", "esp_ota_write", "esp_ota_end",
                    "esp_ota_set_boot_partition"):
            self.assertIn(api, ota_source)
        self.assertNotIn("Update.h", ota_source)
        self.assertIn("Firmware payload does not contain the declared build ID",
                      ota_source)
        self.assertIn("requestMaintenanceHold", network_header)
        self.assertIn("maintenanceHoldActive", network_header)
        self.assertIn("releaseMaintenanceHold", network_header)
        self.assertIn("ADSB task entered OTA maintenance hold", network_source)
        self.assertIn("ota_update::service();", main_source)
        self.assertIn("mqtt_service::requestMaintenanceHold();", ota_source)
        self.assertIn("mqtt_service::maintenanceHoldActive()", ota_source)
        self.assertIn("mqtt_service::releaseMaintenanceHold();", ota_source)
        self.assertIn("requestMaintenanceHold", mqtt_header)
        self.assertIn("maintenanceHoldActive", mqtt_header)
        self.assertIn("releaseMaintenanceHold", mqtt_header)
        self.assertIn("MQTT idle for firmware update", mqtt_source)
        combined = ota_source + network_source + mqtt_source + main_source
        self.assertNotIn("setInsecure()", combined)
        self.assertNotIn("HTTPClient::GET()", combined)


if __name__ == "__main__":
    unittest.main()
