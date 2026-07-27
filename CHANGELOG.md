# Changelog

All notable **confirmed** changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
Dates follow preserved firmware build markers. Commit links refer to the current
`main` history in `bcarriveau/esp-aircraft-radar` where a standalone commit is
available.

This consolidated file replaces the earlier split changelog that stopped after
Product 42. It contains the continuous confirmed history from the current Product
51 source back through Product 15, the first hardened version-controlled baseline.
Earlier Product history is intentionally omitted where the repository does not
provide authoritative evidence.

## Current status

- **Current source:** Product 51
- **Current build marker:** `7IN-20260727-PRODUCT51-AIRPORT-UI`
- **Current branch:** `main`
- **Product 51 source baseline:** Product 50 local hardware-tested source derived from [`29cb94c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/29cb94c1ab6d899483ebcd0269ea6f291fa3d85f)
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262 RGB LCD, GT911 touch, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback baseline:** Product 15
- **Recommended rollback tag:** `product-15-hardened`

Product 51 is host-verified but still requires a complete PlatformIO compile/link,
flash and memory report, firmware upload, physical regression checks, and extended
soak testing before being called a fully verified hardware release.

## Product 51 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT51-AIRPORT-UI`
**Status:** Host-verified airport-label and System-UI refinement candidate

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

### Pending verification

- PlatformIO compile/link and flash/RAM report.
- Upload and confirmation of the Product 51 build marker.
- Physical review of stable airport labels, System layout, touch behavior, and the
  padded Save Airports control.
- Normal radar interaction checks and soak testing.

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
