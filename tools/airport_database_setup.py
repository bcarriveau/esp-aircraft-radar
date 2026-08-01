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
    build_header,
    category_counts,
    load_airports,
    write_header_atomic,
)

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
    command = [sys.executable, str(root / "tests" / "test_airport_database.py")]
    result = subprocess.run(command, cwd=root, check=False)
    if result.returncode != 0:
        raise RuntimeError("The generated database did not pass validation")


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
    content = build_header(airports, date.today().isoformat(), coverage, radius)
    counts = category_counts(airports)
    estimated_bytes = len(airports) * 56

    print("\nPreview")
    print("-" * 40)
    print(f"Coverage name:       {coverage.upper()}")
    print(f"Coverage radius:     {radius:.0f} miles")
    print(f"Total records:       {len(airports)}")
    for index, name in enumerate(CATEGORY_NAMES):
        print(f"{name.title() + ':':<20}{counts[index]}")
    print(f"Runway details:      {stats.runway_matches}")
    print(f"Approx. flash data:  {estimated_bytes / 1024:.1f} KiB")
    print("Runtime cache:       nearest 192 within 90 miles (category bounded)")
    if stats.duplicate_idents:
        print(f"Duplicate IDs skipped: {stats.duplicate_idents}")
    print("\nThe exact home coordinates are not written to the generated header.")

    confirm = input("\nReplace include/generated_airport_database.h? [y/N]: ").strip().lower()
    if confirm not in {"y", "yes"}:
        print("No files were changed.")
        return 0

    output = root / "include" / "generated_airport_database.h"
    previous = output.read_bytes() if output.exists() else None
    try:
        write_header_atomic(output, content)
        print("\nRunning database validation ...", flush=True)
        run_database_test(root)
    except Exception:
        if previous is not None:
            descriptor, temporary_name = tempfile.mkstemp(
                prefix=f".{output.name}.", suffix=".restore", dir=output.parent
            )
            with os.fdopen(descriptor, "wb") as handle:
                handle.write(previous)
                handle.flush()
                os.fsync(handle.fileno())
            Path(temporary_name).replace(output)
            print("The previous generated header was restored.")
        else:
            output.unlink(missing_ok=True)
        raise

    print("\nSUCCESS - AIRPORT DATABASE READY")
    print("\nNext steps:")
    print("  1. Build the normal PlatformIO project.")
    print("  2. Upload by USB or use the generated release/firmware.radarota.")
    print("  3. Enter the same home coordinates on the radar's System page.")
    print("\nChanging coordinates later within this region does not require")
    print("running this tool again. Run it again only for a different region.")
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
