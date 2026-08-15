from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Product95FactoryFirmwareBrandingTests(unittest.TestCase):
    def test_owner_visible_firmware_branding_uses_esp_aircraft_radar(self) -> None:
        files = {
            "settings": (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8"),
            "splash": (ROOT / "src" / "boot_splash.cpp").read_text(encoding="utf-8"),
            "ota": (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8"),
            "github": (ROOT / "src" / "github_ota_installer.cpp").read_text(encoding="utf-8"),
            "mqtt": (ROOT / "src" / "mqtt_service.cpp").read_text(encoding="utf-8"),
            "main": (ROOT / "src" / "main.cpp").read_text(encoding="utf-8"),
        }
        for name, source in files.items():
            self.assertNotIn("Bill's Aircraft Radar", source, name)
            self.assertNotIn("BILL'S AIRCRAFT RADAR", source, name)
            self.assertNotIn("BILLS AIRCRAFT RADAR", source, name)
            self.assertNotIn("BILLS Aircraft Radar", source, name)

        self.assertIn('return String("ESP AIRCRAFT RADAR")', files["settings"])
        self.assertGreaterEqual(files["splash"].count('"ESP AIRCRAFT RADAR"'), 2)
        self.assertIn('<title>ESP AIRCRAFT RADAR Update</title>', files["ota"])
        self.assertIn('<title>ESP AIRCRAFT RADAR Airports</title>', files["ota"])
        self.assertIn('<h1>ESP AIRCRAFT RADAR</h1>', files["ota"])
        self.assertIn('ESP AIRCRAFT RADAR OTA package', files["github"])
        self.assertGreaterEqual(files["mqtt"].count('ESP AIRCRAFT RADAR'), 2)
        self.assertIn('ESP AIRCRAFT RADAR 7-inch bring-up', files["main"])

    def test_binary_compatibility_identifiers_are_unchanged(self) -> None:
        ota = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
        github = (ROOT / "src" / "github_ota_installer.cpp").read_text(encoding="utf-8")
        airport = (ROOT / "include" / "airport_package_format.h").read_text(encoding="utf-8")
        self.assertIn('PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA"', ota)
        self.assertIn('PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA"', github)
        self.assertIn('PACKAGE_MAGIC[16] = "BILLS-AIRPORTDB"', airport)

if __name__ == "__main__":
    unittest.main()
