#!/usr/bin/env python3
"""Focused static checks for Product 88 mobile airport upload."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
STORE = (ROOT / "src" / "airport_store.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing Product 88 requirement: {needle}"


def main() -> None:
    require(BUILD, "FIRMWARE_VERSION_CODE = 88")
    require(BUILD, "PRODUCT88-AIRPORT-WEB-UPLOAD")
    require(OTA, 'server.on("/airports", HTTP_GET')
    require(OTA, 'server.on("/airports/status", HTTP_GET')
    require(OTA, 'server.on("/airports/upload", HTTP_POST')
    require(OTA, 'accept=".radarapt,application/octet-stream"')
    require(OTA, 'name="viewport"')
    require(OTA, "@media(max-width:500px)")
    require(OTA, "airport_store::maxPackageSize()")
    require(OTA, "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT")
    require(OTA, "airport_store::installPackage(")
    require(OTA, "upload.totalSize != airportUploadReceived")
    require(OTA, "Airport upload was interrupted; stored database unchanged")
    require(OTA, "Restart radar to activate it")
    require(STORE, "FLASH_ERASE_SECTOR_SIZE = 4096")
    require(STORE, "const size_t eraseSize =")
    require(STORE, "esp_partition_erase_range(partitionHandle, 0, eraseSize)")
    assert "esp_partition_erase_range(partitionHandle, 0, partitionHandle->size)" not in STORE

    # Browser upload must stage the full package before the destructive installer.
    copy_pos = OTA.index("memcpy(airportUploadBuffer + airportUploadReceived")
    install_pos = OTA.index("airport_store::installPackage(")
    assert copy_pos < install_pos

    # Existing firmware OTA endpoints remain present.
    for route in ('"/prepare"', '"/status"', '"/cancel"', '"/upload"', '"/update"'):
        require(OTA, route)
    require(OTA, "esp_ota_begin(")
    require(OTA, "esp_ota_set_boot_partition(")
    require(OTA, "parkCoreOneForRestart()")

    print("Product 88 mobile airport upload structural checks passed")


if __name__ == "__main__":
    main()
