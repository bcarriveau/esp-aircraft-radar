#!/usr/bin/env python3
"""Static integration checks for the Product 53R2 airport table revision."""

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

    require(build, "7IN-20260727-PRODUCT53R2-AIRPORT-TABLE",
            "Product 53R2 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // NEARBY", "airport directory title")
    require(ui, "AIRPORTS // DISPLAY OPTIONS", "airport options title")
    require(ui, "SYSTEM // STATUS & SETTINGS", "System page")

    # Airport main view is directory-first and bounded.
    require(ui, "AIRPORT_DIRECTORY_CAPACITY = 32", "bounded directory capacity")
    require(ui, "airportDirectoryTable = lv_table_create",
            "airport directory table")
    require(ui, '"ID", "AIRPORT", "TYPE", "DIST", "BRG", "RUNWAY"',
            "single-line airport column order")
    require(ui, 'lv_label_set_text(airportOptionsLabel, "DISPLAY SETTINGS")',
            "display-settings action")
    require(ui, "LV_TABLE_CELL_CTRL_TEXT_CROP",
            "single-line airport table cells")
    for needle in (
        "lv_table_set_col_width(airportDirectoryTable, 0, 62);",
        "lv_table_set_col_width(airportDirectoryTable, 1, 294);",
        "lv_table_set_col_width(airportDirectoryTable, 2, 84);",
        "lv_table_set_col_width(airportDirectoryTable, 3, 78);",
        "lv_table_set_col_width(airportDirectoryTable, 4, 70);",
        "lv_table_set_col_width(airportDirectoryTable, 5, 126);",
    ):
        require(ui, needle, "airport directory column sizing")
    require(ui, 'snprintf(value, sizeof(value), "%03.0f", airport.bearingDegrees);',
            "compact numeric bearing")
    assert '"%03.0f %s"' not in ui, "directory bearing must not wrap with compass text"
    require(ui, 'lv_label_set_text(airportBackLabel, "BACK TO AIRPORTS")',
            "airport options back action")
    require(ui, "enum class AirportView", "airport subview state")
    require(ui, "AIRPORTS // AIRPORT PROFILE", "airport detail title")
    require(ui, "airportDirectoryTableEvent", "airport row detail action")
    require(ui, "airportDetailLeftLabel", "airport detail location column")
    require(ui, "airportDetailRightLabel", "airport detail runway column")
    require(ui, "airportOptionsDirty", "unsaved airport state")
    require(ui, "settings::airportSymbolMask(range)",
            "directory uses saved symbol settings")
    require(ui, "settings::airportLabelMask(range)",
            "directory uses saved label settings")

    # Airport settings remain bounded, range-specific, and RAM-cached.
    require(ui, "airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]",
            "airport range matrix")
    require(ui, "lv_obj_set_size(airportSaveButton, 160, 34);",
            "airport save button")
    require(ui, 'lv_label_set_text(airportSaveLabel, "SAVE SETTINGS")',
            "airport save wording")
    require(settings_h, "saveAirportSettings", "airport NVS API")
    require(settings_cpp, "cachedAirportSymbols", "RAM-cached symbol settings")
    require(settings_cpp, "cachedAirportLabels", "RAM-cached label settings")
    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

    # System content occupies the upper workspace and Maintenance sits at bottom.
    for needle in (
        "lv_obj_set_size(systemStatusCard, 270, 215);",
        "lv_obj_set_pos(systemStatusCard, 8, 58);",
        "lv_obj_set_size(deviceNetworkCard, 456, 215);",
        "lv_obj_set_pos(deviceNetworkCard, 286, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 58);",
        "lv_obj_set_pos(maintenanceCard, 8, 281);",
        "lv_obj_set_pos(saveSettingsButton, 128, 166);",
        "rgb(120, 240, 155), 8, 198);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT53R2-AIRPORT-TABLE"',
    ):
        require(ui, needle, "System layout")
    require(ui, '"DATA AGE   %lu sec\\n"', "separate data-age diagnostic")
    require(ui, '"FETCH      %lu ms / %lu B\\n"',
            "separate fetch diagnostic")

    # Product 52 airport map behavior remains untouched.
    require(renderer, "bool labelBoxesOverlap(", "airport collision helper")
    require(renderer, "bool placeAirportLabel(", "deterministic airport placement")
    require(renderer, "LabelBox placedLabels[AIRPORT_LABEL_CAPACITY]",
            "bounded label boxes")
    assert "airportLabelOverlapsContact" not in renderer

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
    require(main_cpp, "WARNING: Airport overlay unavailable; aircraft radar continuing",
            "optional airport failure behavior")
    print("Product 53R2 airport directory integration checks passed")


if __name__ == "__main__":
    main()
