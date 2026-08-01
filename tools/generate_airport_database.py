#!/usr/bin/env python3
"""Generate the radar's regional airport database from OurAirports CSV files.

This is the lower-level command-line generator. Most Windows users should run
``tools/Build Airport Database.bat`` instead.

The generated header contains public airport data only. The generation center is
used to select records but is deliberately not written to the header.
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import tempfile
import unicodedata
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Iterable

EARTH_RADIUS_MILES = 3958.7613
DEFAULT_RADIUS_MILES = 120.0
MIN_RADIUS_MILES = 90.0
MAX_RADIUS_MILES = 500.0
MAX_IDENT_CHARS = 7
MAX_NAME_CHARS = 31
GENERATOR_VERSION = 2

CATEGORY_MAJOR = 0
CATEGORY_PUBLIC = 1
CATEGORY_PRIVATE = 2
CATEGORY_HELIPORT = 3
CATEGORY_NAMES = ("major", "public", "private", "heliport")


@dataclass(frozen=True)
class RunwayInfo:
    length_ft: int = 0
    heading_degrees: int = 0


@dataclass(frozen=True)
class Airport:
    ident: str
    name: str
    latitude: float
    longitude: float
    elevation: int
    runway_length: int
    runway_heading: int
    category: int
    distance: float


@dataclass
class LoadStats:
    source_rows: int = 0
    eligible_rows: int = 0
    outside_radius: int = 0
    invalid_rows: int = 0
    duplicate_idents: int = 0
    runway_matches: int = 0


def distance_miles(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (
        math.sin(dlat / 2.0) ** 2
        + math.cos(p1) * math.cos(p2) * math.sin(dlon / 2.0) ** 2
    )
    return 2.0 * EARTH_RADIUS_MILES * math.atan2(
        math.sqrt(a), math.sqrt(max(0.0, 1.0 - a))
    )


def _ascii(text: str) -> str:
    normalized = unicodedata.normalize("NFKD", text or "")
    return normalized.encode("ascii", "ignore").decode("ascii")


def clean_name(text: str) -> str:
    value = " ".join(_ascii(text).upper().replace("\\", " ").replace('"', " ").split())
    return value[:MAX_NAME_CHARS]


def clean_coverage(text: str) -> str:
    value = " ".join(_ascii(text).upper().replace("\\", " ").replace('"', " ").split())
    return (value or "CUSTOM REGIONAL DATABASE")[:63]


def clean_ident(text: str) -> str:
    value = _ascii(text).upper().strip()
    value = "".join(ch for ch in value if ch.isalnum() or ch == "-")
    return value[:MAX_IDENT_CHARS]


def _parse_float(value: str | None) -> float | None:
    try:
        parsed = float(value) if value not in (None, "") else None
    except (TypeError, ValueError):
        return None
    return parsed if parsed is not None and math.isfinite(parsed) else None


def _parse_int(value: str | None, default: int = 0) -> int:
    parsed = _parse_float(value)
    return default if parsed is None else int(round(parsed))


def classify(row: dict[str, str]) -> int | None:
    facility_type = (row.get("type") or "").strip().lower()
    if facility_type in {"closed", "seaplane_base", "balloonport"}:
        return None
    if facility_type == "heliport":
        return CATEGORY_HELIPORT
    if facility_type in {"large_airport", "medium_airport"}:
        return CATEGORY_MAJOR
    if facility_type != "small_airport":
        return None

    # OurAirports does not have one worldwide public/private field. This project
    # uses a conservative US-friendly heuristic: scheduled service, an ICAO/GPS
    # K-code, or a short local code is treated as public. Other small fields are
    # categorized as private. Users can still hide/show individual labels later.
    scheduled = (row.get("scheduled_service") or "").strip().lower() == "yes"
    icao = clean_ident(row.get("icao_code") or "")
    gps = clean_ident(row.get("gps_code") or "")
    local = clean_ident(row.get("local_code") or "")
    public_code = icao.startswith("K") or gps.startswith("K")
    short_local = bool(local) and len(local) <= 3
    return CATEGORY_PUBLIC if scheduled or public_code or short_local else CATEGORY_PRIVATE


def _runway_heading(row: dict[str, str]) -> int:
    heading = _parse_float(row.get("le_heading_degT"))
    if heading is None:
        heading = _parse_float(row.get("he_heading_degT"))
    if heading is None:
        return 0
    rounded = int(round(heading)) % 180
    return 180 if rounded == 0 and heading > 0 else rounded


def load_runways(path: Path | None) -> dict[str, RunwayInfo]:
    if path is None:
        return {}
    best: dict[str, RunwayInfo] = {}
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        required = {"airport_ident", "length_ft"}
        if not required.issubset(set(reader.fieldnames or ())):
            missing = ", ".join(sorted(required - set(reader.fieldnames or ())))
            raise ValueError(f"runways.csv is missing required column(s): {missing}")
        for row in reader:
            if (row.get("closed") or "0").strip().lower() in {"1", "true", "yes"}:
                continue
            airport_ident = (row.get("airport_ident") or "").strip()
            length = _parse_int(row.get("length_ft"))
            if not airport_ident or length <= 0:
                continue
            candidate = RunwayInfo(
                length_ft=max(0, min(65535, length)),
                heading_degrees=max(0, min(360, _runway_heading(row))),
            )
            previous = best.get(airport_ident)
            if previous is None or candidate.length_ft > previous.length_ft:
                best[airport_ident] = candidate
            elif candidate.length_ft == previous.length_ft and (
                candidate.heading_degrees < previous.heading_degrees
            ):
                best[airport_ident] = candidate
    return best


def _identifier_candidates(row: dict[str, str]) -> Iterable[str]:
    for key in ("gps_code", "icao_code", "local_code", "ident"):
        candidate = clean_ident(row.get(key) or "")
        if candidate:
            yield candidate


def load_airports(
    airports_path: Path,
    runways_path: Path | None,
    center_lat: float,
    center_lon: float,
    radius: float,
) -> tuple[list[Airport], LoadStats]:
    if not -90.0 <= center_lat <= 90.0:
        raise ValueError("latitude must be between -90 and 90")
    if not -180.0 <= center_lon <= 180.0:
        raise ValueError("longitude must be between -180 and 180")
    if not MIN_RADIUS_MILES <= radius <= MAX_RADIUS_MILES:
        raise ValueError(
            f"radius must be between {MIN_RADIUS_MILES:.0f} and {MAX_RADIUS_MILES:.0f} miles"
        )

    runways = load_runways(runways_path)
    result: list[Airport] = []
    used_idents: set[str] = set()
    stats = LoadStats()

    with airports_path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        required = {"ident", "type", "name", "latitude_deg", "longitude_deg"}
        if not required.issubset(set(reader.fieldnames or ())):
            missing = ", ".join(sorted(required - set(reader.fieldnames or ())))
            raise ValueError(f"airports.csv is missing required column(s): {missing}")

        for row in reader:
            stats.source_rows += 1
            category = classify(row)
            if category is None:
                continue
            latitude = _parse_float(row.get("latitude_deg"))
            longitude = _parse_float(row.get("longitude_deg"))
            if latitude is None or longitude is None or not (-90 <= latitude <= 90) or not (-180 <= longitude <= 180):
                stats.invalid_rows += 1
                continue
            distance = distance_miles(center_lat, center_lon, latitude, longitude)
            if distance > radius:
                stats.outside_radius += 1
                continue

            ident = ""
            for candidate in _identifier_candidates(row):
                if candidate not in used_idents:
                    ident = candidate
                    break
            if not ident:
                stats.duplicate_idents += 1
                continue

            internal_ident = (row.get("ident") or "").strip()
            runway = runways.get(internal_ident, RunwayInfo())
            if runway.length_ft:
                stats.runway_matches += 1
            elevation = max(-32768, min(32767, _parse_int(row.get("elevation_ft"))))
            name = clean_name(row.get("name") or ident) or ident
            result.append(
                Airport(
                    ident=ident,
                    name=name,
                    latitude=latitude,
                    longitude=longitude,
                    elevation=elevation,
                    runway_length=runway.length_ft,
                    runway_heading=runway.heading_degrees,
                    category=category,
                    distance=distance,
                )
            )
            used_idents.add(ident)
            stats.eligible_rows += 1

    result.sort(key=lambda item: (item.ident, item.latitude, item.longitude))
    return result, stats


def validate_airports(airports: list[Airport]) -> None:
    if not airports:
        raise ValueError("No eligible airports found; the existing generated header was not changed")
    if len(airports) > 65535:
        raise ValueError("Too many records for the firmware uint16_t database count")
    idents = [airport.ident for airport in airports]
    if len(idents) != len(set(idents)):
        raise ValueError("duplicate airport identifiers remain after generation")
    for airport in airports:
        if not (1 <= len(airport.ident) <= MAX_IDENT_CHARS):
            raise ValueError(f"invalid identifier length: {airport.ident!r}")
        if not (1 <= len(airport.name) <= MAX_NAME_CHARS):
            raise ValueError(f"invalid airport name length: {airport.name!r}")
        if not (-90 <= airport.latitude <= 90 and -180 <= airport.longitude <= 180):
            raise ValueError(f"invalid coordinates for {airport.ident}")
        if airport.category not in range(4):
            raise ValueError(f"invalid category for {airport.ident}")
        if not (0 <= airport.runway_length <= 65535):
            raise ValueError(f"invalid runway length for {airport.ident}")
        if not (0 <= airport.runway_heading <= 360):
            raise ValueError(f"invalid runway heading for {airport.ident}")


def category_counts(airports: list[Airport]) -> tuple[int, int, int, int]:
    counts = [0, 0, 0, 0]
    for airport in airports:
        counts[airport.category] += 1
    return tuple(counts)  # type: ignore[return-value]


def build_header(
    airports: list[Airport],
    database_date: str,
    coverage: str,
    radius_miles: float,
) -> str:
    validate_airports(airports)
    lines = [
        "#pragma once\n",
        "#include <Arduino.h>\n",
        "#include <stdint.h>\n\n",
        "// Generated from OurAirports public-domain airports.csv and runways.csv.\n",
        "// Awareness only: not for navigation. The private generation-center\n",
        "// coordinates are intentionally not stored in this file.\n\n",
        "namespace generated_airports {\n\n",
        f'constexpr const char* DATABASE_DATE = "{clean_coverage(database_date)}";\n',
        f'constexpr const char* DATABASE_COVERAGE = "{clean_coverage(coverage)}";\n',
        f"constexpr uint16_t DATABASE_RADIUS_MILES = {int(round(radius_miles))};\n",
        f"constexpr uint8_t DATABASE_GENERATOR_VERSION = {GENERATOR_VERSION};\n\n",
        "struct Record {\n",
        "  char ident[8];\n",
        "  char name[32];\n",
        "  int32_t latitudeE6;\n",
        "  int32_t longitudeE6;\n",
        "  int16_t elevationFt;\n",
        "  uint16_t runwayLengthFt;\n",
        "  uint16_t runwayHeadingDegrees;\n",
        "  uint8_t category;\n",
        "};\n\n",
        "static const Record RECORDS[] PROGMEM = {\n",
    ]
    for airport in airports:
        lines.append(
            f'  {{"{airport.ident}", "{airport.name}", '
            f"{round(airport.latitude * 1_000_000)}, "
            f"{round(airport.longitude * 1_000_000)}, "
            f"{airport.elevation}, {airport.runway_length}, "
            f"{airport.runway_heading}, {airport.category}}},\n"
        )
    lines.extend(
        [
            "};\n\n",
            "constexpr uint16_t RECORD_COUNT = sizeof(RECORDS) / sizeof(RECORDS[0]);\n\n",
            "}  // namespace generated_airports\n",
        ]
    )
    return "".join(lines)


def write_header_atomic(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        temporary.replace(path)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate a bounded regional airport table for Bill's Aircraft Radar"
    )
    parser.add_argument("airports_csv", type=Path)
    parser.add_argument("--runways-csv", type=Path)
    parser.add_argument(
        "--output", type=Path, default=Path("include/generated_airport_database.h")
    )
    parser.add_argument("--latitude", type=float, required=True)
    parser.add_argument("--longitude", type=float, required=True)
    parser.add_argument("--radius", type=float, default=DEFAULT_RADIUS_MILES)
    parser.add_argument("--date", default=date.today().isoformat())
    parser.add_argument("--coverage", default="CUSTOM REGIONAL DATABASE")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    airports, stats = load_airports(
        args.airports_csv,
        args.runways_csv,
        args.latitude,
        args.longitude,
        args.radius,
    )
    content = build_header(airports, args.date, args.coverage, args.radius)
    counts = category_counts(airports)
    if not args.dry_run:
        write_header_atomic(args.output, content)
    action = "Would generate" if args.dry_run else "Generated"
    print(f"{action} {len(airports)} airports for a {args.radius:.0f}-mile region")
    print(
        "Categories: "
        + ", ".join(f"{name}={counts[index]}" for index, name in enumerate(CATEGORY_NAMES))
    )
    print(f"Runway details matched: {stats.runway_matches}/{len(airports)}")
    if stats.duplicate_idents:
        print(f"Skipped duplicate display identifiers: {stats.duplicate_idents}")
    if not args.dry_run:
        print(f"Wrote: {args.output}")


if __name__ == "__main__":
    main()
