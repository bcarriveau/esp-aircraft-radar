#!/usr/bin/env python3
"""Region-independent checks for the compiled airport database header."""

from __future__ import annotations

import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "generated_airport_database.h"
SOURCE = ROOT / "src" / "airport_data.cpp"

RECORD_RE = re.compile(
    r'\{"([^"]+)",\s*"([^"]+)",\s*(-?\d+),\s*(-?\d+),\s*'
    r'(-?\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}'
)
COUNT_RE = re.compile(r"constexpr\s+uint16_t\s+RECORD_COUNT")


def records() -> list[tuple[str, str, int, int, int, int, int, int]]:
    text = HEADER.read_text(encoding="utf-8")
    return [
        (ident, name, int(lat), int(lon), int(elev), int(length), int(heading), int(category))
        for ident, name, lat, lon, elev, length, heading, category in RECORD_RE.findall(text)
    ]


def main() -> None:
    text = HEADER.read_text(encoding="utf-8")
    data = records()
    assert data, "airport database contains no records"
    assert COUNT_RE.search(text), "RECORD_COUNT declaration is missing"
    assert "DATABASE_DATE" in text and "DATABASE_COVERAGE" in text
    assert "DATABASE_CENTER" not in text, "private generation center must not be stored"

    idents = [record[0] for record in data]
    assert len(idents) == len(set(idents)), "duplicate airport identifiers"
    assert all(1 <= len(record[0]) <= 7 for record in data)
    assert all(1 <= len(record[1]) <= 31 for record in data)
    assert all(-90_000_000 <= record[2] <= 90_000_000 for record in data)
    assert all(-180_000_000 <= record[3] <= 180_000_000 for record in data)
    assert all(-32768 <= record[4] <= 32767 for record in data)
    assert all(0 <= record[5] <= 65535 for record in data)
    assert all(0 <= record[6] <= 360 for record in data)
    assert all(0 <= record[7] <= 3 for record in data)

    if SOURCE.exists():
        source = SOURCE.read_text(encoding="utf-8")
        assert "float radians(" not in source, (
            "Arduino defines radians(...) as a macro; use a non-conflicting helper name"
        )
        assert "degreesToRadians(" in source

    categories = Counter(record[7] for record in data)
    print(
        "Airport database checks passed: "
        f"{len(data)} records; "
        + ", ".join(f"category {key}={categories[key]}" for key in sorted(categories))
    )


if __name__ == "__main__":
    main()
