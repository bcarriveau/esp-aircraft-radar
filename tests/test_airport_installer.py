#!/usr/bin/env python3
"""Focused structural checks for Product 87 airport package installation."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "include" / "airport_store.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "src" / "airport_store.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing required installer safeguard: {needle}"


def main() -> None:
    require(HEADER, "size_t maxPackageSize();")
    require(HEADER, "bool installPackage(")
    require(SOURCE, "validatePackageBuffer(")
    require(SOURCE, "calculatePayloadDigest(")
    require(SOURCE, "airport_package_format::parseHeader(")
    require(SOURCE, "airport_package_format::parseRecord(")
    require(SOURCE, "esp_partition_erase_range(")
    require(SOURCE, "esp_partition_write(")
    require(SOURCE, "if (!initialize())")
    require(SOURCE, "Airport package readback does not match uploaded package")

    # The complete package must be validated before the first destructive flash
    # operation. This guards malformed/truncated mobile uploads from replacing a
    # known-good regional database.
    validate_call = SOURCE.index("if (!validatePackageBuffer(")
    erase_call = SOURCE.index("esp_partition_erase_range(")
    write_call = SOURCE.index("esp_partition_write(")
    assert validate_call < erase_call < write_call

    # Product identity is intentionally advanced only for the installer layer.
    assert 'FIRMWARE_VERSION_CODE = 87' in BUILD
    assert 'PRODUCT87-AIRPORT-PACKAGE-INSTALLER' in BUILD

    # The installer must not allocate the package internally; the future web
    # upload layer owns a bounded PSRAM buffer.
    assert "heap_caps_malloc" not in SOURCE
    assert "malloc(" not in SOURCE
    assert "new " not in SOURCE

    print("Product 87 airport package installer checks passed")


if __name__ == "__main__":
    main()
