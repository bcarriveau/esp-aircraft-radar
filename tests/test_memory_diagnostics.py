#!/usr/bin/env python3
"""Focused Product 63 Phase 1 internal-memory checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_product_63_memory_marker_and_system_diagnostics() -> None:
    build = read("include/build_info.h")
    header = read("include/app_state.h")
    state = read("src/app_state.cpp")
    ui = read("src/ui.cpp")

    assert "7IN-20260802-PRODUCT63-MEMORY-PHASE1" in build
    assert "lastFetchMinimumFreeHeap" in header
    assert "lastFetchMinimumLargestInternalBlock" in header
    assert "lastFetchMinimumBlockStage" in header
    assert "minimumAdsbTaskStackFreeBytes" in header
    assert 'const char* value = stage && stage[0] ? stage : "unlabelled";' in state
    assert "if (!state.fetchInProgress) return;" in state
    assert 'observeMemoryLocked("fetch-complete");' in state
    assert "state.fetchInProgress = false;" in state
    assert state.index('observeMemoryLocked("fetch-complete");') < state.index(
        "state.fetchInProgress = false;", state.index("void recordFetchSuccess")
    )

    assert '"FETCH LOW  %u / %u KB\\n"' in ui
    assert "lv_mem_monitor_t lvMemory{};" in ui
    assert "lv_mem_monitor(&lvMemory);" in ui
    assert '"LVGL FREE  %u KB  BIG %u KB  F%u%%\\n"' in ui
    assert '"ADSB STK   %u B FREE\\n"' in ui


def test_adsb_json_and_payload_are_psram_only() -> None:
    fetch = read("src/adsb_fetch.cpp")

    assert "class PsramAllocator final : public ArduinoJson::Allocator" in fetch
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in fetch
    assert "JsonDocument filter(&psramJsonAllocator);" in fetch
    assert "JsonDocument doc(&psramJsonAllocator);" in fetch
    assert "filter.overflowed()" in fetch

    allocate = fetch[
        fetch.index("uint8_t* allocatePayload") :
        fetch.index("class BundleVerifiedSecureClient")
    ]
    assert "heap_caps_malloc" in allocate
    assert "malloc(" not in allocate.replace("heap_caps_malloc", "")
    assert "capacity + 1" in allocate


def test_adsb_request_hot_path_uses_bounded_character_buffers() -> None:
    fetch = read("src/adsb_fetch.cpp")

    assert "char path[96];" in fetch
    assert "char url[128];" in fetch
    assert "pathLength < 0" in fetch
    assert "urlLength < 0" in fetch
    assert "String path =" not in fetch
    assert "String url =" not in fetch
    assert "fetchAttempt(const char* path" in fetch
    assert "fetchAttemptWithSecureClient(const char* path" in fetch


def test_memory_stage_attribution_covers_fetch_and_mqtt() -> None:
    fetch = read("src/adsb_fetch.cpp")
    network = read("src/adsb_network.cpp")
    mqtt = read("src/mqtt_service.cpp")

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

    assert "app_state::observeMemory(stage);" in fetch
    assert "app_state::observeMemory(stage);" in network
    assert "app_state::observeMemory(stage);" in mqtt
    assert 'logFetchMemory("task-before-fetch")' in network
    assert 'logFetchMemory("task-after-fetch")' in network
    assert "recordAdsbTaskStackFreeBytes" in network
    assert "uxTaskGetStackHighWaterMark(nullptr)" in network
    assert "MEM ADSB FETCH LOW" in network


def test_airport_directory_storage_is_psram_only_and_nonfatal() -> None:
    ui = read("src/ui.cpp")

    assert (
        "airport_data::NearbyAirport* airportDirectoryEntries = nullptr;" in ui
    )
    assert "heap_caps_calloc(AIRPORT_DIRECTORY_CAPACITY" in ui
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in ui
    assert (
        "Airport directory entries unavailable; directory page disabled" in ui
    )
    assert "FATAL: Airport directory entries PSRAM allocation failed" not in ui
    assert "PSRAM STORAGE UNAVAILABLE" in ui
    assert "if (!airportDirectoryEntries)" in ui
