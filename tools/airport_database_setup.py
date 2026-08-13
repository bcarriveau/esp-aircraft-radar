#!/usr/bin/env python3
"""Friendly, guided airport database setup for Bill's Aircraft Radar."""

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
    build_header,
    category_counts,
    load_airports,
    write_header_atomic,
)
from airport_package import write_package_atomic

AIRPORTS_URL = "https://davidmegginson.github.io/ourairports-data/airports.csv"
RUNWAYS_URL = "https://davidmegginson.github.io/ourairports-data/runways.csv"
DOWNLOAD_TIMEOUT_SECONDS = 90


def project_root() -> Path:
    root = Path(__file__).resolve().parents[1]
    required = [root / "platformio.ini", root / "include" / "generated_airport_database.h"]
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


def ask_float(prompt: str, minimum: float, maximum: float, default: float | None = None) -> float:
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
    print("  1. Download the latest official data (recommended)")
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
        headers={"User-Agent": "Bills-Aircraft-Radar-Airport-Setup/1.0"},
    )
    with urllib.request.urlopen(request, timeout=DOWNLOAD_TIMEOUT_SECONDS) as response:
        with destination.open("wb") as handle:
            shutil.copyfileobj(response, handle, length=1024 * 1024)
    if destination.stat().st_size < 1024:
        raise RuntimeError(f"Downloaded file is unexpectedly small: {destination.name}")


def download_latest(cache: Path) -> tuple[Path, Path]:
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="airport-download-", dir=cache) as temporary_name:
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
        raise RuntimeError("No complete cached airport download exists yet. Choose option 1 first.")
    return airports, runways


def custom_files() -> tuple[Path, Path]:
    while True:
        airports = Path(input("Path to airports.csv: ").strip().strip('"')).expanduser()
        runways = Path(input("Path to runways.csv: ").strip().strip('"')).expanduser()
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
            try:
                airports, runways = cached_files(cache)
            except RuntimeError:
                raise
            answer = input("Use the previous cached copy instead? [Y/n]: ").strip().lower()
            if answer in {"", "y", "yes"}:
                return airports, runways
            raise RuntimeError("Airport setup cancelled because current data could not be downloaded")
    if choice == "2":
        return cached_files(cache)
    return custom_files()


def run_database_test(root: Path) -> None:
    for test_name in ("test_airport_database.py", "test_airport_package.py", "test_airport_generator.py"):
        command = [sys.executable, str(root / "tests" / test_name)]
        result = subprocess.run(command, cwd=root, check=False)
        if result.returncode != 0:
            raise RuntimeError(f"Airport validation failed: {test_name}")


def _restore_file(path: Path, previous: bytes | None) -> None:
    if previous is None:
        path.unlink(missing_ok=True)
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".restore", dir=path.parent
    )
    with os.fdopen(descriptor, "wb") as handle:
        handle.write(previous)
        handle.flush()
        os.fsync(handle.fileno())
    Path(temporary_name).replace(path)


def main() -> int:
    print("=" * 66)
    print(" BILL'S AIRCRAFT RADAR - AIRPORT DATABASE SETUP")
    print("=" * 66)
    print("\nUse this only when moving the radar to a different region.")
    print("For a nearby move inside the current region, change coordinates on")
    print("the radar's System page instead.\n")
    print("Enter decimal degrees. In the United States, longitude is normally negative.\n")

    root = project_root()
    latitude = ask_float("Home latitude", -90.0, 90.0)
    longitude = ask_float("Home longitude", -180.0, 180.0)
    radius = ask_float(
        "Database coverage radius in miles",
        MIN_RADIUS_MILES,
        MAX_RADIUS_MILES,
        DEFAULT_RADIUS_MILES,
    )
    print("\n120 miles is recommended: the radar displays up to 80 miles and")
    print("keeps a 90-mile nearby cache, leaving reasonable movement margin.")
    coverage = input("Short region name [CUSTOM REGIONAL DATABASE]: ").strip()
    coverage = coverage or "CUSTOM REGIONAL DATABASE"

    airports_csv, runways_csv = select_source()
    print("\nReading and matching airport/runway data ...")
    airports, stats = load_airports(
        airports_csv, runways_csv, latitude, longitude, radius
    )
    database_date = date.today().isoformat()
    content = build_header(airports, database_date, coverage, radius)
    package_content = build_binary_package(airports, database_date, coverage, radius)
    counts = category_counts(airports)

    print("\nPreview")
    print("-" * 40)
    print(f"Coverage name:       {coverage.upper()}")
    print(f"Coverage radius:     {radius:.0f} miles")
    print(f"Total records:       {len(airports)}")
    for index, name in enumerate(CATEGORY_NAMES):
        print(f"{name.title() + ':':<20}{counts[index]}")
    print(f"Runway details:      {stats.runway_matches}")
    print(f"Compiled record data:{len(airports) * 56 / 1024:8.1f} KiB approx.")
    print(f"Binary package:      {len(package_content) / 1024:8.1f} KiB")
    print("Runtime cache:       nearest 192 within 90 miles (category bounded)")
    if stats.duplicate_idents:
        print(f"Duplicate IDs skipped: {stats.duplicate_idents}")
    print("\nThe exact home coordinates are not written to either generated output.")

    confirm = input(
        "\nReplace the compiled airport header and generate release/airports.radarapt? [y/N]: "
    ).strip().lower()
    if confirm not in {"y", "yes"}:
        print("No files were changed.")
        return 0

    header_output = root / "include" / "generated_airport_database.h"
    package_output = root / "release" / "airports.radarapt"
    previous_header = header_output.read_bytes() if header_output.exists() else None
    previous_package = package_output.read_bytes() if package_output.exists() else None
    try:
        write_header_atomic(header_output, content)
        write_package_atomic(package_output, package_content)
        print("\nRunning airport validation ...", flush=True)
        run_database_test(root)
    except Exception:
        _restore_file(header_output, previous_header)
        _restore_file(package_output, previous_package)
        print("The previous airport outputs were restored.")
        raise

    print("\nSUCCESS - AIRPORT DATABASE READY")
    print(f"\nCompiled header: {header_output.relative_to(root)}")
    print(f"Persistent package prototype: {package_output.relative_to(root)}")
    print("\nCurrent Product 85 firmware still uses the compiled header.")
    print("The .radarapt file is the verified transition artifact for the")
    print("future persistent-partition/browser upload work.")
    print("\nNext steps:")
    print("  1. Build the normal PlatformIO project as before.")
    print("  2. Keep release/airports.radarapt for the separation tests.")
    print("  3. Enter the same home coordinates on the radar's System page.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nAirport setup cancelled.")
        raise SystemExit(1)
    except Exception as error:
        print(f"\nERROR: {error}")
        raise SystemExit(1)
