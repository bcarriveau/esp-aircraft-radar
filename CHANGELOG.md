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

- **Current replacement source:** Product 81
- **Current build marker:** `7IN-20260807-PRODUCT81-80MI-HEADING`
- **Source baseline branch:** `main`
- **Current committed baseline:** Product 80 `ec320cca4ad53294f78cf1e933a54978af414e3a`
- **Product 79 source commit:** `997a5813bb1031ac6d5929cf849950ce23d51a2b`
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262 RGB LCD,
  GT911 touch, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback baseline:** Product 15
- **Recommended rollback tag:** `product-15-hardened`

Product 81 is a focused replacement-source candidate based directly on committed
Product 80 `main` commit `ec320cca4ad53294f78cf1e933a54978af414e3a`.
It changes only the 80-mile aircraft contact orientation behavior plus Product
identity/documentation/tests. PlatformIO and physical verification are not claimed.

## Product 81 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT81-80MI-HEADING`  
**Source baseline:** Product 80 `main` commit
`ec320cca4ad53294f78cf1e933a54978af414e3a`  
**Status:** Focused replacement-source candidate; focused host validation complete;
PlatformIO and physical verification pending

### Changed

- The 11x11 aircraft symbols at the 80-mile range now use each contact's existing
  discretized heading index instead of forcing every 80-mile symbol to the
  north-oriented heading bucket.
- Retains the existing 16 heading buckets and existing aircraft contact bitmap
  database; no new artwork, bitmap cache, task, timer, or render-loop allocation
  is introduced.
- Keeps the Product 79 25x25 symbols at 20 miles and 17x17 heading-aware symbols
  at 40 miles unchanged.

### Preserved

- Product 80 avionics boot splash, saved-name branding, boot radar sweep, and live
  north marker are unchanged.
- Selected amber, tracked red, normal cyan, sweep tint, rings, labels, stable ICAO
  hit testing, dirty-region restoration, and coherent snapshot rendering remain
  unchanged.
- ADS-B networking, native/fallback HTTPS, 15-second cadence, Wi-Fi/TLS recovery,
  stale-response rejection, last-good retention, MQTT, OTA, 200-target capacity,
  panel timing, DMA, OPI PSRAM, and the 20-scanline RGB bounce buffer are unchanged.

### Validation

- Five focused Product 81 range-symbol regression tests passed.
- Complete renderer comparison against the Product 80 retained renderer confirmed
  the intended runtime behavior change is limited to using `screen.headingIndex`
  for 80-mile contacts instead of forcing heading bucket 0.
- PlatformIO compile/link, memory totals, upload, physical display testing, and soak
  testing were not run here.

### Pending verification

- Confirm boot serial output reports `7IN-20260807-PRODUCT81-80MI-HEADING`.
- At 80 miles, confirm aircraft silhouettes visibly follow heading while remaining
  readable at 11x11.
- Confirm 20- and 40-mile symbol appearance and heading behavior remain unchanged.
- Confirm selection, tracking, labels, sweep smoothness, touch hit testing, and
  dirty-region restoration remain stable under dense 80-mile traffic.

## Product 80 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT80-BOOT-SPLASH`  
**Source baseline:** Product 79 `main` commit
`997a5813bb1031ac6d5929cf849950ce23d51a2b`

### Added

- Added the lightweight avionics boot splash with saved System display-name
  branding, centered radar emblem, north marker, startup status text, and a
  1.6-second clockwise sweep.
- Added the small centered `N` marker above the live radar circle.
- Keeps the splash visible for at least 3.8 seconds while normal startup proceeds.

### Preserved

- The splash overlays the existing operational UI and does not replace the radar
  renderer or alter contact projection.
- Product 79 aircraft symbols, stable ICAO interaction, networking, OTA, MQTT,
  target capacity, panel timing, DMA, OPI PSRAM, and bounce-buffer protections
  remain unchanged.

## Product 79 - 2026-08-06

**Build:** `7IN-20260806-PRODUCT79-RANGE-SYMBOLS`  
**Source baseline:** Product 78 `main` commit
`87e3382d23b68a55f3bc7ec7167641d3a7eceea4`

### Changed

- Keeps the existing full-size 25x25, 16-heading aircraft contact sprites unchanged
  at 20 miles.
- Reuses the same checked-in contact sprites directly at render time, sampling them
  to 17x17 at 40 miles and 11x11 at 80 miles.
- Preserves heading direction at 40 miles; Product 79 initially used a stable
  north-oriented type silhouette at 80 miles.
- Removes the 40/80-mile dot substitution without adding new bitmap assets,
  startup caches, or frame-loop allocations.
- Preserves tracked red, selected amber, normal cyan, sweep tint, rings, stable ICAO
  hit testing, label priority, dirty-region restoration, target capacity, ADS-B
  networking, TLS, panel timing, DMA, OPI PSRAM, and the 20-scanline RGB bounce
  buffer.

## Product 78 - 2026-08-05

**Build:** `7IN-20260805-PRODUCT78-PAGE-TOP-RESET`  
**Source baseline:** Product 77 `main` commit
`d4b60cddfecdc943c2d33231bc1a76289b85760b`  
**Status:** Focused replacement-source candidate; host validation complete;
PlatformIO and physical verification pending

### Diagnosed

- The fixed 112-pixel selected/tracked secondary heading still used Montserrat 16.
  `NEAR SELECT` reached the panel edge and visibly clipped on the 800x480 display.
- Tracks deliberately preserved its prior LVGL table scroll position across refreshes,
  but that same state also survived leaving and re-entering the page.
- The Airports directory likewise retained its prior table scroll position when the
  page or directory view was entered again.

### Changed

- Reduced only the selected/tracked secondary heading to Montserrat 14 so
  `NEAR SELECT`, `NEAR TRACK`, `POSITION LOST`, and `NO OTHER` fit the existing
  fixed panel without changing its geometry.
- Added explicit navigation-time scroll resets for Tracks and the Airports
  directory.
- Entering Tracks, returning from a Tracks Aircraft Profile, entering Airports, or
  returning to the Airports directory now starts at the top.
- Applies each reset after the table rows are populated, including no-data and
  optional-storage-unavailable directory states.
- Preserves the established scroll position during ordinary target or airport refreshes
  while the user remains on the page; refreshes do not repeatedly force the top.
- Reuses existing LVGL tables and adds no task, timer, dynamic allocation, page object,
  target buffer, or capacity-scaled storage.

### Preserved

- Product 77 live stable-ICAO Aircraft Profiles and current/last-known states.
- Product 76 relative-neighbor calculation, row actions, selected/tracked state, and
  radar-renderer behavior.
- Tracks row-count scroll clamping during in-page refresh, airport edit/tap safety,
  airport directory bounds, and all page content.
- ADS-B networking, native/fallback HTTPS, Wi-Fi recovery, MQTT, local/remote OTA,
  200-target capacity, panel timing, DMA, OPI PSRAM, 128 KiB LVGL pool, 12 KiB ADS-B
  task stack, and the 20-scanline bounce buffer.

### Validation

- Thirteen focused Product 78 Python tests passed for build identity, compact heading
  font, Tracks/Airports entry resets after content rendering, profile/directory return
  behavior, in-page scroll preservation, and allocation-free implementation.
- The complete Product 77 live-profile focused suite still passed: 14 tests.
- The retained Product 76 priority-neighbor suite passed: 8 tests.
- A strict C++17 page-entry scroll model passed with warnings treated as errors,
  AddressSanitizer, and UndefinedBehaviorSanitizer.
- Complete changed-file lexical, whitespace, build-marker, forbidden-API, and
  unintended-scope checks passed.
- PlatformIO compile/link, OTA asset generation, upload, physical display/touch
  testing, and soak testing were not run here.

### Pending verification

- Confirm the Product 78 marker at boot.
- Confirm all selected/tracked secondary heading states fit without clipping.
- Scroll Tracks and Airports down, leave and re-enter, and confirm both start at the
  top. Repeat after returning from their profile/settings views.
- Stay on each page through live refreshes and confirm the current scroll position is
  retained until the page is entered again.
- Confirm Product 77 live profiles, Product 76 neighbor rows, 20/40/80 ranges, MQTT,
  local/remote OTA, airports, display stability, and memory recovery remain unchanged.

## Product 77 - 2026-08-05

**Build:** `7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE`  
**Commit:** [`d4b60cd`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d4b60cddfecdc943c2d33231bc1a76289b85760b)  
**Status:** Committed focused implementation; host validation recorded; PlatformIO
and physical verification not recorded here

### Diagnosed

- Opening **INFO** copied one aircraft target into the profile and rendered all
  fields once.
- While the detail overlay remained open, normal Radar rendering and page-content
  refresh intentionally stayed paused.
- ADS-B publications continued, but the open profile did not resolve its aircraft
  again, so distance, bearing, altitude, speed, heading, vertical rate, identity
  text, and preview remained frozen until the profile was closed and reopened.

### Changed

- Keeps every open Aircraft Profile keyed to its stable ICAO hex rather than a
  target-array position.
- Added one dedicated profile-render helper that updates the stored target, title,
  values, aircraft preview, freshness state, tracking text, button state, and button
  color as one coherent UI operation.
- Added target-version, range-generation, and tracking-version gates. An unchanged
  80 ms UI frame performs no target snapshot copy or profile redraw.
- After a relevant version change, copies one coherent bounded app-state snapshot,
  resolves the same ICAO, and refreshes the profile from current data.
- Fresh data shows `CURRENT UPDATE`.
- A selected aircraft absent from the latest snapshot shows
  `NOT IN CURRENT UPDATE / LAST KNOWN VALUES`; its stale values remain visible but
  starting a new track is disabled.
- A tracked aircraft absent during the established grace period shows
  `TRACK SIGNAL LOST / LAST KNOWN VALUES` and keeps `STOP TRACKING` available.
- When the same ICAO returns, the open profile automatically resumes current values.
- Resets profile version state cleanly on open, close, navigation, and track action.
- Creates no new task, timer, dynamic target container, capacity-scaled buffer, or
  replacement LVGL object during refresh.

### Preserved

- Product 76 relative-neighbor rows and their stable-ICAO actions.
- Selected timeout handling, Radar/Tracks detail origins, BACK behavior, manual
  tracking, tracked-loss grace, STOP TRACK, outward auto-zoom, hit-test priority,
  collision-aware labels, and MPH display.
- ADS-B transport, native/fallback HTTPS, Wi-Fi recovery, MQTT, local and remote OTA,
  airports, 200-target capacity, panel timing, DMA, OPI PSRAM, 128 KiB LVGL pool,
  12 KiB ADS-B task stack, and the 20-scanline bounce buffer.

### Validation

- Fourteen focused Product 77 Python tests passed for build identity, version
  gating, stable-ICAO resolution, one-snapshot refresh, current and last-known
  states, disabled stale tracking, tracked-loss action retention, state reset, and
  200-target bounds.
- The exact changed profile functions passed strict C++17 syntax compilation with
  `-Wall -Wextra -Werror -pedantic` against focused interface stubs.
- A bounded stable-ICAO/version-state model passed strict compilation,
  AddressSanitizer, and UndefinedBehaviorSanitizer.
- The complete changed `src/ui.cpp` passed lexical delimiter, comment, string,
  character, trailing-whitespace, scope, and forbidden-API checks.
- PlatformIO compile/link, generated memory totals, OTA asset generation, upload,
  physical display/touch testing, and soak testing were not run here.

### Pending verification

- Confirm the Product 77 marker at boot.
- Leave Radar and Tracks profiles open across several 15-second publications and
  confirm values follow the same ICAO after target reordering.
- Confirm selected-current, selected-missing, tracked-current, tracked-missing, return,
  STOP TRACK, and third-confirmed-miss behavior.
- Confirm no regressions in Product 76 neighbor rows, 20/40/80 ranges, pages, MQTT,
  local/remote OTA, airports, display stability, heap/PSRAM recovery, and soak.

## Product 76 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT76-PRIORITY-NEIGHBORS`
**Commit:** [`817462c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/817462c8a6f2cb8157c7223a1788c8ff34dfc621)
**Status:** Committed relative-neighbor baseline retained by Products 77 and 78;
separate device validation not recorded here

### Changed

- Replaced the selected/tracked secondary-aircraft ordering by home distance with
  horizontal separation from the current selected or tracked aircraft.
- Converts each candidate's existing home-relative distance and bearing into a
  bounded local north/east position, then calculates relative separation and
  bearing from the priority aircraft.
- Keeps only the nearest three candidates using one pass and a fixed three-entry
  insertion list; no dynamic container or sort was added.
- Uses stable ICAO hex for exclusion, deterministic distance ties, and the existing
  row actions.
- Displays relative distance and compass direction in each secondary row.
- Replaced the old `NEAREST 1/2/3` heading with `NEAR SELECT`, `NEAR TRACK`,
  `NO OTHER`, or `POSITION LOST`.
- Preserved one coherent target snapshot per radar update and did not add a second
  aircraft-copy path.

### Preserved

- Product 75 update-page and boot-state behavior.
- Stable ICAO selection/tracking, tracked-loss grace period, outward auto-zoom,
  collision-aware labels, MPH display, and hit-test priority.
- ADS-B transport, MQTT, local and remote OTA, airports, 200-target capacity,
  panel timing, DMA, OPI PSRAM, and the 20-scanline bounce buffer.

### Validation recorded in the commit

- Added focused Product 76 source/model tests for fixed top-three storage,
  relative rather than home distance, selected/tracked/lost-position states,
  deterministic ties, and build identity.
- PlatformIO compile/link, upload, and physical Product 76 verification are not
  claimed by this documentation update.

## Product 75 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT75-UPDATE-UI-BOOT-CLEAR`
**Commit:** [`16343a1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/16343a1bac4345233ba91721fc36fa8f05bde04f)
**Status:** Focused Software Update UI and boot-state correction; based on the
physically verified Product 73 remote-install implementation

### Changed

- Shortened the System-page update summary button from `UPDATE CHECK` to `UPDATES`
  and reduced/repositioned it to prevent header overlap.
- Reflowed the Software Update page for the 800x480 display with wrapped subtitle,
  installed/available build labels, notes, messages, and aligned action buttons.
- Shortened compact button states to `UPDATES`, `READY`, `CHECKING`, `QUEUED`,
  `INSTALL`, percentage, or `FAILED`.
- Added a new persistent-state schema and a boot-session reset helper.
- Reboot now clears transient checking, manual queue, installation queue,
  installation progress/result, retained available-release identity, and the prior
  attempt timestamp used by the active session.
- A new boot starts a fresh five-minute automatic-check delay while leaving
  **CHECK NOW** ready immediately.
- A failed GitHub check clears the retained release so stale availability cannot
  survive a later failed verification.

### Preserved

- Product 73 package transport, manifest recheck, verification, inactive-partition
  selection, and restart implementation.
- Product 74's physically proven remote-update path.
- Product 72 redirect and transmit-buffer bounds, Product 69 ADS-B budget, local
  browser OTA priority, MQTT serialization, and radar behavior.

## Product 74 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT74-GITHUB-OTA-TEST`
**Commit:** [`156909b`](https://github.com/bcarriveau/esp-aircraft-radar/commit/156909bc7f57d2c16ef9e0c0c8be6a9e42fd0f10)
**Status:** Physically working GitHub OTA test release

### Purpose

- Promoted the Product 73 implementation to numeric version 74 with a distinct
  Product 74 build marker, generated manifest, local package, and versioned GitHub
  package.
- Made no firmware implementation change beyond release identity and generated
  assets.
- Provided the numerically newer release required to exercise Product 73's
  user-confirmed on-device GitHub installer.

### Physical result

- Product 73's remote installer discovered the newer compatible release, downloaded
  and verified the Product 74 package, wrote the inactive OTA partition, completed
  the hardened restart handoff, and booted the newer marker.
- This is the first confirmed working GitHub-to-radar OTA update milestone.

## Product 73 - 2026-08-04

**Build:** `7IN-20260804-PRODUCT73-GITHUB-OTA-INSTALL`
**Commit:** [`d2200f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d2200f4effadf136460c126d18e6505d50aa2740)
**Status:** Remote-install implementation physically verified through Product 74

### Added

- Added a two-tap **DOWNLOAD & INSTALL** action for a previously validated newer
  stable release.
- The first tap arms a 15-second confirmation state; the second queues installation
  for the next successful current-generation ADS-B/network-safe window.
- Rechecks and revalidates the stable manifest immediately before installation.
- Retains version, build ID, package/firmware sizes, and both SHA-256 digests as the
  release identity.
- Cancels installation and requires fresh confirmation if any retained identity
  value changed during the recheck.
- Streams the exact generated `.radarota` asset through verified native ESP-IDF
  HTTPS without allocating the complete package.
- Restricts redirects to approved GitHub asset hosts, HTTPS, no user information,
  no explicit port, and at most three redirects.
- Uses a bounded 4096-byte PSRAM receive buffer and a 1024-byte internal-RAM
  flash-write staging buffer.
- Enforces an eight-second connect/header ceiling per request, fifteen-second
  body-idle ceiling, and three-minute absolute installation ceiling.
- Verifies package magic/version/header, exact hardware, exact build ID, package and
  firmware sizes, package SHA-256, firmware SHA-256, ESP image magic, ESP32-S3 chip
  identity, embedded build ID, exact byte counts, and `esp_ota_end()`.
- Selects the inactive boot partition only after every transport, framing, package,
  image, build, size, and digest check succeeds.
- Reuses the hardened Core-0 restart task and Core-1 IRAM park from local OTA.
- Leaves the verified partition selected and instructs a power cycle if restart
  handoff fails after a fully verified write.

### Ownership and cancellation

- The existing Core-0 ADS-B owner performs the installation, preventing overlap
  with later ADS-B requests.
- MQTT remains gated for the complete install and restart handoff.
- Range/reconnect commands, cancellation, or local browser OTA can stop the remote
  operation at the next bounded transport block.
- Local browser OTA retains priority as the recovery and manual-installation path.
- Last-good aircraft remain displayed during the intentional install.

### Authenticity boundary

- Verified TLS and SHA-256 protect against corruption, truncation, and accidental
  mismatch.
- The manifest and firmware package share the same GitHub publishing account, so
  hashes are not independent protection from repository/account compromise.
- Public-key package signing and automatic first-boot rollback remain separate work.

## Product 72 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT72-GITHUB-TX-BUFFER-FIX`
**Commit:** [`f999347`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f999347897185d41761dc6c896229e002cb7482f)
**Status:** GitHub signed-redirect request-transmission correction retained

### Fixed

- Replaced the fixed 512-byte ESP-IDF HTTP transmit buffer used by GitHub checks.
- Sizes the transmit buffer from the validated current URL length.
- Keeps short requests at a 1024-byte minimum.
- Reserves 512 bytes for request-line suffix and bounded headers.
- Caps the longest accepted request allocation at 4607 bytes for the 4095-character
  redirect limit.
- Reports ESP-IDF open/send failures with URL and transmit-buffer sizes.

### Preserved

- Product 71's 16 KiB response-header bound, 4095-character redirect bound, exact
  header diagnostics, verified TLS, approved hosts, strict framing, and three
  redirects.
- Product 70 scheduling, Product 69 bounded ADS-B transport, local browser OTA
  priority, MQTT serialization, UI, and radar rendering.

## Product 71 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT71-GITHUB-REDIRECT-FIX`
**Commit:** [`a52ee1c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a52ee1cd39f1d39182730418de7192b9779a4307)
**Status:** Real-world GitHub response-header and signed-redirect correction

### Fixed

- Raised the bounded aggregate streamed response-header allowance from 4 KiB to
  16 KiB for real GitHub security/cache header sets.
- Expanded the PSRAM-backed redirect URL capacity from 1023 to 4095 characters.
- Kept only bounded framing fields and the redirect URL rather than retaining all
  headers.
- Added overflow-safe header-byte accumulation.
- Added exact failure classification for total headers, invalid/conflicting
  `Content-Length`, repeated/unsupported `Transfer-Encoding`, oversized Location,
  ambiguous framing, and ESP-IDF fetch failure.
- Added realistic signed-redirect and larger-header sanitizer coverage.

### Preserved

- Verified TLS and hostname checking.
- Approved GitHub hosts and maximum three redirects.
- Rejection of mixed length/chunked framing and other ambiguous responses.
- Product 70 scheduling and Product 69 ADS-B transport bounds.

## Product 70 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT70-GITHUB-UPDATE-CHECK`
**Commit:** [`b20c2d4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b20c2d47c5b3a758564e944e9b912fd562334c1d)
**Status:** Restored bounded GitHub stable-release checker

### Added

- Added a real System-page **CHECK NOW** action with queued, checking, deferred,
  aborted, current, available, no-release, and failed states.
- Added no new task, timer callback, animation, or free-running background network
  loop.
- The existing Core-0 owner may claim one GitHub check only after a successful
  current-generation ADS-B fetch and while established network serialization is
  still active.
- Automatic checks require five stable minutes after boot, approximately 24 hours
  since a completed attempt, successful current ADS-B publication, at least eight
  seconds of cadence slack, connected Wi-Fi, no recovery, local OTA inactive, and
  MQTT outside transition/maintenance.
- **CHECK NOW** bypasses only the five-minute and 24-hour timers.
- Added a six-second absolute check ceiling, bounded connect/header/body work,
  1.5-second ADS-B guard, maximum three redirects, 2048-byte manifest body limit,
  and strict compatibility validation.
- Range refresh, reconnect, or browser OTA aborts the check at the next bounded
  boundary.
- Manual checks aborted by a safe command/OTA condition are requeued without
  consuming the daily allowance or changing ADS-B failures/recovery.
- Added fixed release manifest and versioned `.radarota` generation to the existing
  post-build workflow.
- Added hardware, channel, numeric version, Product label, build ID, asset, size,
  digest, updater-version, and release-note checks.
- Added a static green update indicator and release detail view; **LATER** only
  closes details.

### Initial boundary

- Product 70 checked metadata only. Direct installation was added in Product 73.

## Product 69 - 2026-08-03

**Build:** `7IN-20260803-PRODUCT69-BOUNDED-TRANSPORT`
**Commit:** [`0111ff5`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0111ff5ad7c38ec4fe7464a3064c8a7e218791d6)
**Status:** Physically exercised bounded ADS-B transport baseline retained

### Changed

- Added one shared 12-second complete-fetch budget below the fixed 15-second
  start-to-start cadence.
- Reserved 1.5 seconds for PSRAM JSON deserialization/extraction, leaving a
  10.5-second transport budget.
- Reduced native retry, verified fallback, TCP probe, connect, header, body read,
  idle, release, and wait operations by remaining shared budget.
- Added minimum remaining-budget gates before starting another native attempt or
  fallback.
- Added explicit transport-budget exhaustion reporting.
- Added a read-only OTA cancellation signal checked between bounded blocking calls,
  body/header reads, waits, retries, and fallback operations.
- A cancelled request acknowledges the maintenance hold without recording a false
  ADS-B failure, incrementing recovery, clearing last-good data, or consuming the
  normal cadence.
- Preserved failure counters and most-advanced failure classification for genuine
  transport failures.

### Preserved

- Native HTTPS preference and verified fallback policy.
- Header/body limits, strict framing, certificate/hostname verification, stale
  response rejection, last-good retention, request serialization, and recovery.

## Product 68 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT68-FETCH-CONTENTION`
**Commit:** [`7874b9d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/7874b9d7f87cc375538a2c9e0407522beafcf3fa)
**Status:** Fetch-contention reduction retained by Product 69+

### Changed

- Replaced routine per-stage success and memory lines with one bounded ADS-B
  completion summary.
- Kept detailed successful-stage diagnostics behind
  `ADSB_VERBOSE_FETCH_LOGGING=1` while preserving all failure/retry/recovery logs.
- Measured JSON deserialization and aircraft extraction separately.
- Yielded one scheduler tick after every 16 extracted records, bounded by the
  200-record source capacity.
- Split radar-gap attribution into JSON deserialize, JSON extract, MQTT, and
  diagnostic-output stages.
- Recorded MQTT service windows only when meaningful enough to affect attribution.

### Preserved

- Product 66 dirty-region renderer, Product 63 PSRAM-only payload/JSON policy,
  native/fallback HTTPS, 15-second cadence, 128 KiB LVGL pool, 12 KiB ADS-B stack,
  target capacity, panel timing, and stable ICAO interaction.

## Product 67 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT67-RADAR-GAP-ATTRIBUTION`
**Commit:** [`266659d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/266659d9fb9f7803f5d9703bd65533ce2c8933cd)
**Status:** Physically tested diagnostic build retained in later diagnostics

### Added

- Added a fixed bounded activity-window history for DNS, TLS, response body, JSON,
  publication, radar cache, idle, and other work.
- Attributed each active radar frame-start gap to the stage with greatest overlap.
- Added last/maximum gap-stage diagnostics, per-stage maximums, cache-build duration,
  and System-page maximum-gap stage.

### Physical result

- At roughly 130 retained aircraft, normal radar renders remained around 14-16 ms
  with a measured maximum near 53 ms.
- Captured maximum repeating fetch-associated gaps included roughly 174 ms TLS,
  122 ms body reception, and 180 ms combined JSON work.
- Publication and cache did not produce the repeating hitch in the captured run.
- A one-time 213 ms idle-attributed maximum appeared after MQTT startup, leading to
  Product 68's separate MQTT attribution.

## Product 66 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS`
**Commit:** [`d867327`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d8673272526b71f22b0b4b521565633c8e21854c)
**Status:** Bounded dirty-region renderer retained

### Changed

- Removed Product 65's full 430x360 cached-canvas copy on dense steady-state frames.
- Added a deterministic PSRAM dirty-region list sized from `MAX_TARGETS`.
- Merged overlapping sweep, contact, and tag rectangles before bounded row restores.
- Kept a complete cached-layer fallback if the optional dirty list is unavailable.
- Version-gated the coherent radar target snapshot so it is recopied only after a
  target publish, range generation, or tracking-state change.
- Shared that snapshot with tracked-aircraft auto-zoom.
- Added throttled `RADAR PERF` and `RADAR CACHE` diagnostics.

### Physical result

- Dense 80-mile hardware runs with roughly 124-136 retained aircraft showed normal
  render times around 14-26 ms, worst observed render below 61 ms, `fallback=0`,
  and stable memory recovery.

## Product 65 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT65-RADAR-FRAME-CADENCE`
**Commit:** [`f307ad5`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f307ad558e0effca4d931012d67a8a28fed580c1)
**Status:** Physically tested; core sweep/cache behavior retained

### Changed

- Replaced frame-count sweep movement with elapsed-time movement at approximately
  27.5 degrees per second.
- Added an optional 309,600-byte PSRAM cache for the fixed grid and configured
  airport layer.
- Rebuilt the cache only after range, location, airport-setting, or temporary-focus
  changes.
- Restored prior sweep/contact/tag regions on sparse frames.
- Used a full cached-base copy on dense frames as an initial bounded strategy.
- Added radar render-duration and active frame-gap diagnostics.

### Physical result

- Elapsed-time sweep and static caching improved visible motion.
- Dense 80-mile frames still copied the full cached base every frame and remained
  visibly jerky during TLS, leading to Product 66.

## Product 64 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT64-MEMORY-PHASE2`
**Commit:** [`21e2e06`](https://github.com/bcarriveau/esp-aircraft-radar/commit/21e2e0649f01bb848c728cf37a7f8b6c6b1ed16a)
**Status:** Hardware-verified native HTTPS memory path retained

### Changed

- Reduced the measured core-0 ADS-B task stack from 16 KiB to 12 KiB, recovering
  4 KiB permanent internal memory while retaining roughly 7 KiB observed headroom.
- Added an active fetch-stage label and explicit `tls-handshake` checkpoint so
  periodic low-water samples blocked in `esp_http_client_open()` retain correct
  attribution.
- Moved Firmware / OTA to the System-page header and bounded the title width.
- Kept the 128 KiB LVGL pool unchanged.

### Hardware result

- Repeated native HTTPS requests restored heap/largest block without progressive
  loss, and the 12 KiB task retained approximately 7.4 KiB free on the observed path.

## Product 63 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT63-MEMORY-PHASE1`
**Commit:** [`cf8504f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/cf8504fb68b069fce096187b3f3012bdacee4866)
**Status:** PSRAM memory architecture retained

### Changed

- Added a PSRAM-only ArduinoJson allocator for both the filter and response document.
- Removed internal `malloc()` fallback for ADS-B response payload storage.
- Replaced per-fetch Arduino `String` request/path construction with bounded buffers.
- Moved the 64-entry airport directory array from internal DRAM to PSRAM; allocation
  failure disables only the optional directory page.
- Added fetch-only heap/largest-block lows, lifetime/fetch stage attribution, ADS-B
  task stack headroom, and LVGL pool use/free/largest/fragmentation diagnostics.
- Kept the 128 KiB LVGL pool and then-16 KiB task stack unchanged until measurements.

### Hardware evidence

- Native requests restored internal and PSRAM baselines; JSON completion no longer
  reduced internal heap/largest block.
- Measurements supported the Product 64 task-stack reduction.

## Product 62 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT62-AIRPORT-EYE-COVERAGE`
**Commit:** [`2bae5aa`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2bae5aadeb4207c4c846cdc1f70e52b333b88214)
**Status:** Airport-directory visibility fix retained

### Fixed

- Kept the Airports directory bounded at 64 rows while examining the complete
  bounded nearby-airport set through an optional PSRAM scratch buffer.
- Retained every stable airport identifier whose label was rendered on Radar.
- Replaced only farther non-visible rows and restored distance order afterward.
- Preserved nearest-64 behavior if the optional scratch allocation fails.

## Product 61 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT61-OTA-SOCKET-PACING`
**Commit:** [`5ab7db0`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5ab7db06de45883eda8e96bd5c8633d2fb63b862)
**Status:** Physically verified local browser OTA release

### Diagnosed

- Chrome HAR captures showed rapid connection handoffs resetting `/status` and the
  first multipart `/upload` before firmware writing began.
- Successful retries occurred only after a small delay; responses also contained a
  duplicate `Connection: close` header.

### Changed

- Added 500 ms prepare/status and ready/upload pacing.
- Polls status once per second.
- Added bounded 300/600 ms network-only control retries.
- Allows one upload retry only when authenticated status proves zero received and
  written bytes.
- Exposed transfer counters and removed the duplicate close header.

### Physical result

- One prepare, one status, and one multipart upload completed without connection
  reset; the verified image installed, restarted with `Reset reason: 3`, and resumed
  native ADS-B HTTPS and MQTT with stable resources.

## Product 60 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT`
**Commit:** [`9d23ef3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9d23ef3de361ef4fc900e715a32b484a7a3cc4e9)
**Status:** OTA preparation correction retained

### Fixed

- Repeated authenticated `/prepare` requests during `PREPARING` return HTTP 202;
  repeated requests during `READY` return HTTP 200.
- Duplicate requests do not reset the upload session, request maintenance again,
  or extend the preparation deadline.
- Added one bounded browser recovery path using `/status` when the first prepare
  response is lost.
- Preserved the Product 57 multipart upload route and writer.

## Product 59 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT59-NETWORK-RECOVERY-MEMORY`
**Commit:** [`a040fe4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a040fe46363013f6f18b8c33d74ba8fe6fa19a08)
**Status:** Memory/recovery ownership retained

### Changed

- Released unused ESP32-S3 BLE controller memory during startup.
- Required Core-1 MQTT to close its socket, destroy clients, and release bounded
  buffers before Core 0 recycles the station radio.
- Added bounded acknowledgement and lwIP settlement; hard recovery is deferred if
  MQTT cannot acknowledge.
- Kept the MQTT hold through the reconnect window.
- Made hard Wi-Fi recovery and OTA maintenance mutually exclusive.
- Rejected OTA enable/preparation while recovery owns the network and prevented
  recovery from starting after OTA reservation.

## Product 58 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT58-OTA-EXCLUSIVE-HOLD`
**Commit:** [`a6e96d4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a6e96d4bb5e41ec342b3807039498d5c76bef1c8)
**Status:** Physically verified across consecutive local OTA cycles

### Changed

- Made the complete five-minute OTA window network-exclusive from **ENABLE OTA**.
- ADS-B finishes only an in-flight request and then parks; MQTT disconnects and
  releases its resources immediately.
- Suppressed refresh, reconnect, Wi-Fi recovery, and fallback restart while OTA owns
  the network.
- Retained ownership through preparation, retry, upload, verification, and restart.
- Fixed the dual-core restart crash with an interrupt-masked IRAM Core-1 park,
  atomic acknowledgement, and bounded Core-0 restart task.
- Added reset-reason diagnostics.

### Physical result

- Two consecutive browser updates alternated inactive partitions and restarted
  cleanly with `Reset reason: 3`; ADS-B and MQTT resumed normally.

## Product 57 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT57-AIRPORT-DATABASE-SETUP`
**Commit:** [`c30c330`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c30c330ff3182798d690cbd919c322819a2d17f9)
**Status:** Airport tooling and documentation retained

### Added

- Added `tools/Build Airport Database.bat` as the guided Windows entry point.
- Added a region-independent Python flow for download/cache/local CSV sources.
- Combined airport and runway data and selected the longest open runway.
- Added preview counts and estimated flash use before replacement.
- Added atomic header replacement, validation, and rollback.
- Added generic database/generator tests and `docs/AIRPORT_DATABASE.md`.
- Kept the generated regional header tracked while ignoring downloaded CSVs,
  Python caches, interrupted temporaries, and delivery notes.

## Product 56 R6 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R6-SYSTEM-UI`
**Commit:** [`49035a6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/49035a62e4684ef1696fb325fbc12b0a8a53cce7)
**Status:** Hardware-reviewed Product 56 baseline

- Retained the lightweight Product 56 R5 MQTT implementation and final System-page
  arrangement under the checked-in R6 identity.
- Preserved physically reviewed System Status, Device & Network, and maintenance
  control layout and successful preferred native HTTPS behavior with MQTT connected.

## Product 56 R5 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R5-SYSTEM-UI`
**Status:** Hardware-tested Product 56 final behavior

### Finalized

- Removed the redundant Home Assistant Last Update Age discovery entity and raw field.
- Added one-time retained discovery cleanup for the obsolete entity.
- Kept internal data age for `LIVE`, `UPDATING`, `STALE`, and `OFFLINE` status.
- Finalized the System Status, Device & Network, and two-row maintenance layout.
- Corrected field spacing, card padding, button placement, and reconnect color.

## Product 56 R3 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R3-LIGHTWEIGHT-MQTT`
**Commit:** [`01298c9`](https://github.com/bcarriveau/esp-aircraft-radar/commit/01298c97d0068e1c84dde6ddb7e28e9f3dd30b43)
**Status:** Hardware-tested MQTT memory correction retained

### Changed

- Replaced native ESP-MQTT with task-free PubSubClient 2.8 after Product 56 R2
  fragmented the largest internal block and prevented later TLS handshakes.
- Removed native MQTT task, input/output buffers, and outbox.
- Added a 384-byte internal packet buffer and streamed larger retained messages from
  bounded PSRAM JSON storage.
- Limited publication to one message per service interval and paused all MQTT socket
  work while ADS-B reports an active fetch.
- Delayed cold-boot MQTT startup until an ADS-B-safe window.

### Hardware result

- Repeated preferred native ADS-B TLS requests continued with MQTT connected and no
  recurrence of the Product 56 R2 allocation failure.

## Product 56 R2 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R2-MQTT-MEMORY`
**Commit:** [`66d939a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/66d939a238dd7fdffe37b463fc8acdbcb3e99f2f)
**Status:** Hardware-tested and superseded by R3

- Reduced native MQTT task stack and I/O buffers.
- Hardware still showed persistent largest-block collapse after the first ADS-B TLS
  request, causing later native and fallback TLS allocation failures.

## Product 56 R1 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-R1-MEMORY`
**Commit:** [`ba90a31`](https://github.com/bcarriveau/esp-aircraft-radar/commit/ba90a317085ea938bcf7847b0999623b78dfe03c)
**Status:** Diagnostic revision superseded by R2/R3

- Added heap, largest-block, and PSRAM checkpoints across ADS-B and MQTT stages.
- Added current/minimum largest internal block to System.
- Confirmed native MQTT persistent allocations reduced the contiguous block needed
  by ADS-B TLS.

## Product 56 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT56-HA-MQTT`
**Commit:** [`8b4fca1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/8b4fca13c4893b863b7413eec447fcef3d0b8612)
**Status:** Initial MQTT implementation, corrected by later revisions

### Added

- Added optional, disabled-by-default Home Assistant MQTT discovery and retained
  availability.
- Added backlight, shared range, and refresh controls.
- Added bounded aircraft, tracked, nearest, Airspace, Wi-Fi, build, OTA, and MQTT
  telemetry.
- Added supplied dashboard YAML using built-in cards.
- Added System-page MQTT control and checked NVS enable state.
- Coordinated MQTT shutdown/resource release with local OTA maintenance.

## Product 55 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT55-AIRSPACE`
**Commit:** [`0bc4142`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0bc41425a89570a1698dbf5d601ae4218b2f15b6)
**Status:** Airspace interaction retained

- Made the green `CURRENT RANGE` card cycle the shared 20/40/80-mile range.
- Replaced dominant category with highest airborne.
- Added independent rows for nearest, fastest, lowest, and highest airborne.
- Made each populated row resolve stable ICAO against a fresh snapshot and select
  the aircraft on Radar.
- Preserved active tracking as authoritative.

## Product 54 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT54-LOCAL-WEB-OTA`
**Commits:** [`68c4ef2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/68c4ef20d46910cf95286172299c1fb5e617baf6),
[`3dd1d65`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3dd1d65519076a8d0f5b925a30cf4b3ba0c50df0),
[`fa802c2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/fa802c232d808b6a9543de893f86da37fac7ef06)
**Status:** Local browser OTA foundation retained

### Added

- Added temporarily armed, access-code protected local HTTP OTA.
- Added a generated 512-byte hardware/build/size/SHA package header.
- Added streamed inactive-partition writing without a firmware-sized allocation.
- Added exact hardware, ESP32-S3 image, embedded build marker, length, SHA, finalization,
  and boot-partition validation.
- Added Core-0 ADS-B maintenance acknowledgement before upload.
- Added post-build package generation and project-local release copy.

### Compile corrections

- Avoided Arduino's `DISABLED` macro collision.
- Used the mbedTLS SHA API supplied by Arduino-ESP32 3.0.7.
- Value-initialized OTA status objects safely.

## Product 53 R6 R1 - 2026-07-31

**Build:** `7IN-20260731-PRODUCT53R6R1-AIRPORT-TAP-FIX`
**Commit:** [`e915314`](https://github.com/bcarriveau/esp-aircraft-radar/commit/e915314ccf594ab3f5b56010464329b6d2def9e0)
**Status:** Airport table tap correction retained

- Processed filtered table `LV_EVENT_VALUE_CHANGED` while LVGL's selected cell is
  still valid rather than waiting for `RELEASED/CLICKED` after it is cleared.
- Preserved movement, duration, scroll-begin, and press-lost cancellation gates.
- Restored deliberate profile opening and label editing.
- Added missing radar-tap dismissal of temporary airport focus.

## Product 53 R6 - 2026-07-31

**Build:** `7IN-20260731-PRODUCT53R6-AIRPORT-TOUCH`
**Commit:** [`acb4475`](https://github.com/bcarriveau/esp-aircraft-radar/commit/acb44759d095f592976361b0ac243f9620aac989)
**Status:** Superseded by R6 R1

- Added explicit `EDIT / DONE` protection for `AUTO / SHOW / HIDE` changes.
- Added bounded no-scroll/no-excess-motion row actions.
- Added `SHOW ON RADAR`, automatic range choice, and temporary stable-identifier
  airport highlight.
- Hardware showed deliberate row taps did not complete because of LVGL event timing;
  corrected by R6 R1.

## Product 53 R5 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R5-AIRPORT-EYE`
**Commit:** [`38c9215`](https://github.com/bcarriveau/esp-aircraft-radar/commit/38c921573e4ecade30a0b35b4231970bf3f75e9c)
**Status:** Airport eye indicator retained

- Added an open-eye indicator beside `AUTO` or `SHOW` when the stable airport
  identifier was actually rendered on the latest completed radar frame.
- Added bounded PSRAM storage for the completed-frame visible identifier set.
- Kept preference and current visibility as separate concepts.

## Product 53 R4 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R4-AIRPORT-CONTROL`
**Commit:** [`2e86492`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2e864923ae767292a74a04c6303472ec95cfa307)
**Status:** Per-airport control retained

- Replaced bearing with `AUTO / SHOW / HIDE` label control.
- Added checked versioned NVS storage for up to 64 stable-identifier exceptions.
- Prioritized manual `SHOW`, suppressed `HIDE`, and retained `AUTO` defaults.
- Expanded the bounded directory from 32 to 64 rows.

## Product 53 R3 - 2026-07-29

**Build:** `7IN-20260729-PRODUCT53R3-AIRPORT-LABELS`
**Commit:** [`3e1849d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3e1849d2ba3ed37db3471929a7c555cff173db82)
**Status:** Fit-based airport label policy retained

- Removed small 9/8/6 airport-label quotas.
- Attempted every enabled airport label that fit deterministic collision rules.
- Added PSRAM placement storage and actual rendered-label diagnostics.
- Kept major/public enabled by default and private/heliports disabled.

## Product 53 R2 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT53R2-AIRPORT-TABLE`
**Commit:** [`e314db7`](https://github.com/bcarriveau/esp-aircraft-radar/commit/e314db791e8b9d6c4dd5d9dfefb151d514a88757)
**Status:** Directory table correction retained

- Rebalanced the directory into single-line ID, airport, type, distance, bearing,
  and runway columns.
- Applied text cropping to every cell and kept full values in Airport Profile.

## Product 53 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT53-PAGE-REDESIGN`
**Commit:** [`d3b1ea0`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d3b1ea0d3cff6fe8a070148a9791858175b927e0)
**Status:** Airports/System page foundation retained

- Rebuilt Airports as a directory-first operational page.
- Added Airport Profile and a separate Display Options view.
- Preserved scroll position during periodic refresh.
- Enlarged/reorganized System cards and maintenance controls.

## Product 52 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT52-UI-POLISH`
**Commit:** [`f7de409`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f7de4097362282315adfe906855029d222870abe)
**Status:** Physically reviewed; airport label/layout refinements retained

- Added deterministic airport-to-airport collision resolution.
- Kept all airport symbols independent of label placement.
- Subdued airport labels below aircraft.
- Rebalanced System cards, maintenance fit, and Save Airports sizing.

## Product 51 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT51-AIRPORT-UI`
**Commit:** [`c02bc66`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c02bc6662d6714eab9f9559f80e49e35f2033f58)
**Status:** Physically reviewed; stable airport map layer retained

- Made airport identifiers a fixed background layer drawn before symbols and aircraft.
- Removed moving-aircraft collision decisions from airport label placement.
- Reorganized System into status, device/network, and maintenance cards.

## Product 50 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT50-AIRPORT-OVERLAY`
**Commit:** [`90da002`](https://github.com/bcarriveau/esp-aircraft-radar/commit/90da002df5d816c851f68eb3597f321a6fc512f4)
**Status:** Offline airport architecture retained

### Added

- Added bounded offline airport cache, runway-oriented symbols, heliport symbol,
  labels, and per-category 20/40/80 settings.
- Replaced Setup navigation with Airports and moved settings under System.
- Added checked NVS persistence and a regional starter database/generator.
- Fixed an Arduino `radians` macro collision by renaming the helper.

## Product 49 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT49-20MI-LABEL-SELECTION`
**Commit:** [`2813ee1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/29cb94c1ab6d899483ebcd0269ea6f291fa3d85f)
**Status:** Exact label-touch behavior retained

- Made every visible 20-mile label a stable ICAO selection target.
- Gave exact label rectangles priority over aircraft icons.
- Added a four-pixel padded phase only after exact rectangles miss.
- Resolved overlapping padded areas by tracked, selected, rectangle distance,
  center distance, and stable ICAO.
- Kept 40/80-mile behavior unchanged.

## Product 48 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT48-TRACKS-SCROLL-CLAMP`
**Commit:** [`f299720`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f299720257042fe9d38cc20d4dfdcc3a7eb7b0b4)
**Status:** Tracks-page reliability fix retained

- Prevented a stale large-list scroll offset from leaving a shortened Tracks table
  apparently empty.
- Preserved valid offsets, clamped invalid offsets, and cancelled stale scroll
  animation before restoring the bounded position.

## Product 47 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT47-VERTICAL-STATE-BITMAPS`
**Commit:** [`2059f34`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2059f34f1e3825d5912e01a241a5f635a48c825f)
**Status:** Vertical-state display retained

- Added distinct climbing, level, and descending fuselage assets.
- Added a PSRAM LVGL canvas, plain-language state, and rounded FT/MIN.
- Added stable-ICAO hysteresis with +/-200 ft/min entry and +/-100 ft/min return.

## Product 46 R2 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-R2-SUBTLE-SWEEP-TINT`
**Commit:** [`c75141f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c75141f93468b8118c4c63ddd39c780584b712d5)
**Status:** Sweep styling retained

- Replaced an expanded yellow halo with same-footprint muted yellow sweep tint.
- Kept tags cyan and selected/tracked contacts amber/red.

## Product 46 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-RADAR-OVERLAP-STABILITY`
**Commit:** [`58aee5b`](https://github.com/bcarriveau/esp-aircraft-radar/commit/58aee5b0fda1e364aec68d277ea1d00ffa9a71c3)
**Status:** Contact overlap behavior retained

- Stopped deleting lower-priority 20-mile contacts during overlap.
- Layered contacts by tracked, selected, distance, stable ICAO, and final index.
- Added a subtle silhouette around the higher-priority bitmap.
- Removed overlap-driven tag suppression.

## Product 45 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`
**Commit:** [`c704ca1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c704ca1d15e4da26c48b768cfa04726b9be865c2)
**Status:** Classifier safety cleanup retained

- Removed the template that recovered a `Target` by subtracting `offsetof` from a
  `char[9]` member reference.
- Converted aircraft details, Tracks, Airspace, previews, side icons, and radar to
  explicit target-aware classifier APIs.
- Added standalone `char[9]` and ambiguous-family regression coverage.

## Product 44 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT44-NVS-WRITE-VERIFY`
**Commit:** [`ec3ac4f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/ec3ac4fcee8e2764c315be959e7a78d2ac9488d9)
**Status:** NVS reliability retained

- Checked namespace open and every required string/float write.
- Correctly handled successful empty-string writes.
- Made initialization, save, and reset report failure accurately.
- Continued with compile defaults when NVS is unavailable while disabling save/reset.
- Added `NVS: READY/ERROR` diagnostics without printing private values.

## Product 43 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT43-WIFI-TIMESTAMP-SYNC`
**Commit:** [`cbe6d0f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/cbe6d0fae79ebd15d5d68d09b8aa78cf940e0a42)
**Status:** Cross-core synchronization fix retained

- Removed `volatile` from the cross-core Wi-Fi attempt timestamp.
- Protected all access with the existing command critical section.
- Made due evaluation and reservation atomic.

## Product 42 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT42-STALE-TRANSPORT-SUCCESS`
**Commit:** [`251fbc4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/251fbc42fb2350a616dbdbb40c7a72a6f97e5f97)
**Status:** Stale-response bookkeeping retained

- Treated a fully completed stale response as transport success.
- Reset failure streak/outage recovery without publishing obsolete data.
- Kept discarded-response diagnostics separate and did not advance tracking misses.

## Product 41 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT41-FAST-BODY-RECOVERY`
**Commit:** [`8438633`](https://github.com/bcarriveau/esp-aircraft-radar/commit/843863321b5fb2458d8fe60350a585e597129245)
**Status:** Response-body recovery retained

- Treated the first bounded native EAGAIN/EWOULDBLOCK/timeout body read as terminal
  instead of repeating known-dead three-second reads.
- Hard-recycled the station radio after a partial native body and retried promptly.
- Preserved successful 15-second cadence and request serialization.

## Product 40 R3 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-DB-HTTPS-RECOVERY-R3`
**Commit:** [`0b099db`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0b099db0f80efbaae1625dbadde68a6ebc86e01a)
**Status:** Superseded by Product 41

- Stopped native retries and fallback after a partial ADS-B response body.
- Returned immediately to Wi-Fi recovery while preserving eligible fallback for
  connection/header failures.

## Product 40 R2 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-DB-HTTPS-RECOVERY-R2`
**Commit:** [`c2a853f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c2a853f7cb7dd6190e7bb368f97bc57a22fc366f)
**Status:** Superseded by Product 40 R3

- Combined the generated aircraft database with corrected native/fallback sequencing.
- Completed native attempts before one independent verified fallback.
- Kept fallback disabled for status, oversize, allocation, DNS/Wi-Fi, JSON, and
  partial-body failures.

## Product 40 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-TYPE-DATABASE`
**Commit:** [`244f337`](https://github.com/bcarriveau/esp-aircraft-radar/commit/244f337ed6f88d2bccd5fd8a7500376ba22493b6)
**Status:** Generated aircraft database retained

- Added 2,697 unique sorted aircraft designators and 11,716 collision-checked
  description aliases from a pinned source snapshot.
- Added deterministic generation, metadata, checksums, duplicate/collision tests,
  and sensitive-model regression coverage.
- Used exact designator binary search first, conservative fallback second, and
  retained `UNKNOWN` for ambiguity.
- Used no runtime heap, PSRAM, or network lookup.

## Product 39 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT39-STARTUP-FAILURE-PROPAGATION`
**Commit:** [`35b9eca`](https://github.com/bcarriveau/esp-aircraft-radar/commit/35b9ecae8d5c5949950c5512fc3359d575bd1210)
**Status:** Startup reliability retained

- Made UI and ADS-B startup return explicit success/failure.
- Allocated the core-0 incoming buffer before task creation.
- Checked `xTaskCreatePinnedToCore()` and cleaned up on failure.
- Started networking and set startup complete only after required components were
  ready.
- Added a stable `STARTUP HALTED` screen for post-display fatal failures.

## Product 38 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT38-LOCATION-INVALIDATION`
**Commit:** [`34a3c2d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/34a3c2d6dc7193b6b3a38490cfefb9ddbfd72c1c)
**Status:** Location invalidation retained

- Immediately invalidated old visible aircraft when saved radar coordinates changed.
- Advanced generation/version and showed location-updating state until current data.
- Retained tracked ICAO internally without counting configuration invalidation as a
  miss.

## Product 37 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT37-FALLBACK-HTTPS-HARDENED`
**Commit:** [`63892b1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/63892b11529f609370176159353c4dd67dddd23a)
**Status:** Hardened fallback retained

### Changed

- Removed insecure fallback behavior and attached the Espressif CA bundle with
  hostname verification.
- Removed whole-response Arduino `String` buffering and duplicate copies.
- Added PSRAM-first bounded streaming reader with 250,000-byte decoded limit.
- Added bounded request writes, header/line/chunk limits, header/idle/total deadlines,
  and valid length/chunked/required-close body support.
- Rejected conflicting lengths, duplicate/unsupported encodings, mixed framing,
  malformed CRLF, forbidden trailers, and oversized bodies.
- Limited fallback to eligible TCP/TLS/header transport failures.

## Product 36 R4 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-OVERLAP-PRIORITY-R4`
**Commit:** [`9de21e3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9de21e3d7ac242cdbc57c240b1328b05bc6f53c3)
**Status:** Superseded; priority concepts refined by Product 46

- Added deterministic overlap priority for tracked, selected, and normal contacts.
- Refined contact order, tag clearance, and touch behavior in dense 20-mile traffic.

## Product 36 R3 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-HEADING-SPRITES-R3`
**Commit:** [`022e7b3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/022e7b3e99311b7ab53effe7399d45b3e1459055)
**Status:** Heading-sprite architecture retained

- Added 25x25 contact sprites and sixteen precomputed 22.5-degree heading variants
  per category.
- Selected variants from ADS-B track with north fallback.
- Avoided runtime rotation, temporary allocation, and per-contact LVGL objects.

## Product 36 R2 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS-R2`
**Commit:** [`9494747`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9494747a6b1f8d1233ce03d8c40db140322dd529)
**Status:** Superseded by Product 36 R3

- Refined recognizable category silhouettes while keeping the change asset/render-only.

## Product 36 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS`
**Commit:** [`3ea59bb`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3ea59bbefd674b3728beb98af0a692e56ebb5819)
**Status:** First bitmap-contact candidate

- Replaced plain 20-mile dots with category-specific bitmap contacts.
- Kept 40/80-mile contacts compact and preserved selection/tracking colors.

## Product 35 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT35-DESCRIPTION-TYPE-FALLBACK`
**Commit:** [`0441bd7`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0441bd70f85abb2da72314540b6c0abb15f9683c)
**Status:** Description fallback retained under later database/classifier APIs

- Added conservative description classification only when exact type classification
  remained unknown.
- Added target-aware helpers and ambiguity regression tests.
- Used no heap, `String`, regex, or network lookup.

## Product 34 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY`
**Commit:** [`239a7a5`](https://github.com/bcarriveau/esp-aircraft-radar/commit/239a7a5af8442562e8024f5dec98b6f58c9eddc2)
**Status:** Tracking-loss behavior retained

- Added a three-successful-current-generation-update grace period.
- Kept tracking through one/two confirmed misses and showed `TRACK SIGNAL LOST`.
- Cleared tracking atomically after the third confirmed miss.
- Did not count failures, stale discards, location/range invalidation, or manual stop.
- Moved PlatformIO work outside Google Drive and skipped downloaded packages in
  cppcheck.

## Product 33 R5 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`
**Commit:** [`14fe9f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/14fe9f46b8c131ed1a22f5adb93e898f3188e3b1)
**Status:** Superseded by Product 34

- Corrected Setup spacing.
- Made the old nearest-other heading reflect populated row count.
- Hid unused rows/icons/ICAO slots and completed Airspace fit cleanup.
- Preserved stable ICAO row actions.

## Product 33 R2 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R2`
**Commit:** [`52367df`](https://github.com/bcarriveau/esp-aircraft-radar/commit/52367dfa5f85491f2813a3d7103c0aa4a5cf6953)
**Status:** Superseded by R5

- Improved Setup styling, nearest-other heading, Airspace live-highlights fit, and
  credit card.
- Removed Airspace highlight scrolling and kept all sections visible.

## Product 32 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT32-UI-DASHBOARD`
**Commit:** [`a4cc594`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a4cc594f52c8d8e562cd751dbb528a744d72d00e)
**Status:** Airspace dashboard retained

- Added Airspace totals, range, category counts/percentages, and live highlights.
- Added up to three secondary aircraft rows with stable ICAO identity.
- Removed duplicate Setup range controls and kept the compact radar selector.
- Expanded fixed side-icon storage in PSRAM.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`
**Commit:** [`1747cb1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1747cb169969dff12f3a5d794f561b56dc2acc83)
**Status:** Side bitmap/heading behavior retained

- Added aircraft-type side icons to idle, selected/tracked, and nearest-five rows.
- Added a rotating idle-nearest heading arrow and heading value.
- Used independent persistent point storage and stable ICAO actions.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`
**Commit:** [`50821ba`](https://github.com/bcarriveau/esp-aircraft-radar/commit/50821badc68efb2e0c8dab597d1db5f1267628ab)
**Status:** Current target-capacity/memory baseline

- Increased bounded storage to 200 retained targets.
- Moved all capacity-scaled target, snapshot, hit, contact, and label-collision
  buffers to required PSRAM.
- Preserved returned tracked aircraft, then retained nearest first.
- Added received/eligible/stored/dropped/visible diagnostics and clean startup failure
  on required PSRAM allocation failure.
- Corrected an earlier attempt that consumed contiguous internal RAM needed by TLS.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`
**Commit:** [`237d06a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/237d06a2b79b0ca76960ad594c435ee0947716a2)
**Status:** UI-state behavior retained

- Matched label-box allocation/bounds to capacity.
- Used stable rendered ICAO for nearest cards/lists.
- Added explicit Radar/Tracks detail origins and correct return-tab behavior.
- Paused/refreshed selected timeout around details and closed overlays on tab changes.

## Product 28 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT28-RADAR-STATE-FLOW`
**Commit:** [`61f104c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/61f104ca4a8adb6bf7d9ffb6caf3509ddb05797c)
**Status:** Superseded by Product 29

- Restored idle nearest information to the left panel.
- Gave selected/tracked details right-panel priority.
- Moved STOP TRACK into the tracked card and added explicit detail returns.
- Preserved selection after stop when practical.

## Product 27 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT27-RADAR-LAYOUT`
**Commit:** [`2dda7ae`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2dda7ae4606dff08fcd34d303610f9d920185362)
**Status:** Superseded

- Reworked idle, selected, and tracked panel layout.
- Added INFO, TRACK, CLEAR, and right-panel STOP TRACK.
- Kept the compact range selector and made nearest-list taps select aircraft.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`
**Commit:** [`298c87a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/298c87ab43d83ae51e0276fb152789e05ad7423e)
**Status:** Themed tag/direct interaction foundation retained

- Added dark-navy 20-mile tags with cyan identifiers and teal borders.
- Added stable ICAO hit regions and tracked/selected/closest priority.
- Added temporary amber selection, INFO/TRACK, and compact 20/40/80 selector.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`
**Commit:** [`1c1d555`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee)
**Status:** First-soak cleanup retained

- Eliminated expected first-run missing-key Preferences errors.
- Initialized missing title, Wi-Fi, latitude, and longitude keys without replacing
  saved values.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`
**Commit:** [`dec570e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/dec570eab7324ae6cc00747a037af2090cdf94bd)
**Status:** Recovery architecture retained and later hardened

- Added bounded retries for stalled/incomplete bodies.
- Closed native clients before retry/fallback and preserved most-advanced failure.
- Reconnected after incomplete downloads, escalated to radio recycle, retried promptly,
  moved last-resort restart to the main loop, and retained last-good aircraft.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`
**Commit:** [`faa9bc2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/faa9bc28b8524ff9fb850636e1bf356f508d71da)
**Status:** Physical crash fix confirmed

- Replaced unsupported floating-point LVGL formatting with integer headings/range.
- Fixed the core-1 `LoadProhibited` crash after aircraft population.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`
**Commit:** [`3203bd3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3203bd3a4263b7c1f65a839466a083d7b9cd8c90)
**Status:** Large-response handling retained

- Retried temporary native body EAGAIN/EWOULDBLOCK/timeouts within independent idle
  and total deadlines.
- Physical testing completed a 105,690-byte response with 189 parsed aircraft.

## Products 19-21 - 2026-07-21

**Final build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`
**Commit:** [`5d5b0b6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9)
**Status:** Combined record; individual Product boundaries not preserved

- Removed 160 miles and limited ranges to 20/40/80.
- Added predictive outward auto-zoom, popup keyboard, non-wrapping identifiers,
  tracked left-panel information, rotating heading arrow/value, and detail selection
  for the represented aircraft.
- Kept tracked presentation through temporary omissions.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`
**Commit:** [`69dce61`](https://github.com/bcarriveau/esp-aircraft-radar/commit/69dce612211326a4a41f0f66becc8eb7d46191f9)
**Status:** First physically working native TLS baseline

- Attached Espressif's CA bundle to native HTTPS with hostname verification.
- Corrected Product 17's missing verification configuration.
- Added secure fallback and reduced unnecessary Wi-Fi reconnect churn.

## Product 17 - 2026-07-21

**Standalone commit:** Not preserved
**Status:** Confirmed precursor documented by Product 18 history

- Replaced Arduino secure-client/manual HTTP with native ESP-IDF streaming HTTPS.
- Re-resolved DNS and created a fresh client per retry.
- Preserved deadlines, size guards, PSRAM body, generations, and single-snapshot
  publication.
- Initial physical test failed because server verification was not configured;
  Product 18 corrected it.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`
**Commit:** [`c81b34e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c81b34e5d640e734723964f895c9bb4ec49c1af8)
**Status:** Superseded by native HTTPS Products 17-18

- Used resolved IP for TCP while retaining hostname SNI.
- Increased TLS allowance, logged exact mbedTLS errors, and avoided recycling a
  healthy station for TLS/HTTP/body/JSON failures.
- Physical TLS timeouts led to Product 17.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`
**Commit:** [`b2a0a49`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b2a0a492c424cf192edf71eb7f5dd496ec0bbab8)
**Tag:** `product-15-hardened`
**Status:** First hardened version-controlled and permanent rollback baseline

### Added

- Added modular PlatformIO/C++/LVGL firmware for the exact Waveshare 7-inch device.
- Added core-0 ADS-B networking and Wi-Fi recovery ownership.
- Added true 15-second start-to-start polling with non-overlapping requests.
- Added bounded connect, header, body-idle, and total deadlines.
- Added classified Wi-Fi, DNS, TCP, TLS, HTTP, body, JSON, and stale failures.
- Added request generations, obsolete-response rejection, last-good retention,
  thread-safe publication, and coherent radar snapshots.
- Added collision-aware 20-mile labels, stable ICAO tracking, Setup validation,
  protected reset behavior, diagnostics, aircraft categories, and unknown artwork.
- Added private configuration example while excluding `include/config.h`.

### Display and memory baseline

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM framework.
- OPI PSRAM with `BOARD_HAS_PSRAM`.
- Waveshare RGB timing, DMA, anti-rolling protections, and 20-scanline bounce buffer.

### Verification

- Compile/link passed.
- Product 15 remains the permanent hardened rollback baseline.

---

## History boundary

The authoritative numbered GitHub Product history begins at Product 15. Product
15 preserved the proven RGB anti-rolling configuration from Product 14, but this
repository does not contain authoritative Product 1-14 history. Those earlier
releases are intentionally omitted rather than reconstructed from memory, previous
chats, or uncertain files.
