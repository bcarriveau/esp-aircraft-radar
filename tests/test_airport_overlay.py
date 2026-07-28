#!/usr/bin/env python3
"""Static integration checks for the Product 53 page redesign."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {context}: {needle}"


def main() -> None:
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    settings_h = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    require(build, "7IN-20260728-PRODUCT53-PAGE-REDESIGN", "Product 53 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // NEARBY", "airport main page")
    require(ui, "AIRPORTS // DISPLAY SETTINGS", "airport settings subpage")
    require(ui, "SYSTEM // DEVICE & NETWORK", "System page")

    # Airports main view is a bounded operational table; display controls live
    # on a separate subpage rather than competing for the same screen.
    require(ui, "airportMainPanel", "airport main view")
    require(ui, "airportSettingsPanel", "airport settings view")
    require(ui, "airportTable = lv_table_create", "nearby airport table")
    require(ui, 'lv_label_set_text(settingsButtonLabel, "DISPLAY SETTINGS")',
            "airport settings button")
    require(ui, 'lv_label_set_text(airportBackLabel, "BACK TO AIRPORTS")',
            "airport settings back button")
    for heading in ("IDENT", "AIRPORT", "TYPE", "DIST", "BRG", "RUNWAY"):
        require(ui, f'"{heading}"', f"airport table {heading} column")
    require(ui, "airport_data::NearbyAirport nearby[32]{};", "bounded table copy")
    require(ui, "if (!airportListDirty && listKey == lastAirportListKey) return;",
            "airport table scroll-preserving refresh guard")

    # Airport settings remain bounded, range-specific, and RAM-cached.
    require(ui, "airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]",
            "airport range matrix")
    require(ui, "lv_obj_set_size(airportSaveButton, 176, 36);",
            "airport save button")
    require(ui, "lv_obj_set_style_pad_hor(airportSaveButton, 16, 0);",
            "airport save horizontal padding")
    require(settings_h, "saveAirportSettings", "airport NVS API")
    require(settings_cpp, "cachedAirportSymbols", "RAM-cached symbol settings")
    require(settings_cpp, "cachedAirportLabels", "RAM-cached label settings")
    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

    # Main System cards regain height while Maintenance is a compact lower strip.
    for needle in (
        "lv_obj_set_size(systemStatusCard, 286, 220);",
        "lv_obj_set_size(deviceNetworkCard, 438, 220);",
        "lv_obj_set_pos(deviceNetworkCard, 304, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 48);",
        "lv_obj_set_pos(maintenanceCard, 8, 284);",
        "lv_obj_set_pos(retryButton, 126, 2);",
        "lv_obj_set_pos(reconnectButton, 318, 2);",
        "lv_obj_set_pos(resetSettingsButton, 510, 2);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT53-PAGE-REDESIGN"',
    ):
        require(ui, needle, "System layout")

    # Airport map labels remain deterministic and below aircraft.
    require(renderer, "bool placeAirportLabel(", "deterministic airport placement")
    require(renderer, "LabelBox placedLabels[AIRPORT_LABEL_CAPACITY]", "bounded labels")
    render_body = renderer[renderer.index("bool render("):]
    assert render_body.index("drawAirportLabels(rangeMiles);") < render_body.index(
        "drawAirportSymbols(rangeMiles);"
    )
    assert render_body.index("drawAirportSymbols(rangeMiles);") < render_body.index(
        "drawContacts(workTargets"
    )
    assert render_body.index("drawContacts(workTargets") < render_body.index(
        "drawContactLabels(workTargets"
    )

    # Aircraft touch behavior remains the existing ICAO-only hit test.
    require(renderer, "bool hitTest(int canvasX, int canvasY, HitResult& result)",
            "aircraft hit test")
    hit_test = renderer[renderer.index("bool hitTest(int canvasX"):]
    assert "airport" not in hit_test.lower()

    require(main_cpp, "airport_data::initialize(settings::homeLatitude()",
            "optional airport initialization")
    print("Product 53 airport and System page integration checks passed")


if __name__ == "__main__":
    main()
