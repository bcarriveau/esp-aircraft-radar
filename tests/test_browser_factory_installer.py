from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HTML = ROOT / "tools" / "factory" / "browser" / "INSTALL_RADAR.html"
JS = ROOT / "tools" / "factory" / "browser" / "factory-installer.js"


class BrowserFactoryInstallerTests(unittest.TestCase):
    def test_browser_installer_pins_esptool_and_requires_fixed_hardware(self) -> None:
        script = JS.read_text(encoding="utf-8")
        self.assertIn("esptool-js@0.6.0/bundle.js", script)
        self.assertIn('hardware: "waveshare-esp32-s3-touch-lcd-7"', script)
        self.assertIn('chipName: "ESP32-S3"', script)
        self.assertIn('flashSize: "16MB"', script)
        self.assertIn('distributionMarker: "RADAR-DISTRIBUTION-BUILD"', script)
        self.assertIn('confirmation: "ERASE RADAR"', script)

    def test_browser_installer_validates_hashes_before_destructive_work(self) -> None:
        script = JS.read_text(encoding="utf-8")
        verify_pos = script.index("async function verifyBundle")
        connect_pos = script.index("async function connectAndVerify")
        erase_pos = script.index("await state.loader.eraseFlash()")
        write_pos = script.index("await state.loader.writeFlash")
        self.assertLess(verify_pos, connect_pos)
        self.assertLess(connect_pos, erase_pos)
        self.assertLess(erase_pos, write_pos)
        self.assertIn('crypto.subtle.digest("SHA-256"', script)
        self.assertIn("bytesContainAscii(firmware, manifest.build_id)", script)
        self.assertIn("bytesContainAscii(firmware, EXPECTED.distributionMarker)", script)

    def test_browser_installer_uses_exact_four_image_layout(self) -> None:
        script = JS.read_text(encoding="utf-8")
        expected = {
            "bootloader.bin": "0x00000000",
            "partitions.bin": "0x00008000",
            "boot_app0.bin": "0x0000e000",
            "firmware.bin": "0x00010000",
        }
        for name, address in expected.items():
            self.assertIn(f'name: "{name}"', script)
            self.assertIn(address, script)
        self.assertIn("manifest.files.length === EXPECTED.layout.length", script)

    def test_browser_installer_requires_positive_chip_and_flash_size(self) -> None:
        script = JS.read_text(encoding="utf-8")
        self.assertIn("loader.chip.CHIP_NAME === EXPECTED.chipName", script)
        self.assertIn("await loader.detectFlashSize()", script)
        self.assertIn("flashSize === EXPECTED.flashSize", script)
        self.assertNotIn("eraseAll: true", script)
        self.assertIn("eraseAll: false", script)
        self.assertIn("calculateMD5Hash: md5Hex", script)

    def test_release_browser_uses_explicit_en_reset_then_releases_serial(self) -> None:
        script = JS.read_text(encoding="utf-8")
        install = script[script.index("async function eraseAndInstall"):script.index('el.bundleFiles.addEventListener')]
        self.assertNotIn('state.loader.after("hard_reset")', install)
        dtr = install.index("await state.transport.setDTR(false)")
        reset_low = install.index("await state.transport.setRTS(true)")
        low_delay = install.index("setTimeout(resolve, 250)", reset_low)
        reset_release = install.index("await state.transport.setRTS(false)", low_delay)
        boot_delay = install.index("setTimeout(resolve, 750)", reset_release)
        disconnect = install.index("await releaseSerialTransport({ updateStatus: false })", boot_delay)
        self.assertLess(dtr, reset_low)
        self.assertLess(reset_low, low_delay)
        self.assertLess(low_delay, reset_release)
        self.assertLess(reset_release, boot_delay)
        self.assertLess(boot_delay, disconnect)
        self.assertIn("await transport.disconnect()", script)
        self.assertIn("Web Serial transport released.", script)
        self.assertIn("If the display remains stopped, press RESET once and report it.", script)


    def test_local_file_disables_adjacent_bundle_fetch(self) -> None:
        html = HTML.read_text(encoding="utf-8")
        script = JS.read_text(encoding="utf-8")
        self.assertIn('id="bundleLoadHelp"', html)
        self.assertIn('window.location.protocol !== "file:"', script)
        self.assertIn('el.loadAdjacent.disabled = busy || !adjacentBundleAvailable', script)
        self.assertIn('el.loadAdjacent.textContent = "HOSTED USE ONLY"', script)
        self.assertIn('Adjacent loading is unavailable from a file:// page.', script)
        self.assertIn('assert(adjacentBundleAvailable, "Adjacent bundle loading requires HTTPS or localhost.', script)

    def test_factory_installer_owner_branding_is_consistent(self) -> None:
        html = HTML.read_text(encoding="utf-8")
        recovery = (ROOT / "tools" / "factory" / "FLASH_RADAR_FACTORY.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn("<title>ESP AIRCRAFT RADAR Factory Installer</title>", html)
        self.assertIn("<h1>ESP AIRCRAFT RADAR</h1>", html)
        self.assertIn(" ESP AIRCRAFT RADAR - FACTORY INSTALLER", recovery)
        self.assertNotIn("Bill's Aircraft Radar Factory Installer", html)
        self.assertNotIn("ESP32 AIRCRAFT RADAR", html)
        self.assertNotIn("BILL'S AIRCRAFT RADAR - FACTORY INSTALLER", recovery)

    def test_browser_page_contains_destructive_owner_warning(self) -> None:
        html = HTML.read_text(encoding="utf-8")
        self.assertIn("erases the ENTIRE 16 MB flash chip", html)
        self.assertIn("Wi-Fi", html)
        self.assertIn("home location", html)
        self.assertIn("MQTT/Home Assistant", html)
        self.assertIn("airport database", html)
        self.assertIn("ERASE RADAR", html)


if __name__ == "__main__":
    unittest.main()
