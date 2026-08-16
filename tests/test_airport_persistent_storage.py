#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "partitions" / "airport_separation_16MB.csv"
PLATFORMIO = ROOT / "platformio.ini"
FORMAT_HEADER = ROOT / "include" / "airport_package_format.h"
PY_PACKAGE = ROOT / "tools" / "airport_package.py"

def parse_int(value: str) -> int:
    return int(value, 0)

def main() -> None:
    rows = []
    with PARTITIONS.open(newline="", encoding="utf-8") as handle:
        for row in csv.reader(line for line in handle if not line.lstrip().startswith("#")):
            if not row:
                continue
            rows.append([item.strip() for item in row])

    expected = {
        "nvs": (0x9000, 0x5000),
        "otadata": (0xE000, 0x2000),
        "app0": (0x10000, 0x640000),
        "app1": (0x650000, 0x640000),
        "airports": (0xC90000, 0x80000),
        "spiffs": (0xD10000, 0x2E0000),
        "coredump": (0xFF0000, 0x10000),
    }
    seen = {}
    for name, ptype, subtype, offset, size, *_ in rows:
        seen[name] = (parse_int(offset), parse_int(size))
    assert seen == expected, f"unexpected partition layout: {seen}"

    ordered = sorted((offset, offset + size, name) for name, (offset, size) in seen.items())
    for (_, end, name), (next_start, _, next_name) in zip(ordered, ordered[1:]):
        assert end <= next_start, f"partition overlap: {name} -> {next_name}"
    assert ordered[-1][1] == 0x1000000, "16 MB flash map must end exactly at 0x1000000"

    ini = PLATFORMIO.read_text(encoding="utf-8")
    assert "board_build.partitions = partitions/airport_separation_16MB.csv" in ini
    assert "board_build.flash_size = 16MB" in ini
    assert "board_build.arduino.memory_type = qio_opi" in ini
    assert "board_build.psram_type = opi" in ini
    assert "-DBOARD_HAS_PSRAM" in ini

    cpp = FORMAT_HEADER.read_text(encoding="utf-8")
    py = PY_PACKAGE.read_text(encoding="utf-8")
    assert "PACKAGE_HEADER_SIZE = 256" in cpp
    assert "RECORD_SIZE = 55" in cpp
    assert 'PACKAGE_MAGIC[16] = "BILLS-AIRPORTDB"' in cpp
    assert 'PACKAGE_MAGIC = b"BILLS-AIRPORTDB\\0"' in py
    assert "PACKAGE_HEADER_SIZE = 256" in py
    assert "RECORD_SIZE = RECORD_STRUCT.size" in py

    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler:
        with tempfile.TemporaryDirectory() as td:
            exe_name = "airport_format_test.exe" if Path(compiler).suffix.lower() == ".exe" else "airport_format_test"
            exe = Path(td) / exe_name
            command = [
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(ROOT / "include"),
                str(ROOT / "src" / "airport_package_format.cpp"),
                str(ROOT / "tests" / "test_airport_package_format.cpp"),
                "-o", str(exe),
            ]
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run([str(exe)], cwd=ROOT, check=True)
        print(f"Host C++ format test passed with {Path(compiler).name}")
    else:
        print("Host C++ format test skipped: no g++ or clang++ found in PATH")

    print("Persistent airport storage structural checks passed")

if __name__ == "__main__":
    main()
