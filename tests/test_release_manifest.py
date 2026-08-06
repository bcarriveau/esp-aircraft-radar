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
SCRIPT = ROOT / "scripts" / "build_radar_ota.py"
SPEC = importlib.util.spec_from_file_location("build_radar_ota", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ReleaseManifestTests(unittest.TestCase):
    def make_firmware(self, build_id: str) -> bytes:
        firmware = bytearray(96 * 1024)
        firmware[0] = MODULE.ESP_IMAGE_MAGIC
        struct.pack_into("<H", firmware, 12, MODULE.ESP32_S3_CHIP_ID)
        encoded = build_id.encode("ascii")
        firmware[4096 : 4096 + len(encoded)] = encoded
        return bytes(firmware)

    def test_generated_package_and_manifest_are_self_consistent(self) -> None:
        identity = MODULE.read_build_identity(ROOT / "include" / "build_info.h")
        firmware = self.make_firmware(identity.build_id)
        package = MODULE.create_package(firmware, identity.build_id)
        metadata = MODULE.package_metadata(package)
        asset_name = MODULE.versioned_package_name(identity)
        manifest_bytes = MODULE.create_release_manifest(
            identity, metadata, asset_name
        )
        manifest = json.loads(manifest_bytes)

        self.assertLessEqual(len(manifest_bytes), MODULE.MAX_MANIFEST_BYTES)
        self.assertEqual(manifest["schema"], 1)
        self.assertEqual(manifest["tag"], f"product-{identity.version_code}")
        self.assertEqual(manifest["hardware"], identity.hardware)
        self.assertEqual(manifest["channel"], "stable")
        self.assertEqual(manifest["version_code"], identity.version_code)
        self.assertEqual(manifest["version_label"], identity.version_label)
        self.assertEqual(manifest["build_id"], identity.build_id)
        self.assertEqual(manifest["asset"], asset_name)
        self.assertEqual(manifest["package_size"], len(package))
        self.assertEqual(
            manifest["package_sha256"], hashlib.sha256(package).hexdigest()
        )
        self.assertEqual(manifest["firmware_size"], len(firmware))
        self.assertEqual(
            manifest["firmware_sha256"], hashlib.sha256(firmware).hexdigest()
        )
        self.assertEqual(manifest["min_updater"], identity.updater_version)

    def test_release_directory_contains_only_versioned_package_and_manifest(self) -> None:
        identity = MODULE.read_build_identity(ROOT / "include" / "build_info.h")
        firmware = self.make_firmware(identity.build_id)
        with tempfile.TemporaryDirectory() as temporary:
            temp = Path(temporary)
            firmware_path = temp / "firmware.bin"
            package_path = temp / "firmware.radarota"
            release_dir = temp / "release"
            firmware_path.write_bytes(firmware)
            MODULE.write_package(
                firmware_path, ROOT / "include" / "build_info.h", package_path
            )
            asset_path, manifest_path, metadata = MODULE.write_release_assets(
                package_path, ROOT / "include" / "build_info.h", release_dir
            )
            self.assertEqual(asset_path.name, MODULE.versioned_package_name(identity))
            self.assertEqual(manifest_path.name, MODULE.MANIFEST_ASSET_NAME)
            self.assertEqual(asset_path.read_bytes(), package_path.read_bytes())
            self.assertEqual(metadata.package_size, package_path.stat().st_size)
            self.assertEqual(
                {path.name for path in release_dir.iterdir()},
                {MODULE.versioned_package_name(identity), MODULE.MANIFEST_ASSET_NAME},
            )
            self.assertFalse((release_dir / "firmware.radarota").exists())
            self.assertNotIn("PROJECT_PACKAGE_PATH", vars(MODULE))
            self.assertNotIn("copy_project_package", vars(MODULE))

    def test_build_identity_rejects_mismatched_label(self) -> None:
        identity = MODULE.read_build_identity(ROOT / "include" / "build_info.h")
        original = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "build_info.h"
            path.write_text(
                original.replace(
                    f'"Product {identity.version_code}"',
                    f'"Product {identity.version_code - 1}"',
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "does not match"):
                MODULE.read_build_identity(path)


if __name__ == "__main__":
    unittest.main()
