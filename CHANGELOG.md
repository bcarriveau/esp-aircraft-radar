# Changelog

All notable confirmed changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented in the repository changelog.

This file records the Product 46 through Product 42 updates. Confirmed Product 41
through Product 15 history remains unchanged from the existing repository
`CHANGELOG.md`.

## Current status

- **Current source:** Product 46
- **Current build marker:** `7IN-20260726-PRODUCT46-RADAR-OVERLAP-STABILITY`
- **Intended branch:** `main`
- **Product 46 source baseline commit:** `c704ca1d15e4da26c48b768cfa04726b9be865c2`
- **Hardened rollback baseline:** Product 15
- **Recommended baseline tag:** `product-15-hardened`

## Product 46 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-RADAR-OVERLAP-STABILITY`  
**Status:** Host-verified radar rendering candidate; full PlatformIO and physical
verification pending

### Fixed

- Removed the 20-mile overlap rule that marked lower-priority aircraft contacts
  invisible and caused their bitmaps and associated tags to disappear.
- Keeps every in-range aircraft bitmap represented at the 20-mile view, including
  aircraft occupying the same or nearby screen coordinates.
- Layers overlapping contacts deterministically by tracked state, selected state,
  nearest distance, stable ICAO hex, and target index as the final tie-breaker.
- Draws a subtle dark one-pixel silhouette around a higher-priority overlapping
  bitmap so the upper aircraft remains distinct without deleting the aircraft
  underneath it.
- Keeps the normal aircraft bitmap cyan while adding a temporary yellow halo
  underneath it as the radar sweep passes, restoring the radar-paint flair without
  recoloring the aircraft or its tag.
- Uses the existing 24-degree trailing sweep window for the halo at 20, 40, and
  80 miles.
- Keeps selected contacts amber and tracked contacts red without applying the
  sweep halo to either priority state.
- Removes contact-overlap visibility gating from tag rendering while retaining the
  existing collision-aware tag placement and selected/tracked tag priority.

### Preserved

- Product 45 explicit target-aware classifier APIs remain in use for radar
  previews, radar contacts, side icons, and radar summaries.
- Product 45 classifier output, generated database, and regression coverage are
  unchanged.
- Core-0 serialized ADS-B ownership, non-overlapping requests, 15-second cadence,
  stale-response rejection, Wi-Fi/TLS recovery, and last-good retention are
  unchanged.
- Native ESP-IDF HTTPS remains the preferred transport, and the bounded secure
  fallback policy is unchanged.
- `MAX_TARGETS=200`, PSRAM ownership, deterministic retention, stable ICAO
  selection and tracking, outward auto-zoom, MPH display, and touch behavior are
  unchanged.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer are unchanged.
- `include/config.h` and credentials were not read, modified, or packaged.

### Verification

- Confirmed GitHub `main` at Product 45 commit
  `c704ca1d15e4da26c48b768cfa04726b9be865c2` before rebasing the radar change.
- Confirmed the Product 45 build marker before editing:
  `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`.
- Compared Product 44 and Product 45 and confirmed Product 45 changed
  `src/radar_renderer.cpp` only at five explicit classifier call sites.
- Preserved all Product 45 `bitmapForTarget()` and `kindName(const Target&)`
  renderer calls in the complete Product 46 replacement file.
- Complete `src/radar_renderer.cpp` host C++17 syntax checking passed with
  `-Wall -Wextra -Werror` using focused interface stubs.
- Focused actual-renderer overlap tests passed under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Tests confirmed two nearby normal 20-mile aircraft retain visible bitmap pixels.
- Tests confirmed a normal contact under the sweep retains a cyan center while a
  yellow bitmap-shaped halo is painted outside it, and the halo clears after the
  sweep window passes.
- Tests confirmed the 40/80-mile dot rendering uses the same yellow-under-cyan
  sweep treatment.
- Tests confirmed selected aircraft remain the top amber layer and tracked
  aircraft remain the top red layer at exact overlap, with no sweep halo applied
  to either state.
- Static checks confirmed contact visibility suppression and sweep-driven base
  recoloring remain absent, and legacy target-backed classifier calls are absent.
- Static checks confirmed no networking, TLS, Wi-Fi, settings, target-capacity,
  display-driver, panel-timing, DMA, bounce-buffer, or credential source was
  changed.
- Final package inspection confirmed only complete replacement files and delivery
  metadata are included.

### Pending verification

- Full PlatformIO compile and link.
- Flash usage and internal-RAM usage report.
- Firmware upload and physical Product 46 build-marker confirmation.
- Physical 20-mile overlap test confirming both aircraft remain represented.
- Physical confirmation that normal aircraft stay cyan while a brief yellow halo
  appears underneath them as the sweep passes, without tag color or visibility
  flicker.
- Selection, INFO, TRACK, CLEAR, STOP TRACK, stable ICAO hit testing, and
  selected/tracked overlap priority.
- Normal aircraft preview, side-icon, and radar-summary classification behavior.
- Normal 15-second ADS-B updates, native and fallback TLS behavior, Wi-Fi/TLS
  recovery, stale-response rejection, 20/40/80 range changes, page switching, no
  screen rolling, heap/PSRAM stability, and extended soak testing.

## Product 45 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`
**Status:** Host-verified classifier safety cleanup; full PlatformIO and physical
verification pending

### Fixed

- Removed the public-header template that recovered a complete `Target` by
  subtracting `offsetof(Target, typeCode)` from a matching character array.
- Removed the implicit `categoryForType()`, `bitmapForType()`, and templated
  `kindName()` dispatch that could reinterpret an unrelated standalone
  `char[9]` as though it were a `Target::typeCode` member.
- Updated aircraft details, Tracks, Airspace, previews, side icons, and radar
  summaries to call the explicit target-aware APIs:
  `categoryForTarget()`, `bitmapForTarget()`, and `kindName(const Target&)`.
- Kept the explicit type-code-only APIs for genuine C-string inputs.

### Classifier regression coverage

- Verifies the checked-in public header contains no member-recovery symbols,
  `offsetof` dispatch, or legacy implicit classifier API declarations.
- Compiles and runs the real `src/aircraft_data.cpp` against strict C++17 host
  stubs using an unrelated standalone `char[9]` only through the explicit
  type-code APIs.
- Verifies the removed implicit APIs no longer compile for that standalone
  array.
- Directly exercises description fallback and ambiguous model-family cases for
  TBM, generic piston descriptions, Mooney, PC-12 versus PC-24, Citation versus
  Cessna piston aircraft, Piper Apache versus AH-64 Apache, Airbus helicopter
  versus Airbus airliner, and Bombardier Global versus CRJ.
- Retains generated table size, ordering, duplicate, hash-collision, sensitive
  model, and exact-regeneration checks.

### Preserved

- Aircraft classification output and generated database contents are unchanged.
- Product 44 NVS write verification and saving-disable behavior are unchanged.
- Product 43 Wi-Fi timestamp synchronization, Product 42 stale-response handling,
  Product 41 partial-body recovery, and all hardened HTTPS rules are unchanged.
- Core-0 serialized ADS-B ownership, non-overlapping requests, 15-second cadence,
  stale rejection, last-good retention, stable ICAO selection/tracking,
  `MAX_TARGETS=200`, PSRAM ownership, radar behavior, and touch behavior are
  unchanged.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer are unchanged.
- `include/config.h` and credentials were not read, modified, or packaged.

### Verification

- Confirmed the supplied Git checkout is on `main` at Product 44 commit
  `ec3ac4fcee8e2764c315be959e7a78d2ac9488d9`, matching its stored
  `origin/main` ref.
- Confirmed the Product 44 source build marker before editing:
  `7IN-20260726-PRODUCT44-NVS-WRITE-VERIFY`.
- Confirmed reported working-tree differences were line-ending-only before
  editing the affected files.
- Focused aircraft database and explicit-API host tests passed with
  `-std=c++17 -Wall -Wextra -Werror -pedantic`.
- Static checks confirmed all target-backed production call sites use the
  explicit target APIs and no unsafe member-recovery machinery remains.
- Static checks confirmed no networking, TLS, Wi-Fi, settings, capacity, display
  driver, panel timing, DMA, bounce-buffer, or credential source was changed.
- Final complete-file comparison showed only the intended classifier API,
  call-site, test, build-marker, and changelog changes.

### Pending verification

- Full PlatformIO compile and link.
- Flash and internal-RAM usage report.
- Firmware upload and physical Product 45 build-marker confirmation.
- Normal aircraft details, Tracks, Airspace, preview, side-icon, and radar-summary
  classification display.
- Normal 15-second ADS-B updates, native and fallback TLS behavior, Wi-Fi/TLS
  recovery, stale-response rejection, 20/40/80 range changes, selection,
  tracking, STOP TRACK, touch, page switching, no screen rolling, heap/PSRAM
  stability, and extended soak testing.

## Product 44 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT44-NVS-WRITE-VERIFY`
**Status:** Host-verified NVS reliability candidate; full PlatformIO and physical
verification pending

### Fixed

- Checks `Preferences.begin()` and reports whether the settings namespace is
  usable.
- Verifies the expected byte count and stored value for every required string
  and float write.
- Handles a valid empty Wi-Fi password by verifying the persisted key and value,
  because a successful empty-string write and a failed write can both report
  zero bytes.
- Makes default initialization, settings saves, and reset-to-defaults report
  failure instead of assuming success.
- Prevents the Setup page from reporting a successful save or reset unless every
  required NVS operation succeeds.

### Storage fallback and diagnostics

- Continues startup when NVS cannot be opened.
- Uses compile-time defaults whenever stored values are unavailable.
- Marks NVS as unhealthy and disables further Save Settings and Reset Defaults
  operations after an initialization or write failure.
- Keeps readable stored values available after a write error while refusing to
  claim that storage is healthy.
- Shows `NVS: READY` or `NVS: ERROR` on the System page.
- Shows NVS state on the Setup page and visibly disables saving when storage is
  unavailable or unhealthy.
- Emits serial warnings without printing Wi-Fi credentials, coordinates, or
  private settings.

### Preserved

- Core-0 serialized ADS-B ownership, non-overlapping requests, 15-second cadence,
  stale-response rejection, Wi-Fi/TLS recovery, and last-good retention are
  unchanged.
- Native ESP-IDF HTTPS remains the preferred transport, and the bounded secure
  fallback policy is unchanged.
- `MAX_TARGETS=200`, PSRAM ownership, deterministic retention, stable ICAO
  selection and tracking, outward auto-zoom, MPH display, radar rendering, and
  touch behavior are unchanged.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer are unchanged.
- `include/config.h` and credentials were not touched or packaged.

### Verification

- Confirmed GitHub `main` at Product 43 commit
  `cbe6d0fae79ebd15d5d68d09b8aa78cf940e0a42` before editing.
- Confirmed the Product 43 build marker before editing:
  `7IN-20260726-PRODUCT43-WIFI-TIMESTAMP-SYNC`.
- Verified the complete original `src/ui.cpp` reconstruction against Git blob
  `8072076c6f557e11a171842108b51c8cd1fbb721` before modification.
- Complete `src/settings.cpp`, `src/ui.cpp`, and `src/main.cpp` host C++17
  syntax checking passed with `-Wall -Wextra -Werror` using focused interface
  stubs.
- Focused NVS initialization, fallback, save, reset, per-key failure, empty-string,
  and write-length tests passed under AddressSanitizer and UndefinedBehaviorSanitizer.
- Static checks confirmed every settings write is routed through checked helpers,
  save/reset success is conditional, NVS status is exposed, and saving is disabled
  after storage errors.
- Static checks confirmed no networking, TLS, target-capacity, radar-rendering,
  panel-timing, DMA, bounce-buffer, or credential-file change was introduced.
- Final complete-file comparison showed only the intended settings reliability,
  Setup/System diagnostics, startup warning, Product 44 build marker, and
  changelog changes.

### Pending verification

- Full PlatformIO compile and link.
- Flash usage and internal-RAM usage report.
- Firmware upload and physical Product 44 build-marker confirmation.
- Normal boot with healthy NVS showing `NVS: READY`.
- Fault-injection or corrupted-NVS test showing `NVS: ERROR`, compile-time fallback,
  disabled saving, and no false success message.
- Successful Save Settings and Reset Defaults behavior on hardware.
- Normal 15-second ADS-B updates, native and fallback TLS behavior, Wi-Fi/TLS
  recovery, stale-response rejection, 20/40/80 range changes, selection,
  tracking, STOP TRACK, touch, page switching, no screen rolling, heap/PSRAM
  stability, and extended soak testing.

## Product 43 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT43-WIFI-TIMESTAMP-SYNC`  
**Status:** Host-verified cross-core synchronization candidate; full PlatformIO
and physical verification pending

### Fixed

- Removed `volatile` from the cross-core `lastWifiAttempt` timestamp.
- Protects every read and write of `lastWifiAttempt` with the existing
  `commandMux` FreeRTOS critical section.
- Performs the reconnect-due check and timestamp reservation in one critical
  section so the main loop cannot overwrite a newer network-task attempt time.
- Records the actual network-task Wi-Fi attempt time under the same lock.

### Preserved

- Wi-Fi reconnect intervals and reconnect eligibility are unchanged.
- Core-0 serialized ADS-B ownership, non-overlapping requests, 15-second cadence,
  stale-response rejection, outage recovery, and last-good retention are
  unchanged.
- Native ESP-IDF HTTPS remains the preferred transport, and the verified bounded
  fallback policy is unchanged.
- Product 42 stale-success bookkeeping remains unchanged.
- `MAX_TARGETS=200`, PSRAM ownership, deterministic retention, stable ICAO
  selection and tracking, outward auto-zoom, MPH display, radar rendering, touch
  handling, and page behavior remain unchanged.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer remain unchanged.
- `include/config.h` and credentials were not touched or packaged.

### Verification

- Confirmed GitHub `main` at Product 42 commit
  `251fbc42fb2350a616dbdbb40c7a72a6f97e5f97`.
- Confirmed the Product 42 build marker before editing:
  `7IN-20260726-PRODUCT42-STALE-TRANSPORT-SUCCESS`.
- Confirmed the original Git blob identities before modification:
  - `src/adsb_network.cpp`:
    `937aa8b21ce9f3726551ef74e2231ec0a479ffc6`
  - `include/build_info.h`:
    `07f52193d8b3b844e204ad5652548df049928f06`
  - `CHANGELOG.md`:
    `43c2bb2550a69b7f77b8accd8147493a6cafd18a`
- Complete `src/adsb_network.cpp` host C++17 syntax checking passed with
  `-Wall -Wextra -Werror` using focused interface stubs.
- Focused synchronization tests passed for locked Wi-Fi-attempt recording,
  atomic reconnect check-and-reservation, reconnect throttling, and timer
  rollover behavior.
- Static checks confirmed `lastWifiAttempt` is no longer volatile and has no
  unprotected read or write.
- Static checks confirmed no `setInsecure()`, blocking `HTTPClient::GET()`,
  whole-response `readString()`, target-capacity change, UI/display change, or
  credential file was introduced.
- Final complete-file comparison showed only the intended timestamp
  synchronization, Product 43 build marker, and changelog changes.

### Pending verification

- Full PlatformIO compile and link.
- Flash usage and internal-RAM usage report.
- Firmware upload and physical Product 43 build-marker confirmation.
- Normal automatic Wi-Fi reconnect timing during a disconnect and recovery.
- Normal 15-second ADS-B updates, native and fallback TLS behavior, stale-response
  rejection, 20/40/80 range changes, selection, tracking, STOP TRACK, touch,
  page switching, no screen rolling, heap/PSRAM stability, and extended soak
  testing.

## Product 42 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT42-STALE-TRANSPORT-SUCCESS`  
**Status:** Host-verified transport-state candidate; full PlatformIO and physical
verification pending

### Fixed

- Treats a fully completed ADS-B response as a transport success even when its
  request generation has become obsolete and the payload must be discarded.
- Resets `consecutiveFailures` when a successfully completed stale response is
  discarded.
- Clears `lastFailureStage` to `NONE` instead of recording `STALE_RESULT` as the
  current transport failure.
- Resets the core-0 network task's outage timer and per-outage recovery count
  after a successfully completed stale response.
- Prevents an old transport-failure streak from causing the next genuine failure
  to trigger Wi-Fi or TLS recovery earlier than intended.

### Diagnostics and state behavior

- Continues incrementing `discardedResponses` for obsolete successful responses.
- Keeps stale-response diagnostics separate from transport-failure diagnostics.
- Does not advance published-data freshness or `lastSuccessMs` for a stale
  response because no aircraft snapshot was published.
- Continues rejecting the obsolete payload before publication.
- Continues scheduling an immediate current-generation follow-up request.
- Does not advance tracked-aircraft missing-update state because the stale
  snapshot remains unpublished.

### Preserved

- Native ESP-IDF HTTPS remains the preferred transport.
- The verified bounded `WiFiClientSecure` fallback policy and framing parser are
  unchanged.
- Wi-Fi, DNS, TCP, TLS, HTTP-header, response-body, HTTP-status, allocation, and
  JSON failure classification remains unchanged for genuine failures.
- Core-0 serialized ADS-B ownership and non-overlapping requests remain
  unchanged.
- The normal successful-poll 15-second start-to-start cadence remains unchanged.
- Stale-response rejection and last-good snapshot retention remain unchanged.
- Product 41 fast response-body failure recovery remains unchanged.
- `MAX_TARGETS=200`, PSRAM ownership, deterministic target retention, stable
  ICAO selection and tracking, outward auto-zoom, MPH display, radar rendering,
  touch handling, and page behavior remain unchanged.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer remain unchanged.
- `include/config.h` and credentials were not touched or packaged.

### Verification

- Confirmed the complete replacement sources were based on GitHub `main` at
  Product 41 commit `843863321b5fb2458d8fe60350a585e597129245`.
- Confirmed the Product 41 build marker before editing:
  `7IN-20260726-PRODUCT41-FAST-BODY-RECOVERY`.
- Verified the original Git blob identities before modification:
  - `src/app_state.cpp`:
    `1b8c8d39be2f1c396033717f9c6741f09a8b1131`
  - `src/adsb_network.cpp`:
    `df3d57ad98d282fbb16266515be9bcc5f6f241f0`
  - `include/build_info.h`:
    `3427aa1232d918d030e104e9bab0605fe10640d4`
- Complete `src/app_state.cpp` host C++17 syntax checking passed with
  `-Wall -Wextra -Werror` using focused interface stubs.
- Complete `src/adsb_network.cpp` host C++17 syntax checking passed with
  `-Wall -Wextra -Werror` using focused interface stubs.
- Focused stale-success sequencing tests passed under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Static checks confirmed the stale-success path clears
  `consecutiveFailures`, `lastFailureStage`, `outageStartedAt`, and
  `outageRecoveries` while retaining `discardedResponses` and the immediate
  follow-up.
- Static checks confirmed no `setInsecure()`, blocking `HTTPClient::GET()`,
  whole-response `readString()`, target-capacity change, UI/display change, or
  credential file was introduced.
- Final complete-file comparison showed only the intended stale-success
  bookkeeping and Product 42 build-marker changes.

### Pending verification

- Full PlatformIO compile and link.
- Flash usage and internal-RAM usage report.
- Firmware upload and physical Product 42 build-marker confirmation.
- A stale-generation test showing the discard followed immediately by a
  current-generation request.
- Confirmation that a genuine failure after the stale success starts at
  consecutive failure 1.
- Normal 15-second ADS-B updates, native and fallback TLS behavior, Wi-Fi/TLS
  recovery, 20/40/80 range changes, selection, tracking, STOP TRACK, touch,
  page switching, no screen rolling, heap/PSRAM stability, and extended soak
  testing.
