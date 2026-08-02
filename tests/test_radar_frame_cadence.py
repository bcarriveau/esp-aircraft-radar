#!/usr/bin/env python3
"""Focused Product 66 bounded radar dirty-region regression checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_product66_marker_and_scope():
    build = (ROOT / "include/build_info.h").read_text(encoding="utf-8")
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    renderer_h = (ROOT / "include/radar_renderer.h").read_text(encoding="utf-8")
    ui = (ROOT / "src/ui.cpp").read_text(encoding="utf-8")
    lv_conf = (ROOT / "include/lv_conf.h").read_text(encoding="utf-8")
    panel = (ROOT / "include/waveshare_panel_board.h").read_text(encoding="utf-8")

    assert "7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS" in build
    assert "SWEEP_DEGREES_PER_SECOND = 27.5f" in renderer
    assert "elapsedMs * (SWEEP_DEGREES_PER_SECOND / 1000.0f)" in renderer
    assert "sweepDegrees + 2.2f" not in renderer

    assert "Radar static base cache in PSRAM" in renderer
    assert "Radar dirty-region buffer in PSRAM" in renderer
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in renderer
    assert "restorePreviousDynamicLayer(sweepDegrees)" in renderer
    assert "rebuildStaticRadarLayer(rangeMiles, snapshot.rangeGeneration)" in renderer
    assert "invalidateStaticRadarLayer();" in renderer

    assert "struct PerformanceStats" in renderer_h
    assert "copyPerformanceStats" in renderer_h
    assert '"RADAR      %lu / %lu ms  GAP %lu ms\\n"' in ui
    assert "RADAR PERF render=" in renderer
    assert "RADAR CACHE range=" in renderer

    assert "#define LV_MEM_SIZE (128U * 1024U)" in lv_conf
    assert "ESP_PANEL_LCD_WIDTH * 20" in panel


def test_dense_frames_use_bounded_merged_dirty_regions():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    assert "DIRTY_REGION_CAPACITY" in renderer
    assert "aircraft::MAX_TARGETS * 2U + 4U" in renderer
    assert "bool dirtyRectsTouch" in renderer
    assert "bool addDirtyRegion" in renderer
    assert "dirtyRegions[index] = dirtyRegions[--dirtyRegionCount]" in renderer
    assert "for (uint16_t index = 0; index < dirtyRegionCount; ++index)" in renderer
    assert "DIRTY_RESTORE_CONTACT_LIMIT" not in renderer
    assert "contactFrame.count <=" not in renderer
    assert "Dense 40/80-mile frames" not in renderer


def test_dirty_restore_bounds_cover_dynamic_contacts_and_tags():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    assert "BITMAP_CONTACT_CLEAR_RADIUS = 14" in renderer
    assert "DOT_CONTACT_CLEAR_RADIUS = 6" in renderer
    assert "contact.x - clearRadius" in renderer
    assert "hit.tag.x1 - 1" in renderer
    assert "restoreLineFromBase" in renderer
    assert "restoreRectFromBase" in renderer
    assert "currentRestoreBytes" in renderer
    assert "fullCacheFallbacks" in renderer


def test_target_snapshot_copy_is_version_gated():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    render = renderer[renderer.index("bool render("):renderer.index("void copyPerformanceStats")]
    assert "app_state::targetVersion()" in render
    assert "app_state::rangeGeneration()" in render
    assert "app_state::trackingVersion()" in render
    assert "cachedRadarSnapshotValid" in render
    assert render.count("app_state::copySnapshot(workTargets, cachedRadarSnapshot)") == 1
    assert "++performanceStats.snapshotCopies" in render
    assert "if (renderedSnapshot) *renderedSnapshot = snapshot" in render
    ui = (ROOT / "src/ui.cpp").read_text(encoding="utf-8")
    auto_zoom = ui[ui.index("void autoExpandTrackedRange"):ui.index("void showTargetDetails")]
    assert "app_state::copySnapshot" not in auto_zoom
    assert "radar::render(uiTargets, selectedHex, &snapshot)" in ui


def test_static_airport_layer_invalidation_paths():
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    focus = renderer[renderer.index("void focusAirport"):renderer.index("bool airportLabelCount")]
    assert focus.count("invalidateStaticRadarLayer();") >= 2
    invalidator = renderer[renderer.index("void invalidateAirportLabelCount"):renderer.index("void drawAircraftPreview")]
    assert "invalidateStaticRadarLayer();" in invalidator
