#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = (ROOT / "README.md").read_text(encoding="utf-8")
CHANGELOG = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")

def main():
    marker = "7IN-20260813-PRODUCT92-AIRPORT-LOCATION-PREFILL"
    assert marker in README and marker in CHANGELOG
    assert "**Current commit:**" not in CHANGELOG
    current_block = README.split("## Current status", 1)[1].split("## Core features", 1)[0]
    assert "e5afe84d8bb0cd56eb19900437b9a347e7605ee7" not in current_block
    assert "Active `release/` policy" in README
    assert "Active release-artifact policy" in CHANGELOG
    assert "release/waveshare-esp32-s3-touch-lcd-7-product-92.radarota" in README
    assert "release/waveshare-esp32-s3-touch-lcd-7.manifest.json" in README
    assert "many Product-numbered `.radarota` files" not in README
    assert "Product 93" not in README
    assert "Product 93" not in CHANGELOG
    print("Final Product 92 repository-housekeeping documentation checks passed")

if __name__ == "__main__":
    main()
