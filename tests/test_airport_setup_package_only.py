#!/usr/bin/env python3
"""Verify the guided airport setup is package-only after separation."""

from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import airport_database_setup  # noqa: E402
from airport_package import build_package, write_package_atomic  # noqa: E402

SETUP = (TOOLS / "airport_database_setup.py").read_text(encoding="utf-8")
BAT = (ROOT / "tools" / "Build Airport Database.bat").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing package-builder requirement: {needle}"



def verify_written_package_api() -> None:
    airport = SimpleNamespace(
        ident="KTEST",
        name="TEST AIRPORT",
        latitude=42.0,
        longitude=-88.0,
        elevation=800,
        runway_length=5000,
        runway_heading=180,
        category=1,
    )
    package = build_package(
        [airport],
        database_date="2026-08-16",
        coverage="TEST",
        radius_miles=120,
        generator_version=3,
    )
    with tempfile.TemporaryDirectory() as temporary_name:
        output = Path(temporary_name) / "airports.radarapt"
        write_package_atomic(output, package)
        airport_database_setup.validate_written_package(output, 1, 120)


def main() -> None:
    require(SETUP, 'root / "airports.radarapt"')
    require(SETUP, "build_binary_package(")
    require(SETUP, "write_package_atomic(")
    require(SETUP, "parse_package(")
    require(SETUP, "validate_written_package(")
    require(SETUP, "test_airport_package.py")
    require(SETUP, "test_airport_generator.py")
    require(SETUP, "does NOT rebuild firmware")
    assert 'root / "release" / "airports.radarapt"' not in SETUP
    require(SETUP, "does NOT change the radar's saved")
    require(SETUP, "does not store the center/home coordinates")
    require(BAT, r"release\airports.radarapt")

    # The guided setup must no longer modify the compiled fallback header.
    assert "generated_airport_database.h" not in SETUP
    assert "build_header" not in SETUP
    assert "write_header_atomic" not in SETUP
    assert "parsed.header" not in SETUP
    assert "parsed.records" not in SETUP

    verify_written_package_api()
    print("Package-only airport setup checks passed")


if __name__ == "__main__":
    main()
