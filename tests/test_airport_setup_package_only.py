#!/usr/bin/env python3
"""Verify the guided airport setup is package-only after separation."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SETUP = (ROOT / "tools" / "airport_database_setup.py").read_text(encoding="utf-8")
BAT = (ROOT / "tools" / "Build Airport Database.bat").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing package-builder requirement: {needle}"


def main() -> None:
    require(SETUP, 'root / "release" / "airports.radarapt"')
    require(SETUP, "build_binary_package(")
    require(SETUP, "write_package_atomic(")
    require(SETUP, "parse_package(")
    require(SETUP, "validate_written_package(")
    require(SETUP, "test_airport_package.py")
    require(SETUP, "test_airport_generator.py")
    require(SETUP, "does NOT rebuild firmware")
    require(SETUP, "does NOT change the radar's saved")
    require(SETUP, "does not store the center/home coordinates")
    require(BAT, r"release\airports.radarapt")

    # The guided setup must no longer modify the compiled fallback header.
    assert "generated_airport_database.h" not in SETUP
    assert "build_header" not in SETUP
    assert "write_header_atomic" not in SETUP

    print("Package-only airport setup checks passed")


if __name__ == "__main__":
    main()
