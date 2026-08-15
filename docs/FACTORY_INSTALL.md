# Factory installation and distribution firmware

Product 95 keeps the Product 94 distribution safety boundary and adds the normal
end-user browser/Web Serial factory installer.

Three operations remain deliberately separate:

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

The firmware contains the marker `RADAR-DISTRIBUTION-BUILD`. Public OTA and
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

After a successful factory install the unit boots as a new-owner distribution unit
and requires normal setup.

## Product 95 browser installer

The preferred owner path is now the browser installer generated into each factory
bundle:

```text
INSTALL_RADAR.html
factory-installer.js
```

The browser installer uses Web Serial through a pinned Espressif `esptool-js` 0.6.0
module. It does not require PlatformIO, Python, PowerShell, or manually entering a
COM port.

Before destructive work it verifies:

- factory manifest schema and hardware ID
- exact four-file flash layout and fixed addresses
- exact file sizes
- SHA-256 for every flash image
- `RADAR-DISTRIBUTION-BUILD` inside `firmware.bin`
- the manifest build ID inside `firmware.bin`
- connected chip is positively identified as `ESP32-S3`
- detected flash size is exactly `16MB`
- the owner has explicitly checked the destructive warning and typed `ERASE RADAR`

Only after every package and hardware check passes does the browser call a complete
chip erase. It then writes the four verified distribution images. `esptool-js` is
given an MD5 callback so each written image is compared against the flash-side MD5
before the installer proceeds. After all images verify, the browser hard-resets the
radar and reports completion.

There is no automatic retry after destructive work. A failed/interrupted install
requires an intentional reconnect/retry or the offline recovery installer.

### Browser use

The browser page supports two package-loading modes:

1. **Hosted beside the bundle** — use `LOAD ADJACENT BUNDLE`; the page fetches the
   manifest and four binary files from the same directory.
2. **Local/extracted bundle** — use `CHOOSE FACTORY BUNDLE` and select the extracted
   Product factory folder. The browser reads and verifies the files locally.

Web Serial requires a browser/context that exposes the API. The intended public
host is HTTPS in current Chrome or Edge. Local `file://` use may be available where
the browser treats local files as a trustworthy context; otherwise serve the page
from HTTPS or localhost.

## Generated factory bundle

A successful distribution build runs both release generators:

- `scripts/build_radar_ota.py`
- `scripts/build_factory_bundle.py`

For Product 95 the factory generator creates:

```text
release/factory/product-95/
```

with:

- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `factory-manifest.json`
- `INSTALL_RADAR.html`
- `factory-installer.js`
- `FLASH_RADAR_FACTORY.ps1`

The manifest records Product/build identity, hardware requirements, fixed flash
layout, file sizes, SHA-256 hashes, and the pinned browser-installer version.

The PowerShell installer remains an **offline/developer recovery path**. It consumes
the same manifest and identical verified binaries as the browser path. Product 95
does not maintain a second firmware image or alternate flash layout for the browser.

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
