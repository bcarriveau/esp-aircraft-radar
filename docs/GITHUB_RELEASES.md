# Stable GitHub release and on-device installation

This document describes the current Product 94 release boundary for Bill's
Waveshare ESP32-S3-Touch-LCD-7 Aircraft Radar.

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
neutral Wi-Fi/location/MQTT defaults, and excludes the compiled regional airport
fallback.

Only the distribution environment runs the public release post-build generators.
The application image contains `RADAR-DISTRIBUTION-BUILD`; the OTA packager and
factory-bundle generator independently refuse firmware without that marker.

A matching Product build ID alone is not sufficient proof that an image is safe for
public distribution.

## Stable OTA release assets

The distribution build generates the Product-numbered OTA package and fixed-name
manifest:

```text
release/waveshare-esp32-s3-touch-lcd-7-product-94.radarota
release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

The manifest remains bounded to the firmware's supported schema and contains the
matching Product/version, build ID, hardware ID, package/firmware sizes and SHA-256
digests, updater minimum, channel, tag, asset name, and bounded release notes.

Do not rename or hand-edit generated release assets. Do not publish a `.radarota`
from the private environment.

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
release/factory/product-94/
```

containing the exact bootloader, partition table, OTA bootstrap, application image,
a factory manifest, and the offline recovery PowerShell installer.

The factory manifest uses a fixed approved layout:

```text
0x00000000  bootloader.bin
0x00008000  partitions.bin
0x0000E000  boot_app0.bin
0x00010000  firmware.bin
```

The bundle records SHA-256 and size for every image. Factory generation refuses a
firmware image without the distribution provenance marker.

A true factory installation is intentionally destructive: it erases the complete
16 MB flash chip before writing the verified distribution images. That removes NVS,
saved Wi-Fi/location, MQTT/Home Assistant state, the persistent airport database,
OTA state, and all other owner-specific flash contents.

See `docs/FACTORY_INSTALL.md` for the complete distinction between a distribution
build, PlatformIO upload, destructive factory install, and normal OTA.

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
5. Confirm generated `.radarota`, release manifest, and factory manifest agree on
   Product/build identity and hashes.
6. Physically test the normal non-destructive OTA path when the Product changes OTA
   behavior or release packaging.
7. Physically test a destructive factory install when the factory bundle/installer,
   partition layout, or first-owner setup path changes.
8. Confirm the destructive test boots with no prior Wi-Fi/location, no installed
   airport database, neutral integration defaults, and the intended Product marker.
9. Publish the matching stable tag/release only after those checks pass.
10. Attach only assets generated from the verified distribution build.

Public-key package signing remains a separate future hardening phase; TLS plus the
current SHA-256 checks protect integrity and mismatch detection but do not create an
independent signing authority from the repository account.
