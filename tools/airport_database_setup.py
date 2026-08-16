#!/usr/bin/env python3
"""Friendly regional airport-package builder for Bill's Aircraft Radar.

This tool is intentionally PC-side. It downloads/reads the public OurAirports
CSV data, filters a bounded regional dataset around coordinates supplied by the
user, and writes only ``release/airports.radarapt``.

It does not modify firmware, the compiled fallback airport header, radar NVS
settings, or the user's saved radar location.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
from datetime import date
from pathlib import Path

from generate_airport_database import (
    CATEGORY_NAMES,
    DEFAULT_RADIUS_MILES,
    MAX_RADIUS_MILES,
    MIN_RADIUS_MILES,
    build_binary_package,
    category_counts,
    load_airports,
)
from airport_package import parse_package, write_package_atomic

AIRPORTS_URL = "https://davidmegginson.github.io/ourairports-data/airports.csv"
RUNWAYS_URL = "https://davidmegginson.github.io/ourairports-data/runways.csv"
DOWNLOAD_TIMEOUT_SECONDS = 90


def project_root() -> Path:
    root = Path(__file__).resolve().parents[1]
    required = [
        root / "platformio.ini",
        root / "tools" / "generate_airport_database.py",
        root / "tools" / "airport_package.py",
    ]
    missing = [str(path.relative_to(root)) for path in required if not path.exists()]
    if missing:
        raise RuntimeError(
            "This tool must be run from the aircraft-radar project. Missing: "
            + ", ".join(missing)
        )
    return root


def cache_directory() -> Path:
    if os.name == "nt" and os.environ.get("LOCALAPPDATA"):
        base = Path(os.environ["LOCALAPPDATA"])
    else:
        base = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return base / "BillsAircraftRadar" / "airport-data"


def ask_float(
    prompt: str, minimum: float, maximum: float, default: float | None = None
) -> float:
    while True:
        suffix = f" [{default:g}]" if default is not None else ""
        raw = input(f"{prompt}{suffix}: ").strip()
        if not raw and default is not None:
            return default
        try:
            value = float(raw)
        except ValueError:
            print("Please enter a number.")
            continue
        if not minimum <= value <= maximum:
            print(f"Enter a value from {minimum:g} through {maximum:g}.")
            continue
        return value


def ask_choice() -> str:
    print("\nAirport data source:")
    print("  1. Download the latest OurAirports data (recommended)")
    print("  2. Use the last downloaded copy")
    print("  3. Use airports.csv and runways.csv already on this computer")
    while True:
        choice = input("Choose 1, 2, or 3 [1]: ").strip() or "1"
        if choice in {"1", "2", "3"}:
            return choice
        print("Please choose 1, 2, or 3.")


def _download(url: str, destination: Path) -> None:
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "Bills-Aircraft-Radar-Airport-Package-Builder/1.0"},
    )
    with urllib.request.urlopen(
        request, timeout=DOWNLOAD_TIMEOUT_SECONDS
    ) as response:
        with destination.open("wb") as handle:
            shutil.copyfileobj(response, handle, length=1024 * 1024)
    if destination.stat().st_size < 1024:
        raise RuntimeError(
            f"Downloaded file is unexpectedly small: {destination.name}"
        )


def download_latest(cache: Path) -> tuple[Path, Path]:
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="airport-download-", dir=cache
    ) as temporary_name:
        temporary = Path(temporary_name)
        airports_temp = temporary / "airports.csv"
        runways_temp = temporary / "runways.csv"
        print("\nDownloading airports.csv ...")
        _download(AIRPORTS_URL, airports_temp)
        print("Downloading runways.csv ...")
        _download(RUNWAYS_URL, runways_temp)
        airports_final = cache / "airports.csv"
        runways_final = cache / "runways.csv"
        airports_temp.replace(airports_final)
        runways_temp.replace(runways_final)
    return airports_final, runways_final


def cached_files(cache: Path) -> tuple[Path, Path]:
    airports = cache / "airports.csv"
    runways = cache / "runways.csv"
    if not airports.is_file() or not runways.is_file():
        raise RuntimeError(
            "No complete cached airport download exists yet. Choose option 1 first."
        )
    return airports, runways


def custom_files() -> tuple[Path, Path]:
    while True:
        airports = Path(
            input("Path to airports.csv: ").strip().strip('"')
        ).expanduser()
        runways = Path(
            input("Path to runways.csv: ").strip().strip('"')
        ).expanduser()
        if airports.is_file() and runways.is_file():
            return airports, runways
        print("Both files must exist. Please try again.")


def select_source() -> tuple[Path, Path]:
    cache = cache_directory()
    choice = ask_choice()
    if choice == "1":
        try:
            return download_latest(cache)
        except (OSError, RuntimeError, urllib.error.URLError) as error:
            print(f"\nDownload failed: {error}")
            airports, runways = cached_files(cache)
            answer = input("Use the previous cached copy instead? [Y/n]: ").strip().lower()
            if answer in {"", "y", "yes"}:
                return airports, runways
            raise RuntimeError(
                "Airport package build cancelled because current data "
                "could not be downloaded"
            )
    if choice == "2":
        return cached_files(cache)
    return custom_files()


def run_package_tests(root: Path) -> None:
    for test_name in (
        "test_airport_package.py",
        "test_airport_generator.py",
    ):
        path = root / "tests" / test_name
        if not path.is_file():
            raise RuntimeError(f"Required validation is missing: {test_name}")
        result = subprocess.run(
            [sys.executable, str(path)], cwd=root, check=False
        )
        if result.returncode != 0:
            raise RuntimeError(f"Airport package validation failed: {test_name}")


def validate_written_package(
    package_output: Path,
    expected_records: int,
    expected_radius: int,
) -> None:
    info, records = parse_package(package_output.read_bytes())
    if info.record_count != expected_records:
        raise RuntimeError(
            "Written airport package record count does not match generated data"
        )
    if info.radius_miles != expected_radius:
        raise RuntimeError(
            "Written airport package radius does not match requested coverage"
        )
    if len(records) != expected_records:
        raise RuntimeError(
            "Written airport package record payload is incomplete"
        )


def main() -> int:
    print("=" * 70)
    print(" BILL'S AIRCRAFT RADAR - REGIONAL AIRPORT PACKAGE BUILDER")
    print("=" * 70)
    print(
        "\nThis creates the airports.radarapt file used by the radar's "
        "Airport Database web page."
    )
    print(
        "It does NOT rebuild firmware and does NOT change the radar's saved "
        "home location."
    )
    print(
        "\nEnter decimal degrees. In the United States, longitude is normally negative."
    )

    root = project_root()
    latitude = ask_float("Package center latitude", -90.0, 90.0)
    longitude = ask_float("Package center longitude", -180.0, 180.0)
    radius = ask_float(
        "Database coverage radius in miles",
        MIN_RADIUS_MILES,
        MAX_RADIUS_MILES,
        DEFAULT_RADIUS_MILES,
    )
    print(
        "\n120 miles is recommended: the radar displays up to 80 miles and "
        "keeps a 90-mile nearby cache."
    )
    coverage = input(
        "Short region name [CUSTOM REGIONAL DATABASE]: "
    ).strip() or "CUSTOM REGIONAL DATABASE"

    airports_csv, runways_csv = select_source()
    print("\nReading and matching airport/runway data ...")
    airports, stats = load_airports(
        airports_csv, runways_csv, latitude, longitude, radius
    )
    database_date = date.today().isoformat()
    package_content = build_binary_package(
        airports, database_date, coverage, radius
    )
    counts = category_counts(airports)

    print("\nPackage preview")
    print("-" * 44)
    print(f"Coverage name:       {coverage.upper()}")
    print(f"Coverage radius:     {radius:.0f} miles")
    print(f"Total records:       {len(airports)}")
    for index, name in enumerate(CATEGORY_NAMES):
        print(f"{name.title() + ':':<20}{counts[index]}")
    print(f"Runway details:      {stats.runway_matches}")
    print(f"Package size:        {len(package_content) / 1024:8.1f} KiB")
    if stats.duplicate_idents:
        print(f"Duplicate IDs skipped: {stats.duplicate_idents}")
    print(
        "\nThe package does not store the center/home coordinates used "
        "to select this region."
    )

    confirm = input("\nCreate release\\airports.radarapt? [y/N]: ").strip().lower()
    if confirm not in {"y", "yes"}:
        print("No files were changed.")
        return 0

    package_output = root / "release" / "airports.radarapt"
    package_output.parent.mkdir(parents=True, exist_ok=True)
    previous = package_output.read_bytes() if package_output.exists() else None

    try:
        write_package_atomic(package_output, package_content)
        validate_written_package(
            package_output, len(airports), int(round(radius))
        )
        print("\nRunning airport-package validation ...", flush=True)
        run_package_tests(root)
    except Exception:
        if previous is None:
            package_output.unlink(missing_ok=True)
        else:
            write_package_atomic(package_output, previous)
        print("The previous airports.radarapt was restored.")
        raise

    print("\nSUCCESS - AIRPORT PACKAGE READY")
    print(f"\nFile: {package_output}")
    print(
        "\nNow open the radar's Airport Database web page, choose this file, "
        "and install it."
    )
    print(
        "After a successful install, the radar validates the stored database "
        "and restarts automatically."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nAirport package build cancelled.")
        raise SystemExit(1)
    except Exception as error:
        print(f"\nERROR: {error}")
        raise SystemExit(1)
