#!/usr/bin/env python3
"""Product 67 bounded cross-core radar-gap attribution checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_product67_gap_categories_and_ring_are_bounded():
    header = (ROOT / "include/app_state.h").read_text(encoding="utf-8")
    source = (ROOT / "src/app_state.cpp").read_text(encoding="utf-8")
    assert "7IN-20260802-PRODUCT68-FETCH-CONTENTION" in (
        ROOT / "include/build_info.h").read_text(encoding="utf-8")
    assert "enum class ActivityStage" in header
    for name in ("IDLE", "DNS", "TLS_HANDSHAKE", "RESPONSE_BODY",
                 "JSON_DESERIALIZE", "JSON_EXTRACT", "PUBLISH", "RADAR_CACHE", "MQTT", "DIAGNOSTICS", "OTHER", "COUNT"):
        assert name in header
    assert "ACTIVITY_WINDOW_CAPACITY = 16" in source
    assert "ActivityWindow activityWindows[ACTIVITY_WINDOW_CAPACITY]" in source
    assert "activityWindowCount < ACTIVITY_WINDOW_CAPACITY" in source


def test_fetch_labels_map_to_expected_runtime_windows():
    source = (ROOT / "src/app_state.cpp").read_text(encoding="utf-8")
    expected = {
        '"native-start"': "ActivityStage::DNS",
        '"tls-handshake"': "ActivityStage::TLS_HANDSHAKE",
        '"fallback-start"': "ActivityStage::TLS_HANDSHAKE",
        '"payload-ready"': "ActivityStage::RESPONSE_BODY",
        '"fallback-payload"': "ActivityStage::RESPONSE_BODY",
        '"transport-released"': "ActivityStage::JSON_DESERIALIZE",
        '"fallback-release"': "ActivityStage::JSON_DESERIALIZE",
        '"json-extract"': "ActivityStage::JSON_EXTRACT",
    }
    for label, stage in expected.items():
        assert label in source
        assert stage in source


def test_frame_gap_stats_include_stage_specific_maxima():
    header = (ROOT / "include/radar_renderer.h").read_text(encoding="utf-8")
    source = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src/ui.cpp").read_text(encoding="utf-8")
    assert "maximumFrameGapByStage" in header
    assert "maximumFrameGapStage" in header
    assert "dominantActivityStage(lastRenderStartedMs" in source
    assert "RADAR GAP MAX idle=" in source
    assert "mqtt=%lu diag=%lu other=%lu ms" in source
    assert "G%lu %s" in ui


def test_product67_is_diagnostic_only_for_network_and_display_contracts():
    network_h = (ROOT / "include/adsb_network.h").read_text(encoding="utf-8")
    network = (ROOT / "src/adsb_network.cpp").read_text(encoding="utf-8")
    lv_conf = (ROOT / "include/lv_conf.h").read_text(encoding="utf-8")
    panel = (ROOT / "include/waveshare_panel_board.h").read_text(encoding="utf-8")
    assert "FETCH_INTERVAL_MS = 15000" in network_h
    assert "ADSB_TASK_STACK_BYTES = 12U * 1024U" in network
    assert "#define LV_MEM_SIZE (128U * 1024U)" in lv_conf
    assert "ESP_PANEL_LCD_WIDTH * 20" in panel
