#!/usr/bin/env python3
"""Focused source checks for Product 56-preserved Product 55 Airspace range and navigation behavior."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {context}: {needle}"


def section(text: str, start: str, end: str) -> str:
    start_index = text.index(start)
    end_index = text.index(end, start_index)
    return text[start_index:end_index]


def main() -> None:
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    radar_control = (ROOT / "src" / "radar_control.cpp").read_text(
        encoding="utf-8"
    )

    require(build, "7IN-20260801-PRODUCT56-R3-LIGHTWEIGHT-MQTT",
            "Product 56 build marker")

    # CURRENT RANGE is an intentionally green, clickable card with no helper copy.
    require(ui, '"TOTAL", "WITHIN 20 MI", "WITHIN 40 MI", "CURRENT RANGE"',
            "Airspace metric titles")
    require(ui, "lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);",
            "clickable range card")
    require(ui, "lv_obj_add_event_cb(card, airspaceRangeEvent, LV_EVENT_CLICKED",
            "range-card callback")
    require(ui, "lv_obj_set_style_bg_color(card, rgb(18, 92, 60), 0);",
            "green range card")
    assert "TAP TO CHANGE" not in ui

    # Radar and Airspace manual controls share the exact state-change path.
    manual_range = section(ui, "bool applyManualRangeIndex", "void rangeEvent")
    require(manual_range,
            "radar_control::setManualRangeMiles(RADAR_RANGES[index])",
            "shared non-LVGL range command")
    require(manual_range, "syncRangeControls(rangeMiles);",
            "shared Radar control synchronization")
    require(manual_range, "if (currentPage == 2) updatePageContent();",
            "immediate Airspace refresh")
    require(radar_control, "radar::clearAirportFocus();",
            "shared airport-focus clearing")
    require(radar_control, "app_state::setRadarRangeMiles(rangeMiles)",
            "shared range state update")
    require(radar_control, "adsb::requestRefresh();",
            "shared ADS-B refresh request")

    radar_range = section(ui, "void rangeEvent", "void airspaceRangeEvent")
    require(radar_range, "applyManualRangeIndex((uint8_t)index);",
            "Radar selector shared helper")
    airspace_range = section(ui, "void airspaceRangeEvent", "float projectedTrackedDistance")
    require(airspace_range,
            "const uint8_t nextIndex = (currentIndex + 1) % RANGE_OPTION_COUNT;",
            "20/40/80 cyclic range selection")
    require(airspace_range, "applyManualRangeIndex(nextIndex);",
            "Airspace shared helper")

    # Four independent rows replace the old monolithic summary label.
    require(ui, "AIRSPACE_HIGHLIGHT_COUNT = 4", "four highlights")
    require(ui,
            '"NEAREST", "FASTEST", "LOWEST AIRBORNE", "HIGHEST AIRBORNE"',
            "highlight names")
    require(ui, "airspaceHighlightRows[AIRSPACE_HIGHLIGHT_COUNT]",
            "independent highlight rows")
    require(ui, "airspaceHighlightValueLabels[AIRSPACE_HIGHLIGHT_COUNT]",
            "independent highlight values")
    assert "airspaceHighlightsLabel" not in ui
    assert '"DOMINANT\n' not in ui

    # Highest airborne is range-snapshot-derived and excludes invalid altitude.
    render = section(ui, "void renderAirspacePage", "void renderAirportsPage")
    require(render, "const aircraft::Target* highestTarget = nullptr;",
            "highest target accumulator")
    require(render, "target.altitudeFt > 0 &&",
            "airborne altitude validation")
    require(render, "target.altitudeFt > highestTarget->altitudeFt",
            "highest-airborne comparison")
    require(render, "setAirspaceHighlight(3, highestTarget, value);",
            "highest row population")

    # Tap identities are stable ICAO hex strings and are re-resolved before use.
    require(ui, "char airspaceHighlightHex[AIRSPACE_HIGHLIGHT_COUNT][7]{};",
            "stable bounded ICAO storage")
    setter = section(ui, "void setAirspaceHighlight", "void renderAirspacePage")
    require(setter, "strncpy(airspaceHighlightHex[index], target->hex",
            "ICAO copy")
    callback = section(ui, "void airspaceHighlightEvent", "void selectedInfoEvent")
    require(callback, "app_state::hasManualTracking()", "tracking guard")
    require(callback, "copyVisibleTargetByHex(airspaceHighlightHex[index], target)",
            "fresh ICAO lookup")
    require(callback, "selectAircraftHex(target.hex);", "normal Radar selection")
    require(callback, "selectPage(0);", "Radar navigation")
    assert "uiTargets[index]" not in callback


if __name__ == "__main__":
    main()
