# Changelog

All notable **confirmed** changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
The authoritative numbered history begins with Product 15, the first hardened
version-controlled baseline. Earlier numbered history is intentionally not invented.

## Current status

- **Current Product:** Product 97
- **Build marker:** `7IN-20260816-PRODUCT97-UNIFIED-AIRPORT-STORAGE`
- **Current branch:** `main`
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262, GT911, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback:** Product 15 / `product-15-hardened`

### Active release-artifact policy

The active branch keeps only the current Product-numbered `.radarota` package and
its matching fixed-name manifest in `release/`.

The redundant tracked `release/firmware.radarota` copy and stale Product 70-91
packages were removed after Product 92. Historical firmware remains recoverable from
the Git history/tag/release associated with each Product.

Current firmware identity comes from `include/build_info.h` plus the matching
generated Product package/manifest. Documentation-only and housekeeping commits may
advance repository HEAD without creating a new firmware Product.


## Product 97 - 2026-08-16

**Build:** `7IN-20260816-PRODUCT97-UNIFIED-AIRPORT-STORAGE`

### Changed

- Removed the private-firmware runtime fallback to the compiled regional airport
  table. Private and public/distribution firmware now use only the validated
  persistent `airports.radarapt` store.
- Normal VS Code application uploads and normal `.radarota` updates therefore share
  the same airport-data behavior and leave the dedicated airport partition alone.
- The lower-level airport generator now writes a persistent `.radarapt` package by
  default instead of regenerating an application header.
- Retained `include/generated_airport_database.h` only as a record-free legacy
  placeholder so the obsolete compiled-data path cannot silently return.
- Updated airport/release/factory documentation and regression tests around the one
  persistent-data model.

### Preserved

- Product 96 NVS-backed MQTT credential preservation across public OTA.
- Existing browser airport builder/installer, package validation, PSRAM-first upload,
  dedicated 512 KiB airport partition, firmware OTA slots, NVS settings, ADS-B/TLS
  behavior, radar UI, display timing, OPI PSRAM, DMA, and target capacity.

## Product 96 - 2026-08-16

**Build:** `7IN-20260816-PRODUCT96-MQTT-OTA-PRESERVATION`

### Fixed

- MQTT broker URI, username, and password are now stored in the existing `radar_cfg`
  NVS namespace instead of being runtime-only compile-time values.
- Private development builds seed missing MQTT credential keys from ignored
  `include/config.h`; public/distribution builds contain no private credentials.
- The MQTT service reads the saved NVS credentials, allowing a later public
  distribution/OTA image to keep using the owner's existing MQTT configuration.
- Clarified the airport transition boundary: PlatformIO factory-environment upload
  does not erase the dedicated airport partition, but a legacy compiled private
  airport fallback is not part of public firmware and must be replaced by a
  persistent browser-installed regional database.

### Preserved

- Wi-Fi/location NVS, persistent airport partition, OTA slot behavior, ADS-B/TLS
  networking, 15-second cadence, radar rendering, stable ICAO tracking, display
  timing/DMA/bounce buffer, OPI PSRAM, and 200-target capacity.

## Product 94 - 2026-08-14

**Build:** `7IN-20260814-PRODUCT94-FACTORY-DISTRIBUTION`

### Added

- Dedicated `waveshare-s3-touch-lcd-7-factory` PlatformIO environment for
  credential-safe blank-unit provisioning.
- Neutral `include/distribution/config.h` selected only by that environment.
- Factory build begins with blank Wi-Fi, neutral coordinates, MQTT disabled,
  and blank MQTT broker credentials.
- Factory build omits the generated regional airport fallback. With an empty
  persistent airport partition, the Airports page therefore has no owner-specific
  airport list and reports that a regional database is not installed.

### Preserved

- The normal `waveshare-s3-touch-lcd-7` development environment continues to use
  the private `include/config.h`; Product 94 does not edit, package, or expose it.
- Product 93 update-page release notes and last-used 20/40/80 range persistence.
- Product 86+ custom partition layout and persistent airport partition.
- Native/fallback ADS-B HTTPS hardening, 15-second cadence, recovery, stale-result
  rejection, last-good retention, MQTT runtime behavior in the normal private
  build, radar rendering, stable ICAO tracking, 200-target capacity, OPI PSRAM,
  display timing, DMA, and 20-scanline bounce buffer.

### Factory-build boundary

The Product 94 factory environment is for blank-unit provisioning and intentionally
does not run the normal OTA post-build release-copy script. Public GitHub OTA
distribution remains a separate follow-up until owner-configurable MQTT broker
credentials are available.

## Product 93 - 2026-08-13

**Build:** `7IN-20260813-PRODUCT93-UPDATE-NOTES-RANGE-PERSISTENCE`

### Changed

- Software Update continues to show the installed Product and build ID.
- When a newer validated release is available, the panel now labels the manifest's
  bounded release note as **WHAT'S NEW** and identifies the remote Product/build as
  `UPDATE AVAILABLE`.
- The release note remains sourced from `FIRMWARE_RELEASE_NOTES` through the existing
  generated manifest; no new GitHub scraping or update transport is introduced.
- The last manually selected 20/40/80-mile radar range is stored in the existing
  `radar_cfg` NVS namespace and restored before ADS-B networking starts.
- Missing or invalid stored range values fall back to 80 miles.
- A live range change still succeeds even if the persistence write fails; the failure
  is logged and NVS saving follows the existing verified-write health behavior.
- Reset-to-defaults restores the saved radar range to 80 miles.

### Preserved

- Native/fallback ADS-B HTTPS behavior, 15-second cadence, request generation/stale
  rejection, last-good retention, and Wi-Fi/TLS recovery.
- GitHub update checking/install verification, local browser OTA, partition layout,
  persistent airport storage, radar rendering, stable ICAO selection/tracking,
  display timing/DMA/bounce buffer, PSRAM architecture, and 200-target capacity.

## Repository housekeeping after Product 92 - 2026-08-13

**Firmware Product marker:** unchanged at Product 92

- Removed redundant tracked `release/firmware.radarota`.
- Removed stale Product 70-91 `.radarota` packages from the active branch while
  preserving their history in Git/releases.
- Removed old tracked serial-capture logs from the active tree; future `*.log` files
  remain ignored.
- Stopped pinning a self-staling "current commit" SHA in README/CHANGELOG status.
- Kept the Product 92 versioned `.radarota` and matching manifest as the active
  release assets.

## Product 92 - 2026-08-13

**Build:** `7IN-20260813-PRODUCT92-AIRPORT-LOCATION-PREFILL`  
**Commit:** `e5afe84d8bb0cd56eb19900437b9a347e7605ee7`

### Changed

- Exposes the radar's existing saved home coordinates only through the authenticated
  Airport Database status response.
- After the six-digit access code succeeds, the browser prefills valid latitude and
  longitude from the current radar settings.
- Prefill happens once so later status refreshes do not overwrite a deliberately
  edited future package center.
- Removed user/location-specific coordinate examples from the public page and
  replaced them with generic placeholders.

### Preserved

- Product 90 WebServer pacing/safe retry.
- Product 91 verified automatic restart.
- Existing NVS ownership and settings validation.
- ADS-B/TLS/Wi-Fi, firmware OTA, radar UI, display, PSRAM, partition layout, and
  target capacity.

## Product 91 - 2026-08-13

**Build:** `7IN-20260813-PRODUCT91-AIRPORT-AUTO-RESTART`  
**Final retained commit in Product 92 ancestry:** `d8aa4929557c48db941869400814839086cb2fde`

### Changed

- After `airport_store::installPackage()` completes package validation, flash write,
  re-read, and persistent-copy verification, the airport path schedules the same
  hardened restart machinery already used by firmware OTA.
- Sends the success HTTP response before the bounded restart delay.
- New persistent airport data becomes active automatically on reboot.
- Renamed the airport-page navigation control to `BACK TO FIRMWARE UPDATE`.
- Updated page guidance to describe automatic activation.

### Preserved

- No direct/simple `ESP.restart()` airport path.
- Product 90 upload timing and safe retry.
- Firmware OTA validation, ADS-B networking, UI/radar rendering, partition layout,
  display timing, PSRAM architecture, and target capacity.

## Product 90 - 2026-08-13

**Build:** `7IN-20260813-PRODUCT90-AIRPORT-UPLOAD-HANDOFF`  
**Commit:** `ca119eb3b61885917864d71bd81a0e7a6b1c3de6`

### Fixed

- Applied the proven firmware browser-uploader handoff to airport uploads:
  `PREPARE -> READY -> 500 ms settle -> multipart POST`.
- Added bounded retry treatment for short airport control/status requests.
- Allows one large airport upload retry only when the browser observed no upload
  bytes and the ESP independently reports `READY` with zero airport bytes received.
- Never blindly retries a partial or ambiguous airport transfer.

### Physical result

- The previously observed airport `Upload connection reset` condition was corrected
  in hardware/browser testing before later Products were continued.

## Product 89 - 2026-08-13

**Build:** `7IN-20260813-PRODUCT89-BROWSER-AIRPORT-BUILDER`  
**Commit:** `028863b90908dac2766abdcb090b84337d68f8e1`

### Added

- Made the Airport Database page the normal end-user regional database workflow.
- Browser accepts latitude, longitude, coverage radius, and optional region name.
- Browser downloads current OurAirports airport/runway CSV data directly.
- Filters/classifies the region locally on the phone/computer.
- Reproduces the v1 `.radarapt` header/record format in JavaScript.
- Uses an HTTP-safe pure JavaScript SHA-256 implementation rather than depending on
  secure-context Web Crypto.
- Uploads the generated Blob directly through the existing PSRAM-first installer.
- Retains manual `.radarapt` download/upload as an advanced developer/debug fallback.

### Validation

- Browser-package bytes were parity-checked against the Python reference package
  format during implementation.

## Product 88 - 2026-08-13

**Build:** Product 88 mobile airport database upload  
**Commit:** `bd87c757e9957892d5b6efe2c4b16eee043fa492`

### Added

- Added responsive `/airports` page on the existing local port-80 maintenance
  WebServer.
- Reuses the same six-digit access code and bounded maintenance hold as firmware OTA.
- Buffers the complete `.radarapt` upload in PSRAM before any flash erase.
- Enforces airport partition capacity during upload.
- Validates/installs only through the Product 87 `airport_store` boundary.
- Reports persistent-store state, region/date, record count, and radius.
- Interrupted/invalid pre-validation uploads cannot alter the stored database.
- Reduced destructive erase from the whole partition to the 4 KiB-aligned span
  required by the incoming package.

## Airport separation tooling checkpoint - 2026-08-13

**Commit:** `aec1c7918bae49c273120f595d6f4a08f5e8e4ee`  
**Firmware Product marker:** unchanged from Product 88

### Changed

- Converted the guided Windows/Python airport setup to package-only output.
- Generates `release/airports.radarapt` instead of rewriting the compiled fallback
  header.
- Retains data download/cache, regional filtering, package parse-back validation,
  and generator regression tests.
- Does not rebuild firmware or modify the radar's saved home location.
- Keeps package-generation center coordinates out of the package.

This remains developer/reference/recovery tooling; Product 89 made the browser the
normal user path.

## Product 87 - 2026-08-13

**Build:** Product 87 persistent airport installer  
**Commit:** `e67843dc48061cb16fa85e3a6f1bd5c9875f14ae`

### Added

- Added the destructive-write boundary for separated regional airport data.
- Accepts one complete bounded `.radarapt` package from caller-owned PSRAM.
- Validates package format/records before changing flash.
- Erases/writes only the dedicated 512 KiB airport partition.
- Re-reads and fully validates the persistent copy after installation.
- Reports bounded installation errors.
- Exposes partition capacity for the web upload layer.

### Preserved

- Product 85 compiled airport database remains the runtime fallback.
- This stage did not add web-server routes.
- ADS-B and UI behavior remained unchanged.

## Product 86 - 2026-08-13

**Build:** Product 86 persistent airport storage/fallback  
**Commit:** `2e460a0269aa6323dab5a718bc5c4ab188794804`

### Added

- Introduced the ESP-side foundation for separating location-specific airport data
  from universal firmware.
- Added custom 16 MB partition layout while preserving the intended two large OTA
  application slots and existing NVS/coredump placement.
- Added a dedicated 512 KiB airport partition.
- Added validated persistent airport package access.
- Kept the checked-in compiled airport database as the fallback source when
  persistent storage is empty/invalid.
- Added package-format and storage-focused host/source tests.

### Installation note

A device coming from the older partition layout requires an appropriate USB/
PlatformIO flash once to install the custom partition table. Ordinary later firmware
OTA is not intended to erase the persistent airport partition or NVS settings.

## Product 85 - 2026-08-11

**Build:** Product 85 radar contact clipping fix  
**Commit:** `79216b4a3a787274c8485b1714b892562c797761`

### Fixed

- Removed the hard rectangular renderer exclusion that clipped aircraft bitmap
  pixels near the MILES/range selector.
- Relied on the LVGL range control overlay instead of destructively clipping the
  underlying aircraft bitmap.

### Preserved

- Aircraft assets/scaling/heading behavior.
- Radar labels and range control.
- ADS-B networking, display configuration, and target behavior.

## Product 84 - 2026-08-09

**Build:** `7IN-20260809-PRODUCT84-LARGE-PRIORITY-AIRCRAFT-ICON`  
**Commit:** `8e816fdb7a0d1dffdfd7cb0b960f8b9cd5c815e8`

- Enlarged only the selected/tracked aircraft type image using the existing checked-in
  aircraft sprite source.
- Kept normal nearest/list/Airspace icons at their established smaller size.
- Preserved networking, display timing, PSRAM architecture, and target capacity.

## Product 83 - 2026-08-08

**Build:** `7IN-20260808-PRODUCT83-SETTINGS-KEYBOARD-VISIBILITY`  
**Commit:** `e1a0c39535ccd1d4a52391f860e7cb51d74a4638`

- Keeps the active Device & Network setting visible when the LVGL keyboard opens.
- Scrolls only the settings card, not the fixed page/header/status areas.
- Restores normal position/state on keyboard close, page changes, and overlays.
- Preserves password masking, coordinate entry, settings validation, and networking.

## Product 82 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT82-AIRSPACE-HANDOFF`  
**Commit:** `d3e769b34eb112d7153c43f75a1dd5d3e69eb276`

- Resolves a tapped Airspace aircraft by stable ICAO before changing tracking.
- If needed, stops the prior tracked aircraft only after the tapped target is proven
  current.
- Returns to Radar with the tapped target selected in amber; TRACK remains a deliberate
  second action.

## Product 81 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT81-80MI-HEADING`  
**Commit:** `33c8982029363d11be08d7771e4d64eb553cc7b6`

- Uses each target's heading bucket for 80-mile aircraft silhouettes.
- Retains Product 79/80 symbol sizes and zero-allocation rendering.
- Preserves selection, tracking, dirty restoration, networking, display timing, DMA,
  and PSRAM protections.

## Product 80 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT80-BOOT-SPLASH`  
**Commit:** `ec320cca4ad53294f78cf1e933a54978af414e3a`

- Added lightweight avionics boot splash with saved display-name branding.
- Added startup radar sweep and north marker.
- Added centered north marker to the live radar.
- Preserved normal startup/network/rendering architecture underneath the splash.

## Product 79 - 2026-08-06

**Build:** `7IN-20260806-PRODUCT79-RANGE-SYMBOLS`  
**Commit:** `997a5813bb1031ac6d5929cf849950ce23d51a2b`

- Retained full 25x25 heading-aware contacts at 20 miles.
- Added 17x17 aircraft symbols at 40 miles using the same sprite source.
- Added compact 11x11 aircraft silhouettes at 80 miles.
- Removed the 40/80 dot substitution without introducing per-frame allocations.

## Product 78 - 2026-08-05

**Build:** `7IN-20260805-PRODUCT78-PAGE-TOP-RESET`  
**Commit:** `87e3382d23b68a55f3bc7ec7167641d3a7eceea4`

- Reduced selected/tracked secondary heading size to fit the fixed panel.
- Tracks and Airports return to the top on page/directory entry.
- In-page live refresh preserves a valid current scroll position.

## Product 77 - 2026-08-05

**Build:** `7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE`  
**Commit:** `d4b60cddfecdc943c2d33231bc1a76289b85760b`

- Keeps open Aircraft Profile keyed to stable ICAO rather than target-array position.
- Version-gates profile refresh on relevant aircraft/range/tracking state.
- Updates coherent values while profile remains open.
- Clearly distinguishes current versus last-known selected/tracked states.

## Product 76 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT76-PRIORITY-NEIGHBORS`  
**Commit:** `817462c8a6f2cb8157c7223a1788c8ff34dfc621`

- Ranks the three secondary aircraft by separation from the selected/tracked aircraft
  rather than from home.
- Uses one bounded pass and fixed three-entry storage.
- Uses stable ICAO for deterministic exclusion/ties/actions.

## Product 75 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT75-UPDATE-UI-BOOT-CLEAR`  
**Commit:** `16343a1bac4345233ba91721fc36fa8f05bde04f`

- Refined Software Update layout for 800x480.
- Clears transient queued/checking/install/available-release state on reboot.
- New boot starts a fresh automatic-check delay while leaving `CHECK NOW` available.

## Product 74 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT74-GITHUB-OTA-TEST`  
**Commit:** `156909bc7f57d2c16ef9e0c0c8be6a9e42fd0f10`

- Versioned test release used to physically prove Product 73's remote GitHub installer.
- Verified complete download, package validation, inactive-slot write, hardened restart,
  and boot into the newer marker.

## Product 73 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT73-GITHUB-OTA-INSTALL`  
**Commit:** `d2200f4effadf136460c126d18e6505d50aa2740`

- Added explicit two-tap `DOWNLOAD & INSTALL` for a validated newer GitHub release.
- Revalidates manifest immediately before installation.
- Uses verified bounded native HTTPS, approved redirects, PSRAM receive buffer, and
  internal-RAM flash staging.
- Verifies package/image/build/hardware/size/SHA before boot-slot selection.
- Reuses hardened local-OTA restart path.

## Product 72 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT72-GITHUB-TX-BUFFER-FIX`  
**Commit:** `f999347897185d41761dc6c896229e002cb7482f`

- Sizes GitHub updater transmit buffer from validated URL length.
- Preserves redirect/header/url bounds and verified TLS.

## Product 71 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT71-GITHUB-REDIRECT-FIX`  
**Commit:** `a52ee1cd39f1d39182730418de7192b9779a4307`

- Raised bounded aggregate response-header allowance for real GitHub responses.
- Expanded bounded signed redirect URL capacity.
- Preserved strict framing and approved-host HTTPS redirect policy.

## Product 70 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT70-GITHUB-UPDATE-CHECK`  
**Commit:** `b20c2d47c5b3a758564e944e9b912fd562334c1d`

- Restored bounded GitHub stable-release metadata checking.
- Runs only in a safe serialized network window.
- Added compatibility/manifest validation and System-page availability state.
- Product 70 checked metadata only; direct install followed in Product 73.

## Product 69 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT69-BOUNDED-TRANSPORT`  
**Commit:** `0111ff5ad7c38ec4fe7464a3064c8a7e218791d6`

- Added shared complete-fetch budget below the fixed 15-second cadence.
- Reserved JSON processing headroom.
- Added remaining-budget gates to native retry/fallback work.
- Added safe OTA cancellation checks without manufacturing false ADS-B failures.

## Product 68 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT68-FETCH-CONTENTION`  
**Commit:** `7874b9d7f87cc375538a2c9e0407522beafcf3fa`

- Reduced routine fetch logging contention.
- Split JSON deserialize/extract timing.
- Added bounded yielding during record extraction.
- Preserved failure/recovery logging.

## Product 67 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT67-RADAR-GAP-ATTRIBUTION`  
**Commit:** `266659d9fb9f7803f5d9703bd65533ce2c8933cd`

- Added bounded activity-window history and radar-frame-gap attribution.
- Added stage maximum diagnostics used to locate fetch-associated hitches.

## Product 66 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS`  
**Commit:** `d8673272526b71f22b0b4b521565633c8e21854c`

- Replaced dense-frame full cached-canvas copying with bounded PSRAM dirty regions.
- Version-gated coherent radar target snapshot copies.
- Retained complete cached-layer fallback.

## Product 65 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT65-RADAR-FRAME-CADENCE`  
**Commit:** `f307ad558e0effca4d931012d67a8a28fed580c1`

- Made radar sweep elapsed-time based.
- Added optional PSRAM cache for fixed radar grid/configured airport layer.
- Added render-duration/frame-gap diagnostics.

## Product 64 - 2026-08-02

**Commit:** `21e2e0649f01bb848c728cf37a7f8b6c6b1ed16`

- Reduced measured core-0 ADS-B stack to 12 KiB with diagnostics retained.
- Improved fetch-stage attribution.
- Moved Firmware / OTA control into the System header.

## Product 63 - 2026-08-02

**Commit:** `cf8504fb68b069fce096187b3f3012bdacee4866`

- Kept ADS-B payload and ArduinoJson documents in PSRAM-only storage.
- Replaced hot-path Arduino String request construction with bounded arrays.
- Moved Airports directory storage to PSRAM.
- Expanded memory diagnostics.

## Product 62 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT62-AIRPORT-EYE-COVERAGE`  
**Commit:** `2bae5aadeb4207c4c846cdc1f70e52b333b88214`

- Kept Airports directory bounded while ensuring every actually rendered airport
  label can have its matching eye indicator represented.

## Product 61 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT61-OTA-SOCKET-PACING`  
**Commit:** `5ab7db06de45883eda8e96bd5c8633d2fb63b862`

- Added proven browser handoff pacing between `/prepare`, `/status`, and firmware
  multipart upload.
- Added bounded control retries and one safe zero-byte upload retry.
- Physically verified successful browser firmware OTA/restart.

## Product 60 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT`  
**Commit:** `9d23ef3de361ef4fc900e715a32b484a7a3cc4e9`

- Made authenticated `/prepare` idempotent while already PREPARING/READY.
- Added bounded recovery from a lost preparation response.

## Product 59 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT59-NETWORK-RECOVERY-MEMORY`  
**Commit:** `a040fe46363013f6f18b8c33d74ba8fe6fa19a08`

- Released unused BLE controller memory.
- Requires MQTT teardown before hard Wi-Fi/radio recycle.
- Deferred unsafe hard recovery instead of tearing down underneath MQTT.

## Product 58 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT58-OTA-EXCLUSIVE-HOLD`  
**Commit:** `a6e96d4bb5e41ec342b3807039498d5c76bef1c8`

- Claimed exclusive network maintenance ownership for the complete local OTA window.
- Parked ADS-B/released MQTT for maintenance.
- Added hardened cross-core restart handoff.

## Product 57 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT57-AIRPORT-DATABASE-SETUP`  
**Commit:** `c30c330ff3182798d690cbd919c322819a2d17f9`

- Added guided Windows and CLI tooling for the original compiled regional airport
  database workflow.
- Combined OurAirports airport/runway data and atomic output validation.

**Historical note:** Products 86-92 superseded this as the normal user architecture.
The tooling was converted to package-only developer/reference use rather than removed.

## Product 56 - 2026-08-01

**Initial:** `8b4fca13c4893b863b7413eec447fcef3d0b8612`  
**Final:** `49035a62e4684ef1696fb325fbc12b0a8a53cce7`

- Added optional Home Assistant MQTT discovery, retained availability, controls,
  telemetry, and dashboard support.
- Kept MQTT disabled by default and isolated from Wi-Fi recovery ownership.
- Iterated memory behavior before settling on lightweight PubSubClient 2.8 and
  bounded PSRAM-backed payload handling.

## Product 55 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT55-AIRSPACE`  
**Commit:** `0bc41425a89570a1698dbf5d601ae4218b2f15b6`

- Added Airspace range toggle and stable-ICAO highlight shortcuts.
- Added category/live-highlight cards while preserving radar range ownership.

## Product 54 - 2026-08-01

**Primary:** `68c4ef20d46910cf95286172299c1fb5e617baf6`  
**Compatibility:** `3dd1d65519076a8d0f5b925a30cf4b3ba0c50df0`  
**Release copy:** `fa802c232d808b6a9543de893f86da37fac7ef06`

- Added hardware-bound `.radarota` package format and local browser updater.
- Verifies package/image/build ID/hardware/hash before inactive-slot selection.
- Added acknowledged ADS-B maintenance hold.

## Product 53 - 2026-07-27 to 2026-08-01

**Initial:** `d3b1ea0d3cff6fe8a070148a9791858175b927e0`

- Added Airports directory/profile and display-settings subpage.
- Added bounded 64-row behavior and stable-ID `AUTO / SHOW / HIDE`.
- Added rendered-label eye indicators and `SHOW ON RADAR`.
- Hardened touch/scroll/edit behavior through the retained R6/R6R1 revisions.

## Product 52 - 2026-07-27

**Commit:** `f7de4097362282315adfe906855029d222870abe`

- Resolved airport-label collisions deterministically.
- Rebalanced System layout.

## Product 51 - 2026-07-27

**Commit:** `c02bc6662d6714eab9f9559f80e49e35f2033f58`

- Drew airport identifiers as a subdued fixed map layer beneath aircraft.
- Reorganized System into status/settings/maintenance areas.

## Product 50 - 2026-07-27

**Commit:** `90da002df5d816c851f68eb3597f321a6fc512f4`

- Added bounded offline airport cache and runway-oriented radar symbols.
- Added collision-aware airport labels and per-category display settings.
- Replaced Setup navigation with Airports and moved device/network setup to System.

## Product 49 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT49-20MI-LABEL-SELECTION`  
**Commit:** `29cb94c1ab6d899483ebcd0269ea6f291fa3d85f`

- Made each visible 20-mile aircraft label a stable-ICAO touch target.
- Added deterministic exact/padded label-hit priority before icon fallback.

## Product 48 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT48-TRACKS-SCROLL-CLAMP`  
**Commit:** `f299720257042fe9d38cc20d4dfdcc3a7eb7b0b4`

- Prevented Tracks from appearing empty when a shorter table inherited an invalid
  old LVGL scroll offset.

## Product 47 - 2026-07-26

**Commit:** `2059f34f1e3825d5912e01a241a5f635a48c825f`

- Added aviation-themed climb/level/descent fuselage indicator and FT/MIN.
- Uses deterministic hysteresis keyed to stable ICAO.

## Product 46 - 2026-07-26

**Initial:** `58aee5b0fda1e364aec68d277ea1d00ffa9a71c3`  
**R2:** `c75141f93468b8118c4c63ddd39c780584b712d5`

- Prevented overlap handling from suppressing lower-priority contacts.
- Added deterministic contact layering.
- R2 refined sweep tint without growing/flashing normal aircraft symbols.

## Product 45 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`  
**Commit:** `c704ca1d15e4da26c48b768cfa04726b9be865c2`

- Replaced unsafe implicit member-recovery classification with explicit Target-aware
  APIs while preserving classifier output.

## Product 44 - 2026-07-26

**Commit:** `ec3ac4fcee8e2764c315be959e7a78d2ac9488d9`

- Verified Preferences/NVS initialization and write lengths.
- Stopped reporting Settings saved when writes fail.
- Exposed NVS READY/ERROR.

## Product 43 - 2026-07-26

**Commit:** `cbe6d0fae79b0ca76960ad594c435ee0947716a2`

- Replaced the cross-core volatile Wi-Fi timestamp race with synchronized access.

## Product 42 - 2026-07-26

**Commit:** `251fbc42fb2350a616dbdbb40c7a72a6f97e5f97`

- Treats a fully successful stale-generation response as transport success while
  still rejecting obsolete payload publication.

## Product 41 - 2026-07-26

**Commit:** `843863321b5fb2458d8fe60350a585e597129245`

- Fails fast after a partial-body transport stall and returns to existing Wi-Fi
  recovery rather than retrying a dead body path.

## Product 40 - 2026-07-26

**Initial:** `244f337`  
**R2:** `c2a853f`  
**R3:** `0b099db`

- Added generated aircraft designator/description database.
- Corrected native retry/fallback ordering and eligibility.
- Avoided retry/fallback after partial response-body transport failure.

## Product 39 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT39-STARTUP-FAILURE-PROPAGATION`  
**Commit:** `35b9eca`

- Made required subsystem startup failures explicit.
- Added stable STARTUP HALTED behavior instead of continuing partially initialized.

## Product 38 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT38-LOCATION-INVALIDATION`  
**Commit:** `34a3c2d`

- Clears visible aircraft immediately after saved radar-center change.
- Advances request generation and rejects old-location results.

## Product 37 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT37-FALLBACK-HTTPS-HARDENED`  
**Commit:** `63892b1`

- Removed insecure HTTPS fallback.
- Added verified CA/hostname behavior, bounded streaming parsing, PSRAM-first body
  storage, strict framing, response limits, and transport-only fallback eligibility.

## Product 36 - 2026-07-25

**Initial:** `3ea59bb`  
**R2:** `9494747`  
**R3:** `022e7b3`  
**R4:** `9de21e3`

- Replaced 20-mile dots with aircraft category bitmaps.
- Refined recognizable silhouettes.
- Added 16 precomputed heading variants.
- Added deterministic overlap priority and refined dense contact behavior.

## Product 35 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT35-DESCRIPTION-TYPE-FALLBACK`  
**Commit:** `0441bd7`

- Added conservative description-based type fallback when exact ICAO designator
  remains unknown.

## Product 34 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY`  
**Commit:** `239a7a5`

- Added three-successful-current-generation-miss grace period before clearing tracking.
- Failed requests/stale results/config changes do not count as misses.

## Product 33 - 2026-07-24

**R2:** `52367dfa5f85491f2813a3d7103c0aa4a5cf6953`  
**R5:** `14fe9f46b8c131ed1a22f5adb93e898f3188e3b1`

- UI fit/polish, Setup spacing, Airspace fit, project credit, and nearest-other heading
  improvements.
- No unverified R3/R4 history is invented.

## Product 32 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT32-UI-DASHBOARD`  
**Commit:** `a4cc594f52c8d8e562cd751dbb528a744d72d00e`

- Added Airspace dashboard cards/live highlights.
- Added bounded secondary aircraft rows.
- Kept Radar as owner of the 20/40/80 range state.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`  
**Commit:** `1747cb169969dff12f3a5d794f561b56dc2acc83`

- Added aircraft-type icons to side panels/lists.
- Added rotating nearest-aircraft heading arrow/value.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`  
**Commit:** `50821badc68efb2e0c8dab597d1db5f1267628ab`

- Increased deterministic target capacity to 200.
- Moved capacity-scaled target/snapshot/render metadata to required PSRAM.
- Retains tracked aircraft first, then nearest by distance.
- Added received/eligible/stored/dropped/visible diagnostics.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`  
**Commit:** `237d06a2b79b0ca76960ad594c435ee0947716a2`

- Corrected target-capacity label metadata.
- Used stable rendered ICAO IDs for nearest list/card actions.
- Added explicit detail origins and improved selection timeout/overlay behavior.

## Product 28 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT28-RADAR-STATE-FLOW`  
**Commit:** `61f104ca4a8adb6bf7d9ffb6caf3509ddb05797c`

- Restored idle nearest information.
- Gave selected/tracked details right-panel priority.
- Moved STOP TRACK into tracked card and preserved selection after stop when practical.

## Product 27 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT27-RADAR-LAYOUT`  
**Commit:** `2dda7ae4606dff08fcd34d303610f9d920185362`

- Reworked idle/selected/tracked radar panel layout.
- Added INFO/TRACK/CLEAR and right-panel STOP TRACK.
- Nearest list taps select aircraft rather than immediately changing pages.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`  
**Commit:** `298c87ab43d83ae51e0276fb152789e05ad7423e`

- Added themed 20-mile tags.
- Added stable-ICAO radar hit regions with tracked/selected/closest priority.
- Added temporary selection and compact range selector.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`  
**Commit:** `1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee`

- Eliminated expected first-run missing-key Preferences noise.
- Initialized absent defaults without overwriting saved values.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`  
**Commit:** `dec570eab7324ae6cc00747a037af2090cdf94bd`

- Added bounded response recovery/escalation.
- Closed native connections before retry/fallback.
- Preserved last-good data and moved last-resort restart safely to main loop.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`  
**Commit:** `faa9bc28b8524ff9fb850636e1bf356f508d71da`

- Replaced unsupported floating-point LVGL formatting that caused the populated-data
  heading crash.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`  
**Commit:** `3203bd3a4263b7c1f65a839466a083d7b9cd8c90`

- Retried temporary native body-read no-progress conditions within bounded deadlines.
- Preserved independent no-progress/total-response limits.

## Products 19-21 - 2026-07-21

**Product 21 build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`  
**Product 21 commit:** `5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9`

Reliable standalone Product 19 and Product 20 boundaries are not preserved, so they
remain documented as one confirmed sequence rather than inventing commits.

Across the preserved sequence:

- Added tracked-aircraft panel and heading display.
- Capped radar at 80 miles and added outward tracked auto-zoom.
- Added popup keyboard for setup.
- Removed redundant status overlay.
- Kept tracked panel active while waiting for fresh data.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`  
**Commit:** `69dce612211326a4a41f0f66becc8eb7d46191f9`

- Attached Espressif CA bundle to native ESP-IDF HTTPS with hostname verification.
- Established the first physically working native-TLS baseline.
- Added fallback HTTPS path while preserving core-0 ownership and bounds.

## Product 17 - 2026-07-21

**Standalone commit/build:** not preserved

- Documented precursor that replaced the earlier secure client/parser path with native
  ESP-IDF HTTPS.
- Initial physical test lacked configured server-verification method; Product 18
  corrected this with the CA bundle.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`  
**Commit:** `c81b34e5d640e734723964f895c9bb4ec49c1af8`

- Used resolved server IP for TCP while retaining hostname for TLS SNI.
- Increased handshake allowance and improved mbedTLS diagnostics.
- Limited Wi-Fi recycle to appropriate failure classes.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`  
**Commit:** `b2a0a492c424cf192edf71eb7f5dd496ec0bbab8`  
**Tag:** `product-15-hardened`

### Established

- First hardened modular version-controlled baseline for the exact Waveshare
  ESP32-S3-Touch-LCD-7.
- Core-0 ADS-B network ownership.
- Generation-safe publication/stale-result rejection.
- Last-good aircraft retention.
- Bounded failure diagnostics/recovery.
- Arduino-ESP32 3.0.7 high-performance XIP/OPI PSRAM configuration.
- Proven Waveshare RGB timing/DMA anti-rolling path with 20-scanline bounce buffer.
- LVGL radar/System architecture retained as the permanent rollback foundation.
