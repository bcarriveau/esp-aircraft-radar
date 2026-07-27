#!/usr/bin/env python3
"""Static integration checks for the Product 51 airport overlay."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    settings_h = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    assert "7IN-20260727-PRODUCT51-AIRPORT-UI" in build
    assert '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}' in ui
    assert "AIRPORTS // RADAR OVERLAY" in ui
    assert "SYSTEM // DEVICE & NETWORK" in ui
    assert "airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]" in ui
    assert "lv_obj_set_size(airportSaveButton, 158, 38);" in ui
    assert "lv_obj_set_style_pad_hor(airportSaveButton, 14, 0);" in ui
    assert "systemStatusCard = lv_obj_create(pagePanel);" in ui
    assert "deviceNetworkCard = lv_obj_create(pagePanel);" in ui
    assert "maintenanceCard = lv_obj_create(pagePanel);" in ui
    assert "RETRY ADS-B" in ui and "RECONNECT WI-FI" in ui
    assert "saveAirportSettings" in settings_h
    assert "cachedAirportSymbols" in settings_cpp
    assert "cachedAirportLabels" in settings_cpp

    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

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
    contact_labels = renderer[
        renderer.index("void drawContactLabels("):
        renderer.index("void updateSideIcon(")
    ]
    assert "drawAirportLabels(" not in contact_labels
    airport_labels = renderer[
        renderer.index("void drawAirportLabel("):
        renderer.index("uint8_t radarContactHeadingIndex")
    ]
    assert "airportLabelOverlapsContact" not in renderer
    assert "LabelBox" not in airport_labels
    assert "rectangle.border_width = 1;" in airport_labels
    assert "fixed range-specific positions" in airport_labels
    assert "bool hitTest(int canvasX, int canvasY, HitResult& result)" in renderer
    assert "airport_data::initialize(settings::homeLatitude()" in main_cpp
    assert "WARNING: Airport overlay unavailable; aircraft radar continuing" in main_cpp
    print("Airport overlay integration checks passed")


if __name__ == "__main__":
    main()
