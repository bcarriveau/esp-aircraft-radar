#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT/"include/build_info.h").read_text(encoding="utf-8")
H = (ROOT/"include/settings.h").read_text(encoding="utf-8")
S = (ROOT/"src/settings.cpp").read_text(encoding="utf-8")
M = (ROOT/"src/main.cpp").read_text(encoding="utf-8")
R = (ROOT/"src/radar_control.cpp").read_text(encoding="utf-8")
U = (ROOT/"src/update_ui.cpp").read_text(encoding="utf-8")
README = (ROOT/"README.md").read_text(encoding="utf-8")
CHANGELOG = (ROOT/"CHANGELOG.md").read_text(encoding="utf-8")

def need(text, value):
    assert value in text, f"missing: {value}"

def main():
    need(BUILD, "FIRMWARE_VERSION_CODE = 93")
    need(BUILD, "PRODUCT93-UPDATE-NOTES-RANGE-PERSISTENCE")
    need(BUILD, "Shows update release notes clearly and restores the last selected radar range after restart.")

    need(H, "uint8_t radarRangeMiles();")
    need(H, "bool setRadarRangeMiles(uint8_t rangeMiles);")
    need(S, 'KEY_RADAR_RANGE = "radar_rng"')
    need(S, "DEFAULT_RADAR_RANGE_MILES = 80")
    need(S, "rangeMiles == 20 || rangeMiles == 40 || rangeMiles == 80")
    need(S, "preferences.getType(KEY_RADAR_RANGE) != PT_U8")
    need(S, "!radarRangeValid(storedRange)")
    need(S, "writeUCharChecked(KEY_RADAR_RANGE, DEFAULT_RADAR_RANGE_MILES)")
    need(S, "bool setRadarRangeMiles(uint8_t rangeMiles)")
    need(S, 'markStorageError("radar range save")')

    # Restore happens after settings/app-state init and before networking starts.
    si = M.index("settings::initialize()")
    ai = M.index("app_state::initialize();")
    restore = M.index("settings::radarRangeMiles()")
    adsb = M.index("adsb::begin()")
    assert si < ai < restore < adsb

    # Manual range remains immediate; persistence failure cannot undo it.
    state_change = R.index("app_state::setRadarRangeMiles(rangeMiles)")
    persist = R.index("settings::setRadarRangeMiles(savedRange)")
    refresh = R.index("adsb::requestRefresh()")
    assert state_change < persist < refresh
    need(R, "WARNING: radar range changed to %u miles but NVS save failed")

    # Update page uses already-validated remote manifest notes.
    need(U, "UPDATE AVAILABLE")
    need(U, "WHAT'S NEW\\n%s")
    need(U, "status.notes")
    assert "FIRMWARE_RELEASE_CHANNEL" not in U
    assert "branch" not in U.lower()

    # No forbidden networking/API regression in changed files.
    changed_runtime = "\n".join([S, M, R, U])
    assert "HTTPClient::GET()" not in changed_runtime
    assert "setInsecure()" not in changed_runtime

    need(README, "Product 93")
    need(CHANGELOG, "## Product 93 - 2026-08-13")
    print("Product 93 focused source checks passed")

if __name__ == "__main__":
    main()
