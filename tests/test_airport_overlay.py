#!/usr/bin/env python3
"""Static integration checks for the amended Product 53R6R1 airport hotfix."""

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

    require(build, "7IN-20260731-PRODUCT53R6R1-AIRPORT-TAP-FIX",
            "Product 53R6R1 marker")
    require(ui, '{"RADAR", "TRACKS", "AIRSPACE", "AIRPORTS", "SYSTEM"}',
            "five-tab navigation")
    require(ui, "AIRPORTS // NEARBY", "airport directory title")
    require(ui, "AIRPORTS // DISPLAY OPTIONS", "airport options title")
    require(ui, "SYSTEM // STATUS & SETTINGS", "System page")

    # The existing directory is updated, not redesigned.
    require(ui, "AIRPORT_DIRECTORY_CAPACITY = 64", "bounded 64-row directory")
    require(ui, "airportDirectoryTable = lv_table_create",
            "airport directory table")
    require(ui, '"ID", "AIRPORT", "TYPE", "DIST", "RUNWAY"',
            "preserved airport data columns")
    require(ui, 'makeLabel(airportTableHeader, "LABEL"',
            "label header beside edit lock")
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

    # LABEL editing is explicitly unlocked; ordinary taps open Airport Profile.
    require(ui, "airportLabelEditMode", "directory edit-lock state")
    require(ui, 'airportLabelEditMode ? "DONE" : "EDIT"',
            "EDIT/DONE lock control")
    require(ui, "if (column != 5 || !airportLabelEditMode)",
            "locked LABEL cells open profile without changing preference")
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

    # LVGL 8.3 clears the table's selected row/column during RELEASED before
    # CLICKED is sent. Handle its no-scroll VALUE_CHANGED event while the selected
    # cell is still valid, but retain the independent movement/time/scroll gates.
    require(ui, "AIRPORT_TABLE_TAP_MOVE_LIMIT = 10", "tap movement threshold")
    require(ui, "AIRPORT_TABLE_TAP_MAX_MS = 900", "tap duration threshold")
    require(ui, "LV_EVENT_SCROLL_BEGIN", "scroll cancels airport tap")
    require(ui, "LV_EVENT_PRESS_LOST", "lost press cancels airport tap")
    require(ui, "if (code != LV_EVENT_VALUE_CHANGED) return;",
            "selected-cell action before LVGL clears it")
    table_registration = ui[ui.index(
        "airportDirectoryTable = lv_table_create"):ui.index(
        "airportOptionsView = lv_obj_create")]
    require(table_registration, "LV_EVENT_VALUE_CHANGED",
            "gated table value-change callback")
    assert "LV_EVENT_CLICKED" not in table_registration, (
        "airport table must not wait until LVGL has cleared the selected cell")
    assert "lv_indev_get_scroll_obj(input) == nullptr" not in ui, (
        "redundant late scroll-object check must not reject valid taps")

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

    # Airport Profile can temporarily focus one stable identifier on Radar.
    require(renderer_h, "void focusAirport(const char* ident",
            "temporary airport-focus API")
    require(renderer_h, "void clearAirportFocus();",
            "airport-focus clear API")
    require(ui, 'setLabelTextIfChanged(airportDetailShowLabel, "SHOW ON RADAR")',
            "Airport Profile radar action")
    require(ui, "distanceMiles <= 18.0f", "20-mile focus margin")
    require(ui, "distanceMiles <= 36.0f", "40-mile focus margin")
    require(ui, "AIRPORT_FOCUS_DURATION_MS = 15000",
            "bounded focus duration")
    require(ui, "radar::focusAirport(airportDetailAirport.ident",
            "stable identifier focus request")
    require(ui, "app_state::setRadarRangeMiles(focusRange)",
            "automatic 20/40/80 focus range")
    require(ui, "radar::clearAirportFocus();", "focus cleanup")
    radar_event = ui[ui.index("void radarCanvasEvent("):
                     ui.index("void detailBackEvent(")]
    require(radar_event, "radar::clearAirportFocus();",
            "radar-canvas airport-focus dismissal")
    assert radar_event.index("radar::clearAirportFocus();") < \
        radar_event.index("radar::hitTest("), (
            "airport focus must clear before aircraft hit testing so both empty "
            "and aircraft-contact taps dismiss it")
    require(renderer, "AirportIdent airportFocusIdent", "bounded focus state")
    require(renderer, "airportFocusExpiresAt", "focus timeout state")
    require(renderer, "focused ? rgb(255, 190, 70)",
            "amber focused airport symbol")
    require(renderer, "drawAirportLabel(airport, placement, true)",
            "amber focused airport label")
    focus_pos = renderer.index("// A temporary Airport Profile focus")
    show_pos = renderer.index("// Manual SHOW entries are placed next")
    auto_pos = renderer.index("// AUTO entries retain deterministic")
    assert focus_pos < show_pos < auto_pos, (
        "focused airport must be placed before SHOW and AUTO labels")

    # Renderer placement order: temporary focus, manual SHOW, then AUTO.
    require(renderer, "hasManualShow", "manual SHOW frame inclusion")
    require(renderer, "airport_data::CATEGORY_MASK_ALL",
            "manual categories copied into bounded frame")
    draw_labels = renderer[renderer.index("void drawAirportLabels("):
                           renderer.index("uint8_t radarContactHeadingIndex")]
    manual_show_pos = draw_labels.index("AirportLabelMode::SHOW")
    auto_loop_pos = draw_labels.index(
        "for (uint8_t category = 0; category < airport_data::CATEGORY_COUNT")
    assert manual_show_pos < auto_loop_pos, "SHOW entries must precede AUTO"
    require(draw_labels, "AirportLabelMode::AUTO", "AUTO-only category pass")
    require(renderer, "++airportLabelStatsCount", "actual labels-drawn count")
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
    require(ui, "lv_obj_set_pos(airportDetailShowButton, 558, 46);",
            "focused profile action below Back")
    for needle in (
        "lv_obj_set_size(systemStatusCard, 270, 215);",
        "lv_obj_set_pos(systemStatusCard, 8, 58);",
        "lv_obj_set_size(deviceNetworkCard, 456, 215);",
        "lv_obj_set_pos(deviceNetworkCard, 286, 58);",
        "lv_obj_set_size(maintenanceCard, 734, 58);",
        "lv_obj_set_pos(maintenanceCard, 8, 281);",
        'SYSTEM_BUILD_TEXT = "BUILD ID  PRODUCT53R6R1-TAP-FIX"',
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
    print("Product 53R6R1 airport interaction hotfix checks passed")


if __name__ == "__main__":
    main()
