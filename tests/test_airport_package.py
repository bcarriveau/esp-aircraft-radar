#!/usr/bin/env python3
"""Deterministic tests for the persistent airport package format."""

from __future__ import annotations

import dataclasses
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import airport_package as package  # noqa: E402


@dataclasses.dataclass(frozen=True)
class Airport:
    ident: str
    name: str
    latitude: float
    longitude: float
    elevation: int
    runway_length: int
    runway_heading: int
    category: int


def expect_rejected(raw: bytes, reason: str) -> None:
    try:
        package.parse_package(raw)
    except ValueError:
        return
    raise AssertionError(f"corrupt package was accepted: {reason}")


def main() -> None:
    airports = [
        Airport("KMSN", "DANE COUNTY REGIONAL", 43.1399, -89.337502, 887, 9006, 2, 0),
        Airport("KUES", "WAUKESHA COUNTY AIRPORT", 43.041, -88.237099, 911, 5849, 102, 1),
        Airport("WI86", "ORI AIRPORT", 42.6609, -88.135902, 800, 1600, 0, 2),
        Airport("WI01", "AURORA MEDICAL CENTER KENOSHA", 42.570599, -87.936096, 698, 50, 0, 3),
    ]

    first = package.build_package(
        airports,
        database_date="2026-08-13",
        coverage="TEST REGION",
        radius_miles=120,
        generator_version=2,
    )
    second = package.build_package(
        airports,
        database_date="2026-08-13",
        coverage="TEST REGION",
        radius_miles=120,
        generator_version=2,
    )
    assert first == second, "airport package generation must be deterministic"
    assert package.RECORD_SIZE == 55, "disk record layout must remain explicitly packed"
    assert len(first) == package.PACKAGE_HEADER_SIZE + len(airports) * package.RECORD_SIZE

    info, decoded = package.parse_package(first)
    assert info.format_version == 1
    assert info.generator_version == 2
    assert info.record_count == 4
    assert info.radius_miles == 120
    assert info.database_date == "2026-08-13"
    assert info.coverage == "TEST REGION"
    assert decoded[0].ident == "KMSN"
    assert decoded[0].latitude_e6 == 43_139_900
    assert decoded[0].longitude_e6 == -89_337_502
    assert decoded[-1].category == 3

    # The package intentionally has no generation-center/home-location fields.
    assert b"CENTER_LAT" not in first and b"CENTER_LON" not in first

    expect_rejected(first[:-1], "truncated payload")
    damaged = bytearray(first)
    damaged[-1] ^= 0x01
    expect_rejected(bytes(damaged), "payload digest mismatch")
    damaged = bytearray(first)
    damaged[0] ^= 0x01
    expect_rejected(bytes(damaged), "magic mismatch")

    try:
        package.build_package(
            [],
            database_date="2026-08-13",
            coverage="EMPTY",
            radius_miles=120,
            generator_version=2,
        )
    except ValueError:
        pass
    else:
        raise AssertionError("empty airport package must be rejected")

    with tempfile.TemporaryDirectory() as temporary_name:
        output = Path(temporary_name) / "airports.radarapt"
        package.write_package_atomic(output, first)
        assert output.read_bytes() == first
        package.parse_package(output.read_bytes())

    print("Airport package format checks passed")


if __name__ == "__main__":
    main()
