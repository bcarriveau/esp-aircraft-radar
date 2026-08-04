# Stable GitHub Release format

Product 70 restores the bounded GitHub stable-release notifier on top of the
physically exercised Product 69 bounded ADS-B transport. It checks release
metadata only. Firmware installation remains the existing local browser OTA.

## Runtime behavior

No new task, timer callback, animation, or background polling loop is created.
The existing Core-0 ADS-B owner may claim one disposable GitHub check only after
a successful current-generation ADS-B fetch and only when all normal network
owners are serialized.

Automatic checks require:

- at least five stable minutes since boot
- no completed automatic or manual attempt within approximately 24 hours
- a successful current-generation ADS-B fetch
- at least eight seconds before the next fixed 15-second ADS-B start
- connected Wi-Fi with no recovery in progress
- local browser OTA inactive
- MQTT not starting, connecting, stopping, or in maintenance

The System-page **CHECK NOW** action bypasses only the five-minute and 24-hour
timers. It still waits for the next successful ADS-B cycle and all safety gates.
The UI and serial log identify queued, checking, deferred, aborted, current,
available, no-release, and failed states.

A claimed check has:

- a six-second absolute ceiling
- a 2.5-second connect/header ceiling reduced by remaining total budget
- a 1.5-second response-body idle ceiling
- a 1.5-second guard before the next ADS-B poll
- at most three HTTPS redirects
- a 2048-byte manifest body limit
- a 4096-byte aggregate header limit

MQTT service calls are skipped only while the disposable check owns the network.
The check is claimed while the existing ADS-B exclusion is still active, so no
MQTT socket operation can enter between ADS-B and GitHub. The normal ADS-B
`fetchInProgress` state is not faked, so the radar does not show a false
UPDATING state.

A range refresh, Wi-Fi reconnect, or browser OTA request aborts the check at the
next bounded transport boundary. A user-requested CHECK NOW is requeued after a
safe command/OTA abort and resumes at the next eligible ADS-B window. Automatic
checks simply remain due. Command/OTA aborts do not increment ADS-B failures,
change recovery counters, clear last-good aircraft, or consume the 24-hour check
allowance. A true timeout, HTTP error, invalid manifest, or transport failure is
recorded as a completed attempt and is throttled.

## Release identity

Publish a normal, non-draft, non-prerelease GitHub Release with matching names:

```text
Tag:          product-70
Release name: Product 70
Channel:      stable
```

The firmware compares the numeric `version_code`; it never orders build-marker
strings alphabetically.

## Required assets

PlatformIO's existing post-build script creates all three files:

```text
release/firmware.radarota
release/waveshare-esp32-s3-touch-lcd-7-product-70.radarota
release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

Attach the versioned `.radarota` and fixed-name manifest to the Product 70 GitHub
Release. `release/firmware.radarota` remains the local browser-install fallback.
Do not rename or hand-edit generated assets.

## Manifest contract

The manifest is compact ASCII JSON, schema 1, and at most 2048 bytes. It contains:

- exact release tag
- exact hardware identifier
- stable channel
- monotonic numeric Product version
- readable Product label
- embedded build ID
- exact firmware asset name
- package and firmware sizes
- package and firmware SHA-256 digests
- minimum supported updater version
- bounded plain-text release summary

The radar rejects unknown schemas, other hardware, other channels, inconsistent
Product fields, unsupported updater versions, oversized values, invalid digests,
ambiguous HTTP framing, non-HTTPS URLs, URL user information, explicit ports,
unsafe redirect hosts, and excessive redirects.

## User interface

The System page contains a real **CHECK NOW** button. Pressing it queues one check
and reports why it is waiting when a safety gate is active. A compatible newer
stable release shows one static green download icon in the header. Tapping the
icon opens release details. **LATER** only closes the detail view; it performs no
network or NVS operation and leaves an available-update icon visible.

Direct GitHub download and flash installation are intentionally absent. Use the
existing **FIRMWARE / OTA** page to install a downloaded `.radarota` file.

## Publishing checklist

1. Build the exact intended Product 70 source with the user-side PlatformIO environment.
2. Confirm `7IN-20260803-PRODUCT70-GITHUB-UPDATE-CHECK` at boot.
3. Run the checked-in host tests.
4. Confirm `release/firmware.radarota` still installs through local browser OTA.
5. Create a normal published `product-70` GitHub Release.
6. Attach the generated fixed manifest and Product 70 versioned package.
7. To test the green indicator, later publish a compatible Product 71 release.
8. Do not publish packages for other hardware under these asset names.

## Authenticity boundary

Verified TLS and SHA-256 protect against transport corruption, truncation, and
accidental mismatch. Because the manifest and firmware are published together,
the hashes are not independent protection against compromise of the repository
or publishing account. Public-key package signing remains separate future work.
