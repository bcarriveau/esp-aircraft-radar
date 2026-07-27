#!/usr/bin/env python3
"""Generate include/generated_airport_database.h from OurAirports airports.csv.

The checked-in Product 50 table is a regional starter database for hardware
validation. This utility makes the data source reproducible and allows a wider or
different regional table to be generated without changing firmware code.

OurAirports data is public domain: https://ourairports.com/data/
"""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path

EARTH_RADIUS_MILES = 3958.7613


@dataclass
class Airport:
    ident: str
    name: str
    latitude: float
    longitude: float
    elevation: int
    category: int


def distance_miles(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_RADIUS_MILES * math.atan2(math.sqrt(a), math.sqrt(max(0.0, 1.0 - a)))


def classify(row: dict[str, str]) -> int | None:
    facility_type = row.get("type", "")
    if facility_type in {"closed", "seaplane_base", "balloonport"}:
        return None
    if facility_type == "heliport":
        return 3
    if facility_type in {"large_airport", "medium_airport"}:
        return 0
    if facility_type != "small_airport":
        return None

    # OurAirports does not provide a universal public/private field. For US
    # regional generation, assigned ICAO-style K codes and scheduled service are
    # treated as public; state-number identifiers remain private-field candidates.
    scheduled = row.get("scheduled_service", "").lower() == "yes"
    gps = row.get("gps_code", "").strip().upper()
    local = row.get("local_code", "").strip().upper()
    return 1 if scheduled or gps.startswith("K") or (local and len(local) <= 3) else 2


def clean(text: str, limit: int) -> str:
    return " ".join(text.upper().replace('"', "").split())[: limit - 1]


def load_airports(path: Path, center_lat: float, center_lon: float, radius: float) -> list[Airport]:
    result: list[Airport] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            category = classify(row)
            if category is None:
                continue
            try:
                latitude = float(row["latitude_deg"])
                longitude = float(row["longitude_deg"])
            except (KeyError, TypeError, ValueError):
                continue
            if distance_miles(center_lat, center_lon, latitude, longitude) > radius:
                continue
            ident = row.get("gps_code") or row.get("local_code") or row.get("ident") or "--"
            try:
                elevation = int(round(float(row.get("elevation_ft") or 0)))
            except ValueError:
                elevation = 0
            result.append(Airport(clean(ident, 8), clean(row.get("name", ident), 32),
                                  latitude, longitude, elevation, category))
    result.sort(key=lambda airport: (airport.latitude, airport.longitude, airport.ident))
    return result


def write_header(path: Path, airports: list[Airport], date: str, coverage: str) -> None:
    lines = [
        "#pragma once\n\n#include <Arduino.h>\n#include <stdint.h>\n\n",
        "// Generated from OurAirports public-domain airport records.\n\n",
        "namespace generated_airports {\n\n",
        f'constexpr const char* DATABASE_DATE = "{date}";\n',
        f'constexpr const char* DATABASE_COVERAGE = "{clean(coverage, 64)}";\n\n',
        "struct Record {\n  char ident[8];\n  char name[32];\n  int32_t latitudeE6;\n",
        "  int32_t longitudeE6;\n  int16_t elevationFt;\n  uint16_t runwayLengthFt;\n",
        "  uint16_t runwayHeadingDegrees;\n  uint8_t category;\n};\n\n",
        "static const Record RECORDS[] PROGMEM = {\n",
    ]
    for airport in airports:
        lines.append(
            f'  {{"{airport.ident}", "{airport.name}", '
            f'{round(airport.latitude * 1_000_000)}, {round(airport.longitude * 1_000_000)}, '
            f'{airport.elevation}, 0, 0, {airport.category}}},\n'
        )
    lines.extend([
        "};\n\n",
        "constexpr uint16_t RECORD_COUNT = sizeof(RECORDS) / sizeof(RECORDS[0]);\n\n",
        "}  // namespace generated_airports\n",
    ])
    path.write_text("".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("airports_csv", type=Path)
    parser.add_argument("--output", type=Path, default=Path("include/generated_airport_database.h"))
    parser.add_argument("--latitude", type=float, required=True)
    parser.add_argument("--longitude", type=float, required=True)
    parser.add_argument("--radius", type=float, default=100.0)
    parser.add_argument("--date", required=True)
    parser.add_argument("--coverage", default="CUSTOM REGIONAL DATABASE")
    args = parser.parse_args()
    airports = load_airports(args.airports_csv, args.latitude, args.longitude, args.radius)
    if not airports:
        raise SystemExit("No eligible airports found; generated file was not changed")
    write_header(args.output, airports, args.date, args.coverage)
    print(f"Generated {len(airports)} airports in {args.output}")


if __name__ == "__main__":
    main()
