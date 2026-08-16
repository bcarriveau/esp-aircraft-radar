# Stable GitHub release and on-device installation

This document describes the current Product 97 release boundary for Bill's
Waveshare ESP32-S3-Touch-LCD-7 Aircraft Radar.

## Three installation paths, one public producer

Product 97 distinguishes three operations that must not be treated as interchangeable:

1. **Destructive factory install / fresh start** — erases the complete flash and installs the verified distribution factory image.
2. **Private development build** — may use ignored `include/config.h`, normally preserves NVS, and never produces public OTA/factory release assets.
3. **Public GitHub OTA** — produced only by the distribution environment and updates the application slot without intentionally erasing owner NVS or airport storage.

## Two build environments, one public producer

The repository has two distinct PlatformIO environments:

```text
waveshare-s3-touch-lcd-7
waveshare-s3-touch-lcd-7-factory
```

The first is the private development build. It may use the ignored
`include/config.h` and it must not generate public release artifacts.

The second is the credential-safe distribution build. It defines
`RADAR_DISTRIBUTION_BUILD=1`, resolves `config.h` from `include/distribution`, uses
neutral Wi-Fi/location/MQTT defaults. Airport data is never compiled into either
build variant; both use only the persistent airport partition.

Only the distribution environment runs the public release post-build generators.
The application image contains `RADAR-DISTRIBUTION-BUILD`; the OTA packager and
factory-bundle generator independently refuse firmware without that marker.

A matching Product build ID alone is not sufficient proof that an image is safe for
public distribution.

## Stable OTA release assets

The distribution build generates the Product-numbered OTA package and fixed-name
manifest:

```text
release/waveshare-esp32-s3-touch-lcd-7-product-97.radarota
release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

The manifest remains bounded to the firmware's supported schema and contains the
matching Product/version, build ID, hardware ID, package/firmware sizes and SHA-256
digests, updater minimum, channel, tag, asset name, and bounded release notes.

Do not rename or hand-edit generated release assets. Do not publish a `.radarota`
from the private environment. `include/config.h` is private input only and must never
be committed, packaged, or used to generate GitHub release artifacts.

## OTA installation remains non-destructive

The local browser updater and GitHub installer accept only the validated Bill's
Radar `.radarota` format. The installer verifies package/image identity, exact
lengths, ESP32-S3 application image identity, build ID, and SHA-256 values before
selecting the inactive OTA slot.

Normal Product OTA writes the application slot only. It does not intentionally erase
NVS or the dedicated airport-data partition. Owner Wi-Fi/location and an installed
regional airport database are therefore expected to survive ordinary Product
updates.

The existing hardened transport and ownership rules remain unchanged: Core-0 owns
the network operation, ADS-B requests do not overlap, native verified HTTPS remains
preferred, bounded fallback rules remain intact, and stale/last-good protections are
preserved.

## Destructive factory release bundle

The same distribution build also generates:

```text
release/factory/product-97/
```

containing the exact bootloader, partition table, OTA bootstrap, application image,
a factory manifest, the Product 97 browser installer, and the offline recovery
PowerShell installer.

The factory manifest uses a fixed approved layout:

```text
0x00000000  bootloader.bin
0x00008000  partitions.bin
0x0000E000  boot_app0.bin
0x00010000  firmware.bin
```

The bundle records SHA-256 and size for every image. Factory generation refuses a
firmware image without the distribution provenance marker.

Product 97 also places a generated single-file `INSTALL_RADAR.html` in the factory
bundle. Owners can double-click that file in current Chrome or Edge; no Python or
local web server is required. The page verifies the same manifest/images, performs
the full erase and flash, verifies writes, explicitly pulses the ESP32-S3 EN/reset
line for boot, then releases the reset/boot control lines and closes Web Serial.

A true factory installation is intentionally destructive: it erases the complete
16 MB flash chip before writing the verified distribution images. That removes NVS,
saved Wi-Fi/location, MQTT/Home Assistant state, the persistent airport database,
OTA state, and all other owner-specific flash contents.

## Product 97 browser factory installer

Product 97 makes the browser/Web Serial path the preferred owner interface. The
generated factory bundle includes the single owner-facing file:

```text
INSTALL_RADAR.html
```

The repository keeps `factory-installer.js` as maintainable source, and the factory
generator embeds it into the generated HTML so local owners can double-click the
installer without starting a web server. The browser installer pins Espressif
`esptool-js` 0.6.0 and consumes the same
`factory-manifest.json` and four binaries as the PowerShell recovery path.

Before destructive work it verifies the fixed manifest/hardware/layout, every image
size and SHA-256, the distribution marker and build ID in `firmware.bin`, then
positively checks the connected chip is ESP32-S3 with exactly 16 MB flash. The owner
must also acknowledge the destructive warning and type `ERASE RADAR`.

The browser performs an explicit full-chip erase before writing. `esptool-js` is
given an MD5 callback so the library compares each written image against the
flash-side MD5 before proceeding. After all four images finish successfully, the
installer drives IO0 inactive, holds EN/reset low for a bounded pulse, releases EN,
waits for boot to begin, then closes Web Serial.

The PowerShell installer remains in the same bundle for offline/developer recovery;
it is not the normal owner-facing path.

See `docs/FACTORY_INSTALL.md` for browser loading/hosting details and the complete
distinction between distribution build, PlatformIO upload, destructive factory
install, and normal OTA.

## Private development firmware after a factory fresh start

A destructive factory boot intentionally writes neutral owner values: blank Wi-Fi
SSID/password and `0,0` location. If the next firmware flashed is the private
development build and that complete neutral tuple is still present, Product 97
seeds the private `config.h` Wi-Fi/password/location and MQTT enabled default.

This reseed code is compiled only when `RADAR_DISTRIBUTION_BUILD` is absent. Any
non-neutral owner state disables the reseed, so ordinary private uploads preserve
settings. Distribution firmware compiles this path out completely, preventing
private defaults from becoming part of public OTA behavior.

## Important: PlatformIO factory upload is not a factory reset

Running:

```text
pio run -e waveshare-s3-touch-lcd-7-factory -t upload
```

builds/uploads the credential-safe distribution firmware, but PlatformIO/esptool
normally erases only the regions being written. Existing NVS and airport-partition
contents may survive that operation.

Do not use a normal PlatformIO upload as evidence of virgin first-owner behavior.
Use the destructive factory-install path when the test requires a genuinely erased
unit.

## Remote GitHub update flow

A compatible newer release enables explicit user-confirmed installation. Before
writing firmware, the updater revalidates release identity and package metadata.
The package transport remains bounded and verified; the full package is not kept in
internal RAM, and flash writes retain the established internal-RAM staging required
by the ESP32-S3 flash/cache behavior.

The inactive partition becomes the boot partition only after complete validation and
`esp_ota_end()` success. Earlier transport, framing, identity, digest, write, or
finalization failures leave the current boot partition active.

No update is silently installed merely because files exist under `release/`.

## Publishing checklist

Before publishing a stable Product release:

1. Confirm the intended branch and Product/build marker.
2. Build `waveshare-s3-touch-lcd-7-factory`, not the private environment.
3. Confirm the build log identifies the distribution variant/marker.
4. Run focused release/factory tests.
5. Confirm generated `.radarota`, release manifest, factory manifest, and browser
   installer agree on Product/build identity and the fixed hardware/layout.
6. Physically test the normal non-destructive OTA path when the Product changes OTA
   behavior or release packaging.
7. Physically test the Product 97 browser destructive factory install in current
   Chrome or Edge before publishing it as the normal owner path.
8. Confirm the destructive test boots with no prior Wi-Fi/location, no installed
   airport database, neutral integration defaults, and the intended Product marker.
9. Keep the PowerShell path as an offline recovery check when factory packaging
   changes.
10. Publish the matching stable tag/release only after those checks pass and attach
    only assets generated from the verified distribution build.

Public-key package signing remains a separate future hardening phase; TLS plus the
current SHA-256 checks protect integrity and mismatch detection but do not create an
independent signing authority from the repository account.
