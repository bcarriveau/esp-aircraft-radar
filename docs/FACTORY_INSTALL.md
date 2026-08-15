# Factory installation and distribution firmware

Product 95 keeps the Product 94 distribution safety boundary and adds the normal
end-user browser/Web Serial factory installer.

Three paths remain deliberately separate:

1. **Destructive factory install / fresh start** — full-chip erase followed by the verified distribution factory image. This intentionally destroys all owner state.
2. **Private development build** — `waveshare-s3-touch-lcd-7`. This may use the ignored private `include/config.h`, never produces public release assets, and normally preserves NVS.
3. **Public GitHub OTA / distribution build** — generated only by `waveshare-s3-touch-lcd-7-factory`. Normal `.radarota` installation updates the application slot without intentionally erasing owner NVS or airport storage.

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
public release post-build scripts. `include/config.h` remains ignored and must never
be packaged, committed, or used to produce GitHub OTA/factory release artifacts.

### Private build after a destructive factory install

A true factory boot creates the neutral distribution owner tuple: blank Wi-Fi SSID,
blank Wi-Fi password, latitude `0`, and longitude `0`. If the next firmware flashed
is the private development build and that complete neutral tuple is still present,
Product 95 seeds Wi-Fi/password/location and the private MQTT enabled default from
the private build's `config.h`.

That reseed path is compiled only when `RADAR_DISTRIBUTION_BUILD` is **not** defined.
It does not run if any of the neutral owner values have changed, so ordinary private
firmware uploads continue preserving owner-entered NVS. The public distribution
build compiles the reseed path out completely.

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
bundle as one owner-facing file:

```text
INSTALL_RADAR.html
```

The factory generator embeds the repository's `factory-installer.js` source inside
that generated HTML. Owners can therefore double-click `INSTALL_RADAR.html`; no
Python/local web server, PlatformIO, PowerShell, or manually entered COM port is
required. The embedded module loads the pinned Espressif `esptool-js` 0.6.0 library
from HTTPS.

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
before the installer proceeds. After all images verify, the browser explicitly keeps
IO0 inactive, pulses the ESP32-S3 EN/reset line low, releases EN, allows the board to
begin booting, then releases the reset/boot control lines and closes Web Serial.

There is no automatic retry after destructive work. A failed/interrupted install
requires an intentional reconnect/retry or the offline recovery installer.

### Browser use

For an extracted/local Product factory bundle, double-click `INSTALL_RADAR.html` in
current Chrome or Edge, choose the same `product-95` folder when prompted, then
connect the radar. This is the normal offline owner workflow.

When the same bundle is hosted over HTTPS, `LOAD ADJACENT BUNDLE` may load the
manifest and four images directly from the page's directory instead.

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

## Normal public GitHub Product OTA is non-destructive

The `.radarota` path is intentionally different from factory installation. Public
OTA packages are produced only by the distribution environment, never the private
build. The verified updater writes the inactive application slot and then selects it
for boot. It does not intentionally erase NVS or the dedicated airport partition.

Normal public Product updates therefore preserve each owner's Wi-Fi/location and
installed regional airport database. They do not contain or restore the developer's
private `config.h` values.

## First-owner sequence

After a true factory install:

1. Boot the radar and enter Wi-Fi/location through the normal setup UI.
2. Confirm ADS-B acquisition.
3. Open the Airport Database browser page during an armed maintenance window.
4. Build/install the owner's regional airport database.
5. Configure optional integrations when the product supports the desired runtime
   configuration path.
6. Use normal `.radarota`/GitHub Product updates afterward rather than factory erase.
