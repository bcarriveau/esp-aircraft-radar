#!/usr/bin/env python3
"""Focused Product 56 R1 internal-memory diagnostic checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_product_56_r1_tracks_contiguous_internal_memory() -> None:
    build = read("include/build_info.h")
    header = read("include/app_state.h")
    state = read("src/app_state.cpp")
    ui = read("src/ui.cpp")

    assert "7IN-20260801-PRODUCT56-R1-MEMORY" in build
    assert "minimumLargestInternalBlock" in header
    assert "heap_caps_get_largest_free_block" in state
    assert "MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT" in state
    assert '"BLOCK      %u KB  MIN %u KB\\n"' in ui
    assert "diagnostics.minimumLargestInternalBlock" in ui
    assert "lv_obj_set_pos(systemFirmwareButton, 3, 182);" in ui


def test_adsb_memory_checkpoints_cover_transport_and_parse() -> None:
    fetch = read("src/adsb_fetch.cpp")
    network = read("src/adsb_network.cpp")

    for stage in (
        "native-start",
        "after-dns",
        "client-init",
        "tls-connected",
        "headers-complete",
        "payload-ready",
        "transport-released",
        "json-complete",
        "fallback-start",
        "fallback-tls",
        "fallback-headers",
        "fallback-payload",
        "fallback-release",
        "fallback-json",
    ):
        assert f'logMemoryStage("{stage}")' in fetch

    assert 'logFetchMemory("task-before-fetch")' in network
    assert 'logFetchMemory("task-after-fetch")' in network
    assert "app_state::observeMemory();" in fetch
    assert "app_state::observeMemory();" in network


def test_mqtt_memory_checkpoints_cover_lifecycle() -> None:
    mqtt = read("src/mqtt_service.cpp")

    for stage in (
        "start-begin",
        "psram-ready",
        "client-init",
        "client-started",
        "broker-connected",
        "discovery-complete",
        "destroy-begin",
        "destroy-complete",
    ):
        assert f'logMemoryStage("{stage}")' in mqtt

    # Diagnostics only: retain the existing resource bounds and service behavior.
    assert "mqttConfig.task.stack_size = 6144" in mqtt
    assert "mqttConfig.buffer.size = MQTT_BUFFER_BYTES" in mqtt
    assert "mqttConfig.outbox.limit = MQTT_OUTBOX_BYTES" in mqtt
