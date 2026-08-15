#!/usr/bin/env python3
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT/"platformio.ini").read_text(encoding="utf-8")
CFG = (ROOT/"include/distribution/config.h").read_text(encoding="utf-8")
APT = (ROOT/"src/airport_data.cpp").read_text(encoding="utf-8")
BUILD = (ROOT/"include/build_info.h").read_text(encoding="utf-8")
def main():
    assert "FIRMWARE_VERSION_CODE = 94" in BUILD
    assert "PRODUCT94-FACTORY-DISTRIBUTION" in BUILD
    assert "[env:waveshare-s3-touch-lcd-7-factory]" in PIO
    factory = PIO.split("[env:waveshare-s3-touch-lcd-7-factory]",1)[1]
    assert "-DRADAR_DISTRIBUTION_BUILD=1" in factory
    lines = [line.strip() for line in factory.splitlines()]
    assert lines.index("-I include/distribution") < lines.index("-I include")
    assert re.search(r"(?m)^extra_scripts\s*=\s*$", factory)
    assert '#define WIFI_SSID ""' in CFG
    assert '#define WIFI_PASS ""' in CFG
    assert '#define MQTT_ENABLED_DEFAULT 0' in CFG
    assert '#define MQTT_BROKER_URI ""' in CFG
    assert "YOUR_WIFI" not in CFG
    assert "#if !defined(RADAR_DISTRIBUTION_BUILD)" in APT
    assert "no regional database installed" in APT
    assert 'return "NOT INSTALLED";' in APT
    assert "setInsecure()" not in APT
    assert "HTTPClient::GET()" not in APT
    print("Product 94 factory/distribution structural checks passed")
if __name__ == "__main__":
    main()
