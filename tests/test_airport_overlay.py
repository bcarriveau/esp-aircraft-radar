#!/usr/bin/env python3
"""Static integration checks for the Product 50 airport overlay."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    settings_h = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    assert "7IN-20260727-PRODUCT50-AIRPORT-OVERLAY" in build
    assert '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}' in ui
    assert "AIRPORTS // RADAR OVERLAY" in ui
    assert "SYSTEM // DEVICE & NETWORK" in ui
    assert "airportToggleButtons[AIRPORT_CATEGORY_COUNT][AIRPORT_RANGE_COUNT]" in ui
    assert "saveAirportSettings" in settings_h
    assert "cachedAirportSymbols" in settings_cpp
    assert "cachedAirportLabels" in settings_cpp

    getters = settings_cpp[
        settings_cpp.index("bool airportsEnabled()"):
        settings_cpp.index("bool saveAirportSettings")
    ]
    assert "preferences." not in getters, "renderer-facing getters must use RAM cache"

    render_body = renderer[renderer.index("bool render("):]
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
    assert contact_labels.rindex("drawAirportLabels(") > contact_labels.index(
        "if (rangeMiles <= 20.1f)"
    )
    assert "bool hitTest(int canvasX, int canvasY, HitResult& result)" in renderer
    assert "airport_data::initialize(settings::homeLatitude()" in main_cpp
    assert "WARNING: Airport overlay unavailable; aircraft radar continuing" in main_cpp
    print("Airport overlay integration checks passed")


if __name__ == "__main__":
    main()
