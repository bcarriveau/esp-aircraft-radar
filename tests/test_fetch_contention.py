#!/usr/bin/env python3
"""Product 68 fetch-contention and quiet-logging checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_product68_marker_and_compile_time_quiet_policy():
    assert "7IN-20260802-PRODUCT68-FETCH-CONTENTION" in read(
        "include/build_info.h")
    policy = read("include/adsb_diagnostics.h")
    fetch = read("src/adsb_fetch.cpp")
    network = read("src/adsb_network.cpp")
    assert "#define ADSB_VERBOSE_FETCH_LOGGING 0" in policy
    assert "-DADSB_VERBOSE_FETCH_LOGGING=1" in policy
    assert "#if ADSB_VERBOSE_FETCH_LOGGING" in fetch
    assert "#if ADSB_VERBOSE_FETCH_LOGGING" in network
    assert "ADSB %s accepted=" in network
    assert "MEM ADSB %-18s" in network


def test_json_phases_are_measured_and_extraction_yields_are_bounded():
    header = read("include/adsb_fetch.h")
    fetch = read("src/adsb_fetch.cpp")
    policy = read("include/adsb_diagnostics.h")
    for field in ("jsonDeserializeUs", "jsonExtractUs",
                  "verboseDiagnosticUs", "extractionYieldCount"):
        assert field in header
    assert "EXTRACTION_YIELD_INTERVAL = 16" in policy
    assert "if (++processedSinceYield >=" in fetch
    assert "delay(1);" in fetch
    assert 'logMemoryStage("json-extract")' in fetch
    assert 'logMemoryStage("json-complete")' in fetch
    assert "result.jsonDeserializeUs = attemptResult.jsonDeserializeUs;" in fetch
    assert "result.jsonExtractUs = micros() - extractStartedUs;" in fetch
    assert fetch.index('logMemoryStage("json-extract")') < fetch.index(
        "parseAircraft(doc")
    assert fetch.index("parseAircraft(doc") < fetch.index(
        'logMemoryStage("json-complete")')


def test_mqtt_and_diagnostic_activity_are_separate_gap_categories():
    header = read("include/app_state.h")
    state = read("src/app_state.cpp")
    mqtt = read("src/mqtt_service.cpp")
    radar = read("src/radar_renderer.cpp")
    for stage in ("JSON_DESERIALIZE", "JSON_EXTRACT", "MQTT",
                  "DIAGNOSTICS"):
        assert stage in header
    for name in ('return "json-d"', 'return "json-x"', 'return "mqtt"',
                 'return "diag"'):
        assert name in state
    assert "ActivityWindowScope activityWindow(" in mqtt
    assert "MQTT_ACTIVITY_MIN_MS = 2" in mqtt
    assert "endedMs - startedMs_ >= MQTT_ACTIVITY_MIN_MS" in mqtt
    assert "app_state::ActivityStage::MQTT" in mqtt
    assert "ActivityStage::DIAGNOSTICS" in read("src/adsb_network.cpp")
    assert "jsond=%lu" in radar and "jsonx=%lu" in radar
    assert "mqtt=%lu" in radar and "diag=%lu" in radar


def test_product68_does_not_change_core_contracts():
    network_h = read("include/adsb_network.h")
    network = read("src/adsb_network.cpp")
    lv_conf = read("include/lv_conf.h")
    panel = read("include/waveshare_panel_board.h")
    assert "FETCH_INTERVAL_MS = 15000" in network_h
    assert "ADSB_TASK_STACK_BYTES = 12U * 1024U" in network
    assert "#define LV_MEM_SIZE (128U * 1024U)" in lv_conf
    assert "ESP_PANEL_LCD_WIDTH * 20" in panel
