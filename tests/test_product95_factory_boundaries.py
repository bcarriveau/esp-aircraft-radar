from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / "src" / "settings.cpp"
PLATFORMIO = ROOT / "platformio.ini"
BUILD_INFO = ROOT / "include" / "build_info.h"
FACTORY_DOC = ROOT / "docs" / "FACTORY_INSTALL.md"
RELEASE_DOC = ROOT / "docs" / "GITHUB_RELEASES.md"


class Product95FactoryBoundaryTests(unittest.TestCase):
    def test_private_reseed_is_compiled_out_of_distribution_build(self) -> None:
        source = SETTINGS.read_text(encoding="utf-8")
        helper = source.index("bool factoryNeutralOwnerStatePresent()")
        seed = source.index("bool seedPrivateDefaultsFromFactoryState()")
        guard = source.rfind("#if !defined(RADAR_DISTRIBUTION_BUILD)", 0, helper)
        end_guard = source.index("#endif", seed)
        self.assertGreaterEqual(guard, 0)
        self.assertLess(guard, helper)
        self.assertLess(seed, end_guard)

        initialize = source.index("bool initialize()")
        init_guard = source.index("#if !defined(RADAR_DISTRIBUTION_BUILD)", initialize)
        init_call = source.index("seedPrivateDefaultsFromFactoryState()", init_guard)
        init_end = source.index("#endif", init_call)
        self.assertLess(init_guard, init_call)
        self.assertLess(init_call, init_end)

    def test_private_reseed_requires_complete_neutral_factory_tuple(self) -> None:
        source = SETTINGS.read_text(encoding="utf-8")
        start = source.index("bool factoryNeutralOwnerStatePresent()")
        end = source.index("bool seedPrivateDefaultsFromFactoryState()", start)
        policy = source[start:end]
        self.assertIn("preferences.getType(KEY_WIFI_SSID) == PT_STR", policy)
        self.assertIn("preferences.getType(KEY_WIFI_PASS) == PT_STR", policy)
        self.assertIn("preferences.getString(KEY_WIFI_SSID", policy)
        self.assertIn("preferences.getString(KEY_WIFI_PASS", policy)
        self.assertIn("storedFloatMatches(KEY_LAT, 0.0f)", policy)
        self.assertIn("storedFloatMatches(KEY_LON, 0.0f)", policy)

    def test_private_reseed_uses_config_defaults_without_logging_secrets(self) -> None:
        source = SETTINGS.read_text(encoding="utf-8")
        start = source.index("bool seedPrivateDefaultsFromFactoryState()")
        end = source.index("#endif", start)
        seed = source[start:end]
        self.assertIn("defaultWifiSsid()", seed)
        self.assertIn("defaultWifiPassword()", seed)
        self.assertIn("defaultLatitude()", seed)
        self.assertIn("defaultLongitude()", seed)
        self.assertIn("MQTT_ENABLED_DEFAULT ? 1 : 0", seed)
        self.assertNotIn("String(WIFI_SSID)", seed)
        self.assertNotIn("String(WIFI_PASS)", seed)
        self.assertNotIn("HOME_LAT", seed)
        self.assertNotIn("HOME_LON", seed)

    def test_public_release_scripts_remain_distribution_only(self) -> None:
        platformio = PLATFORMIO.read_text(encoding="utf-8")
        private = platformio.split("[env:waveshare-s3-touch-lcd-7]", 1)[1].split(
            "[env:waveshare-s3-touch-lcd-7-factory]", 1
        )[0]
        factory = platformio.split("[env:waveshare-s3-touch-lcd-7-factory]", 1)[1]
        self.assertNotIn("build_radar_ota.py", private)
        self.assertNotIn("build_factory_bundle.py", private)
        self.assertIn("-DRADAR_DISTRIBUTION_BUILD=1", factory)
        self.assertIn("post:scripts/build_radar_ota.py", factory)
        self.assertIn("post:scripts/build_factory_bundle.py", factory)

    def test_product_marker_stays_product_95_with_unique_handoff_build(self) -> None:
        build = BUILD_INFO.read_text(encoding="utf-8")
        self.assertIn("FIRMWARE_VERSION_CODE = 95", build)
        self.assertIn('FIRMWARE_VERSION_LABEL = "Product 95"', build)
        self.assertIn('"7IN-20260815-PRODUCT95-FACTORY-HANDOFF"', build)

    def test_docs_state_three_paths_and_private_config_boundary(self) -> None:
        combined = FACTORY_DOC.read_text(encoding="utf-8") + RELEASE_DOC.read_text(
            encoding="utf-8"
        )
        self.assertIn("Destructive factory install / fresh start", combined)
        self.assertIn("Private development build", combined)
        self.assertIn("Public GitHub OTA", combined)
        self.assertIn("must never", combined)
        self.assertIn("config.h", combined)
        self.assertIn("double-click", combined)


if __name__ == "__main__":
    unittest.main()
