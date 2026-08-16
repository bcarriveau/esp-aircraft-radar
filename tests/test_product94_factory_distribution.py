#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
CFG = (ROOT / "include/distribution/config.h").read_text(encoding="utf-8")
APT = (ROOT / "src/airport_data.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include/build_info.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PACKAGER = (ROOT / "scripts/build_radar_ota.py").read_text(encoding="utf-8")
FACTORY_PACKAGER = (ROOT / "scripts/build_factory_bundle.py").read_text(encoding="utf-8")
FACTORY_INSTALLER = (ROOT / "tools/factory/FLASH_RADAR_FACTORY.ps1").read_text(encoding="utf-8")


def env_section(name: str) -> str:
    header = f"[env:{name}]"
    start = PIO.index(header) + len(header)
    next_header = PIO.find("\n[env:", start)
    return PIO[start:] if next_header < 0 else PIO[start:next_header]


def main():
    assert "FIRMWARE_VERSION_CODE = 94" in BUILD
    assert "PRODUCT94-FACTORY-DISTRIBUTION" in BUILD
    assert 'FIRMWARE_BUILD_VARIANT = "distribution"' in BUILD
    assert '"RADAR-DISTRIBUTION-BUILD"' in BUILD
    assert 'FIRMWARE_BUILD_VARIANT = "private"' in BUILD
    assert "Distribution marker: %s" in MAIN

    private = env_section("waveshare-s3-touch-lcd-7")
    factory = env_section("waveshare-s3-touch-lcd-7-factory")
    assert "build_radar_ota.py" not in private
    assert "post:scripts/build_radar_ota.py" in factory
    assert "post:scripts/build_factory_bundle.py" in factory
    assert "-DRADAR_DISTRIBUTION_BUILD=1" in factory
    lines = [line.strip() for line in factory.splitlines()]
    assert lines.index("-I include/distribution") < lines.index("-I include")

    assert '#define WIFI_SSID ""' in CFG
    assert '#define WIFI_PASS ""' in CFG
    assert '#define MQTT_ENABLED_DEFAULT 0' in CFG
    assert '#define MQTT_BROKER_URI ""' in CFG
    assert "YOUR_WIFI" not in CFG

    assert "#if !defined(RADAR_DISTRIBUTION_BUILD)" in APT
    assert "no regional database installed" in APT
    assert 'return "NOT INSTALLED";' in APT

    assert 'DISTRIBUTION_FIRMWARE_MARKER = b"RADAR-DISTRIBUTION-BUILD"' in PACKAGER
    assert "write_distribution_package" in PACKAGER
    assert "_platformio_distribution_enabled" in PACKAGER
    assert re.search(
        r"Public Radar OTA packaging requires RADAR_DISTRIBUTION_BUILD",
        PACKAGER,
    )

    assert "write_factory_bundle" in FACTORY_PACKAGER
    assert "RADAR_DISTRIBUTION_BUILD" in FACTORY_PACKAGER
    assert "erase_flash" in FACTORY_INSTALLER
    assert "factory-manifest.json" in FACTORY_INSTALLER
    assert "RADAR-DISTRIBUTION-BUILD" in FACTORY_INSTALLER

    combined = PIO + CFG + APT + BUILD + MAIN + PACKAGER + FACTORY_PACKAGER + FACTORY_INSTALLER
    assert "setInsecure()" not in combined
    assert "HTTPClient::GET()" not in combined
    print("Product 94 factory/distribution structural checks passed")


if __name__ == "__main__":
    main()
