#!/usr/bin/env python3
"""Verify that the Product 62 R1 Core-1 park linked into ESP32-S3 IRAM."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

DEFAULT_MAP = (
    Path.home()
    / ".platformio"
    / "workspaces"
    / "bills_aircraft_radar"
    / "build"
    / "waveshare-s3-touch-lcd-7"
    / "firmware.map"
)

SYMBOL = "parkCoreOneForRestart"
IRAM_START = 0x40370000
IRAM_END = 0x403E0000
FLASH_TEXT_START = 0x42000000
FLASH_TEXT_END = 0x44000000


def find_symbol_address(map_text: str, symbol: str) -> int | None:
    for line in map_text.splitlines():
        if symbol not in line:
            continue
        match = re.match(r"^\s*(0x[0-9A-Fa-f]+)\s+", line)
        if match:
            return int(match.group(1), 16)
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "map_file",
        nargs="?",
        type=Path,
        default=DEFAULT_MAP,
        help="Path to the PlatformIO firmware.map file",
    )
    args = parser.parse_args()

    if not args.map_file.is_file():
        print(f"Map file not found: {args.map_file}", file=sys.stderr)
        return 2

    text = args.map_file.read_text(encoding="utf-8", errors="replace")
    address = find_symbol_address(text, SYMBOL)
    if address is None:
        print(f"Symbol not found in map: {SYMBOL}", file=sys.stderr)
        return 1

    if FLASH_TEXT_START <= address < FLASH_TEXT_END:
        print(
            f"FAIL: {SYMBOL} linked into flash at 0x{address:08x}",
            file=sys.stderr,
        )
        return 1

    if not (IRAM_START <= address < IRAM_END):
        print(
            f"FAIL: {SYMBOL} is outside expected ESP32-S3 IRAM at "
            f"0x{address:08x}",
            file=sys.stderr,
        )
        return 1

    print(f"PASS: {SYMBOL} linked into IRAM at 0x{address:08x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
