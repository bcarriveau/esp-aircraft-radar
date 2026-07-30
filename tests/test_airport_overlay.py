#!/usr/bin/env python3
"""Static integration checks for Product 53R5 airport visibility eyes."""

from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {context}: {needle}"


def paeth(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    left_distance = abs(estimate - left)
    up_distance = abs(estimate - up)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= up_distance and left_distance <= upper_left_distance:
        return left
    if up_distance <= upper_left_distance:
        return up
    return upper_left


def read_png_alpha(path: Path) -> tuple[int, int, list[int]]:
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    width = height = 0
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk)
            )
            assert bit_depth == 8 and color_type == 6
            assert compression == 0 and filtering == 0 and interlace == 0
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            break

    row_bytes = width * 4
    raw = zlib.decompress(bytes(compressed))
    assert len(raw) == height * (row_bytes + 1)
    previous = bytearray(row_bytes)
    alpha: list[int] = []
    raw_offset = 0
    for _ in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        encoded = raw[raw_offset:raw_offset + row_bytes]
        raw_offset += row_bytes
        decoded = bytearray(row_bytes)
        for index, value in enumerate(encoded):
            left = decoded[index - 4] if index >= 4 else 0
            up = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                predictor = paeth(left, up, upper_left)
            else:
                raise AssertionError(f"unsupported PNG filter {filter_type}")
            decoded[index] = (value + predictor) & 0xFF
        alpha.extend(decoded[3::4])
        previous = decoded
    return width, height, alpha


def main() -> None:
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")
    renderer_h = (ROOT / "include" / "radar_renderer.h").read_text(encoding="utf-8")
    settings_h = (ROOT / "include" / "settings.h").read_text(encoding="utf-8")
    status_bitmaps = (ROOT / "include" / "airport_status_bitmaps.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src" / "settings.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "radar_renderer.cpp").read_text(encoding="utf-8")
    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    require(build, "7IN-20260729-PRODUCT53R5-AIRPORT-EYE",
            "Product 53R5 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // NEARBY", "airport directory title")
    require(ui, "AIRPORTS // DISPLAY OPTIONS", "airport options title")
    require(ui, "SYSTEM // STATUS & SETTINGS", "System page")

    # The existing directory is updated, not redesigned.
    require(ui, "AIRPORT_DIRECTORY_CAPACITY = 64", "bounded 64-row directory")
    require(ui, "airportDirectoryTable = lv_table_create",
            "airport directory table")
    require(ui, '"ID", "AIRPORT", "TYPE", "DIST", "RUNWAY", "LABEL"',
            "airport column order with label control")
    require(ui, "LV_TABLE_CELL_CTRL_TEXT_CROP",
            "single-line airport table cells")
    require(ui,
            "lv_obj_set_style_pad_ver(airportDirectoryTable, 7, LV_PART_ITEMS);",
            "preserved readable row padding")
    for needle in (
        "lv_table_set_col_width(airportDirectoryTable, 0, 62);",
        "lv_table_set_col_width(airportDirectoryTable, 1, 304);",
        "lv_table_set_col_width(airportDirectoryTable, 2, 80);",
        "lv_table_set_col_width(airportDirectoryTable, 3, 72);",
        "lv_table_set_col_width(airportDirectoryTable, 4, 104);",
        "lv_table_set_col_width(airportDirectoryTable, 5, 92);",
    ):
        require(ui, needle, "airport directory column sizing")
    assert '"BRG"' not in ui[ui.index("const char* airportHeaders"):ui.index(
        "airportDirectoryTable = lv_table_create")], "BRG column must be removed"

    # Stable per-airport overrides are persisted by identifier.
    require(settings_h, "enum class AirportLabelMode", "override mode enum")
    require(settings_h, "AUTO = 0", "AUTO mode")
    require(settings_h, "SHOW = 1", "SHOW mode")
    require(settings_h, "HIDE = 2", "HIDE mode")
    require(settings_h, "AIRPORT_LABEL_OVERRIDE_CAPACITY = 64",
            "bounded override capacity")
    require(settings_cpp, 'KEY_AIRPORT_OVERRIDES = "apt_ovr"',
            "NVS override key")
    require(settings_cpp, "StoredAirportLabelOverrides",
            "bounded override blob")
    require(settings_cpp, "airportOverrideLowerBound",
            "deterministic sorted lookup")
    require(settings_cpp, "writeBytesChecked(KEY_AIRPORT_OVERRIDES",
            "verified override write")
    require(settings_cpp, "setAirportLabelMode(const char* ident",
            "identifier-based override update")
    require(settings_cpp, "clearAirportOverrideCache();",
            "Reset Defaults clears manual overrides")

    # LABEL cells cycle AUTO -> SHOW -> HIDE -> AUTO; other cells retain profile.
    require(ui, "if (column != 5)", "profile action outside LABEL column")
    require(ui, "AirportLabelMode::SHOW", "SHOW cycle state")
    require(ui, "AirportLabelMode::HIDE", "HIDE cycle state")
    require(ui, "AirportLabelMode::AUTO", "AUTO cycle state")
    require(ui, "airportDirectoryTableDrawEvent", "colored LABEL cells")
    require(ui, "LV_EVENT_DRAW_PART_BEGIN", "LABEL cell style hook")
    require(ui, "airportDirectoryTableEyeDrawEvent",
            "current radar-label eye hook")
    require(ui, "LV_EVENT_DRAW_PART_END", "eye draw after table text")
    require(ui, "airportDirectoryLabelVisible", "per-row visible state")
    require(ui, "AIRPORT_LABEL_EYE_ALPHA", "eye alpha asset draw")
    require(ui, "SHOW %u  HIDE %u", "manual override summary")

    # The renderer publishes the exact stable identifiers drawn in its latest
    # completed airport-label pass. The UI only shows eyes for a matching,
    # current range/mask result.
    require(renderer_h, "bool airportLabelVisible(", "label visibility API")
    require(renderer, "AirportIdent* airportVisibleLabelIdents",
            "bounded visible-ident buffer")
    require(renderer, "Radar airport label-status buffer in PSRAM",
            "PSRAM visibility allocation")
    require(renderer, "airportVisibleLabelIdents[airportLabelStatsCount]",
            "successful-label identifier capture")
    require(renderer, "strncmp(airportVisibleLabelIdents[index], ident",
            "stable identifier visibility lookup")
    require(ui, "airportDirectoryLabelVisibilityCurrent = labelCountCurrent",
            "current frame signature gate")
    require(ui, "radar::airportLabelVisible(",
            "directory visibility lookup")
    require(status_bitmaps, "AIRPORT_LABEL_EYE_W = 14",
            "eye bitmap width")
    require(status_bitmaps, "AIRPORT_LABEL_EYE_H = 9",
            "eye bitmap height")
    require(status_bitmaps, "AIRPORT_LABEL_EYE_ALPHA",
            "eye alpha mask")
    eye_asset = ROOT / "assets" / "airport_label_eye_14x9.png"
    assert eye_asset.is_file(), "eye source PNG must live in assets"
    width, height, source_alpha = read_png_alpha(eye_asset)
    assert (width, height) == (14, 9)
    header_alpha = [int(value, 16) for value in
                    re.findall(r"0x[0-9A-F]{2}", status_bitmaps)]
    assert header_alpha == source_alpha, (
        "compiled eye alpha mask differs from source PNG")

    # Directory lists all nearby categories so disabled private/heliports can be
    # individually promoted with SHOW.
    directory = ui[ui.index("void updateAirportDirectory()"):
                   ui.index("void showAirportDirectory()")]
    require(directory, "airport_data::CATEGORY_MASK_ALL",
            "all-category nearby directory")
    assert "bearingDegrees" not in directory, "bearing column remains profile-only"

    # Renderer placement order: manual SHOW first, then category-priority AUTO.
    require(renderer, "hasManualShow", "manual SHOW frame inclusion")
    require(renderer, "airport_data::CATEGORY_MASK_ALL",
            "manual categories copied into bounded frame")
    draw_labels = renderer[renderer.index("void drawAirportLabels("):
                           renderer.index("uint8_t radarContactHeadingIndex")]
    show_pos = draw_labels.index("AirportLabelMode::SHOW")
    auto_loop_pos = draw_labels.index(
        "for (uint8_t category = 0; category < airport_data::CATEGORY_COUNT")
    assert show_pos < auto_loop_pos, "SHOW entries must be attempted first"
    require(draw_labels, "AirportLabelMode::AUTO", "AUTO-only category pass")
    require(draw_labels, "++airportLabelStatsCount", "actual labels-drawn count")
    assert "maximumLabels" not in renderer, "small range quotas must remain removed"

    # Defaults remain major/public on, private/heliport off.
    require(settings_cpp,
            "constexpr uint8_t DEFAULT_AIRPORT_LABELS[AIRPORT_RANGE_COUNT] = {\n"
            "  0x03, 0x03, 0x03\n};",
            "major/public label defaults")
    require(settings_cpp,
            "constexpr uint8_t DEFAULT_AIRPORT_SYMBOLS[AIRPORT_RANGE_COUNT] = {\n"
            "  0x03, 0x03, 0x03\n};",
            "major/public symbol defaults")

    # Existing page/subview and System layouts remain unchanged.
    require(ui, 'lv_label_set_text(airportOptionsLabel, "DISPLAY SETTINGS")',
            "display-settings action")
    require(ui, 'lv_label_set_text(airportBackLabel, "BACK TO AIRPORTS")',
            "airport options back action")
    require(ui, "AIRPORTS // AIRPORT PROFILE", "airport detail title")
    for needle in (
        "lv_obj_set_size(systemStatusCard, 270, 215);",
        "lv_obj_set_pos(systemStatusCard, 8, 58);",
        "lv_obj_set_size(deviceNetworkCard, 456, 215);",
        "lv_obj_set_pos(deviceNetworkCard, 286, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 58);",
        "lv_obj_set_pos(maintenanceCard, 8, 281);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT53R5-AIRPORT-EYE"',
    ):
        require(ui, needle, "preserved System layout")

    # Airport map remains beneath all aircraft layers and aircraft hit testing is
    # still ICAO-only.
    render_body = renderer[renderer.index("bool render("):]
    assert render_body.index("drawAirportLabels(rangeMiles);") < render_body.index(
        "drawAirportSymbols(rangeMiles);")
    assert render_body.index("drawAirportSymbols(rangeMiles);") < render_body.index(
        "drawContacts(workTargets")
    assert render_body.index("drawContacts(workTargets") < render_body.index(
        "drawContactLabels(workTargets")
    hit_test = renderer[renderer.index("bool hitTest(int canvasX"):]
    assert "airport" not in hit_test.lower()

    require(renderer_h, "bool airportLabelCount(", "label-count API")
    require(main_cpp, "airport_data::initialize(settings::homeLatitude()",
            "optional airport initialization")
    print("Product 53R5 airport visibility-eye integration checks passed")


if __name__ == "__main__":
    main()
