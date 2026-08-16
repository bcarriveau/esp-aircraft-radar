#!/usr/bin/env python3
"""Checks that runtime airport data has one persistent source."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "airport_data.cpp"
LEGACY = ROOT / "include" / "generated_airport_database.h"


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    legacy = LEGACY.read_text(encoding="utf-8")

    assert 'generated_airport_database.h' not in source
    assert 'generated_airports::' not in source
    assert 'compiled fallback' not in source.lower()
    assert 'airport_store::recordCount()' in source
    assert 'airport_store::readRecord' in source
    assert 'no regional database installed' in source

    assert 'RECORDS[' not in legacy
    assert 'RECORD_COUNT' not in legacy
    assert 'persistent .radarapt' in legacy

    assert 'float radians(' not in source
    assert 'degreesToRadians(' in source
    print('Unified persistent airport-source checks passed')


if __name__ == '__main__':
    main()
