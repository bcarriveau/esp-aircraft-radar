#!/usr/bin/env python3
"""Focused checks for Product 90 airport upload handoff timing."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing Product 90 upload-handoff requirement: {needle}"


def main() -> None:
    require(BUILD, "FIRMWARE_VERSION_CODE = 90")
    require(BUILD, "PRODUCT90-AIRPORT-UPLOAD-HANDOFF")

    # Airport control requests now use the same bounded retry wrapper as firmware.
    require(OTA, "async function callOnce(path,options={})")
    require(OTA, "async function call(path,options={},attempts=3)")
    require(OTA, "await call('/prepare',{method:'POST'},3)")
    require(OTA, "await call('/status',{},3)")
    require(OTA, "await call('/airports/status',{},3)")

    # Preserve the proven READY -> 500 ms settle -> large POST handoff.
    ready = OTA.index("await waitReady();el('status').textContent='Radar ready. Settling web connection before upload...';await sleep(500)")
    post = OTA.index("const j=await uploadBlob(blob)")
    assert ready < post

    # A retry is permitted only if the browser never observed upload bytes and
    # the ESP independently reports READY with zero airport bytes received.
    require(OTA, "let transferStarted=false")
    require(OTA, "if(e.loaded>0)transferStarted=true")
    require(OTA, "if(e.httpStatus||e.transferStarted||attempt+1>=2)throw e")
    require(OTA, "if(ota.state!=='READY'||received!==0)")
    require(OTA, "Upload connection reset before transfer; retrying once...")

    # Existing firmware uploader timing remains present and unchanged.
    require(OTA, "status.textContent='Uploading and validating firmware...';await sleep(500);const j=await upload(file)")

    # Product 89 browser generation and manual fallback remain present.
    require(OTA, "function buildRadarapt(")
    require(OTA, "BUILD &amp; INSTALL AIRPORT DATABASE")
    require(OTA, "INSTALL SELECTED PACKAGE")

    print("Product 90 airport upload handoff checks passed")


if __name__ == "__main__":
    main()
