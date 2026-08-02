# Changelog

All notable **confirmed** changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
Dates follow preserved firmware build markers. Commit links refer to the current
`main` history in `bcarriveau/esp-aircraft-radar` where a standalone commit is
available.

The authoritative numbered history begins with Product 15, the first hardened
version-controlled baseline. Earlier history is intentionally omitted where the
repository does not provide authoritative evidence.

## Current status

- **Current source:** Product 62
- **Current build marker:** `7IN-20260802-PRODUCT62-AIRPORT-EYE-COVERAGE`
- **Current branch:** `main`
- **Product 62 source baseline:** Product 61 `main` commit `5ab7db06de45883eda8e96bd5c8633d2fb63b862`
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262 RGB LCD, GT911 touch, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback baseline:** Product 15
- **Recommended rollback tag:** `product-15-hardened`

Product 62 is the current host-verified source candidate on `main`. It corrects the
Airports-directory eye coverage mismatch while preserving the 64-row directory
bound. Product 61 remains the current physically verified known-good release.

## Product 62 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT62-AIRPORT-EYE-COVERAGE`  
**Source baseline:** Product 61 `main` commit `5ab7db06de45883eda8e96bd5c8633d2fb63b862`  
**Status:** Focused host-verified Airports UI fix; PlatformIO and physical verification pending

### Diagnosed

- The radar renderer can evaluate and label airports from the complete bounded
  192-entry nearby-airport cache.
- The Airports page intentionally keeps its scrollable directory bounded at 64 rows.
- Product 61 filled those rows only from the nearest 64 airports, then queried eye
  state for that subset.
- A farther airport whose label fit on Radar could therefore have no corresponding
  directory row or eye indicator. The reported hardware example showed seven visible
  labels but only three eyes while scrolling the list.

### Fixed

- Kept the Airports directory bounded at 64 rows.
- Added one optional 10,752-byte PSRAM scratch buffer for the complete bounded nearby
  airport set; no internal-RAM fallback was added.
- Started from the nearest 64 airports, then retained every airport whose label was
  actually rendered on the latest completed radar frame using its stable airport
  identifier.
- Replaced only the farthest non-visible rows when room was required.
- Restored increasing-distance order after retention while keeping eye state attached
  to the correct stable identifier.
- Preserved the prior nearest-64 behavior as a safe fallback if the optional scratch
  allocation is unavailable.

### Scope and preserved behavior

- Changed only Airports-directory population and eye-state coverage plus the Product
  build marker and focused regression test.
- Did not alter airport label placement, collision handling, radar rendering order,
  airport symbols, saved `AUTO / SHOW / HIDE` preferences, row-action touch filtering,
  or `SHOW ON RADAR` behavior.
- Preserved ADS-B networking, native/fallback HTTPS, 15-second cadence, request
  serialization, Wi-Fi/TLS recovery, stale-response rejection, last-good retention,
  stable ICAO aircraft interaction, 200-target bounds, panel timing, DMA, OPI PSRAM,
  LVGL 8.3.11, and the 20-scanline RGB bounce buffer.

### Validation

- Focused seven-label retention and source-integration regression testing passed.
- An isolated C++17 retention model passed with
  `-Wall -Wextra -Werror -pedantic`.
- The isolated retention test passed under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Modified `src/ui.cpp` passed lexical integrity scanning for balanced delimiters,
  comments, character literals, and string literals.
- Diff whitespace checks passed.
- Static scans found no `HTTPClient::GET()` or `setInsecure()` in the changed UI file.
- Package paths and SHA-256 checksums were verified.

### Pending verification

- PlatformIO compile/link and flash/internal-RAM report.
- Upload and confirmation of the Product 62 marker on the 7-inch display.
- At 20, 40, and 80 miles, physical confirmation that every visible radar airport
  label appears exactly once in the scrollable directory with an eye indicator.
- `AUTO / SHOW / HIDE`, `SHOW ON RADAR`, directory scrolling and row taps, aircraft
  selection/tracking, 15-second ADS-B updates, display stability, memory stability,
  and extended soak testing.

## Product 61 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT61-OTA-SOCKET-PACING`  
**Source baseline:** Product 60 OTA-preparation package derived from Product 57 commit `c30c330ff3182798d690cbd919c322819a2d17f9`  
**Status:** Physically verified known-good OTA release

### Diagnosed

- Two Chrome HAR captures reproduced the same rapid socket-handoff failure. The
  final repeated-click capture contained eight successful `/prepare` responses,
  eight `/status` attempts, and two `/upload` attempts.
- Six `/status` connections and the first multipart `/upload` connection failed with
  `net::ERR_CONNECTION_RESET`. The final 2.4 MB multipart upload then completed
  successfully without a firmware, hardware, or network configuration change.
- Successful follow-up requests were launched only about 0.2-2.6 ms after the prior
  response ended. The failed first upload spent about 80 ms blocked before reset;
  its HAR timing recorded no connect, send, wait, or receive phase.
- The resets occurred while the radar was already in its exclusive OTA hold and
  before firmware writing or reboot. They were not caused by flash activity, package
  validation, ADS-B TLS memory pressure, MQTT, or an access-code rejection.
- Every successful JSON response in the final HAR contained two identical
  `Connection: close` headers: one added by this OTA code and one added automatically
  by Arduino-ESP32 `WebServer`.

### Changed

- Wait 500 ms after accepted preparation before the first `/status` request.
- Poll `/status` once per second instead of every 250 ms.
- Wait 500 ms after `READY` before opening the multipart `/upload` connection.
- Retry transient `/prepare`, `/status`, and cancel connection failures with at most
  three attempts and 300/600 ms bounded backoff; ordinary HTTP error responses are
  not retried.
- On an upload connection reset, query `/status` and retry the multipart upload once
  only when the radar is still `READY` and both received and written firmware byte
  counters remain zero.
- Refuse an automatic upload retry when any transfer may have started, avoiding a
  duplicate write or ambiguous inactive-partition state.
- Expose `received_bytes`, `written_bytes`, and `firmware_bytes` in authenticated
  status JSON for safe browser recovery and diagnostics.
- Remove the manually added `Connection: close` response header because
  Arduino-ESP32 3.0.7 `WebServer` already emits that header.

### Validation

- Added focused regression tests for control-request pacing, bounded network-only
  retries, zero-byte upload retry gating, status counters, duplicate close-header
  removal, and preservation of the Product 57 multipart route.
- The complete checked-in Python suite passed: 62 tests.
- A Node-based browser simulation passed transient control retry, non-retry of HTTP
  errors, one safe zero-byte upload retry, and rejection of a retry after any byte
  was received.
- `src/ota_update.cpp` passed strict host C++17 syntax checking with
  `-Wall -Wextra -Werror` using the existing focused Arduino/ESP-IDF stubs.
- Static checks confirmed the package parser, streamed inactive-slot writer,
  SHA/image validation, Product 58 exclusive hold, and IRAM restart implementation
  were not changed.

### Physical verification

- User-side PlatformIO build and Product 61 `.radarota` generation completed.
- Chrome performed exactly one `POST /prepare` (`202`), one `GET /status` (`200`),
  and one multipart `POST /upload` (`200`) with no connection reset, browser retry,
  duplicate preparation, or repeated upload.
- The measured handoff spacing was about 513 ms from preparation to status and
  about 517 ms from status to upload.
- The 2,409,888-byte firmware image was accepted and verified on the inactive OTA
  partition.
- The radar stopped HTTP and mDNS, completed the IRAM-safe cross-core restart, and
  booted with `Reset reason: 3` and the Product 61 marker.
- ADS-B native HTTPS and MQTT resumed after boot. Subsequent ADS-B responses reached
  about 82.8 KB and 144 received aircraft without a progressive heap or PSRAM loss.
- Restart diagnostics reported `integrity=ok`, 71,616 bytes free heap, a 53,236-byte
  largest internal block, and 3,968,468 bytes free PSRAM.
- The normal one-click OTA path is therefore physically verified. The forced hard
  Wi-Fi recovery branch and duplicate preparation retry branch were not separately
  fault-injected in this final hardware run.

### Preserved

- Product 60 idempotent `/prepare` handling and bounded preparation deadline.
- Product 59 BLE-memory release, MQTT teardown before hard Wi-Fi recovery, bounded
  recovery deferral, and OTA/recovery mutual exclusion.
- Product 58 immediate exclusive OTA hold and physically verified IRAM restart.
- Product 57 `FormData` multipart transport, package format, streamed writer,
  certificate-verified ADS-B networking, stable ICAO behavior, display timing, OPI
  PSRAM/XIP, DMA, and the 20-scanline RGB bounce buffer.

## Product 60 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT`  
**Source baseline:** Product 59 network-recovery package derived from Product 57 commit `c30c330ff3182798d690cbd919c322819a2d17f9`  
**Status:** Focused OTA preparation correction; PlatformIO and hardware verification pending

### Diagnosed

- Hardware logs showed ADS-B and MQTT entering the exclusive OTA hold and the radar
  reaching `READY`, while the browser displayed `OTA service is not available for
  preparation` and no upload began.
- The first `/prepare` request had succeeded. A duplicate or retried request then saw
  `PREPARING` or `READY`, received HTTP 409, and caused the browser workflow to abort.
- With the browser no longer polling or uploading, the radar correctly reached the
  existing 45-second preparation expiry while retaining the exclusive hold.

### Changed

- Treat repeated authenticated `/prepare` requests during `PREPARING` as HTTP 202
  success and during `READY` as HTTP 200 success.
- Return those idempotent responses without resetting the upload session, requesting
  maintenance again, or extending the existing preparation deadline.
- Add one bounded browser-side retry. After a failed preparation response, the page
  checks `/status`; `PREPARING` or `READY` is accepted as success before retrying once.
- Keep the original Product 57 `FormData` multipart `/upload` transport unchanged.

### Validation

- Added focused preparation-idempotency tests covering duplicate `PREPARING` and
  `READY` requests, side-effect-free retries, one bounded deadline, browser lost-
  response recovery, and preservation of the multipart upload path.
- The complete checked-in Python suite passed after the Product 60 marker update.
- `src/ota_update.cpp` passed strict host C++17 syntax checking with
  `-Wall -Wextra -Werror`.
- Static comparison confirmed the package parser, streamed inactive-slot writer,
  SHA/image validation, maintenance lifecycle, and IRAM restart implementation were
  unchanged from Product 59.
- PlatformIO compile/link, generated OTA package, upload of the Product 60 marker,
  and hardware verification were not run here.

### Preserved

- Product 59 BLE-memory release, MQTT teardown before hard Wi-Fi recovery, bounded
  recovery deferral, and OTA/recovery mutual exclusion.
- Product 58 immediate exclusive OTA hold and physically verified IRAM restart.
- Product 57 browser multipart upload, package validation, inactive-slot writer,
  native ADS-B HTTPS behavior, stable ICAO interaction, display configuration, OPI
  PSRAM/XIP, DMA, and the 20-scanline bounce buffer.

## Product 59 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT59-NETWORK-RECOVERY-MEMORY`  
**Source baseline:** Product 58 OTA release derived from Product 57 commit `c30c330ff3182798d690cbd919c322819a2d17f9`  
**Status:** Focused memory and hard-recovery update; PlatformIO and hardware verification pending

### Changed

- Release unused ESP32-S3 BLE controller memory during startup before settings,
  display, Wi-Fi, MQTT, or OTA allocation begins, while logging the resulting free
  internal heap and largest-block gain.
- Before a hard station-radio recycle, reserve exclusive Wi-Fi-operation ownership,
  request a Core-1 MQTT recovery hold, and wait up to two seconds for acknowledgement.
- The Core-1 MQTT service now closes its socket without graceful broker traffic,
  deletes the client objects, and releases its bounded aircraft and JSON buffers
  before acknowledging that the radio may be recycled.
- Allow a bounded 100 ms lwIP settlement interval after MQTT resource teardown and
  log heap, largest internal block, and PSRAM immediately before the hard recycle.
- Defer the hard radio recycle rather than disconnecting Wi-Fi when MQTT does not
  acknowledge the recovery hold.
- Keep the MQTT recovery hold active through the reconnect attempt, then release it
  only after the bounded Wi-Fi connection window finishes.

### OTA/recovery ownership

- Make the OTA maintenance reservation and hard-recovery reservation mutually
  exclusive under the existing Core-0 command lock.
- Reject a new OTA enable or browser preparation request with a retry message when a
  hard Wi-Fi recovery already owns the network.
- Prevent a hard Wi-Fi recovery from starting once OTA has reserved its exclusive
  ADS-B/MQTT maintenance window.
- Skip OTA HTTP/mDNS servicing while the station-radio transition is in progress.
- Preserve Product 58's behavior where an already-running ADS-B request may finish,
  but cannot enter recovery after OTA requests its hold.

### Validation

- 42 focused OTA package, IRAM restart, exclusive-window, BLE-memory, MQTT teardown,
  hard-recovery ordering, and mutual-exclusion tests passed.
- The complete checked-in Python test suite passed: 48 tests.
- `src/main.cpp` and `src/ota_update.cpp` passed strict host C++17 syntax checks with
  `-Wall -Wextra -Werror`.
- Python syntax compilation passed for all changed tests.
- Complete-file and static checks confirmed the Product 57 multipart upload page,
  `/upload` route, package parser, inactive-slot writer, SHA/image verification, and
  Product 58 IRAM restart implementation were not replaced by later raw, chunked,
  resumable, or extended-timeout experiments.
- Static inspection found no `setInsecure()` or blocking `HTTPClient::GET()`.
- PlatformIO compile/link, generated memory report, hard-recovery fault injection,
  OTA upload of the Product 59 marker, and hardware soak were not run here.

### Preserved

- The physically verified Product 58 network-exclusive OTA window, immediate ADS-B
  and MQTT hold, Product 57 multipart/package writer, alternating application slots,
  and IRAM-safe Core-1 restart.
- Product 57 airport tooling, Product 56 R6 UI and Home Assistant MQTT behavior,
  native preferred ADS-B HTTPS, verified fallback, 15-second cadence, recovery
  thresholds, stale-response rejection, last-good retention, stable ICAO interaction,
  200-target bounds, panel timing, DMA, OPI PSRAM/XIP, LVGL 8.3.11, and the
  20-scanline RGB bounce buffer.

## Product 58 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT58-OTA-EXCLUSIVE-HOLD`  
**Source baseline:** Product 57 commit `c30c330ff3182798d690cbd919c322819a2d17f9`  
**Status:** Runtime implementation physically verified across two consecutive OTA cycles; final Product 58 marker rebuild pending

### Changed

- Made the complete five-minute local OTA window network-exclusive from the moment
  **ENABLE OTA** is pressed rather than waiting for browser preparation.
- The core-0 ADS-B task now finishes only a request already in flight and then parks
  for the complete armed, preparation, retry, upload, verification, and restart
  lifecycle.
- MQTT closes its socket and releases its client, aircraft snapshot, and JSON work
  buffers as soon as OTA claims the network.
- Pending ADS-B refresh and reconnect commands are cleared or ignored while OTA owns
  the network. An in-flight request that fails after the hold request cannot enter
  Wi-Fi recovery or request the last-resort fallback restart.
- Upload and preparation errors retain the exclusive hold for a bounded retry. Normal
  background networking resumes only after cancellation, local disable, expiry, or
  an external Wi-Fi disconnect; HTTP and mDNS stop before the hold is released.

### Restart correction

- Corrected the automatic restart crash caused by Core 1 remaining in flash-resident
  code while the ESP-IDF ESP32-S3 restart path disabled instruction and data caches.
- Added a dedicated noinline `IRAM_ATTR` Core-1 park routine with maskable interrupts
  disabled before it atomically acknowledges quiescence.
- Retained the bounded dedicated Core-0 restart task with an internal 4096-byte stack,
  one-second acknowledgement timeout, atomic cancellation race protection, and the
  public `esp_restart()` API.
- Added reset-reason startup diagnostics so clean software restarts can be
  distinguished from panic resets.

### Physical verification

- The exact runtime implementation was built and tested before the marker-only Product
  58 promotion. A normal rebuild is required to generate a Product 58 OTA package.
- Completed two consecutive browser OTA updates with the Product 57 multipart upload
  transport, package parser, SHA-256 validation, ESP image validation, inactive-slot
  writer, and verification path preserved.
- Confirmed ADS-B entered maintenance hold and MQTT released its resources before
  each upload was permitted. No ADS-B or MQTT network work competed with the upload.
- Confirmed the inactive partitions alternated `app1` then `app0` and returned to
  `app1` for the next cycle.
- Both updates restarted without a Guru Meditation and the following boots reported
  `Reset reason: 3`.
- Pre-restart resources remained stable across the two cycles: approximately 70.4 KB
  free internal heap, a 51.2 KB largest internal block, and 3.97 MB free PSRAM.
- Post-restart ADS-B preferred native HTTPS and MQTT startup resumed normally.

### Validation

- 34 focused OTA package, restart, and exclusive-window tests passed before hardware
  validation.
- `src/ota_update.cpp` passed strict host C++17 syntax checking with warnings treated
  as errors.
- The Product 57 update page, multipart `/upload` handler, package parser, writer, and
  final verification regions remained unchanged.
- Static inspection found no `setInsecure()`, blocking `HTTPClient::GET()`, raw upload
  route, or resumable/chunked browser transport.

### Preserved

- Product 57 airport generation and documentation, Product 56 R6 UI and lightweight
  MQTT behavior outside OTA, native preferred ADS-B HTTPS, verified fallback,
  15-second cadence, recovery, stale-response rejection, last-good retention, stable
  ICAO interaction, 200-target bounds, airport rendering, panel timing, DMA, OPI
  PSRAM/XIP, LVGL 8.3.11, and the 20-scanline RGB bounce buffer.

## Product 57 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT57-AIRPORT-DATABASE-SETUP`  
**Status:** Focused tooling and documentation update; PlatformIO and hardware marker verification pending

### Added

- Added `tools/Build Airport Database.bat` as the normal Windows entry point for
  rebuilding the offline airport region.
- Added a guided Python setup flow that explains nearby coordinate changes versus
  moves to another region, accepts a bounded coverage radius, and offers current
  download, cached download, or existing local CSV input.
- Added current OurAirports `airports.csv` and `runways.csv` handling, including
  longest-open-runway length and heading selection.
- Added a preview showing major, public, private, and heliport counts plus estimated
  flash data before any project file is replaced.
- Added atomic replacement and automatic restoration of the prior generated header
  when validation fails.
- Added region-independent airport-database tests and deterministic generator tests.
- Added `docs/AIRPORT_DATABASE.md` with plain-language relocation, build, safety,
  command-line, and troubleshooting instructions.

### Git hygiene

- Kept `include/generated_airport_database.h` tracked because it is the exact
  regional database compiled into firmware.
- Ignored Python bytecode/cache directories, accidental in-project airport/runway
  CSV downloads, optional tool cache folders, interrupted atomic header files, and
  delivery-only `COMMIT_MESSAGE.txt`, `PACKAGE_README.txt`, and `SHA256SUMS.txt`.
- The guided downloader normally stores its CSV cache outside the repository under
  the operating system's user cache directory.

### Preserved

- Product 56 R6 UI, System layout, Home Assistant MQTT behavior, local OTA behavior,
  ADS-B transport, 15-second cadence, Wi-Fi/TLS recovery, stale-response rejection,
  last-good retention, airport rendering and interaction, stable ICAO tracking,
  200-target bounds, panel timing, DMA, OPI PSRAM, and the 20-scanline bounce buffer.
- No credentials or private coordinates are stored by the generator or included in
  the delivery package.

### Validation

- Python syntax compilation passed for the guided setup and low-level generator.
- Synthetic generation passed for major, public, private, and heliport records.
- Longest-open-runway selection, heading normalization, closed-runway rejection,
  out-of-region exclusion, duplicate handling, deterministic output, and atomic
  replacement were exercised.
- The current checked-in regional header passed the new location-independent test.
- Forced validation failure restored the previous generated header.
- PlatformIO compilation, OTA package generation, upload, and physical hardware
  testing were not run here.

## Product 56 R6 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R6-SYSTEM-UI`  
**Commit:** [`49035a6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/49035a62e4684ef1696fb325fbc12b0a8a53cce7)  
**Status:** Hardware-reviewed Product 56 baseline retained by Product 57

### Finalized

- Retained the Product 56 R5 lightweight MQTT implementation and final System-page
  arrangement under the checked-in Product 56 R6 build identity.
- Preserved the physically reviewed System Status, Device & Network, and two-row
  maintenance-control layout on the 800x480 Waveshare display.
- Preserved the successful preferred native HTTPS/TLS ADS-B behavior observed with
  MQTT connected and the recovered internal-heap margin from Product 56 R3.

### Preserved

- Home Assistant discovery and controls, bounded PSRAM telemetry workspaces,
  MQTT-aware OTA coordination, native/fallback HTTPS behavior, 15-second cadence,
  recovery, stable ICAO interaction, 200-target bounds, airport behavior, panel
  timing, DMA, OPI PSRAM, and the 20-scanline bounce buffer.

## Product 56 R5 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R5-SYSTEM-UI`  
**Status:** Hardware-tested; intended final Product 56 revision

### Finalized

- Removed the Home Assistant **Last Update Age** sensor from MQTT discovery and
  removed its raw `update_age_s` field from the published status payload.
- Added one-time retained discovery cleanup for the legacy update-age topic so
  Home Assistant can remove the obsolete entity after the radar reconnects.
- Retained internal data-age calculation because the radar still uses it for the
  useful `LIVE`, `UPDATING`, `STALE`, and `OFFLINE` data-status states.
- Expanded the System Status card and kept the firmware/OTA button anchored at its
  bottom so heap, largest-block, PSRAM, airport, and network diagnostics remain
  visible without scrolling.
- Reorganized the right side of the System page into a Device & Network card above
  a two-row maintenance-control card.
- Arranged maintenance controls as `RETRY ADS-B` and `RECONNECT WI-FI` on the first
  row, with `HA MQTT` and `RESET DEFAULTS` on the second row.
- Changed the reconnect control from success green to the normal teal action color.
- Removed the redundant **MAINTENANCE** heading and used the recovered height for
  consistent button padding.
- Corrected the maintenance card's inherited internal padding so the lower buttons
  no longer touch the card border.
- Corrected Device & Network vertical spacing so the latitude and longitude fields
  no longer visually overlap the password row or SHOW button, while preserving the
  existing field and touch-target sizes.

### Hardware validation

- Product 56 R3 compiled and produced the normal firmware and radar OTA artifacts in
  the user's pinned PlatformIO environment.
- MQTT connected, Home Assistant discovery completed, and repeated 15-second ADS-B
  updates continued through the preferred native HTTPS path.
- The lightweight MQTT implementation held approximately 69 KB free internal heap
  and a 51 KB largest internal block between ADS-B requests in the captured run.
- Native TLS request low points remained repeatable and recovered after every request;
  the fragmentation collapse seen in Product 56 R2 did not return.
- Product 56 R4 and R5 System-page changes were loaded and reviewed on the
  physical Waveshare display.

### Preserved

- Native ESP-IDF HTTPS remains the preferred ADS-B transport.
- Verified fallback HTTPS, certificate and hostname verification, bounded parsing,
  response limits, header/idle/total deadlines, request serialization, recovery,
  stale rejection, last-good retention, and 15-second cadence are unchanged.
- Core-0 ADS-B ownership, 200-target bounds, stable ICAO selection/tracking,
  outward auto-zoom, MPH display, single-snapshot rendering, dirty/version LVGL
  updates, airport behavior, OTA coordination, panel timing, DMA, OPI PSRAM, and
  the 20-scanline RGB bounce buffer are unchanged.

## Product 56 R3 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R3-LIGHTWEIGHT-MQTT`  
**Status:** Hardware-tested; core MQTT memory correction retained by Product 56 R5

### Changed

- Replaced the optional native ESP-MQTT transport with PubSubClient 2.8 after
  Product 56 R2 hardware logs confirmed persistent internal-heap fragmentation:
  the first ADS-B native TLS request succeeded, but the largest internal block
  then fell to about 27 KB and all later native and fallback TLS allocations
  failed.
- Removed the separate MQTT task, native MQTT input/output buffers, and native
  outbox from the optional Home Assistant path.
- Added a 384-byte MQTT packet buffer and streamed larger retained discovery and
  state payloads directly from the existing bounded PSRAM JSON workspace.
- Limited MQTT state publication to one message per service interval.
- Paused MQTT connect, loop, discovery, and state publication while the core-0
  ADS-B task reports a fetch in progress.
- Delayed cold-boot MQTT startup for five seconds and until an ADS-B idle window,
  preventing MQTT startup from racing the first HTTPS request.
- Corrected the duplicated Home Assistant range-discovery `options` key.
- Added the pinned `knolleary/PubSubClient@2.8` PlatformIO dependency.

### Preserved

- MQTT remains disabled by default and allocates no client or MQTT data buffers
  while disabled.
- Home Assistant controls, telemetry topics, retained availability, dashboard
  compatibility, NVS enable state, and OTA maintenance coordination remain intact.
- Native ADS-B HTTPS remains preferred. Verified fallback, TLS verification,
  deadlines, failure classification, recovery, stale rejection, last-good
  retention, 15-second cadence, UI behavior, target capacity, panel timing,
  PSRAM configuration, and OTA packaging are unchanged.

### Validation

- The user confirmed a successful PlatformIO build, firmware image generation, and
  radar OTA package generation.
- An extended hardware run completed repeated native ADS-B TLS requests with MQTT
  connected and without the Product 56 R2 allocation failures or fragmentation
  buildup.
- All 16 focused repository host tests passed before hardware validation.
- Complete `src/mqtt_service.cpp` passed a strict host C++17 syntax check against
  focused Arduino, Wi-Fi, heap-capability, and PubSubClient API stubs.

## Product 56 R2 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R2-MQTT-MEMORY`  
**Status:** Hardware-tested; superseded by Product 56 R3

### Changed

- Reduced the native ESP-MQTT task stack from 6,144 to 4,096 bytes.
- Reduced native MQTT input and output buffers from 2,048 to 1,024 bytes each.
- Retained the bounded native MQTT outbox and Product 56 Home Assistant behavior.

### Hardware result

- The first native ADS-B TLS request completed, but the largest internal block then
  settled near 27 KB instead of returning to its pre-request level.
- Later native TLS setup failed with memory-allocation errors, verified fallback TLS
  also failed, and Wi-Fi recovery could not restore the contiguous block.
- This confirmed that buffer trimming alone was insufficient and led directly to the
  lightweight Product 56 R3 transport replacement.

## Product 56 R1 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R1-MEMORY`  
**Status:** Diagnostic revision; superseded by Product 56 R2 and R3

### Added

- Added free internal heap, largest contiguous internal block, and PSRAM checkpoints
  around ADS-B DNS, client initialization, TLS, headers, payload handling, transport
  release, JSON parsing, and request completion.
- Added MQTT startup, allocation, connection, discovery, and shutdown checkpoints.
- Added current/minimum largest internal-block reporting to the System page.

### Diagnostic result

- Confirmed that the existing ADS-B HTTPS/TLS request temporarily consumed roughly
  50 KB of internal heap even without MQTT.
- Confirmed that native ESP-MQTT's persistent allocations and task stack reduced the
  contiguous block enough for SHA/AES/TLS allocation failures during overlapping
  ADS-B requests.
- Verified that the hardened fallback path still completed while sufficient memory
  remained, proving the failure was local memory pressure rather than DNS, broker,
  certificate, or ADS-B server behavior.

## Product 56 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-HA-MQTT`  
**Status:** Initial implementation; superseded by Product 56 R3 and finalized by R5

### Added

- Added an optional native ESP-MQTT service with Home Assistant discovery,
  retained availability, Home Assistant birth-message rediscovery, bounded state
  topics, and deterministic default entity IDs.
- Added Home Assistant controls for the physical LCD backlight, shared 20/40/80-mile
  range, and the existing non-overlapping ADS-B refresh command.
- Added bounded telemetry for aircraft count, data status and age, tracked aircraft,
  five nearest aircraft, Product 55 Airspace highlights and category totals, Wi-Fi
  RSSI, build identity, OTA state, and MQTT state.
- Added ready-to-paste Home Assistant dashboard YAML using only built-in cards,
  plus installation and behavior notes.
- Added a System-page **HA MQTT** status/control overlay and a checked NVS setting for
  enabling or disabling the service at runtime.
- Added a project-owned idempotent display-power wrapper around the Waveshare
  CH422G backlight-enable control.
- Added a shared non-LVGL manual-range control path used by both Product 55 physical
  range controls and MQTT commands.

### Resource and failure isolation

- MQTT is disabled by default. Disabled mode creates no MQTT client task, performs no
  broker retries or JSON publication, and allocates no aircraft snapshot or JSON buffer.
- Enabling MQTT allocates exactly one `MAX_TARGETS` snapshot buffer and one 4 KB JSON
  workspace in PSRAM.
- MQTT never owns Wi-Fi setup or recovery, calls `WiFi.begin()`, recycles the radio,
  changes ADS-B deadlines/cadence, or requests a restart. Broker failure remains
  isolated from the radar and OTA service.
- Disabling MQTT publishes retained offline availability when connected, stops and
  destroys the client, clears pending commands, and releases both MQTT PSRAM buffers.

### OTA coordination

- Extended Product 54's exclusive maintenance preparation to MQTT. OTA now waits for
  both the core-0 ADS-B task and the optional MQTT service to become idle before an
  upload can begin.
- MQTT stops accepting publications, disconnects, and releases its PSRAM snapshot and
  JSON buffers during the OTA hold. Cancellation, timeout, Wi-Fi loss, or upload
  failure releases both maintenance holds.
- Updated the local update page and serial status text to describe all network services
  rather than only the ADS-B task.

### Preserved

- Native/fallback ADS-B HTTPS behavior, 15-second cadence, request serialization,
  deadlines, recovery, stale-response rejection, last-good retention, and core-0 task
  ownership are unchanged.
- Product 55 stable-ICAO Airspace shortcuts, aircraft selection/tracking, outward
  auto-zoom, 200-target bounds, airport behavior, single-snapshot rendering, LVGL
  dirty/version updates, display timing, DMA, OPI PSRAM, and the 20-scanline bounce
  buffer are unchanged.
- MQTT configuration values remain private in ignored `include/config.h`; only disabled
  placeholders are present in `include/config.example.h`.

### Initial validation

- Repository Python tests, focused MQTT/discovery/dashboard assertions, strict-warning
  host syntax checks, discovery-payload generation, dashboard YAML parsing, and static
  forbidden-API checks passed.
- PlatformIO and hardware validation were completed only after the Product 56 R3
  transport correction.

## Product 55 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT55-AIRSPACE`  
**Status:** Focused host source validation complete; PlatformIO and hardware
validation pending

### Changed

- Made the green Airspace **CURRENT RANGE** metric card cycle the shared radar range
  through 20, 40, and 80 miles without adding instructional text to the card.
- Routed the Radar selector and Airspace range card through one manual range helper so
  both preserve range generation, Radar-control synchronization, airport-focus clearing,
  immediate visible-page refresh, and the normal ADS-B refresh request.
- Replaced the redundant dominant-category live highlight with **HIGHEST AIRBORNE**
  while retaining the existing six aircraft-category count and percentage cards.
- Replaced the single wrapped live-highlights label with four bounded rows for nearest,
  fastest, lowest airborne, and highest airborne aircraft.
- Made each populated highlight resolve its stored stable ICAO hex against a fresh
  visible snapshot before selecting that aircraft and returning to Radar.
- Preserved active manual tracking as authoritative; Airspace shortcuts cannot replace
  a tracked aircraft or alter STOP TRACK, outward auto-zoom, networking, target capacity,
  airport rendering, display timing, DMA, or PSRAM behavior.

### Validation

- Added focused source checks for the cyclic shared range path, green clickable card,
  highest-airborne calculation, four independent rows, stable ICAO storage, fresh lookup,
  tracking guard, and Radar navigation.
- Updated active-build integration assertions while retaining Product 54 strings used as
  historical synthetic OTA-package test fixtures.

## Product 54 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT54-LOCAL-WEB-OTA`  
**Status:** PlatformIO compile/link confirmed; hardware OTA validation pending

### Added

- Added a dedicated System-page firmware overlay that temporarily enables the local
  update server for five minutes, displays the numeric-IP and mDNS addresses, generates
  a six-digit access code, shows preparation/write state, and reports progress.
- Added a synchronous Arduino `WebServer` endpoint on core 1. It is not listening during
  normal operation and requires the temporary access code for preparation, status,
  cancellation, and upload requests.
- Added a PlatformIO post-build script that creates `firmware.radarota` beside the
  normal `firmware.bin`. The 512-byte package header identifies Bill's Waveshare
  ESP32-S3 7-inch radar, carries the Product build marker, declares the firmware size,
  and contains the firmware SHA-256 digest.
- Added a project-local copy step that writes only the upload-ready package to
  `release/firmware.radarota`. Raw `firmware.bin`, bootloader, partition, ELF, and
  other build outputs remain in the external PlatformIO workspace.
- Added native ESP-IDF OTA streaming into the inactive application slot. Upload data is
  written incrementally without a firmware-sized heap or PSRAM allocation.
- Added exact package-length, hardware identity, ESP application magic, ESP32-S3 chip
  ID, embedded Product build ID, firmware SHA-256, `esp_ota_end()`, and boot-partition
  validation gates.
- Added a core-0 ADS-B maintenance handshake. The browser cannot upload until the
  network task has finished any active request and acknowledged the hold; failed,
  cancelled, disconnected, or expired preparation releases the hold and resumes
  polling.
- Added focused host tests for package layout, SHA-256, multiline build-marker parsing,
  wrong-chip rejection, truncated-image rejection, and the project-local release copy.

### Changed

- Removed the independently maintained short build string from `ui.cpp`; the firmware
  overlay and fatal screen now use `BUILD_ID` as the single Product identity source.
- Added `scripts/build_radar_ota.py` as a PlatformIO post-build action while preserving
  `default_16MB.csv`, the 16 MB flash configuration, Arduino-ESP32 3.0.7
  high-performance libraries, OPI PSRAM, and all existing dependencies.
- Documented `release/firmware.radarota` as the convenient project-local upload
  artifact. USB flashing continues to use the workspace `firmware.bin`.

### Fixed

- Renamed the internal OTA state token from `DISABLED` to `INACTIVE` because
  Arduino-ESP32 defines `DISABLED` as a GPIO macro. The user-facing state text remains
  `DISABLED`.
- Replaced the unavailable `mbedtls_sha256_*_ret()` calls with the
  `mbedtls_sha256_starts()`, `mbedtls_sha256_update()`, and
  `mbedtls_sha256_finish()` API provided by the pinned Arduino-ESP32 3.0.7 framework.
- Reset the OTA status result with C++ value initialization rather than byte-clearing a
  non-trivial object.

### Build result

- The initial Product 54 source reached the OTA translation units but failed because
  of the Arduino `DISABLED` macro collision and incompatible mbedTLS SHA function
  names.
- After the focused two-file compatibility correction, the user confirmed that the
  pinned PlatformIO build completed successfully.
- The ESP Panel custom-board-version and LVGL enum-operation messages remain existing
  third-party warnings and were not Product 54 build failures.

### Preserved

- Native HTTPS remains the preferred ADS-B transport. Fallback HTTPS, certificate and
  hostname verification, bounded parsing, response-size limits, deadlines, failure
  classification, Wi-Fi/TLS recovery, stale-response rejection, last-good retention,
  and the true 15-second cadence are unchanged.
- Aircraft capacity, stable ICAO selection/tracking, single-snapshot rendering,
  dirty/version LVGL updates, 20/40/80 range behavior, outward auto-zoom, MPH display,
  airport rendering, touch behavior, panel timing, DMA, OPI PSRAM, and the 20-scanline
  RGB bounce buffer are unchanged.
- `firmware.bin` remains the USB-upload artifact. The updater accepts only the generated
  `.radarota` package and never writes the bootloader, partition table, NVS, or private
  configuration.

### Pending verification

- USB installation and confirmation of the Product 54 build marker on the display.
- Successful browser upload, validation, restart, and confirmation of the installed
  build.
- Confirmation that the workspace `firmware.radarota` is exactly 512 bytes larger than
  `firmware.bin`.
- Confirmation that `release/firmware.radarota` is created after a successful build and
  is byte-for-byte identical to the workspace package.
- Wrong access code, wrong extension, truncated package, altered SHA-256, interrupted
  upload, Wi-Fi loss, and five-minute timeout tests.
- Confirmation that ADS-B completes an active request before hold, does not start a new
  request during upload, resumes after failure/cancellation, and returns to normal
  15-second operation after restart.
- Display, touch, selection/tracking, airport pages, heap/PSRAM stability, and extended
  soak testing.

## Product 53R6R1 - 2026-07-31

**Build:** `7IN-20260731-PRODUCT53R6R1-AIRPORT-TAP-FIX`  
**Status:** Host-verified focused airport-table event hotfix

### Fixed

- Corrected Product 53R6 airport row actions that never completed on hardware. LVGL
  8.3 clears a table's selected row and column during `LV_EVENT_RELEASED`, before the
  later `LV_EVENT_CLICKED` callback used by Product 53R6; that callback therefore saw
  `LV_TABLE_CELL_NONE` for every deliberate tap.
- Processed the already-filtered table `LV_EVENT_VALUE_CHANGED` while the selected cell
  is still valid. The independent 10-pixel movement limit, 900 ms duration limit,
  `LV_EVENT_SCROLL_BEGIN`, and `LV_EVENT_PRESS_LOST` cancellation gates remain active,
  so scrolling still cannot open Airport Profile or change `AUTO / SHOW / HIDE`.
- Restored deliberate Airport Profile opening while locked and deliberate LABEL cycling
  while `DONE` indicates edit mode.
- Added the missing dismissal path for temporary `SHOW ON RADAR` airport focus. Any
  deliberate tap on the radar canvas now clears the temporary airport highlight before
  normal aircraft hit testing, matching the existing empty-radar aircraft-selection
  behavior without changing saved airport preferences or manual aircraft tracking.

### Preserved

- Product 53R6 page layout, `EDIT / DONE` control, open-eye indicators,
  `SHOW ON RADAR`, automatic range choice, temporary airport focus, 64-row directory,
  saved override format, and all radar rendering behavior.
- ADS-B networking, HTTPS paths, Wi-Fi/TLS recovery, aircraft selection/tracking,
  display timing, DMA, OPI PSRAM, and the 20-scanline bounce buffer are unchanged.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53R6R1 build marker.
- Physical confirmation that deliberate row taps and LABEL edits work while ordinary
  scrolling still produces no airport action.
- Physical confirmation that `SHOW ON RADAR` focus clears from an empty-radar tap and
  from an aircraft-contact tap while normal aircraft selection/tracking remains intact.

## Product 53R6 - 2026-07-31

**Build:** `7IN-20260731-PRODUCT53R6-AIRPORT-TOUCH`  
**Status:** Hardware-tested; superseded by Product 53R6R1 after row taps were found nonfunctional

### Changed

- Added a compact `EDIT / DONE` control beside the existing `LABEL` header. Airport
  label preferences are locked by default, remain visible while locked, and can only
  cycle through `AUTO / SHOW / HIDE` while edit mode is explicitly active.
- Replaced table actions on selection changes with a bounded click-after-no-scroll
  path. Pointer movement, table scrolling, lost presses, and overlong presses cancel
  the action before an Airport Profile can open or a label preference can change.
- Added `SHOW ON RADAR` below `BACK TO AIRPORTS` in Airport Profile. It selects the
  smallest useful 20/40/80-mile range with an edge margin, switches to Radar, and
  temporarily highlights the chosen stable airport identifier in amber for 15 seconds.
- Temporary airport focus is processed before manual `SHOW` and automatic labels. It
  can display a focused airport even when its category, label, symbol, or global
  overlay setting is disabled, without modifying any saved setting.
- Manual range changes, leaving Radar, selecting another airport, or timeout clear the
  temporary focus. Aircraft selection, tracking, ICAO hit testing, and drawing priority
  remain unchanged.

### Preserved

- Product 53R5 directory columns, row spacing, open-eye indicators, 64-row bound,
  Airport Profile data, Display Settings, System layout, and NVS override storage.
- Airport labels remain fixed beneath aircraft. ADS-B networking, native/fallback
  HTTPS, 15-second cadence, stale rejection, Wi-Fi/TLS recovery, panel timing, DMA,
  OPI PSRAM, and the 20-scanline bounce buffer are unchanged.

### Hardware result

- Product 53R6 loaded and displayed the revised Airports directory, edit control, and
  eye indicators, but deliberate row and LABEL-cell taps performed no action. The
  confirmed LVGL table event-timing defect is corrected by Product 53R6R1.

## Product 53R5 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R5-AIRPORT-EYE`  
**Status:** Host-verified focused airport-directory visibility candidate

### Changed

- Added a small open-eye indicator inside the existing `LABEL` cell when that
  airport identifier was actually placed on the latest completed radar frame.
- `AUTO` and `SHOW` continue to describe the saved preference; the eye independently
  reports the current result. `HIDE` never displays the eye.
- Added a bounded PSRAM buffer containing only the stable identifiers successfully
  drawn during the current airport-label pass. The Airports directory reads that
  completed-frame result through a range/mask-validated renderer API.
- Added the eye source artwork under `assets/` and its compact alpha-mask header for
  deterministic LVGL 8.3 drawing after the table text.
- Preserved the Product 53R4 table columns, row spacing, controls, page navigation,
  label priority, collision behavior, override persistence, and 64-row bound.

### Preserved

- Airport labels remain fixed beneath aircraft and never affect aircraft ICAO hit
  testing, selection, tracking, or aircraft-tag priority.
- ADS-B networking, native/fallback HTTPS, 15-second cadence, stale rejection,
  Wi-Fi/TLS recovery, display timing, DMA, OPI PSRAM, and bounce-buffer behavior.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53R5 build marker.
- Physical confirmation that the eye appears only beside labels visible at the
  current 20/40/80-mile range and updates after range or preference changes.

## Product 53R4 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R4-AIRPORT-CONTROL`  
**Status:** Host-verified focused airport-label control candidate

### Changed

- Replaced the airport-directory bearing column with a compact `LABEL` control.
  Tapping that cell cycles the stable airport identifier through `AUTO`, `SHOW`,
  and `HIDE`; tapping any other cell still opens Airport Profile.
- `SHOW` airports are attempted first, nearest first, before the normal automatic
  major/public/private/heliport passes. `HIDE` suppresses only the identifier label;
  runway and heliport symbols remain governed by the existing display settings.
- Preserved `AUTO` as the default for every airport. Major and public categories
  remain enabled by default, while private fields and heliports remain disabled by
  default unless enabled by category or individually promoted with `SHOW`.
- Added a versioned, verified NVS blob containing up to 64 identifier-based manual
  exceptions. The sorted RAM cache provides bounded binary lookup during rendering;
  no NVS reads occur in the render path. Reset Defaults clears all exceptions.
- Increased the bounded airport directory from 32 to 64 rows and lists all airport
  categories in range so disabled private fields and heliports can be individually
  selected without enabling the entire category.
- Updated the directory summary to distinguish actual rendered labels from manual
  `SHOW` and `HIDE` exceptions while retaining the current range and database date.
- Preserved Product 53R3 row height and page structure; only the column allocation
  changed to accommodate the new control.

### Preserved

- Product 53R3 fit-based airport label placement with no small 9/8/6 quotas.
- Fixed airport-to-airport collision behavior, airport rendering beneath aircraft,
  aircraft-only ICAO hit testing, Airport Profile, Display Settings, and System UI.
- Existing per-range symbol and label settings, verified NVS handling, airport
  database/cache bounds, and location-rebuild behavior.
- ADS-B networking, native/fallback HTTPS, 15-second cadence, stale rejection,
  Wi-Fi/TLS recovery, 200-target capacity, panel timing, DMA, OPI PSRAM, and the
  20-scanline bounce buffer.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53R4 build marker.
- Physical confirmation that `AUTO / SHOW / HIDE` taps are reliable, manual `SHOW`
  entries receive priority on the radar, `HIDE` removes only labels, and all 48
  regional records remain reachable at 80 miles.

## Product 53R3 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R3-AIRPORT-LABELS`  
**Status:** Host-verified focused airport-label refinement candidate

### Changed

- Removed the small 9/8/6 airport-label quotas. The renderer now attempts every
  enabled airport in deterministic major, public, private, and heliport order and
  displays every identifier that fits the fixed airport-to-airport collision rules.
- Added a bounded PSRAM label-placement buffer sized from the existing 192-airport
  nearby-cache limit, avoiding additional main-loop stack pressure while retaining
  deterministic storage bounds.
- Added renderer label-count diagnostics so the Airports directory reports the
  actual number of labels drawn rather than describing every in-range entry as
  visible.
- Invalidates the displayed label count after airport setting or home-location
  changes until the radar has rendered the new configuration.
- Set compile-time and reset defaults to show major and public symbols and labels at
  20, 40, and 80 miles; private fields and heliports remain off by default.
- Increased airport-directory vertical cell padding slightly for easier touch and
  reading while preserving the Product 53R2 columns and page structure.

### Preserved

- Product 53R2 Airports directory, profile, display-settings page, column widths,
  single-line cropped rows, and System layout.
- Stable airport labels beneath aircraft, deterministic airport-only collision
  handling, and aircraft-only ICAO hit testing.
- Existing saved airport settings; the revised defaults apply only to new NVS keys
  or an explicit Reset Defaults operation.
- ADS-B networking, HTTPS paths, 15-second cadence, stale rejection, Wi-Fi/TLS
  recovery, 200-target capacity, panel timing, DMA, OPI PSRAM, and the 20-scanline
  bounce buffer.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53R3 build marker.
- Physical confirmation that additional major/public labels fill available space
  without overlap and that the taller directory rows remain comfortable to scroll.

## Product 53R2 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT53R2-AIRPORT-TABLE`  
**Status:** Host-verified airport-directory table correction candidate

### Changed

- Rebalanced the airport directory into a strict single-line `ID / AIRPORT /
  TYPE / DIST / BRG / RUNWAY` layout.
- Applied LVGL table text-crop control to every directory cell so identifiers,
  categories, bearings, and runway values cannot wrap into uneven row heights.
- Gave the airport-name column most of the table width while retaining bounded
  widths for every compact data column and room for the vertical scrollbar.
- Reduced directory bearings to a three-digit numeric value under the `BRG`
  heading; the full compass direction remains available in Airport Profile.
- Kept long airport names on one line and safely clipped them; tapping the row
  still opens the complete Airport Profile.
- Matched the compact one-line directory summary and `DISPLAY SETTINGS` wording
  used by the physically reviewed Product 53 screen.

### Preserved

- Product 53 directory-first Airports page, Airport Profile, display-settings
  subview, System-page redesign, and all saved airport controls.
- Product 52 deterministic airport-label placement beneath aircraft.
- Stable ICAO selection/tracking, aircraft-only radar hit testing, ADS-B
  networking, HTTPS recovery, 200-target capacity, panel timing, DMA, OPI PSRAM,
  and the 20-scanline bounce buffer.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53R2 build marker.
- Physical confirmation that headers and all rows remain single-line, long names
  crop cleanly, scrolling remains usable, and Airport Profile opens correctly.

## Product 53 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT53-PAGE-REDESIGN`
**Status:** Host-verified Airports-directory and System-page redesign candidate

### Changed

- Rebuilt the main Airports page as a directory-first operational screen rather
  than combining configuration controls with a narrow text list.
- Added a bounded, scrollable airport table with aligned IDENT, TYPE, DISTANCE,
  BEARING, RUNWAY, and AIRPORT columns, plus a compact current-range/database
  summary.
- Made airport rows open a dedicated Airport Profile view with airport name,
  category, distance, bearing, runway length, runway heading, and elevation,
  matching the Tracks-page drill-down pattern without affecting radar selection.
- Added a dedicated `DISPLAY OPTIONS` view under the Airports tab, with a clear
  `BACK TO AIRPORTS` action and the full per-range symbol/label matrix using the
  available screen width.
- Reloads saved airport options when entering the options view and discards
  unsaved edits when backing out, so the directory always reflects the settings
  actually used by the radar renderer.
- Moved the Maintenance card to the bottom of the System workspace and increased
  the two primary cards to restore vertical breathing room.
- Realigned all Device & Network fields to a consistent grid, moved Save Settings
  into a dedicated action row, and gave status messages a full-width line below it.
- Expanded System Status vertically and separated data age from fetch duration and
  payload size for easier scanning.

### Preserved

- Product 52 deterministic airport-label layout and background-map draw order.
- Product 50 airport database, bounded cache, per-range settings, verified NVS
  writes, and location-rebuild behavior.
- Stable ICAO aircraft selection/tracking, aircraft-only hit testing, 200-target
  capacity, ADS-B networking, HTTPS recovery, panel timing, DMA, OPI PSRAM, and
  the 20-scanline bounce buffer.

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 53 build marker.
- Physical review of the Airport directory, display-options view, System spacing,
  maintenance strip, settings keyboard, and all save/reset actions.
- Normal radar interaction checks and soak testing.

## Product 52 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT52-UI-POLISH`
**Status:** PlatformIO-compiled, uploaded, and physically reviewed; superseded by Product 53 page redesign

### Changed

- Added deterministic airport-to-airport label collision resolution using a bounded
  set of fixed candidate positions. The result depends only on airport data, range,
  and saved airport settings, never on moving aircraft.
- Preserved airport labels as a background map layer while preventing airport labels
  from stacking on one another around dense airport clusters.
- Kept all enabled airport runway/heliport symbols independent from label placement;
  labels that cannot fit are omitted deterministically rather than overlapping.
- Reduced airport-label fill, border opacity, radius, and text intensity so normal
  airports no longer resemble selectable aircraft tags.
- Rebalanced the System page into wider System Status and Device & Network cards.
- Shortened the on-screen build identification while retaining the exact Product 52
  marker in `build_info.h`.
- Increased the Maintenance card height and moved all three maintenance controls
  fully inside its content area to prevent clipping.
- Tightened field heights and spacing so Save Settings and its status message fit
  within the Device & Network card.
- Corrected the Save Airports control dimensions and padding so it remains fully
  inside the airport options card.

### Preserved

- Product 51 fixed airport-map draw order beneath all aircraft contacts and tags.
- Product 50 airport database, bounded cache, per-range filters, NVS persistence,
  nearby list, and location-rebuild behavior.
- Product 49 aircraft tag priority, stable ICAO selection, tracking, and hit testing.
- ADS-B networking, native/fallback HTTPS, recovery, cadence, target capacity,
  display timing, DMA, OPI PSRAM, and the 20-scanline bounce buffer.

### Physical verification

- PlatformIO compile/link completed successfully at 56.2% internal RAM and 33.8%
  application flash usage.
- Product 52 was uploaded and the deterministic airport-label layout was observed.
- Physical review confirmed the need for a directory-first Airports screen and
  more deliberate System-page vertical spacing, leading to Product 53.

## Product 51 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT51-AIRPORT-UI`
**Status:** PlatformIO-compiled and physically reviewed; superseded by Product 52 layout polish

### Changed

- Made airport identifiers a stable background map layer with one deterministic
  position per airport and radar range.
- Draws airport labels before airport symbols, aircraft contacts, and aircraft tags,
  allowing live traffic to cover the map layer without hiding or repositioning it.
- Removed moving-aircraft and aircraft-label collision decisions from airport-label
  placement, eliminating frame-to-frame label jumping and blinking.
- Gave every normal airport label a thin muted border and subdued category coloring;
  major airports remain only slightly brighter instead of resembling selectable
  aircraft tags.
- Reorganized System into dedicated System Status, Device & Network, and Maintenance
  cards without changing settings, reconnect, retry, reset, or diagnostics behavior.
- Enlarged the Save Airports button and added explicit horizontal and vertical
  padding for cleaner spacing.

### Preserved

- Product 50 airport database, bounded cache, per-range filters, NVS persistence,
  runway symbols, nearby list, and location-rebuild behavior.
- Product 49 aircraft tag priority, stable ICAO touch selection and tracking.
- ADS-B networking, HTTPS paths, recovery, cadence, target capacity, panel timing,
  DMA, OPI PSRAM, and the 20-scanline bounce buffer.

### Physical verification

- PlatformIO compile/link completed successfully at 56.2% internal RAM and 33.8%
  application flash usage.
- Product 51 was uploaded and the stable airport map-layer behavior was observed.
- Physical review identified static airport-to-airport label overlap plus clipped
  System build and Maintenance content, leading to Product 52.

## Product 50 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT50-AIRPORT-OVERLAY`
**Status:** Host-verified regional airport-overlay candidate

### Added

- Added an optional offline airport-awareness layer drawn beneath aircraft contacts.
- Added runway-oriented airport symbols plus a distinct heliport symbol.
- Added collision-aware airport identifiers that are placed only after tracked,
  selected, and ordinary aircraft labels.
- Added a dedicated Airports page with a master overlay control and independent
  symbol/label visibility matrices for major, public, private, and heliport
  categories at 20, 40, and 80 miles.
- Added nearby-airport status, visible counts, and a nearest-airport list.
- Added checked NVS storage for airport settings with RAM-cached runtime values so
  the 80 ms renderer performs no NVS reads.
- Added a bounded 192-entry PSRAM airport cache rebuilt at startup and after a
  confirmed home-location change.
- Added a reproducible OurAirports CSV generator and focused database tests.

### Navigation

- Replaced the bottom Setup tab with Airports.
- Combined the existing device/network settings and system diagnostics under the
  System tab without changing the saved Wi-Fi, title, or location behavior.

### Airport data scope

- Product 50 includes a 49-record southeastern Wisconsin/northern Illinois starter
  database for the first hardware validation run.
- The checked-in generator can create another regional database from the
  public-domain OurAirports CSV without changing the firmware interfaces.
- The overlay is awareness-only and is not intended for navigation.

### Preserved

- Product 49 exact-label touch priority and stable ICAO aircraft selection.
- Aircraft tracking, STOP TRACK, outward auto-zoom, 200-target bounds, and
  single-snapshot radar rendering.
- Core-0 ADS-B ownership, native and fallback HTTPS, 15-second cadence, deadlines,
  stale-response rejection, Wi-Fi/TLS recovery, and last-good retention.
- Waveshare panel timing, DMA, OPI PSRAM, anti-rolling protections, and the
  20-scanline RGB bounce buffer.

### Compile correction

- The first user-side PlatformIO run exposed an Arduino preprocessor collision:
  Arduino defines `radians(...)` as a macro, while the airport cache initially used
  `radians` as a helper-function name.
- Renamed the helper to `degreesToRadians()` without changing distance, bearing,
  cache, rendering, networking, or UI behavior.
- Added a focused regression check that rejects reintroduction of a helper named
  `radians`.

### Verification

- Confirmed the uploaded full project matches GitHub `main` commit `29cb94c`;
  apparent unrelated modifications were line-ending-only.
- Full changed-source C++17 host syntax checks passed with strict warnings for
  `airport_data.cpp`, `settings.cpp`, `main.cpp`, `radar_renderer.cpp`, and
  `ui.cpp` using focused Arduino/LVGL stubs.
- AddressSanitizer and UndefinedBehaviorSanitizer cache tests passed for startup,
  distance ordering, range filtering, category masks, and location rebuilds.
- Generated-table tests passed for duplicate identifiers, all four categories,
  bounds, required regional facilities, and exclusion of known closed fields.
- Static comparison confirmed the Product 49 `hitTest()` implementation, ADS-B
  network source, app-state source, panel configuration, target capacity, and
  PlatformIO configuration were unchanged.

### Pending verification

- Full PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 50 build marker.
- Physical review of runway symbols, label density, and all 20/40/80 category
  settings.
- Location-change cache rebuild, NVS save/reset, page switching, aircraft touch
  priority, tracking, display stability, and soak testing.

## Product 49 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT49-20MI-LABEL-SELECTION`
**Status:** Host-verified 20-mile radar interaction candidate

### Changed

- Made every visible radar label at the 20-mile range a first-class selection
  target using the label's stored stable ICAO hex.
- Gives an exact press inside the visible label rectangle priority over every
  aircraft-icon hit, preventing a nearby contact from stealing an intentional
  label press.
- Adds a four-pixel invisible edge pad only after all exact label rectangles miss.
- Resolves overlapping padded areas deterministically by tracked state, selected
  state, nearest visible label rectangle, nearest label center, and stable ICAO
  hex.
- Falls back to the existing 18-pixel aircraft-icon hit area only when no visible
  label or padded label edge was touched.
- Limits label-first selection to the 20-mile view; established 40/80-mile tag and
  compact-dot hit behavior remains unchanged.

### Preserved

- Stable ICAO selection and tracking, including tracked then selected then normal
  priority within each hit-test phase.
- Product 48 Tracks scroll clamping and Product 47 vertical-state fuselage
  indicators.
- Product 46 R2 sweep tint, Product 46 overlap stability, collision-aware label
  placement, and aircraft rendering.
- Existing renderer PSRAM buffers, `MAX_TARGETS=200`, contact bounds, range control,
  selection timeout, INFO/TRACK/CLEAR/STOP TRACK flow, and outward auto-zoom.
- Core-0 ADS-B ownership, native and fallback HTTPS, 15-second cadence, deadlines,
  stale-response rejection, Wi-Fi/TLS recovery, and last-good retention.
- Waveshare panel timing, DMA, OPI PSRAM, anti-rolling protections, and the
  20-scanline RGB bounce buffer.

### Verification

- Confirmed the implementation is based on current GitHub `main` commit
  `2813ee1e72793453f5da02d6eb4d776125c0d39e` and the exact current renderer blob
  `c8c6de8ac4b9a5ef3a1dc2519f90493782a0bf2a`.
- The complete `src/radar_renderer.cpp` passed a C++17 host syntax check with
  `-Wall -Wextra -Werror -pedantic` using focused Arduino/LVGL interface stubs.
- The actual Product 49 `hitTest()` implementation passed AddressSanitizer and
  UndefinedBehaviorSanitizer tests for exact labels, overlapping padded edges,
  four-pixel boundaries, tracked/selected priority, icon fallback, and unchanged
  40/80-mile behavior.
- Static checks confirmed no new buffers, allocations, capacity changes, network
  source changes, panel/touch-driver changes, forbidden HTTPS APIs, or credential
  files were introduced.

### Pending verification

- Full PlatformIO compile and link, flash/RAM report, upload, and Product 49 marker.
- Physical 20-mile testing with isolated labels, labels near aircraft icons, and
  dense neighboring labels whose four-pixel edge pads overlap.
- Confirmation that exact label presses always select the intended ICAO and that
  icon selection still works when no label is touched.
- Normal 40/80-mile selection, Tracks, vertical-state display, tracking,
  STOP TRACK, page switching, display stability, memory stability, and soak testing.

## Product 48 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT48-TRACKS-SCROLL-CLAMP`
**Commit:** [`f299720`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f299720257042fe9d38cc20d4dfdcc3a7eb7b0b4)
**Status:** Tracks-page reliability fix retained by Product 49

### Fixed

- Prevented the Tracks page from appearing empty after a long 80-mile aircraft
  list was reduced to a much shorter 20-mile list.
- Preserved the existing Tracks scroll position when it remains valid.
- Clamped a stale scroll offset to the shortened table's new maximum range when
  rows disappear.
- Cancelled stale vertical scroll animation before restoring the bounded offset.
- Added a focused serial diagnostic only when an invalid offset is corrected.

### Root cause

- The Tracks page uses an LVGL 8.3 table whose row count follows the current
  aircraft snapshot.
- LVGL recalculated the shorter table but retained the old vertical scroll offset.
- A viewport left below all remaining rows made valid published aircraft appear
  to be missing until a reboot cleared the UI scroll state.

### Preserved

- Tracks columns, sorting, row selection, detail navigation, and aircraft icons.
- Stable ICAO selection and tracking, 200-target bounds, PSRAM ownership, and
  single-snapshot rendering.
- Native and fallback HTTPS, the 15-second cadence, deadlines, stale-response
  rejection, Wi-Fi/TLS recovery, and last-good retention.
- Waveshare panel timing, DMA, OPI PSRAM, anti-rolling protections, and the
  20-scanline RGB bounce buffer.

### Verification

- Focused C++17 host testing covered valid, shortened, empty, negative-offset,
  and null-object cases with strict warnings enabled.
- Static checks found no networking, TLS, capacity, panel-driver, touch-driver,
  or credential changes.

### Pending verification

- Full PlatformIO compile and link, flash/RAM report, upload, and Product 48 marker.
- Physical 80-mile-to-20-mile Tracks reproduction at top, middle, and bottom scroll
  positions.
- Selection, details, tracking, STOP TRACK, touch, page switching, display
  stability, memory stability, and soak testing.

## Product 47 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT47-VERTICAL-STATE-BITMAPS`
**Commit:** [`2059f34`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2059f34f1e3825d5912e01a241a5f635a48c825f)
**Status:** Host-verified selected/tracked-aircraft UI candidate

### Added

- Added distinct 80x36 transparent side-profile fuselage assets for climbing,
  level, and descending flight.
- Added generated flash-resident alpha masks and a dedicated 5,760-byte PSRAM
  LVGL canvas buffer.
- Displayed the aircraft attitude, plain-language state, and rounded vertical rate
  in `FT/MIN` for selected and tracked aircraft.
- Used cyan for climbing, neutral avionics white for level flight, and amber for
  descending.
- Added deterministic hysteresis: climb/descent begins at +/-200 ft/min and
  returns to level inside +/-100 ft/min.
- Keyed state continuity to the priority aircraft's stable ICAO hex.
- Rounded displayed vertical rate to the nearest 50 ft/min.

### Preserved

- Product 46 R2 sweep tint and Product 46 overlap stability.
- Product 45 explicit target-aware classifier APIs.
- Existing selection, tracking, INFO, TRACK, CLEAR, STOP TRACK, auto-zoom, MPH,
  networking, capacity, display, and touch behavior.

### Verification

- Focused tests covered asset dimensions, generated-header parity, distinct
  attitudes, thresholds, hysteresis, labels, and rate rounding.
- The complete radar renderer passed a strict C++17 host syntax check.
- Static checks confirmed the buffer is PSRAM-backed and the indicator is limited
  to selected/tracked priority state.

## Product 46 R2 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-R2-SUBTLE-SWEEP-TINT`
**Commit:** [`c75141f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c75141f93468b8118c4c63ddd39c780584b712d5)
**Status:** Superseded by Product 47; radar styling retained

### Refined

- Replaced the enlarged four-offset yellow halo around 20-mile aircraft bitmaps
  with a same-size muted yellow tint while the sweep passes.
- Kept the aircraft footprint unchanged and returned normal contacts to cyan as
  soon as the existing 24-degree sweep window passed.
- Kept aircraft tags cyan and independent from sweep color.
- Left the compact yellow-under-cyan dot treatment at 40 and 80 miles unchanged.
- Kept selected contacts amber and tracked contacts red without sweep tinting.
- Removed the obsolete expanded bitmap-halo drawing path.

### Verification

- Renderer host compilation and ASan/UBSan-focused tests passed.
- Tests confirmed no 20-mile sweep pixels extend outside the bitmap footprint.
- Exact-overlap priority and Product 46 contact retention remained intact.

## Product 46 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-RADAR-OVERLAP-STABILITY`
**Commit:** [`58aee5b`](https://github.com/bcarriveau/esp-aircraft-radar/commit/58aee5b0fda1e364aec68d277ea1d00ffa9a71c3)
**Status:** Superseded by Product 46 R2; overlap behavior retained

### Fixed

- Removed the 20-mile overlap rule that marked lower-priority contacts invisible
  and caused aircraft bitmaps and tags to disappear.
- Kept every in-range 20-mile aircraft bitmap represented, including nearby or
  exactly overlapping contacts.
- Layered contacts deterministically by tracked state, selected state, nearest
  distance, stable ICAO hex, and target index as a final tie-breaker.
- Added a subtle dark one-pixel silhouette around a higher-priority overlapping
  bitmap so the top aircraft remains distinct without deleting the lower one.
- Removed overlap-driven tag suppression while retaining collision-aware placement
  and selected/tracked label priority.
- Restored radar sweep flair without changing selected or tracked identity colors.

### Verification

- The complete renderer passed strict C++17 host syntax checking.
- Focused overlap tests passed under AddressSanitizer and UndefinedBehaviorSanitizer.
- Tests covered nearby normals, exact selected/tracked overlap, tag retention, and
  sweep-state transitions.

## Product 45 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`
**Commit:** [`c704ca1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c704ca1d15e4da26c48b768cfa04726b9be865c2)
**Status:** Classifier safety cleanup retained by current firmware

### Fixed

- Removed the public-header template that recovered a complete `Target` by
  subtracting `offsetof(Target, typeCode)` from a matching character array.
- Removed implicit classifier dispatch that could reinterpret an unrelated
  standalone `char[9]` as a `Target::typeCode` member.
- Updated aircraft details, Tracks, Airspace, previews, side icons, and radar
  summaries to use explicit target-aware APIs:
  - `categoryForTarget(const Target&)`
  - `bitmapForTarget(const Target&)`
  - `kindName(const Target&)`
- Retained explicit type-code-only APIs for genuine C-string inputs.

### Regression coverage

- Verified the public header contains no member-recovery symbols, `offsetof`
  dispatch, or legacy implicit API declarations.
- Compiled and ran the real classifier against an unrelated standalone `char[9]`.
- Verified the removed implicit APIs no longer compile.
- Retained generated table ordering, duplicate, hash-collision, sensitive-model,
  and deterministic-regeneration checks.
- Added ambiguous family tests covering TBM, Mooney, PC-12/PC-24, Citation/Cessna,
  Piper Apache/AH-64, Airbus helicopter/airliner, and Global/CRJ cases.

## Product 44 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT44-NVS-WRITE-VERIFY`
**Commit:** [`ec3ac4f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/ec3ac4fcee8e2764c315be959e7a78d2ac9488d9)
**Status:** NVS reliability behavior retained by current firmware

### Fixed

- Checked `Preferences.begin()` and reported whether the settings namespace is
  usable.
- Verified expected lengths and stored values for every required string and float
  write.
- Correctly verified a valid empty Wi-Fi password, where a successful empty write
  and a failed write can both report zero bytes.
- Made default initialization, settings save, and reset-to-defaults return failure
  instead of always reporting success.
- Prevented Setup from reporting a successful save or reset unless all required
  NVS operations completed.

### Storage fallback and diagnostics

- Continued startup with compile-time defaults when NVS cannot be opened.
- Marked storage unhealthy and disabled further save/reset operations after a
  storage or write failure.
- Added `NVS: READY` / `NVS: ERROR` diagnostics to Setup and System.
- Avoided printing Wi-Fi credentials, coordinates, or other private values.

### Verification

- Complete settings, UI, and startup sources passed strict host syntax checks.
- Focused NVS tests covered initialization, fallback, save, reset, per-key
  failures, empty strings, and write lengths under ASan/UBSan.

## Product 43 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT43-WIFI-TIMESTAMP-SYNC`
**Commit:** [`cbe6d0f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/cbe6d0fae79ebd15d5d68d09b8aa78cf940e0a42)
**Status:** Cross-core synchronization fix retained

### Fixed

- Removed `volatile` from the cross-core `lastWifiAttempt` timestamp.
- Protected every read and write with the existing `commandMux` FreeRTOS critical
  section.
- Performed reconnect-due evaluation and timestamp reservation atomically.
- Prevented the main loop from overwriting a newer network-task attempt time.

### Verification

- Strict host compilation passed.
- Focused tests covered locked timestamp recording, reconnect reservation,
  throttling, and timer rollover.
- Static checks found no remaining unprotected access.

## Product 42 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT42-STALE-TRANSPORT-SUCCESS`
**Commit:** [`251fbc4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/251fbc42fb2350a616dbdbb40c7a72a6f97e5f97)
**Status:** Stale-response bookkeeping retained

### Fixed

- Treated a fully completed ADS-B response as a transport success even when its
  generation became obsolete and the payload had to be discarded.
- Reset consecutive transport failures and cleared the current failure stage.
- Reset the core-0 outage timer and per-outage recovery count.
- Prevented an old failure streak from causing premature recovery escalation on
  the next genuine failure.

### Diagnostics and state behavior

- Continued incrementing `discardedResponses`.
- Kept stale-response diagnostics separate from transport failures.
- Did not advance published-data freshness because no snapshot was published.
- Rejected the obsolete payload before publication and scheduled an immediate
  current-generation follow-up.
- Did not advance tracked-aircraft miss state.

### Verification

- Complete app-state and network sources passed strict C++17 host checks.
- ASan/UBSan sequencing tests covered stale success followed by current requests
  and genuine failures.

## Product 41 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT41-FAST-BODY-RECOVERY`
**Commit:** [`8438633`](https://github.com/bcarriveau/esp-aircraft-radar/commit/843863321b5fb2458d8fe60350a585e597129245)
**Status:** ADS-B response-body recovery retained

### Fixed

- Treated the first bounded native `ESP_ERR_HTTP_EAGAIN`, `EAGAIN`,
  `EWOULDBLOCK`, or `ETIMEDOUT` body read as a terminal response-body failure
  instead of repeating proven-dead three-second reads.
- Preserved the native body-read, fallback no-progress, and absolute total
  deadlines while avoiding roughly nine seconds of dead native retries.
- Hard-recycled the ESP32-S3 station radio immediately after a partial native
  response body.
- Retried one second after successful recovery through the serialized core-0
  scheduler.
- Preserved non-overlap and the normal 15-second successful-poll cadence.

### Verification

- Physical logs showed soft association reconnect did not clear a poisoned
  TLS/socket state, while a hard radio recycle restored a complete large transfer.
- Focused host transport sequencing and forbidden-API checks passed.

## Product 40 R3 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-DB-HTTPS-RECOVERY-R3`
**Commit:** [`0b099db`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0b099db0f80efbaae1625dbadde68a6ebc86e01a)
**Status:** Superseded by Product 41

### Fixed

- Stopped native retries and verified fallback after a partial ADS-B response-body
  transport failure.
- Returned `RESPONSE_BODY` immediately to the existing Wi-Fi recovery ladder.
- Preserved two native attempts and one verified fallback for eligible TCP, TLS,
  and HTTP-header failures.
- Excluded body, HTTP-status, oversize, allocation, DNS, Wi-Fi, and JSON failures
  from fallback.

## Product 40 R2 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-DB-HTTPS-RECOVERY-R2`
**Commit:** [`c2a853f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c2a853f7cb7dd6190e7bb368f97bc57a22fc366f)
**Status:** Superseded by Product 40 R3

### Changed

- Combined the generated aircraft database with corrected native/fallback HTTPS
  sequencing.
- Completed both bounded native attempts before considering one independent
  verified fallback for eligible connection/header failures.
- Allowed transient native body stalls to run only within bounded idle and total
  deadlines.
- Added transport release/settling time between implementations.
- Kept fallback disabled for ordinary HTTP statuses, oversized responses, local
  allocation failures, DNS/Wi-Fi failures, JSON failures, and partial bodies.

## Product 40 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-TYPE-DATABASE`
**Commit:** [`244f337`](https://github.com/bcarriveau/esp-aircraft-radar/commit/244f337ed6f88d2bccd5fd8a7500376ba22493b6)
**Status:** Generated database retained and refined by later firmware

### Added

- Added a generated sorted database containing 2,697 unique aircraft designators
  from a pinned ICAO Doc 8643-derived snapshot.
- Added 11,716 collision-checked description aliases used only when an exact type
  designator is absent or unresolved.
- Added deterministic offline generation with source schema, row count, duplicate,
  checksum, alias-conflict, and hash-collision validation.
- Added source revision/date/checksum and generated-count metadata.
- Corrected `C190` / `CESSNA 190` to piston while preserving C17, C170, E190,
  B190, PC12, PC24, C208, C25A, and other collision-sensitive types.

### Classification behavior

- Used exact case-insensitive binary search as the authoritative first step.
- Consulted description aliases and conservative strong fallback only when exact
  designator classification remained unknown.
- Kept ambiguous or unsupported types as `UNKNOWN` instead of guessing.
- Stored tables in read-only firmware data with no runtime heap, PSRAM, or network
  dependency.
- Kept `Target`, `MAX_TARGETS=200`, and all capacity-scaled buffers unchanged.

### Verification

- Verified all 2,697 exact records and 11,716 aliases, sorting, uniqueness, source
  checksum, deterministic regeneration, and sensitive model collisions.
- Strict classifier compilation and ASan/UBSan tests passed.

## Product 39 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT39-STARTUP-FAILURE-PROPAGATION`
**Commit:** [`35b9eca`](https://github.com/bcarriveau/esp-aircraft-radar/commit/35b9ecae8d5c5949950c5512fc3359d575bd1210)
**Status:** Startup reliability retained

### Fixed

- Changed `ui::buildUi()` to report success only after all required radar,
  navigation, page-shell, and detail-panel construction completed.
- Prevented networking from starting after required UI construction failed.
- Changed `adsb::begin()` to report initialization failure.
- Allocated the core-0 incoming target buffer before task creation so PSRAM
  failure is visible synchronously.
- Checked `xTaskCreatePinnedToCore()` against `pdPASS` and released the incoming
  buffer on failure.
- Set `startupComplete` only after all required components were ready.
- Added a stable `STARTUP HALTED` LVGL screen for post-display fatal failures.

### Preserved

- Initial Wi-Fi timeout remained recoverable after required memory and task
  creation succeeded.
- Core-0 transport ownership, capacity, location invalidation, radar/UI behavior,
  panel timing, and memory architecture remained unchanged.

## Product 38 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT38-LOCATION-INVALIDATION`
**Commit:** [`34a3c2d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/34a3c2d6dc7193b6b3a38490cfefb9ddbfd72c1c)
**Status:** Location-state behavior retained

### Fixed

- Invalidated the published aircraft snapshot immediately when saved radar-center
  coordinates changed.
- Incremented the request generation, cleared visible count and publication age,
  advanced the target version, and marked the state as awaiting new-center data.
- Displayed `LOCATION CHANGED / UPDATING` until a successful current-generation
  snapshot was published.
- Invalidated on reset-to-defaults only when coordinates actually changed.

### Tracking behavior

- Retained the tracked ICAO internally while new-location data was pending.
- Did not count configuration invalidation as a missing tracked update.
- Suppressed old aircraft and stale tracked presentation until current-generation
  data arrived.
- Preserved last-good retention for ordinary range changes and same-location
  transport failures.

### Verification

- Strict app-state compilation and ASan/UBSan state tests passed for invalidation,
  pending suppression, tracked identity retention, republish, and confirmed misses.

## Product 37 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT37-FALLBACK-HTTPS-HARDENED`
**Commit:** [`63892b1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/63892b11529f609370176159353c4dd67dddd23a)
**Status:** Hardened fallback architecture retained

### Fixed

- Removed insecure fallback TLS behavior and attached the built-in Espressif CA
  bundle while keeping hostname verification active.
- Removed whole-response Arduino `String` buffering and duplicate header/body
  copies.
- Added a bounded streaming fallback reader with PSRAM-first payload allocation.
- Enforced a 250,000-byte decoded response limit before allocation or copy.
- Added 15-second header, 12-second no-progress, and 45-second absolute response
  deadlines.
- Added bounded request writes so partial TLS writes cannot silently truncate the
  request.

### Framing and bounds

- Added strict `Content-Length`, chunked, and required close-delimited body support.
- Added 512-byte line, 8,192-byte header, and 16,384-byte chunk-framing limits.
- Supported bounded chunk extensions and trailers.
- Rejected conflicting content lengths, duplicate/unsupported transfer encodings,
  mixed length/chunked framing, malformed headers, invalid CRLF, forbidden framing
  trailers, and oversized decoded bodies.
- Kept only one decoded body buffer and did not retain raw framing copies.

### Fallback policy

- Kept native ESP-IDF HTTPS as the preferred transport.
- Limited fallback to eligible native TCP, TLS, and HTTP-header transport failures.
- Did not invoke fallback for HTTP 429/500, oversize, local allocation, Wi-Fi,
  DNS, response-body, or JSON failures.

### Verification

- Host fallback compilation and focused parser/framing tests passed.
- Static checks confirmed no `setInsecure()`, blocking `HTTPClient::GET()`, or
  whole-response `readString()` path.

## Product 36 R4 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-OVERLAP-PRIORITY-R4`
**Commit:** [`9de21e3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9de21e3d7ac242cdbc57c240b1328b05bc6f53c3)
**Status:** Superseded; priority concepts refined by Product 46

### Changed

- Added deterministic contact-overlap priority for tracked, selected, and normal
  aircraft.
- Refined contact ordering, tag clearance, and touch-hit behavior in dense
  20-mile traffic.
- Preserved 16-heading sprites at 20 miles and compact dot rendering at 40/80.

### Verification

- Focused renderer and bounds testing covered dense overlap, tag placement, and
  selected/tracked hit priority.

## Product 36 R3 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-HEADING-SPRITES-R3`
**Commit:** [`022e7b3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/022e7b3e99311b7ab53effe7399d45b3e1459055)
**Status:** Heading-sprite architecture retained

### Added

- Added compact 25x25 radar contact sprites derived from the repository aircraft
  artwork.
- Added sixteen precomputed clockwise heading variants per category at 22.5-degree
  spacing.
- Selected a heading variant from each target's ADS-B `track` value.
- Used a north-facing fallback when valid track data was unavailable.

### Performance and bounds

- Performed no runtime image rotation, temporary image allocation, per-contact
  LVGL object creation, or capacity change.
- Kept rendering bounded to 25x25 pixels per visible 20-mile contact.
- Kept existing hit radius coverage and 40/80-mile compact dots.

### Verification

- Strict renderer compilation, ASan/UBSan tests, heading-bucket boundary tests,
  and non-empty-mask checks passed.

## Product 36 R2 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS-R2`
**Commit:** [`9494747`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9494747a6b1f8d1233ce03d8c40db140322dd529)
**Status:** Superseded by Product 36 R3

### Changed

- Refined the radar-contact artwork and category silhouettes for better separation
  at embedded-display scale.
- Improved recognizable airliner, business-jet, turboprop, piston, helicopter,
  and unknown contact forms.
- Kept the change asset/rendering-only.

## Product 36 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS`
**Commit:** [`3ea59bb`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3ea59bbefd674b3728beb98af0a692e56ebb5819)
**Status:** First bitmap-contact candidate; superseded by later Product 36 revisions

### Added

- Replaced plain 20-mile contact dots with category-specific aircraft bitmap
  contacts.
- Used Product 35 target-aware classification to choose the contact artwork.
- Kept 40-mile and 80-mile contacts as compact dots.
- Preserved selected amber, tracked red, collision-aware tags, stable ICAO hit
  testing, outward auto-zoom, and bounded rendering.

## Product 35 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT35-DESCRIPTION-TYPE-FALLBACK`
**Commit:** [`0441bd7`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0441bd70f85abb2da72314540b6c0abb15f9683c)
**Status:** Description fallback retained under Product 40 database

### Added

- Added conservative description classification for targets whose ADS-B type code
  was empty, unknown, or not present in the existing tables.
- Added compact keyword references for airliners, business jets, military/heavy,
  turboprops, piston aircraft, and helicopters.
- Added target-aware helpers so radar, Tracks, details, bitmaps, and Airspace could
  use the stored ADS-B description.

### Behavior

- Kept type-code classification authoritative.
- Consulted `Target::description` only when type-code classification returned
  `UNKNOWN`.
- Normalized text without heap allocation, Arduino `String`, regular expressions,
  or additional network requests.
- Left ambiguous descriptions as `UNKNOWN` rather than guessing from weak terms.

### Verification

- Strict host classifier tests covered C-17/Cessna 172, Citation/Cessna piston,
  PC-24/PC-12, AH-64/Piper Apache, Airbus helicopter/airliner, and Dornier jet/
  turboprop collisions.

## Product 34 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY`
**Commit:** [`239a7a5`](https://github.com/bcarriveau/esp-aircraft-radar/commit/239a7a5af8442562e8024f5dec98b6f58c9eddc2)
**Status:** Tracking-loss behavior retained

### Fixed

- Added a three-successful-current-generation-update grace period for a tracked
  aircraft that temporarily disappears from the feed.
- Kept tracking active through one or two confirmed misses.
- Displayed a temporary `TRACK SIGNAL LOST` state while checking the next update.
- Cleared tracking atomically after the third confirmed miss and returned the UI
  to normal idle behavior.
- Reset the miss counter immediately when the tracked ICAO returned.
- Did not advance the miss count on failed requests, stale discarded responses,
  range/location invalidation, manual STOP TRACK, or selection changes.

### Tooling

- Moved generated PlatformIO work outside the Google Drive project directory.
- Added project-check configuration that skips downloaded package headers during
  cppcheck without changing normal compile/link/upload behavior.

## Product 33 R5 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`
**Commit:** [`14fe9f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/14fe9f46b8c131ed1a22f5adb93e898f3188e3b1)
**Status:** Superseded by Product 34

### Changed

- Corrected Setup longitude-label and input spacing.
- Made the selected/tracked left-panel heading reflect the number of populated
  other-aircraft rows: `NO OTHER`, `NEAREST 1`, `NEAREST 2`, or `NEAREST 3`.
- Kept the list bounded to three entries.
- Hid unused labels, icons, and ICAO slots.
- Increased the nearest-other heading size.
- Completed the static Airspace live-highlights fit cleanup.
- Preserved stable ICAO identity for nearest-other selection and details.

### Revision note

- Repository history retains Product 33 R2 and R5 as standalone commits.
- A physically reviewed Product 33 R3 source was referenced by the documentation,
  but its exact standalone commit is not preserved.
- The Product 33 R4 longitude-spacing correction was folded into R5 rather than
  retained as a separate commit.

## Product 33 R2 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R2`
**Commit:** [`52367df`](https://github.com/bcarriveau/esp-aircraft-radar/commit/52367dfa5f85491f2813a3d7103c0aa4a5cf6953)
**Status:** Superseded by Product 33 R5

### Changed

- Improved Setup field styling and spacing.
- Enlarged and repositioned the nearest-other heading.
- Expanded the Airspace live-highlights card.
- Removed Airspace highlight scrolling and its timer/offset/direction state.
- Kept all live-highlight sections visible at once.
- Repositioned and simplified the project credit card.

## Product 32 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT32-UI-DASHBOARD`
**Commit:** [`a4cc594`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a4cc594f52c8d8e562cd751dbb528a744d72d00e)
**Status:** Airspace dashboard retained

### Added

- Added an Airspace dashboard with total aircraft, aircraft inside 20/40 miles,
  current range, category counts/percentages, and nearest, fastest, lowest, and
  dominant-category highlights.
- Added category cards for airliners, business jets, turboprops, piston aircraft,
  helicopters, and military/other traffic.
- Added up to three nearest-other aircraft during selected/tracked operation.
- Used stable ICAO identity for nearest-other rows.
- Added a restrained project identification card on System.

### Changed

- Removed duplicate Setup range controls.
- Kept the compact radar selector as the only 20/40/80 control.
- Added the radar `MILES` caption and matching label exclusion.
- Expanded fixed side-icon storage in PSRAM from 7 to 16 buffers.

### Verification

- UI/radar strict syntax checks and ASan/UBSan tests covered zero, one, and 200
  targets plus selected/tracked ordering and bounds.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`
**Commit:** [`1747cb1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1747cb169969dff12f3a5d794f561b56dc2acc83)
**Status:** Side-panel bitmap and heading behavior retained

### Added

- Added aircraft-type bitmap icons to the idle nearest card, selected/tracked
  details, and nearest-five list.
- Added a rotating heading arrow and heading value for the idle nearest aircraft.
- Added independent persistent point arrays for idle and selected/tracked arrows.

### Changed

- Removed the redundant plain-text idle heading line.
- Made the arrow and heading label select the same stable ICAO aircraft.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`
**Commit:** [`50821ba`](https://github.com/bcarriveau/esp-aircraft-radar/commit/50821badc68efb2e0c8dab597d1db5f1267628ab)
**Status:** Current target-capacity and memory architecture baseline

### Added

- Added bounded storage for up to 200 retained targets.
- Added diagnostics for received, eligible, stored, capacity-dropped, and visible
  aircraft.
- Required PSRAM for shared targets, incoming ADS-B targets, UI snapshots, radar
  hit regions, screen contacts, and label-collision boxes.
- Added clean startup failure when required PSRAM allocation is unavailable.
- Logged the largest free internal-memory block before ADS-B networking starts.

### Changed

- Preserved the tracked aircraft when returned, then filled remaining capacity
  nearest-first.
- Removed internal-RAM fallback paths for capacity-scaled buffers.
- Prevented UI/network servicing after incomplete startup.

### Fixed

- Corrected an earlier 200-target attempt that left insufficient contiguous
  internal RAM for TLS and caused mbedTLS `-0x7F00` allocation failures.

### Verification

- App-state publication, snapshot, tracking, renderer, hit testing, allocation
  failure, and cleanup paths passed focused tests.
- Physical logs showed required PSRAM allocations and successful large native TLS
  requests.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`
**Commit:** [`237d06a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/237d06a2b79b0ca76960ad594c435ee0947716a2)
**Status:** UI-state behavior retained

### Fixed

- Matched radar label-box allocation and bounds checks to `MAX_TARGETS + 2`.
- Used stable rendered ICAO hex for the left nearest card and right nearest list.
- Limited left-panel click handling to nearest-aircraft content.
- Hid idle details while selected/tracked details had priority.
- Added explicit Radar and Tracks detail origins.
- Kept the correct tab active while details were open.
- Returned INFO from Radar to Radar and Details from Tracks to Tracks.
- Paused the selected timeout while details were open and refreshed it on return.
- Closed detail overlays cleanly when another navigation tab was selected.

## Product 28 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT28-RADAR-STATE-FLOW`
**Commit:** [`61f104c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/61f104ca4a8adb6bf7d9ffb6caf3509ddb05797c)
**Status:** Superseded by Product 29

### Changed

- Restored nearest-aircraft information to the left panel while idle.
- Gave selected and tracked details priority in the right panel.
- Moved STOP TRACK into the right tracked-aircraft card.
- Added explicit detail return behavior and accurate active-tab state.
- Moved selected/tracked actions out of the radar canvas.
- Preserved selection when stopping tracking when practical.

### Fixed

- Corrected confusing page changes when opening aircraft details.
- Improved idle, selected, and tracked visibility transitions.

## Product 27 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT27-RADAR-LAYOUT`
**Commit:** [`2dda7ae`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2dda7ae4606dff08fcd34d303610f9d920185362)
**Status:** Superseded

### Changed

- Reworked idle, selected, and tracked panel layout.
- Moved selected/tracked information into the right aircraft card.
- Added INFO, TRACK, CLEAR, and right-panel STOP TRACK actions.
- Kept the compact range selector at the radar lower-right.
- Improved nearest-aircraft list presentation and selected/tracked colors.
- Removed redundant left-side range text.
- Changed nearest-list taps to select an aircraft instead of immediately changing
  pages.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`
**Commit:** [`298c87a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/298c87ab43d83ae51e0276fb152789e05ad7423e)
**Status:** First themed-tag and direct-interaction candidate

### Added

- Added compact dark-navy 20-mile tags with cyan identifiers and teal borders.
- Added stable ICAO-based hit regions for contacts and tags.
- Added hit-testing priority: tracked, selected, then closest contact.
- Added temporary amber selected-aircraft state.
- Added INFO and TRACK actions.
- Added the compact 20/40/80 selector inside the radar.

### Changed

- Made the nearest-aircraft panel select its aircraft while idle.
- Kept tracked labels and outward auto-zoom at higher priority.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`
**Commit:** [`1c1d555`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee)
**Status:** First-soak cleanup retained

### Fixed

- Eliminated expected first-run Preferences errors for missing NVS keys.
- Initialized missing title, Wi-Fi SSID/password, latitude, and longitude keys
  with configured defaults.
- Kept existing saved values unchanged.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`
**Commit:** [`dec570e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/dec570eab7324ae6cc00747a037af2090cdf94bd)
**Status:** Recovery architecture retained and later hardened

### Fixed

- Added bounded retries for stalled or incomplete ADS-B bodies.
- Treated early zero-byte reads as incomplete responses.
- Closed native connections cleanly before fallback or retry.
- Preserved the most advanced failure stage across attempts.
- Reconnected Wi-Fi after incomplete downloads.
- Escalated repeated failures to a station-radio recycle.
- Retried promptly after recovery.
- Moved last-resort restart execution from the network task to the main loop.
- Preserved the last valid aircraft snapshot during failures.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`
**Commit:** [`faa9bc2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/faa9bc28b8524ff9fb850636e1bf356f508d71da)
**Status:** Physical crash fix confirmed

### Fixed

- Replaced unsupported floating-point LVGL formatting with integer heading
  formatting.
- Fixed the core-1 `LoadProhibited` crash that appeared after aircraft populated.
- Normalized headings to integer values from 000 through 359.
- Removed remaining floating-point LVGL format calls from range text.

### Verification

- Complete compile/link passed.
- Repeated physical aircraft updates confirmed the heading crash was fixed.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`
**Commit:** [`3203bd3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3203bd3a4263b7c1f65a839466a083d7b9cd8c90)
**Status:** Large-response handling retained

### Fixed

- Retried temporary `EAGAIN`, `EWOULDBLOCK`, and timeout conditions during native
  HTTPS body reads.
- Prevented valid large ADS-B responses from being discarded prematurely.
- Retained independent no-progress and total-response deadlines.

### Verification

- Runtime testing completed a 105,690-byte response containing 189 parsed
  aircraft and the then-bounded 100 published targets.

## Products 19-21 - 2026-07-21

**Final build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`
**Commit:** [`5d5b0b6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9)
**Status:** Combined GitHub record; individual Product boundaries not preserved

### Confirmed changes

- Removed 160 miles and limited ranges to 20, 40, and 80 miles.
- Added predictive outward auto-zoom for tracked aircraft.
- Added a full-width popup keyboard for Setup fields.
- Prevented aircraft identifiers from wrapping.
- Removed the redundant upper-left radar status overlay.
- Changed the left panel to tracked-aircraft information while tracking.
- Added the rotating heading arrow and heading value.
- Kept the tracked panel active through temporary target omissions.
- Opened details for whichever aircraft the left panel represented.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`
**Commit:** [`69dce61`](https://github.com/bcarriveau/esp-aircraft-radar/commit/69dce612211326a4a41f0f66becc8eb7d46191f9)
**Status:** First physically working native TLS baseline

### Fixed

- Attached Espressif's CA certificate bundle to native HTTPS.
- Kept hostname verification enabled.
- Corrected Product 17's missing server-verification configuration.
- Added a secure fallback HTTPS path.
- Reduced unnecessary Wi-Fi reconnect churn.

### Verification

- Compile/link passed.
- Initial physical TLS testing passed.

## Product 17 - 2026-07-21

**Standalone commit:** Not preserved
**Status:** Confirmed precursor documented by Product 18 history

### Changed

- Replaced Arduino secure-client plus hand-written HTTP handling with native
  ESP-IDF streaming HTTPS.
- Re-resolved DNS and created a fresh native client for each retry.
- Kept HTTPS work on the core-0 network task.
- Preserved deadlines, size guards, PSRAM payload storage, generation rejection,
  and single-snapshot publication.
- Added detailed native error, socket errno, RSSI, and TCP-versus-TLS diagnostics.

### Known issue

- The first physical test failed before the handshake because a server-verification
  method was not configured. Product 18 corrected this.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`
**Commit:** [`c81b34e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c81b34e5d640e734723964f895c9bb4ec49c1af8)
**Status:** Superseded by native HTTPS Products 17-18

### Changed

- Used the resolved server IP for TCP while retaining hostname TLS SNI.
- Increased the TLS handshake allowance from 10 to 20 seconds.
- Logged exact mbedTLS errors and descriptions.
- Recycled Wi-Fi only for Wi-Fi, DNS, or TCP failures.
- Retried TLS, HTTP, body, and JSON failures without deliberately disconnecting a
  healthy station connection.

### Verification

- Complete compile/link passed.
- Physical testing still encountered TLS timeouts, leading to Product 17.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`
**Commit:** [`b2a0a49`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b2a0a492c424cf192edf71eb7f5dd496ec0bbab8)
**Tag:** `product-15-hardened`
**Status:** First hardened version-controlled and permanent rollback baseline

### Added

- Added modular PlatformIO/C++ and LVGL firmware for the exact Waveshare
  ESP32-S3-Touch-LCD-7.
- Added core-0 ADS-B networking and Wi-Fi recovery ownership.
- Added true 15-second start-to-start polling with non-overlapping requests.
- Added bounded connect, header, body-idle, and total-response deadlines.
- Added separate Wi-Fi, DNS, TCP, TLS, HTTP, response-body, JSON, and stale-response
  failure classification.
- Added request generations, obsolete-response rejection, and last-good retention.
- Added thread-safe single-snapshot publication and radar rendering.
- Added collision-aware 20-mile labels and stable ICAO tracking.
- Added Setup validation, protected reset behavior, and system diagnostics.
- Added explicit aircraft categories and unknown-artwork fallback.
- Added a private configuration example while excluding `include/config.h` from Git.

### Display and memory baseline

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM framework.
- OPI PSRAM with `BOARD_HAS_PSRAM`.
- Waveshare RGB timing, DMA, anti-rolling protections, and the 20-scanline RGB
  bounce buffer.

### Verification

- Compile/link passed.
- Product 15 remains the permanent hardened rollback baseline.

---

## History boundary

The authoritative numbered GitHub Product history begins at Product 15. Product
15 preserved the proven RGB anti-rolling configuration from Product 14, but this
repository does not contain authoritative Product 1-14 history. Those earlier
releases are intentionally omitted rather than reconstructed from memory,
previous chats, or uncertain files.
