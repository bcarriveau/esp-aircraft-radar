#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = (ROOT / "README.md").read_text(encoding="utf-8")
CHANGELOG = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")

def main():
    marker = "7IN-20260813-PRODUCT92-AIRPORT-LOCATION-PREFILL"
    commit = "e5afe84d8bb0cd56eb19900437b9a347e7605ee7"
    assert marker in README and marker in CHANGELOG
    assert commit in README and commit in CHANGELOG
    assert "airport-seperation" in README and "airport-seperation" in CHANGELOG
    assert "BUILD & INSTALL AIRPORT DATABASE" in README
    assert "historical/stale" in CHANGELOG
    assert "historical build artifacts" in README
    assert "Product 86" in CHANGELOG and "Product 92" in CHANGELOG
    assert "Product 15" in CHANGELOG
    assert "Product 78 is a focused replacement-source candidate" not in README
    assert "new generated header and build" not in README
    assert "Current replacement source: Product 84" not in CHANGELOG
    assert "Product 93" not in README
    assert "Product 93" not in CHANGELOG
    print("Product 92 README/CHANGELOG catch-up checks passed")

if __name__ == "__main__":
    main()
