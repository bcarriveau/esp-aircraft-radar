#!/usr/bin/env python3
"""Static integration checks for the Product 53R3 airport-label refinement."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {context}: {needle}"


def main() -> None:
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    renderer_h = (ROOT / "include" / "radar_renderer.h").read_text(encoding="utf-8")
    settings_h = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    require(build, "7IN-20260729-PRODUCT53R3-AIRPORT-LABELS",
            "Product 53R3 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // NEARBY", "airport directory title")
    require(ui, "AIRPORTS // DISPLAY OPTIONS", "airport options title")
    require(ui, "SYSTEM // STATUS & SETTINGS", "System page")

    # Product 53R2 directory structure stays intact with slightly taller rows.
    require(ui, "AIRPORT_DIRECTORY_CAPACITY = 32", "bounded directory capacity")
    require(ui, "airportDirectoryTable = lv_table_create",
            "airport directory table")
    require(ui, '"ID", "AIRPORT", "TYPE", "DIST", "BRG", "RUNWAY"',
            "single-line airport column order")
    require(ui, "LV_TABLE_CELL_CTRL_TEXT_CROP",
            "single-line airport table cells")
    require(ui,
            "lv_obj_set_style_pad_ver(airportDirectoryTable, 7, LV_PART_ITEMS);",
            "slightly taller airport rows")
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

    # The directory summary distinguishes in-range airports from actual labels.
    require(ui, "%u IN RANGE  |  %u LABELS", "accurate airport summary")
    require(ui, "LABELS --", "invalidated label-count state")
    assert "%u VISIBLE" not in ui, "directory must not call all entries visible labels"
    require(renderer_h, "bool airportLabelCount(", "label-count API")
    require(renderer_h, "void invalidateAirportLabelCount();",
            "label-count invalidation API")
    require(ui, "radar::invalidateAirportLabelCount();",
            "label-count invalidation on configuration/location changes")

    # Every enabled airport is attempted; fit/collision rules replace tiny quotas.
    require(renderer, "LabelBox* airportLabelBoxes = nullptr;",
            "bounded PSRAM airport-label placement buffer")
    require(renderer,
            "airport_data::MAX_NEARBY_AIRPORTS, sizeof(LabelBox)",
            "airport-label buffer capacity")
    require(renderer,
            "for (uint8_t category = 0; category < airport_data::CATEGORY_COUNT;",
            "deterministic category ordering")
    require(renderer,
            "for (uint16_t index = 0; index < airportFrameCount; ++index)",
            "all enabled nearby airports attempted")
    require(renderer, "airportLabelStatsCount++", "actual labels-drawn count")
    assert "maximumLabels" not in renderer, "range-specific label quota must be removed"
    assert "rangeMiles <= 20.1f\n      ? 9" not in renderer
    assert "AIRPORT_LABEL_CAPACITY = 10" not in renderer

    # Defaults: major + public on at every range; private + heliport remain off.
    require(settings_cpp,
            "constexpr uint8_t DEFAULT_AIRPORT_LABELS[AIRPORT_RANGE_COUNT] = {\n"
            "  0x03, 0x03, 0x03\n};",
            "major/public label defaults at all ranges")
    require(settings_cpp,
            "constexpr uint8_t DEFAULT_AIRPORT_SYMBOLS[AIRPORT_RANGE_COUNT] = {\n"
            "  0x03, 0x03, 0x03\n};",
            "major/public symbol defaults at all ranges")
    require(settings_h, "saveAirportSettings", "airport NVS API")
    require(settings_cpp, "cachedAirportSymbols", "RAM-cached symbol settings")
    require(settings_cpp, "cachedAirportLabels", "RAM-cached label settings")
    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

    # Existing page/subview and System layouts remain unchanged.
    require(ui, 'lv_label_set_text(airportOptionsLabel, "DISPLAY SETTINGS")',
            "display-settings action")
    require(ui, 'lv_label_set_text(airportBackLabel, "BACK TO AIRPORTS")',
            "airport options back action")
    require(ui, "enum class AirportView", "airport subview state")
    require(ui, "AIRPORTS // AIRPORT PROFILE", "airport detail title")
    require(ui, "airportDirectoryTableEvent", "airport row detail action")
    for needle in (
        "lv_obj_set_size(systemStatusCard, 270, 215);",
        "lv_obj_set_pos(systemStatusCard, 8, 58);",
        "lv_obj_set_size(deviceNetworkCard, 456, 215);",
        "lv_obj_set_pos(deviceNetworkCard, 286, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 58);",
        "lv_obj_set_pos(maintenanceCard, 8, 281);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT53R3-AIRPORT-LABELS"',
    ):
        require(ui, needle, "preserved System layout")

    # Airport map remains deterministic and beneath every aircraft layer.
    require(renderer, "bool labelBoxesOverlap(", "airport collision helper")
    require(renderer, "bool placeAirportLabel(", "deterministic airport placement")
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

    # Aircraft touch behavior remains ICAO-only.
    require(renderer, "bool hitTest(int canvasX, int canvasY, HitResult& result)",
            "aircraft hit test")
    hit_test = renderer[renderer.index("bool hitTest(int canvasX"):]
    assert "airport" not in hit_test.lower()

    require(main_cpp, "airport_data::initialize(settings::homeLatitude()",
            "optional airport initialization")
    require(main_cpp, "WARNING: Airport overlay unavailable; aircraft radar continuing",
            "optional airport failure behavior")
    print("Product 53R3 airport label integration checks passed")


if __name__ == "__main__":
    main()
