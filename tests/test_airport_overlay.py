#!/usr/bin/env python3
"""Static integration checks for the Product 52 airport/UI refinement."""

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

    require(build, "7IN-20260727-PRODUCT52-UI-POLISH", "Product 52 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // RADAR OVERLAY", "Airports page")
    require(ui, "SYSTEM // DEVICE & NETWORK", "System page")

    # Airport settings remain bounded, range-specific, and RAM-cached.
    require(ui, "airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]",
            "airport range matrix")
    require(ui, "lv_obj_set_size(airportSaveButton, 170, 34);",
            "padded airport save button size")
    require(ui, "lv_obj_set_style_pad_hor(airportSaveButton, 16, 0);",
            "airport save horizontal padding")
    require(ui, "lv_obj_set_style_pad_ver(airportSaveButton, 7, 0);",
            "airport save vertical padding")
    require(settings_h, "saveAirportSettings", "airport NVS API")
    require(settings_cpp, "cachedAirportSymbols", "RAM-cached symbol settings")
    require(settings_cpp, "cachedAirportLabels", "RAM-cached label settings")
    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

    # System cards must fit the 742x280 page workspace without the Product 51
    # clipped build marker or maintenance buttons.
    for needle in (
        "lv_obj_set_size(systemStatusCard, 260, 194);",
        "lv_obj_set_pos(systemStatusCard, 8, 58);",
        "lv_obj_set_size(deviceNetworkCard, 466, 194);",
        "lv_obj_set_pos(deviceNetworkCard, 276, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 72);",
        "lv_obj_set_pos(maintenanceCard, 8, 258);",
        "lv_obj_set_size(retryButton, 218, 32);",
        "lv_obj_set_pos(retryButton, 14, 21);",
        "lv_obj_set_pos(reconnectButton, 250, 21);",
        "lv_obj_set_pos(resetSettingsButton, 486, 21);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT52-UI-POLISH"',
    ):
        require(ui, needle, "System layout")

    # Airport labels are resolved only against the static map layer. Aircraft
    # are still drawn later and therefore retain visual and touch priority.
    require(renderer, "bool labelBoxesOverlap(", "airport collision helper")
    require(renderer, "bool placeAirportLabel(", "deterministic airport placement")
    require(renderer, "LabelBox placedLabels[AIRPORT_LABEL_CAPACITY]", "bounded label boxes")
    require(renderer, "same placement every", "stable map-layer comment")
    require(renderer, "rectangle.bg_opa = static_cast<lv_opa_t>(96);",
            "subdued airport background")
    require(renderer, "rectangle.border_opa = static_cast<lv_opa_t>(115);",
            "subdued airport border")
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

    airport_section = renderer[
        renderer.index("bool labelBoxesOverlap("):
        renderer.index("uint8_t radarContactHeadingIndex")
    ]
    assert "renderedHits" not in airport_section
    assert "selected" not in airport_section
    assert "tracked" not in airport_section

    # Aircraft touch behavior remains the existing ICAO-only hit test.
    require(renderer, "bool hitTest(int canvasX, int canvasY, HitResult& result)",
            "aircraft hit test")
    hit_test = renderer[renderer.index("bool hitTest(int canvasX"):]
    assert "airport" not in hit_test.lower()

    require(main_cpp, "airport_data::initialize(settings::homeLatitude()",
            "optional airport initialization")
    require(main_cpp, "WARNING: Airport overlay unavailable; aircraft radar continuing",
            "optional airport failure behavior")
    print("Product 52 airport and System UI integration checks passed")


if __name__ == "__main__":
    main()
