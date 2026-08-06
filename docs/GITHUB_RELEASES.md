# Stable GitHub release and on-device installation

Product 73 extends the Product 72 bounded GitHub release checker with an explicit,
user-confirmed remote installation path. The existing local browser OTA remains
available and retains priority as the recovery and manual installation method.
No update is downloaded or installed automatically.

## Release discovery

The existing Core-0 ADS-B owner may claim a disposable metadata check only after
a successful current-generation ADS-B publication and while the established
network serialization is active.

Automatic checks require:

- at least five stable minutes since boot
- no completed attempt within approximately 24 hours
- a successful current-generation ADS-B fetch
- at least eight seconds before the next fixed 15-second ADS-B start
- connected Wi-Fi with no recovery in progress
- local browser OTA inactive
- MQTT not starting, connecting, stopping, or in maintenance

**CHECK NOW** bypasses only the five-minute and 24-hour timers. It still uses all
network and cadence safety gates.

A metadata check has:

- a six-second absolute ceiling
- a 2.5-second connect/header ceiling reduced by remaining total budget
- a 1.5-second body-idle ceiling
- a 1.5-second guard before the next ADS-B poll
- at most three HTTPS redirects
- a 2048-byte manifest body limit
- a 16384-byte aggregate header limit
- a 4095-character redirect URL limit
- a URL-sized ESP-IDF transmit buffer from 1024 through 4607 bytes

## Explicit installation flow

A compatible newer release enables **DOWNLOAD & INSTALL**. The first tap arms a
15-second **CONFIRM INSTALL** state. A second tap queues installation for the next
successful current-generation ADS-B cycle. **LATER** only closes the detail panel
and performs no network or persistent action.

Before opening the firmware asset, Product 73 downloads and validates the stable
manifest again. The retained release identity includes:

- numeric Product version
- build ID
- package size
- firmware size
- package SHA-256
- firmware SHA-256

If any retained identity value changed, the install is cancelled, the newly
validated release is shown, and another confirmation is required. Update cache
schema 3 stores this identity; older cache formats are ignored.

## Remote package transport

The installer constructs only the deterministic asset URL declared by the
validated manifest. Transport uses native ESP-IDF HTTPS with the certificate
bundle and hostname validation. Redirects must remain HTTPS and are restricted
to:

- `github.com`
- `objects.githubusercontent.com`
- `release-assets.githubusercontent.com`

The package transport has:

- maximum three redirects
- 4095-character URL ceiling
- URL-sized 1024–4607-byte transmit buffer
- 16384-byte aggregate streamed-header ceiling
- strict rejection of conflicting Content-Length and Transfer-Encoding
- exact manifest package length when Content-Length is present
- 4096-byte PSRAM receive buffer
- 1024-byte internal-RAM flash-write staging buffer
- eight-second connect/header ceiling per request
- fifteen-second body-idle ceiling
- three-minute absolute installation ceiling

The full package is never allocated in RAM. The HTTPS receive buffer remains in
PSRAM, but every `esp_ota_write()` source is copied into internal RAM first because
flash operations can make external RAM unavailable while cache access is paused.

## Package verification and partition selection

The installer accepts only the generated Bill's Radar `.radarota` format:

- magic `BILLS-RADAR-OTA`
- format version 1
- 512-byte package header
- hardware `WAVESHARE-ESP32-S3-LCD-7`
- exact build ID from the fresh manifest
- exact firmware size and firmware digest from the fresh manifest

While streaming, it verifies:

- complete package SHA-256
- complete firmware SHA-256
- exact received and written byte counts
- ESP application image magic
- ESP32-S3 image chip identifier
- declared build ID embedded in the firmware payload
- ESP-IDF image finalization through `esp_ota_end()`

The inactive partition is selected with `esp_ota_set_boot_partition()` only after
all transport, package, image, size, build, and digest checks succeed. Any earlier
failure aborts the active OTA handle and leaves the current boot partition active.

## Ownership, cancellation, and restart

The existing Core-0 ADS-B task performs the intentional installation, so no later
ADS-B request can overlap it. MQTT service remains gated for the installation and
restart handoff. The last-good aircraft snapshot remains displayed.

Cancellation is checked between bounded 4096-byte transport blocks; each flash
call inside a block is independently bounded to 1024 bytes. User cancellation, range/reconnect commands, or local browser OTA stop the
remote operation at the next bounded point. Local browser OTA retains priority.
Wi-Fi loss, transport failure, timeout, framing failure, validation failure, or
flash failure is reported as an installation failure.

After a completely verified write, Product 73 mirrors the hardened local-OTA
restart design: Core 1 settles network activity, creates a high-priority Core-0
restart task from internal RAM, and parks Core 1 in IRAM before `esp_restart()`.
If the restart handoff cannot be completed, the verified boot partition remains
selected and the display directs the user to power-cycle the radar.

## Release identity and required assets

Publish a normal, non-draft, non-prerelease release with matching names:

```text
Tag:          product-73
Release name: Product 73
Channel:      stable
```

PlatformIO's post-build hook writes only the two repository release assets:

```text
release/waveshare-esp32-s3-touch-lcd-7-product-73.radarota
release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

Use the Product-numbered `.radarota` for both the local browser OTA page and the
GitHub Release. The browser accepts the full versioned filename, so no generic
`release/firmware.radarota` copy is generated. Attach the fixed-name manifest and
the matching Product-numbered package to the GitHub Release. Do not rename or
hand-edit either generated asset.

The manifest remains compact ASCII JSON, schema 1, and at most 2048 bytes. It
contains the exact tag, hardware, stable channel, numeric version, readable
label, build ID, asset name, package/firmware sizes and SHA-256 digests, minimum
updater version, and bounded release notes.

## Authenticity boundary

Verified TLS and both SHA-256 checks protect against corruption, truncation, and
accidental asset mismatch. Because the manifest and package are published under
the same repository account, their hashes are not independent protection against
repository or publishing-account compromise. Public-key package signing remains
a separate future hardening phase.

## Publishing and physical verification

1. Build the exact intended Product 73 source and confirm
   `7IN-20260804-PRODUCT73-GITHUB-OTA-INSTALL` at boot.
2. Confirm the existing local browser OTA accepts the generated
   Product-numbered `.radarota` package.
3. Publish Product 73 only after local installation and normal radar regression
   checks pass.
4. Exercise remote installation with a later compatible Product release, because
   the installed Product 73 must see a numerically newer manifest.
5. Verify confirmation expiry, cancellation, manifest-change rejection, package
   progress, automatic restart, boot marker, network recovery, and soak behavior.
