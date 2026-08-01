#!/usr/bin/env python3
"""Focused Product 47 vertical-state bitmap and hysteresis checks.

Runs without PlatformIO or third-party Python packages. Verifies the three source
assets, generated bitmap header, distinct aircraft attitudes, state thresholds,
hysteresis, and display rounding with a strict host C++17 compile.
"""
from __future__ import annotations

import pathlib
import re
import shutil
import struct
import subprocess
import tempfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
STATE_HEADER = ROOT / "include/vertical_state.h"
BITMAP_HEADER = ROOT / "include/vertical_state_bitmaps.h"
ASSET_NAMES = (
    "vertical_state_climbing_80x36.png",
    "vertical_state_level_80x36.png",
    "vertical_state_descending_80x36.png",
)


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


def read_png_alpha(path: pathlib.Path) -> tuple[int, int, list[int]]:
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
            width, height, bit_depth, color_type, compression, filtering, interlace = \
                struct.unpack(">IIBBBBB", chunk)
            assert bit_depth == 8
            assert color_type == 6  # RGBA
            assert compression == 0 and filtering == 0 and interlace == 0
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            break

    assert width > 0 and height > 0 and compressed
    raw = zlib.decompress(bytes(compressed))
    bytes_per_pixel = 4
    row_bytes = width * bytes_per_pixel
    expected = height * (row_bytes + 1)
    assert len(raw) == expected, (len(raw), expected)

    previous = bytearray(row_bytes)
    rgba = bytearray(width * height * bytes_per_pixel)
    raw_offset = 0
    output_offset = 0
    for _ in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        encoded = raw[raw_offset:raw_offset + row_bytes]
        raw_offset += row_bytes
        decoded = bytearray(row_bytes)

        for index, value in enumerate(encoded):
            left = decoded[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            up = previous[index]
            upper_left = previous[index - bytes_per_pixel] \
                if index >= bytes_per_pixel else 0
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

        rgba[output_offset:output_offset + row_bytes] = decoded
        output_offset += row_bytes
        previous = decoded

    alpha = list(rgba[3::4])
    return width, height, alpha


def alpha_centroid(width: int, height: int, alpha: list[int]) -> tuple[float, float]:
    weighted_x = 0.0
    weighted_y = 0.0
    total = 0.0
    for y in range(height):
        for x in range(width):
            value = alpha[y * width + x]
            weighted_x += x * value
            weighted_y += y * value
            total += value
    assert total > 0
    return weighted_x / total, weighted_y / total


def nose_tail_delta(width: int, height: int, alpha: list[int]) -> float:
    left_values: list[int] = []
    right_values: list[int] = []
    for y in range(height):
        for x in range(width):
            if alpha[y * width + x] < 96:
                continue
            if x < width // 3:
                left_values.append(y)
            elif x >= width * 2 // 3:
                right_values.append(y)
    assert left_values and right_values
    left_y = sum(left_values) / len(left_values)
    right_y = sum(right_values) / len(right_values)
    return right_y - left_y


def asset_checks() -> dict[str, list[int]]:
    decoded: dict[str, list[int]] = {}
    for name in ASSET_NAMES:
        width, height, alpha = read_png_alpha(ROOT / "assets" / name)
        assert (width, height) == (80, 36)
        assert min(alpha) == 0
        assert max(alpha) > 240
        decoded[name] = alpha

    climbing_delta = nose_tail_delta(80, 36, decoded[ASSET_NAMES[0]])
    level_delta = nose_tail_delta(80, 36, decoded[ASSET_NAMES[1]])
    descending_delta = nose_tail_delta(80, 36, decoded[ASSET_NAMES[2]])
    assert climbing_delta < -4.0, climbing_delta
    assert abs(level_delta) < 3.0, level_delta
    assert descending_delta > 4.0, descending_delta

    assert alpha_centroid(80, 36, decoded[ASSET_NAMES[0]]) != \
        alpha_centroid(80, 36, decoded[ASSET_NAMES[1]])
    assert alpha_centroid(80, 36, decoded[ASSET_NAMES[2]]) != \
        alpha_centroid(80, 36, decoded[ASSET_NAMES[1]])
    return decoded


def generated_header_checks(decoded: dict[str, list[int]]) -> None:
    text = BITMAP_HEADER.read_text(encoding="utf-8")
    assert "VERTICAL_STATE_BITMAP_W = 80" in text
    assert "VERTICAL_STATE_BITMAP_H = 36" in text
    asset_by_name = {
        "CLIMBING": ASSET_NAMES[0],
        "LEVEL": ASSET_NAMES[1],
        "DESCENDING": ASSET_NAMES[2],
    }
    for name, asset_name in asset_by_name.items():
        match = re.search(
            rf"VERTICAL_STATE_BITMAP_{name}"
            r"\[VERTICAL_STATE_BITMAP_W \* VERTICAL_STATE_BITMAP_H\]"
            r" PROGMEM = \{(.*?)\n\};",
            text,
            re.S,
        )
        assert match, name
        values = [int(value, 16) for value in
                  re.findall(r"0x[0-9A-F]{2}", match.group(1))]
        assert len(values) == 80 * 36, (name, len(values))
        assert values == decoded[asset_name], \
            f"{name} generated data differs from source asset"


def host_compile_check() -> None:
    compiler = shutil.which("g++") or shutil.which("c++")
    if not compiler:
        raise RuntimeError("no C++17 compiler found")

    with tempfile.TemporaryDirectory(prefix="vertical-state-") as temp_name:
        temp = pathlib.Path(temp_name)
        test_cpp = temp / "test.cpp"
        test_cpp.write_text(
            '#include "vertical_state.h"\n'
            "#include <cassert>\n"
            "#include <cstring>\n"
            "int main(){using vertical_state::State;"
            "assert(vertical_state::initialState(250.0f)==State::CLIMBING);"
            "assert(vertical_state::initialState(-250.0f)==State::DESCENDING);"
            "assert(vertical_state::initialState(0.0f)==State::LEVEL);"
            "State s=State::LEVEL;"
            "s=vertical_state::updateState(s,210.0f);"
            "assert(s==State::CLIMBING);"
            "s=vertical_state::updateState(s,150.0f);"
            "assert(s==State::CLIMBING);"
            "s=vertical_state::updateState(s,100.0f);"
            "assert(s==State::LEVEL);"
            "s=vertical_state::updateState(s,-210.0f);"
            "assert(s==State::DESCENDING);"
            "s=vertical_state::updateState(s,-150.0f);"
            "assert(s==State::DESCENDING);"
            "s=vertical_state::updateState(s,-100.0f);"
            "assert(s==State::LEVEL);"
            "assert(vertical_state::roundedRateFpm(824.0f)==800);"
            "assert(vertical_state::roundedRateFpm(826.0f)==850);"
            "assert(vertical_state::roundedRateFpm(-1224.0f)==-1200);"
            "assert(vertical_state::roundedRateFpm(-1226.0f)==-1250);"
            'assert(std::strcmp(vertical_state::stateName(State::CLIMBING),'
            '"CLIMBING")==0);'
            'assert(std::strcmp(vertical_state::stateName(State::LEVEL),'
            '"LEVEL")==0);'
            'assert(std::strcmp(vertical_state::stateName(State::DESCENDING),'
            '"DESCENDING")==0);'
            "return 0;}\n",
            encoding="utf-8",
        )
        output = temp / "test"
        subprocess.run(
            [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pedantic",
                f"-I{ROOT / 'include'}",
                str(test_cpp),
                "-o",
                str(output),
            ],
            check=True,
        )
        subprocess.run([str(output)], check=True)


def integration_checks() -> None:
    ui = (ROOT / "src/ui.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src/radar_renderer.cpp").read_text(encoding="utf-8")
    build = (ROOT / "include/build_info.h").read_text(encoding="utf-8")

    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in ui
    assert "verticalStateIconBuffer" in ui
    assert "view.verticalStateIcon = verticalStateIcon;" in ui
    assert "view.verticalStateIconBuffer = verticalStateIconBuffer;" in ui
    assert "view.verticalStateLabel = verticalStateLabel;" in ui
    assert "strcmp(priorityVerticalStateHex, target->hex) == 0" in renderer
    assert "updateVerticalStateDisplay" in renderer
    assert "radarView.verticalStateLabel, primaryTarget" in renderer
    assert "radarView.verticalStateLabel, nullptr" in renderer
    assert "7IN-20260801-PRODUCT54-LOCAL-WEB-OTA" in build
    assert "HTTPClient::GET()" not in renderer
    assert "setInsecure()" not in renderer


def main() -> int:
    decoded = asset_checks()
    generated_header_checks(decoded)
    host_compile_check()
    integration_checks()
    print("vertical-state checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
