#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = (ROOT / "docs" / "AIRPORT_DATABASE.md").read_text(encoding="utf-8")
IGNORE = (ROOT / ".gitignore").read_text(encoding="utf-8")

def main() -> None:
    required = (
        "BUILD & INSTALL AIRPORT DATABASE",
        "automatically restarts the radar",
        "All firmware variants use that same persistent airport partition",
        "does **not** rebuild firmware",
        "PREPARE -> READY -> bounded settle -> multipart upload",
        "settings",
        "persistent `airports`",
    )
    for text in required:
        assert text in DOC, f"missing current airport documentation: {text}"

    stale = (
        "Generate a new regional airport table on the",
        "then build and upload the firmware again",
        "replaces only:",
        "new generated header and build",
        "compiled region needs only",
        "compiled fallback remains available",
    )
    for text in stale:
        assert text not in DOC, f"stale pre-separation wording remains: {text}"

    assert "prototype" not in IGNORE.lower()
    assert "/release/airports.radarapt" in IGNORE
    assert "/APPLY_AIRPORT_PRODUCT92.ps1" in IGNORE
    print("Airport separation documentation cleanup checks passed")

if __name__ == "__main__":
    main()
