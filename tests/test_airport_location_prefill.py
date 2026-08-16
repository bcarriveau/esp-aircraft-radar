#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")

def require(text: str, needle: str) -> None:
    assert needle in text, f"missing Product 92 requirement: {needle}"

def main() -> None:
    require(BUILD, "FIRMWARE_VERSION_CODE = 92")
    require(BUILD, "PRODUCT92-AIRPORT-LOCATION-PREFILL")
    require(OTA, '#include "settings.h"')
    require(OTA, "settings::homeLatitude()")
    require(OTA, "settings::homeLongitude()")
    require(OTA, "settings::coordinatesValid(homeLatitude, homeLongitude)")
    require(OTA, '\\"home_coordinates_valid\\":%s')
    require(OTA, '\\"home_latitude\\":%.6f')
    require(OTA, '\\"home_longitude\\":%.6f')
    require(OTA, "let homeCoordinatesLoaded=false")
    require(OTA, "if(!homeCoordinatesLoaded&&j.home_coordinates_valid)")
    require(OTA, "Number(j.home_latitude).toFixed(5)")
    require(OTA, "Number(j.home_longitude).toFixed(5)")
    require(OTA, "Saved radar location loaded.")
    assert "42.83047" not in OTA
    assert "-88.16204" not in OTA
    require(OTA, 'placeholder="e.g. 40.00000"')
    require(OTA, 'placeholder="e.g. -95.00000"')
    require(OTA, "Radar ready. Settling web connection before upload...")
    require(OTA, "currentState = State::SUCCESS;")
    require(OTA, "restartAtMs = millis() + RESTART_DELAY_MS;")
    require(OTA, "BACK TO FIRMWARE UPDATE")
    print("Product 92 saved-coordinate prefill checks passed")

if __name__ == "__main__":
    main()
