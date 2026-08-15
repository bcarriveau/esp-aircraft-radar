# Factory installation and distribution firmware

Product 94 separates three operations that must not be confused:

1. **Private development build** — `waveshare-s3-touch-lcd-7`
2. **Distribution build** — `waveshare-s3-touch-lcd-7-factory`
3. **Destructive factory install** — full-chip erase followed by the verified distribution factory image

The PlatformIO environment name is historical. Building or uploading the
`waveshare-s3-touch-lcd-7-factory` environment by itself is **not** a factory reset.
A normal PlatformIO upload erases only the regions it writes and can leave NVS and
the dedicated airport partition intact.

## Distribution build boundary

Only `waveshare-s3-touch-lcd-7-factory` may generate public release artifacts.
It defines `RADAR_DISTRIBUTION_BUILD=1`, places `include/distribution` before the
private include directory, and therefore uses neutral defaults:

- blank Wi-Fi SSID/password
- neutral `0,0` location
- MQTT disabled with no broker/user/password
- no compiled regional airport fallback

The firmware also contains the marker `RADAR-DISTRIBUTION-BUILD`. Public OTA and
factory-bundle generation refuse firmware that does not contain that marker.

The normal `waveshare-s3-touch-lcd-7` environment remains private and does not run
public release post-build scripts.

## Destructive factory install

A true factory install is for a brand-new unit, recovery from unknown/corrupt flash,
or an intentional complete owner-data reset.

It performs a **full 16 MB chip erase** before writing:

| Address | File |
| --- | --- |
| `0x00000000` | `bootloader.bin` |
| `0x00008000` | `partitions.bin` |
| `0x0000E000` | `boot_app0.bin` |
| `0x00010000` | `firmware.bin` |

That erase intentionally removes:

- saved Wi-Fi
- saved home location
- MQTT/Home Assistant state
- installed regional airport database
- OTA state
- every other owner-specific value stored in flash

After a successful factory install the unit should boot as a new-owner distribution
unit and require normal setup.

## Generated factory bundle

A successful distribution build runs both Product 94 post-build generators:

- `scripts/build_radar_ota.py`
- `scripts/build_factory_bundle.py`

The factory generator creates:

```text
release/factory/product-94/
```

with:

- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `factory-manifest.json`
- `FLASH_RADAR_FACTORY.ps1`

The generator refuses a firmware image that lacks the distribution marker. The
manifest records the exact Product/build identity, fixed hardware requirements,
fixed flash layout, file sizes, and SHA-256 hashes.

The PowerShell installer is an **offline/developer recovery path**, not the desired
long-term public owner interface. It validates the manifest, exact fixed offsets,
all hashes, the distribution marker and build ID in `firmware.bin`, then positively
identifies an ESP32-S3 and an actual detected 16 MB flash-size line before allowing
the destructive erase.

A future public browser/Web Serial installer should consume this same generated
factory bundle and manifest so the browser and offline recovery path use identical
verified binaries and layout. It must retain the same destructive warning and
hardware/provenance checks.

## PlatformIO upload is not a factory reset

For developer testing, this command:

```text
pio run -e waveshare-s3-touch-lcd-7-factory -t upload
```

uploads the distribution-flavored firmware but does not issue a whole-chip
`erase_flash`. Existing NVS values and the airport partition may therefore survive.
That behavior is useful for development but must not be presented as a clean
new-owner factory installation.

## Normal Product OTA is non-destructive

The `.radarota` path is intentionally different from factory installation.
The verified updater writes the inactive application slot and then selects it for
boot. It does not intentionally erase NVS or the dedicated airport partition.

Normal Product updates should therefore preserve owner Wi-Fi/location and an
installed regional airport database.

## First-owner sequence

After a true factory install:

1. Boot the radar and enter Wi-Fi/location through the normal setup UI.
2. Confirm ADS-B acquisition.
3. Open the Airport Database browser page during an armed maintenance window.
4. Build/install the owner's regional airport database.
5. Configure optional integrations when the product supports the desired runtime
   configuration path.
6. Use normal `.radarota`/GitHub Product updates afterward rather than factory erase.
