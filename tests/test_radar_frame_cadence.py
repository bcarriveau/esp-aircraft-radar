#!/usr/bin/env python3
"""Focused Product 65 radar-frame cadence regression checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_product65_marker_and_scope():
    build = (ROOT / "include/build_info.h").read_text(encoding="utf-8")
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    renderer_h = (ROOT / "include/radar_renderer.h").read_text(encoding="utf-8")
    ui = (ROOT / "src/ui.cpp").read_text(encoding="utf-8")
    lv_conf = (ROOT / "include/lv_conf.h").read_text(encoding="utf-8")
    panel = (ROOT / "include/waveshare_panel_board.h").read_text(encoding="utf-8")

    assert "7IN-20260802-PRODUCT65-RADAR-FRAME-CADENCE" in build
    assert "SWEEP_DEGREES_PER_SECOND = 27.5f" in renderer
    assert "elapsedMs * (SWEEP_DEGREES_PER_SECOND / 1000.0f)" in renderer
    assert "sweepDegrees + 2.2f" not in renderer

    assert "Radar static base cache in PSRAM" in renderer
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in renderer
    assert "Radar static base cache unavailable: using full-frame fallback" in renderer
    assert "restorePreviousDynamicLayer(sweepDegrees)" in renderer
    assert "rebuildStaticRadarLayer(rangeMiles, snapshot.rangeGeneration)" in renderer
    assert "invalidateStaticRadarLayer();" in renderer

    assert "struct PerformanceStats" in renderer_h
    assert "copyPerformanceStats" in renderer_h
    assert '"RADAR      %lu / %lu ms  GAP %lu ms\\n"' in ui

    assert "#define LV_MEM_SIZE (128U * 1024U)" in lv_conf
    assert "ESP_PANEL_LCD_WIDTH * 20" in panel


def test_dirty_restore_bounds_cover_dynamic_contacts_and_tags():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    assert "BITMAP_CONTACT_CLEAR_RADIUS = 14" in renderer
    assert "DOT_CONTACT_CLEAR_RADIUS = 6" in renderer
    assert "DIRTY_RESTORE_CONTACT_LIMIT = 64" in renderer
    assert "contact.x - clearRadius" in renderer
    assert "hit.tag.x1 - 1" in renderer
    assert "restoreLineFromBase" in renderer
    assert "restoreRectFromBase" in renderer


def test_static_airport_layer_invalidation_paths():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    focus = renderer[renderer.index("void focusAirport"):renderer.index("bool airportLabelCount")]
    assert focus.count("invalidateStaticRadarLayer();") >= 2
    invalidator = renderer[renderer.index("void invalidateAirportLabelCount"):renderer.index("void drawAircraftPreview")]
    assert "invalidateStaticRadarLayer();" in invalidator
