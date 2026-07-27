#!/usr/bin/env python3
"""Focused checks for the Product 50 regional airport database."""

from __future__ import annotations

import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "generated_airport_database.h"
SOURCE = ROOT / "src" / "airport_data.cpp"

RECORD_RE = re.compile(
    r'\{"([^"]+)",\s*"([^"]+)",\s*(-?\d+),\s*(-?\d+),\s*'
    r'(-?\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}'
)


def records() -> list[tuple[str, str, int, int, int, int, int, int]]:
    text = HEADER.read_text(encoding="utf-8")
    return [
        (ident, name, int(lat), int(lon), int(elev), int(length), int(heading), int(category))
        for ident, name, lat, lon, elev, length, heading, category in RECORD_RE.findall(text)
    ]


def distance_miles(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    radius = 3958.7613
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * radius * math.atan2(math.sqrt(a), math.sqrt(max(0.0, 1.0 - a)))


def main() -> None:
    data = records()
    assert len(data) >= 40, "starter database unexpectedly small"
    idents = [record[0] for record in data]
    assert len(idents) == len(set(idents)), "duplicate airport identifiers"
    assert {0, 1, 2, 3}.issubset({record[7] for record in data}), "missing airport category"
    assert {"KUES", "KMKE", "KORD", "WS21", "WI26"}.issubset(idents)
    assert all(1 <= len(record[0]) <= 7 for record in data)
    assert all(1 <= len(record[1]) <= 31 for record in data)
    assert all(-90_000_000 <= record[2] <= 90_000_000 for record in data)
    assert all(-180_000_000 <= record[3] <= 180_000_000 for record in data)
    assert all(0 <= record[6] <= 360 for record in data)

    # Use the public KUES facility coordinates as a repeatable regional test center.
    home_lat, home_lon = 43.041, -88.237099
    nearby = [
        record for record in data
        if distance_miles(home_lat, home_lon, record[2] / 1_000_000, record[3] / 1_000_000) <= 90.0
    ]
    assert len(nearby) >= 25, "insufficient records in the hardware-test radius"

    source = SOURCE.read_text(encoding="utf-8")
    assert "float radians(" not in source, (
        "Arduino defines radians(...) as a macro; use a non-conflicting helper name"
    )
    assert "degreesToRadians(" in source

    text = HEADER.read_text(encoding="utf-8").upper()
    assert "KOLLER" not in text, "known closed airport must not be included"
    assert "SIMANDL" not in text, "known closed airport must not be included"
    print(f"Airport database checks passed: {len(data)} records, {len(nearby)} within 90 miles")


if __name__ == "__main__":
    main()
