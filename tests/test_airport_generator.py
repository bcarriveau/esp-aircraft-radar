#!/usr/bin/env python3
"""Focused deterministic tests for the regional airport generator."""

from __future__ import annotations

import csv
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import generate_airport_database as generator  # noqa: E402
import airport_database_setup as setup  # noqa: E402

AIRPORT_FIELDS = [
    "ident", "type", "name", "latitude_deg", "longitude_deg", "elevation_ft",
    "scheduled_service", "icao_code", "gps_code", "local_code",
]
RUNWAY_FIELDS = [
    "airport_ident", "length_ft", "closed", "le_heading_degT", "he_heading_degT",
]


def write_csv(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    with tempfile.TemporaryDirectory() as temporary_name:
        temporary = Path(temporary_name)
        airports_csv = temporary / "airports.csv"
        runways_csv = temporary / "runways.csv"
        output = temporary / "generated.h"

        write_csv(
            airports_csv,
            AIRPORT_FIELDS,
            [
                {"ident":"MAJOR1","type":"large_airport","name":"Major International","latitude_deg":"41.0","longitude_deg":"-93.0","elevation_ft":"900","scheduled_service":"yes","icao_code":"KMAJ","gps_code":"KMAJ","local_code":"MAJ"},
                {"ident":"PUBLIC1","type":"small_airport","name":"Public Municipal","latitude_deg":"41.1","longitude_deg":"-93.0","elevation_ft":"800","scheduled_service":"no","icao_code":"","gps_code":"KPUB","local_code":"PUB"},
                {"ident":"PRIVATE1","type":"small_airport","name":"Private Field","latitude_deg":"41.2","longitude_deg":"-93.0","elevation_ft":"750","scheduled_service":"no","icao_code":"","gps_code":"1IA2","local_code":"1IA2"},
                {"ident":"HELIPORT1","type":"heliport","name":"Medical Heliport","latitude_deg":"41.3","longitude_deg":"-93.0","elevation_ft":"700","scheduled_service":"no","icao_code":"","gps_code":"IA99","local_code":"IA99"},
                {"ident":"CLOSED1","type":"closed","name":"Closed Airport","latitude_deg":"41.0","longitude_deg":"-93.1","elevation_ft":"0","scheduled_service":"no","icao_code":"","gps_code":"","local_code":""},
                {"ident":"FAR1","type":"medium_airport","name":"Far Airport","latitude_deg":"46.0","longitude_deg":"-93.0","elevation_ft":"0","scheduled_service":"yes","icao_code":"KFAR","gps_code":"KFAR","local_code":"FAR"},
            ],
        )
        write_csv(
            runways_csv,
            RUNWAY_FIELDS,
            [
                {"airport_ident":"MAJOR1","length_ft":"5000","closed":"0","le_heading_degT":"90","he_heading_degT":"270"},
                {"airport_ident":"MAJOR1","length_ft":"8000","closed":"0","le_heading_degT":"179.6","he_heading_degT":"359.6"},
                {"airport_ident":"PUBLIC1","length_ft":"3200","closed":"0","le_heading_degT":"45","he_heading_degT":"225"},
                {"airport_ident":"PRIVATE1","length_ft":"2500","closed":"1","le_heading_degT":"10","he_heading_degT":"190"},
            ],
        )

        airports, stats = generator.load_airports(
            airports_csv, runways_csv, 41.0, -93.0, 120.0
        )
        generator.validate_airports(airports)
        by_ident = {airport.ident: airport for airport in airports}
        assert set(by_ident) == {"KMAJ", "KPUB", "1IA2", "IA99"}
        assert generator.category_counts(airports) == (1, 1, 1, 1)
        assert by_ident["KMAJ"].runway_length == 8000
        assert by_ident["KMAJ"].runway_heading == 180
        assert by_ident["KPUB"].runway_length == 3200
        assert by_ident["1IA2"].runway_length == 0
        assert stats.runway_matches == 2

        first = generator.build_header(airports, "2026-08-01", "IOWA TEST", 120.0)
        second = generator.build_header(airports, "2026-08-01", "IOWA TEST", 120.0)
        assert first == second, "generation must be deterministic"
        assert "DATABASE_CENTER" not in first
        assert "DATABASE_RADIUS_MILES = 120" in first
        assert f"DATABASE_GENERATOR_VERSION = {generator.GENERATOR_VERSION}" in first
        assert "PUBLIC MUNICIPAL" in first and "CLOSED AIRPORT" not in first

        generator.write_header_atomic(output, first)
        assert output.read_text(encoding="utf-8") == first

        try:
            generator.validate_airports([])
        except ValueError:
            pass
        else:
            raise AssertionError("empty generation must be rejected")

    cache = setup.cache_directory().resolve()
    assert ROOT.resolve() not in cache.parents and cache != ROOT.resolve(), (
        "downloaded OurAirports data must be cached outside the repository"
    )

    ignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
    for expected in (
        "__pycache__/",
        "*.py[cod]",
        "/airports.csv",
        "/runways.csv",
        "/include/.generated_airport_database.h.*.tmp",
        "/COMMIT_MESSAGE.txt",
        "/PACKAGE_README.txt",
        "/SHA256SUMS.txt",
    ):
        assert expected in ignore, f"missing .gitignore rule: {expected}"
    assert "/include/generated_airport_database.h" not in ignore, (
        "the compiled regional airport header must remain tracked"
    )

    print("Airport generator checks passed")


if __name__ == "__main__":
    main()
