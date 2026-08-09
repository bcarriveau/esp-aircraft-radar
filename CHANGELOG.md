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

- **Current replacement source:** Product 83
- **Current build marker:** `7IN-20260808-PRODUCT83-SETTINGS-KEYBOARD-VISIBILITY`
- **Source baseline branch:** `main`
- **Current committed baseline:** Product 82 `d3e769b34eb112d7153c43f75a1dd5d3e69eb276`
- **Exact hardware:** Waveshare ESP32-S3-Touch-LCD-7, 800x480 ST7262 RGB LCD,
  GT911 touch, OPI PSRAM
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11
- **Hardened rollback baseline:** Product 15
- **Recommended rollback tag:** `product-15-hardened`

Product 83 is a focused replacement-source candidate based directly on committed
Product 82 `main` commit `d3e769b34eb112d7153c43f75a1dd5d3e69eb276`.
It changes only the System-page Settings keyboard visibility behavior plus Product
identity/documentation. PlatformIO and physical verification are not claimed.

### History completeness notes

- Numbered Product history is represented continuously from Product 15 through
  Product 83 using current Git history first and preserved historical documentation
  only where standalone Git boundaries are absent.
- Product 17 is recorded as the documented native-HTTPS precursor inside Product 18;
  no standalone Product 17 commit or build marker is invented.
- Products 19-21 remain a combined record because reliable standalone Product 19/20
  commit/build boundaries are not preserved.
- Product 33 records the standalone R2 and R5 revisions without inventing missing
  R3/R4 commits.
- Product 36, Product 40, Product 46, Product 53, and Product 56 retain their
  confirmed revision chains where those revisions materially changed the product.
- Product 83 remains a local replacement-source candidate; Product 82 remains the
  latest committed GitHub baseline until Product 83 is actually committed.

## Product 83 - 2026-08-08

**Build:** `7IN-20260808-PRODUCT83-SETTINGS-KEYBOARD-VISIBILITY`  
**Source baseline:** Product 82 `main` commit
`d3e769b34eb112d7153c43f75a1dd5d3e69eb276`  
**Status:** Focused replacement-source candidate; focused host validation complete;
PlatformIO and physical verification pending

### Changed

- The System-page **DEVICE & NETWORK** card temporarily enables bounded vertical
  scrolling while the on-screen keyboard is active.
- Focusing Display Name, Wi-Fi SSID, Password, Latitude, or Longitude scrolls only
  that card enough to keep the active field visible above the 250-pixel keyboard
  and its edge/shadow clearance.
- The System page title, header, navigation, System Status card, and maintenance
  controls retain their normal fixed positions.
- Keyboard DONE/CANCEL, opening the OTA or MQTT overlay, leaving the System page, or
  otherwise hiding the Settings form restores the Device & Network card to its
  normal top position and removes the temporary scrolling state.
- The temporary card scrollbar remains hidden so keyboard editing does not change
  the normal System-page visual treatment.
- Changed curser style to be more visable when in use and hidden when no edit is taking place.

### Preserved

- Existing password masking and the **SHOW/HIDE** password control are unchanged.
- Latitude/Longitude continue using the numeric keyboard; text fields keep their
  existing text keyboard behavior.
- Settings validation, NVS save/reset behavior, Wi-Fi reconnect behavior, and
  location-change refresh behavior are unchanged.
- ADS-B networking, native/fallback HTTPS, 15-second cadence, Wi-Fi/TLS recovery,
  stale-response rejection, last-good retention, MQTT, OTA, radar rendering,
  stable ICAO selection/tracking, target capacity, panel timing, DMA, OPI PSRAM,
  and the 20-scanline RGB bounce buffer are unchanged.

### Validation

- Exact Product 82 `src/ui.cpp` and `include/build_info.h` baselines were verified
  against their Git blob SHAs before editing.
- Complete changed-file comparison confirmed `src/ui.cpp` changes are limited to
  Settings keyboard focus/scroll/restore behavior and `include/build_info.h`
  changes are limited to Product 83 identity/release notes.
- A focused C++17 scroll/restore behavior model passed with
  `-Wall -Wextra -Werror -pedantic`, AddressSanitizer, and
  UndefinedBehaviorSanitizer.
- LVGL 8.3.11 compatibility was checked for the scroll APIs used.
- Modified files passed delimiter, trailing-whitespace, build-marker, package,
  checksum, and forbidden-API checks.
- PlatformIO compile/link, generated memory totals, upload, physical display/touch
  testing, and soak testing were not run here.

### Pending verification

- Confirm boot serial output reports
  `7IN-20260808-PRODUCT83-SETTINGS-KEYBOARD-VISIBILITY`.
- Tap each editable System field and confirm it remains fully visible above the
  keyboard.
- Confirm password SHOW/HIDE and numeric Latitude/Longitude keyboard behavior.
- Press DONE/CANCEL, leave/re-enter System, and open OTA/MQTT after editing; confirm
  the Device & Network card always returns to its normal position.
- Confirm page switching, touch response, radar rendering, networking, and display
  stability remain unchanged.

## Product 82 - 2026-08-07

**Build:** `7IN-20260807-PRODUCT82-AIRSPACE-HANDOFF`  
**Commit:** [`d3e769b`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d3e769b34eb112d7153c43f75a1dd5d3e69eb276)  
**Status:** Committed focused Airspace-selection handoff correction; PlatformIO and
physical verification are not recorded here

### Fixed

- Resolves a tapped Airspace live-highlight aircraft by stable ICAO before changing
  the current manual-tracking state.
- If another aircraft is being manually tracked, stops that tracking only after the
  tapped highlight has been confirmed as a current aircraft target.
- Returns to Radar with the tapped aircraft selected in the normal amber state.
- Keeps **TRACK** as a deliberate second action instead of immediately transferring
  tracking to the tapped aircraft.

### Preserved

- Stable ICAO selection/tracking semantics and the established Radar action flow.
- Radar range, rendering, networking, display timing, PSRAM, target capacity, OTA,
  MQTT, and hardened transport/recovery behavior.

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

## Product 64 - 2026-08-02

**Commit:** [`21e2e06`](https://github.com/bcarriveau/esp-aircraft-radar/commit/21e2e0649f01bb848c728cf37a7f8b6c6b1ed16a)  
**Status:** Measured memory/diagnostic refinement retained by later Products

### Changed

- Reduced the measured core-0 ADS-B task stack from 16 KiB to 12 KiB while retaining
  the established stack-headroom diagnostics.
- Improved fetch-stage attribution so time inside native HTTPS open/TLS setup is
  reported as TLS-handshake work rather than being hidden by unrelated service text.
- Moved the existing Firmware / OTA control into the System-page header.
- Preserved the 128 KiB LVGL pool, Product 63 PSRAM parse policy, HTTPS behavior,
  target bounds, display timing, DMA, OPI PSRAM, and bounce buffer.

## Product 63 - 2026-08-02

**Commit:** [`cf8504f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/cf8504fb68b069fce096187b3f3012bdacee4866)  
**Status:** PSRAM parse-memory policy retained

### Changed

- Kept both ADS-B ArduinoJson documents and the response payload in PSRAM-only
  storage.
- Replaced per-fetch Arduino `String` request construction with bounded character
  buffers.
- Moved the optional Airports directory array from internal DRAM to PSRAM.
- Added fetch-only heap/largest-block lows, stage attribution, LVGL-pool metrics,
  and ADS-B task-stack headroom diagnostics.
- Preserved transport policy, radar snapshots, 200-target capacity, and Product 62
  airport-directory eye behavior.

## Product 62 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT62-AIRPORT-EYE-COVERAGE`  
**Commit:** [`2bae5aa`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2bae5aadeb4207c4c846cdc1f70e52b333b88214)  
**Status:** Bounded airport-directory coverage correction

### Fixed

- Kept the Airports directory bounded at 64 rows while scanning the complete bounded
  nearby-airport set through an optional PSRAM scratch buffer.
- Retained every airport whose label was actually rendered on Radar so directory eye
  indicators correspond to visible labels by stable airport identifier.
- Replaced only farther non-visible rows when needed, restored distance ordering,
  and preserved the nearest-64 fallback if optional scratch storage is unavailable.

## Product 61 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT61-OTA-SOCKET-PACING`  
**Commit:** [`5ab7db0`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5ab7db06de45883eda8e96bd5c8633d2fb63b862)  
**Status:** Physically verified local-browser OTA handoff

### Fixed

- Paced the single-client browser handoff between `/prepare`, `/status`, and
  multipart `/upload`.
- Added bounded retries for transient control connections and allowed one upload
  retry only when authenticated status still reported READY with zero transfer bytes.
- Exposed received/written/firmware byte counters and removed the duplicate
  application-added `Connection: close` header.

### Physical verification

- A captured Chrome run completed one prepare, one status request, and one upload
  without connection reset or duplicate transfer.
- The 2,409,888-byte firmware image verified, the radar restarted with software
  reset reason 3, and native ADS-B HTTPS plus MQTT resumed with stable memory.

## Product 60 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT`  
**Commit:** [`9d23ef3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9d23ef3de361ef4fc900e715a32b484a7a3cc4e9)  
**Status:** OTA preparation correction retained

### Fixed

- Made repeated authenticated `/prepare` requests idempotent while the radar is
  already PREPARING or READY.
- Added bounded browser recovery when the first preparation response is lost without
  resetting the upload session or extending the existing preparation deadline.
- Preserved the established multipart upload writer, package validation, inactive-slot
  write path, and restart implementation.

## Product 59 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT59-NETWORK-RECOVERY-MEMORY`  
**Commit:** [`a040fe4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a040fe46363013f6f18b8c33d74ba8fe6fa19a08)  
**Status:** Hard-recovery memory/ownership refinement retained

### Changed

- Released unused ESP32-S3 BLE-controller memory during startup.
- Required MQTT to close its socket, destroy clients, and free bounded work buffers
  before a hard station-radio recycle.
- Deferred hard recovery when MQTT did not acknowledge the bounded hold instead of
  tearing the radio down underneath it.
- Made hard Wi-Fi recovery and the Product 58 OTA window mutually exclusive.

## Product 58 - 2026-08-02

**Build:** `7IN-20260802-PRODUCT58-OTA-EXCLUSIVE-HOLD`  
**Commit:** [`a6e96d4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a6e96d4bb5e41ec342b3807039498d5c76bef1c8)  
**Status:** Network-exclusive OTA/restart behavior physically exercised

### Changed

- Claimed the network for the complete five-minute local OTA window from enable
  through preparation, upload, verification, and restart.
- Parked ADS-B after any request already in flight and released MQTT network/work
  resources while OTA owned the network.
- Suppressed competing refresh, reconnect, recovery, and fallback-restart actions
  during OTA ownership.
- Added the IRAM-safe Core-1 park and bounded Core-0 restart handoff needed for a
  clean ESP32-S3 software restart.

### Physical verification

- Preserved records document consecutive alternating-partition OTA cycles ending in
  clean software restarts with stable heap/PSRAM.

## Product 57 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT57-AIRPORT-DATABASE-SETUP`  
**Commit:** [`c30c330`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c30c330ff3182798d690cbd919c322819a2d17f9)  
**Status:** Regional airport database tooling retained

### Added

- Added a guided Windows workflow and command-line tooling for rebuilding the
  compiled regional airport database.
- Combined OurAirports airport records with longest-open-runway data, previewed
  category counts/estimated flash use, and validated replacement headers atomically.
- Restored the previous generated header on validation failure and documented the
  distinction between nearby coordinate changes and moving to a new region.
- Kept generated database source tracked while ignoring downloaded CSVs, caches,
  interrupted temporary files, and delivery-only notes.

## Product 56 - 2026-08-01

**Initial commit:** [`8b4fca1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/8b4fca13c4893b863b7413eec447fcef3d0b8612)  
**Final Product 56 commit:** [`49035a6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/49035a62e4684ef1696fb325fbc12b0a8a53cce7)  
**Status:** Home Assistant MQTT integration and System UI retained

### Added

- Added optional Home Assistant MQTT discovery, retained availability, controls,
  telemetry, and a dashboard using built-in Home Assistant cards.
- Added display-power, shared-range, and refresh controls while keeping MQTT disabled
  by default and isolated from Wi-Fi ownership/recovery.
- Coordinated MQTT with OTA maintenance ownership and bounded its snapshot/JSON work.

### Memory revisions

- **R1** [`ba90a31`](https://github.com/bcarriveau/esp-aircraft-radar/commit/ba90a317085ea938bcf7847b0999623b78dfe03c): added ADS-B/MQTT heap, largest-block,
  PSRAM, and stage diagnostics.
- **R2** [`66d939a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/66d939a238dd7fdffe37b463fc8acdbcb3e99f2f): reduced native MQTT stack/buffers;
  hardware logs still showed fragmentation severe enough to break later TLS.
- **R3** [`01298c9`](https://github.com/bcarriveau/esp-aircraft-radar/commit/01298c97d0068e1c84dde6ddb7e28e9f3dd30b43): replaced native ESP-MQTT with
  lightweight PubSubClient 2.8, streamed larger payloads from bounded PSRAM, and
  serialized MQTT work around ADS-B requests.
- **R4** [`3bc63ec`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3bc63eca837e0df69d94894dc2d7efbcc9be97ee): removed the redundant update-age entity
  and refined the System page.
- Final Product 56 System UI/MQTT cleanup was recorded in commit `49035a6`, including
  successful hardware MQTT operation and repeated preferred native ADS-B TLS cycles
  without the earlier fragmentation collapse.

## Product 55 - 2026-08-01

**Build:** `7IN-20260801-PRODUCT55-AIRSPACE`  
**Commit:** [`0bc4142`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0bc41425a89570a1698dbf5d601ae4218b2f15b6)  
**Status:** Airspace interaction retained

### Changed

- Added the shared Airspace 20/40/80-mile range toggle.
- Added stable-ICAO Airspace live-highlight shortcuts that hand aircraft to Radar.
- Replaced the redundant dominant-category highlight with highest airborne.
- Preserved radar range ownership, selection/tracking identity, and ADS-B transport.

## Product 54 - 2026-08-01

**Primary commit:** [`68c4ef2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/68c4ef20d46910cf95286172299c1fb5e617baf6)  
**Compatibility fix:** [`3dd1d65`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3dd1d65519076a8d0f5b925a30cf4b3ba0c50df0)  
**Release-copy update:** [`fa802c2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/fa802c232d808b6a9543de893f86da37fac7ef06)  
**Status:** Guarded local browser OTA foundation retained

### Added

- Generated hardware-bound `.radarota` packages and streamed validated ESP32-S3
  firmware into the inactive OTA application slot.
- Added package length, hardware, image/chip, embedded build ID, SHA-256,
  `esp_ota_end()`, and boot-partition validation gates.
- Added an acknowledged core-0 ADS-B maintenance hold and a temporarily armed
  System-page firmware update overlay.
- Corrected Arduino-ESP32 3.0.7 enum/SHA API compatibility and copied only the
  upload-ready package to `release/firmware.radarota` after successful builds.

## Product 53 - 2026-07-27 to 2026-08-01

**Initial commit:** [`d3b1ea0`](https://github.com/bcarriveau/esp-aircraft-radar/commit/d3b1ea0d3cff6fe8a070148a9791858175b927e0)  
**Status:** Airports directory/profile and per-airport controls retained

### Evolution

- Product 53 introduced an operational nearby-airports directory, Airport Profile,
  display-settings subpage, preserved directory scroll position, and a compact
  System maintenance strip.
- **R2** [`e314db7`](https://github.com/bcarriveau/esp-aircraft-radar/commit/e314db791e8b9d6c4dd5d9dfefb151d514a88757) tightened single-line cropped table
  cells and column sizing.
- **R3** [`3e1849d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3e1849d2ba3ed37db3471929a7c555cff173db82) removed old label quotas and reported
  actual rendered label counts.
- **R4** [`2e86492`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2e864923ae767292a74a04c6303472ec95cfa307) added bounded stable-ID
  `AUTO / SHOW / HIDE` controls and expanded the directory to 64 rows.
- **R5** [`38c9215`](https://github.com/bcarriveau/esp-aircraft-radar/commit/38c921573e4ecade30a0b35b4231970bf3f75e9c) added eye indicators for airport labels
  actually rendered on Radar.
- **R6** [`acb4475`](https://github.com/bcarriveau/esp-aircraft-radar/commit/acb44759d095f592976361b0ac243f9620aac989) locked edits behind EDIT/DONE, hardened
  scrolling/touch handling, and added SHOW ON RADAR with automatic range choice.
- **R6R1** [`e915314`](https://github.com/bcarriveau/esp-aircraft-radar/commit/e915314ccf594ab3f5b56010464329b6d2def9e0) corrected LVGL table tap timing while
  preserving movement/scroll/press-loss safety gates.

## Product 52 - 2026-07-27

**Commit:** [`f7de409`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f7de4097362282315adfe906855029d222870abe)  
**Status:** Airport-label/System layout polish retained

- Resolved airport-label collisions deterministically within the static map layer.
- Kept airport symbols and all aircraft rendering above subdued airport labels.
- Rebalanced the System status, Device & Network, and maintenance areas for 800x480.

## Product 51 - 2026-07-27

**Commit:** [`c02bc66`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c02bc6662d6714eab9f9559f80e49e35f2033f58)  
**Status:** Static airport map-layer behavior retained

- Drew airport identifiers as a fixed background map layer beneath aircraft contacts
  and tags.
- Softened normal airport-label styling and reorganized the System page into status,
  Device & Network, and maintenance cards.

## Product 50 - 2026-07-27

**Commit:** [`90da002`](https://github.com/bcarriveau/esp-aircraft-radar/commit/90da002df5d816c851f68eb3597f321a6fc512f4)  
**Status:** Offline airport-awareness foundation retained

### Added

- Added a bounded offline airport cache and runway-oriented radar symbols.
- Added collision-aware airport labels below aircraft label priorities.
- Added per-category 20/40/80-mile symbol/label settings with checked NVS storage.
- Replaced the Setup navigation tab with Airports and moved device/network setup into
  System.

## Product 49 - 2026-07-27

**Build:** `7IN-20260727-PRODUCT49-20MI-LABEL-SELECTION`  
**Commit:** [`29cb94c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/29cb94c1ab6d899483ebcd0269ea6f291fa3d85f)  
**Status:** 20-mile label-selection behavior retained

### Changed

- Made each visible 20-mile aircraft label a first-class stable-ICAO touch target.
- Gave exact label rectangles priority, then a four-pixel padded edge resolution,
  then existing aircraft-icon hit testing.
- Resolved padded-label overlaps deterministically while preserving tracked,
  selected, and normal hit-test priority.

## Product 48 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT48-TRACKS-SCROLL-CLAMP`  
**Commit:** [`f299720`](https://github.com/bcarriveau/esp-aircraft-radar/commit/f299720257042fe9d38cc20d4dfdcc3a7eb7b0b4)  
**Status:** Tracks scroll reliability fix retained

### Fixed

- Prevented Tracks from appearing empty after a long list was replaced by a much
  shorter list while LVGL retained an invalid old scroll offset.
- Preserved a valid scroll position and clamped only offsets beyond the new table
  range.

## Product 47 - 2026-07-26

**Commit:** [`2059f34`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2059f34f1e3825d5912e01a241a5f635a48c825f)  
**Status:** Aviation vertical-state indicator retained

### Added

- Added the small fuselage climbing/level/descending indicator for the selected or
  tracked aircraft.
- Displayed rounded vertical rate in FT/MIN with deterministic hysteresis keyed to
  stable ICAO identity.
- Kept the indicator bounded to the priority aircraft and PSRAM-backed artwork.

## Product 46 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT46-RADAR-OVERLAP-STABILITY`  
**Commit:** [`58aee5b`](https://github.com/bcarriveau/esp-aircraft-radar/commit/58aee5b0fda1e364aec68d277ea1d00ffa9a71c3)  
**R2 build:** `7IN-20260726-PRODUCT46-R2-SUBTLE-SWEEP-TINT`  
**R2 commit:** [`c75141f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c75141f93468b8118c4c63ddd39c780584b712d5)  
**Status:** Overlap stability and subtle sweep treatment retained

### Fixed

- Stopped overlap handling from marking lower-priority 20-mile aircraft invisible.
- Layered contacts deterministically by tracked, selected, distance, stable ICAO,
  and final target-index tie-breaker while retaining collision-aware labels.
- R2 replaced the enlarged sweep halo with a same-size muted yellow tint so normal
  cyan aircraft do not flash or grow as the sweep passes.

## Product 45 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT45-EXPLICIT-CLASSIFIER-API`  
**Commit:** [`c704ca1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c704ca1d15e4da26c48b768cfa04726b9be865c2)  
**Status:** Explicit target-aware classifier APIs retained

### Changed

- Replaced implicit member-recovery classifier calls with explicit Target-aware
  category, bitmap, and kind APIs.
- Removed the unsafe pattern that could reinterpret an unrelated `char[9]` as a
  containing Target.
- Preserved classifier output, generated aircraft database, and radar/UI behavior.

## Product 44 - 2026-07-26

**Commit:** [`ec3ac4f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/ec3ac4fcee8e2764c315be959e7a78d2ac9488d9)  
**Status:** NVS write-verification behavior retained

### Fixed

- Checked settings storage initialization and expected NVS write lengths.
- Made save/reset success conditional on actual writes and exposed NVS READY/ERROR
  state on System.
- Continued with compile-time defaults if storage is unavailable while disabling or
  clearly marking saving rather than reporting false success.

## Product 43 - 2026-07-26

**Commit:** [`cbe6d0f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/cbe6d0fae79ebd15d5d68d09b8aa78cf940e0a42)  
**Status:** Cross-core Wi-Fi timestamp synchronization retained

- Removed the formal cross-core data race around the Wi-Fi attempt timestamp by
  synchronizing accesses through the existing command lock.
- Preserved networking behavior and recovery thresholds.

## Product 42 - 2026-07-26

**Commit:** [`251fbc4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/251fbc42fb2350a616dbdbb40c7a72a6f97e5f97)  
**Status:** Stale-result transport-success handling retained

### Fixed

- Treated a fully successful response discarded only because its generation is stale
  as proof that Wi-Fi/DNS/TCP/TLS/HTTP/body/JSON transport succeeded.
- Reset transport failure/outage state while still rejecting the obsolete payload and
  keeping stale/discard diagnostics separate.

## Product 41 - 2026-07-26

**Commit:** [`8438633`](https://github.com/bcarriveau/esp-aircraft-radar/commit/843863321b5fb2458d8fe60350a585e597129245)  
**Status:** Partial-body fail-fast recovery retained

- Stopped repeating dead native body reads after the first bounded body timeout.
- Hard-recycled the station radio after a partial response body and retried through
  the serialized scheduler.
- Preserved native HTTPS preference, verified fallback policy, capacity, and aircraft
  classification.

## Product 40 - 2026-07-26

**Initial commit:** [`244f337`](https://github.com/bcarriveau/esp-aircraft-radar/commit/244f337ed6f88d2bccd5fd8a7500376ba22493b6)  
**R2 commit:** [`c2a853f`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c2a853f7cb7dd6190e7bb368f97bc57a22fc366f)  
**R3 commit:** [`0b099db`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0b099db0f80efbaae1625dbadde68a6ebc86e01a)  
**Status:** Generated aircraft database and HTTPS retry/recovery corrections retained

### Changed

- Added a generated database containing 2,697 exact aircraft designators and 11,716
  checked description aliases while preserving collision-sensitive classifications.
- R2 completed both native HTTPS attempts before one eligible verified-fallback
  attempt, let transient body stalls run to bounded deadlines, and prevented fallback
  for ordinary HTTP status, oversize, allocation, DNS/Wi-Fi, or JSON failures.
- R3 stopped retry/fallback after a partial response-body transport failure and
  returned immediately to the existing Wi-Fi recovery ladder.

## Product 39 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT39-STARTUP-FAILURE-PROPAGATION`  
**Commit:** [`35b9eca`](https://github.com/bcarriveau/esp-aircraft-radar/commit/35b9ecae8d5c5949950c5512fc3359d575bd1210)  
**Status:** Startup reliability retained

### Fixed

- Made UI and ADS-B startup report explicit success/failure.
- Allocated the core-0 incoming buffer before task creation and checked
  `xTaskCreatePinnedToCore()`.
- Declared startup complete only after all required components were ready and added
  a stable STARTUP HALTED screen instead of continuing partially initialized.

## Product 38 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT38-LOCATION-INVALIDATION`  
**Commit:** [`34a3c2d`](https://github.com/bcarriveau/esp-aircraft-radar/commit/34a3c2d6dc7193b6b3a38490cfefb9ddbfd72c1c)  
**Status:** Location invalidation retained

- Cleared the published visible-aircraft snapshot immediately when saved radar-center
  coordinates changed.
- Advanced location/generation state and showed LOCATION CHANGED / UPDATING until a
  current-generation snapshot published.
- Preserved the tracked ICAO internally without counting the configuration change as
  a lost-aircraft miss.

## Product 37 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT37-FALLBACK-HTTPS-HARDENED`  
**Commit:** [`63892b1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/63892b11529f609370176159353c4dd67dddd23a)  
**Status:** Hardened verified fallback retained

### Changed

- Removed insecure fallback behavior and attached the Espressif CA bundle with
  hostname verification.
- Replaced whole-response Arduino `String` buffering with PSRAM-first bounded
  streaming.
- Added bounded request/header/line/chunk parsing, response-size limits, deadlines,
  valid length/chunked/required-close support, and strict rejection of ambiguous or
  malformed framing.
- Limited fallback to eligible transport-specific failures.

## Product 36 - 2026-07-25

**Initial build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS`  
**Initial commit:** [`3ea59bb`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3ea59bbefd674b3728beb98af0a692e56ebb5819)  
**Status:** Radar bitmap-contact architecture evolved through R4

### Evolution

- Product 36 replaced plain 20-mile dots with category-specific aircraft bitmap
  contacts while leaving 40/80-mile contacts compact.
- **R2** [`9494747`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9494747a6b1f8d1233ce03d8c40db140322dd529) refined recognizable category silhouettes.
- **R3** `7IN-20260725-PRODUCT36-RADAR-HEADING-SPRITES-R3`, commit
  [`022e7b3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/022e7b3e99311b7ab53effe7399d45b3e1459055), added 25x25 contact sprites with
  sixteen precomputed 22.5-degree heading variants and no runtime rotation/allocation.
- **R4** `7IN-20260725-PRODUCT36-RADAR-OVERLAP-PRIORITY-R4`, commit
  [`9de21e3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/9de21e3d7ac242cdbc57c240b1328b05bc6f53c3), added deterministic overlap priority
  and refined dense-contact tag/touch behavior.

## Product 35 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT35-DESCRIPTION-TYPE-FALLBACK`  
**Commit:** [`0441bd7`](https://github.com/bcarriveau/esp-aircraft-radar/commit/0441bd70f85abb2da72314540b6c0abb15f9683c)  
**Status:** Description fallback retained under later generated database/API cleanup

- Kept exact ICAO type-code lookup authoritative.
- When exact classification remained UNKNOWN, used the already-downloaded ADSB.fi
  aircraft description against conservative local flash rules.
- Added no extra DNS/TLS/web request and left ambiguous descriptions UNKNOWN.

## Product 34 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY`  
**Commit:** [`239a7a5`](https://github.com/bcarriveau/esp-aircraft-radar/commit/239a7a5af8442562e8024f5dec98b6f58c9eddc2)  
**Status:** Tracking-loss behavior retained

### Fixed

- Added a three-successful-current-generation-update grace period before clearing a
  tracked aircraft that temporarily disappears from the feed.
- Showed TRACK SIGNAL LOST during the grace period, reset the miss counter immediately
  if the ICAO returned, and did not count failed requests, stale results, configuration
  invalidation, manual stop, or selection changes as misses.
- Kept the associated PlatformIO/cppcheck tooling isolated from the project source
  tree and third-party package headers.

## Product 33 - 2026-07-24

**R2 build:** `7IN-20260724-PRODUCT33-UI-POLISH-R2`  
**R2 commit:** [`52367df`](https://github.com/bcarriveau/esp-aircraft-radar/commit/52367dfa5f85491f2813a3d7103c0aa4a5cf6953)  
**R5 build:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`  
**R5 commit:** [`14fe9f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/14fe9f46b8c131ed1a22f5adb93e898f3188e3b1)  
**Status:** UI fit/polish retained; no standalone R3/R4 Git history is invented

### Changed

- Improved Setup field styling/spacing, nearest-other heading placement, Airspace
  live-highlights fit, and project credit presentation.
- R5 corrected longitude spacing and made the selected/tracked secondary heading
  reflect the actual populated row count: NO OTHER or NEAREST 1/2/3.
- Preserved stable ICAO identity and bounded three-entry nearest-other storage.

## Product 32 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT32-UI-DASHBOARD`  
**Commit:** [`a4cc594`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a4cc594f52c8d8e562cd751dbb528a744d72d00e)  
**Status:** Airspace dashboard retained

### Added

- Added visual Airspace total/range/category cards and nearest, fastest, lowest, and
  category highlights.
- Added up to three secondary aircraft rows with stable ICAO identity.
- Removed duplicate Setup range controls so the compact Radar 20/40/80 selector
  remained the range control.
- Expanded fixed side-icon storage in PSRAM and added the project credit card.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`  
**Commit:** [`1747cb1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1747cb169969dff12f3a5d794f561b56dc2acc83)  
**Status:** Side bitmap/heading behavior retained

- Added aircraft-type bitmap icons to idle, selected/tracked, and nearest-five areas.
- Added a rotating idle-nearest heading arrow/value with independent persistent point
  storage and stable ICAO actions.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`  
**Commit:** [`50821ba`](https://github.com/bcarriveau/esp-aircraft-radar/commit/50821badc68efb2e0c8dab597d1db5f1267628ab)  
**Status:** 200-target capacity/memory architecture retained

### Changed

- Increased deterministic retained-aircraft capacity to 200.
- Moved all capacity-scaled target, snapshot, hit, screen-contact, and label-collision
  buffers to required PSRAM with clean startup failure when unavailable.
- Preserved a returned tracked aircraft first, then filled remaining capacity nearest
  first.
- Added received/eligible/stored/dropped/visible diagnostics and corrected an earlier
  internal-RAM attempt that starved TLS of contiguous memory.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`  
**Commit:** [`237d06a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/237d06a2b79b0ca76960ad594c435ee0947716a2)  
**Status:** UI-state behavior retained

- Matched label-box allocation/bounds to target capacity.
- Used stable rendered ICAO identifiers for nearest cards/lists.
- Added explicit Radar/Tracks detail origins and correct return-tab behavior.
- Paused/refreshed the temporary selection timeout around details and closed overlays
  cleanly on tab changes.

## Product 28 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT28-RADAR-STATE-FLOW`  
**Commit:** [`61f104c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/61f104ca4a8adb6bf7d9ffb6caf3509ddb05797c)  
**Status:** Superseded by Product 29; state-flow concepts retained

- Restored idle nearest-aircraft information to the left panel.
- Gave selected/tracked details priority in the right panel and moved STOP TRACK into
  the tracked card.
- Added explicit detail returns and preserved selection after stop when practical.

## Product 27 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT27-RADAR-LAYOUT`  
**Commit:** [`2dda7ae`](https://github.com/bcarriveau/esp-aircraft-radar/commit/2dda7ae4606dff08fcd34d303610f9d920185362)  
**Status:** Radar interaction layout foundation retained

- Reworked idle, selected, and tracked panel layout.
- Added INFO, TRACK, CLEAR, and right-panel STOP TRACK actions.
- Kept the compact range selector and made nearest-list taps select aircraft instead
  of immediately changing pages.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`  
**Commit:** [`298c87a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/298c87ab43d83ae51e0276fb152789e05ad7423e)  
**Status:** Themed tag/direct-interaction foundation retained

### Added

- Added dark-navy 20-mile aircraft tags with cyan identifiers and themed borders.
- Added stable ICAO canvas hit regions with tracked, selected, then closest priority.
- Added temporary amber selection with INFO/TRACK and the compact 20/40/80-mile radar
  range selector.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`  
**Commit:** [`1c1d555`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee)  
**Status:** First-soak NVS cleanup retained

### Fixed

- Eliminated expected first-run Preferences errors for missing settings keys.
- Initialized missing device title, Wi-Fi SSID/password, latitude, and longitude from
  configured defaults without replacing already-saved values.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`  
**Commit:** [`dec570e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/dec570eab7324ae6cc00747a037af2090cdf94bd)  
**Status:** Recovery architecture retained and later hardened

### Fixed

- Added bounded retries for stalled/incomplete ADS-B response bodies and preserved the
  most advanced failure stage.
- Closed native connections before retry/fallback, reconnected Wi-Fi after incomplete
  downloads, escalated to station-radio recycle, and retried promptly after recovery.
- Moved last-resort restart execution safely to the main loop and preserved last-good
  aircraft during transport failures.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`  
**Commit:** [`faa9bc2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/faa9bc28b8524ff9fb850636e1bf356f508d71da)  
**Status:** Physical crash fix confirmed

### Fixed

- Replaced unsupported floating-point LVGL formatting with integer heading/range text.
- Fixed the core-1 LoadProhibited crash that appeared after aircraft data populated.

### Verification

- Compile/link passed and repeated physical aircraft updates confirmed the heading
  crash no longer occurred.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`  
**Commit:** [`3203bd3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3203bd3a4263b7c1f65a839466a083d7b9cd8c90)  
**Status:** Large-response handling retained

### Fixed

- Retried temporary EAGAIN/EWOULDBLOCK/timeout conditions during native HTTPS body
  reads without discarding a valid large response prematurely.
- Retained independent no-progress and total-response deadlines.

### Physical result

- A preserved runtime test completed a 105,690-byte response containing 189 parsed
  aircraft with the then-bounded 100 published targets.

## Products 19-21 - 2026-07-21

**Product 21 build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`  
**Product 21 commit:** [`5d5b0b6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9)  
**Status:** Combined preserved UI evolution; reliable standalone Product 19/20 boundaries are not claimed

### Changed across the preserved 19-21 sequence

- Added the tracked-aircraft panel and rotating heading arrow/value.
- Capped the radar range at 80 miles and added outward auto-zoom as a tracked aircraft
  approached the radar boundary.
- Added the popup on-screen keyboard for Setup fields and prevented aircraft
  identifiers from wrapping.
- Removed the redundant upper-left radar status overlay during this UI sequence.
- Kept the tracked panel active while waiting for fresh aircraft data.

### History note

- Repository documentation identifies Product 20 with the redundant status-overlay
  removal and Product 21 with the tracked-heading candidate, but Git history does not
  preserve authoritative standalone Product 19 and Product 20 commit/build boundaries.
- Product 18 remained the physically confirmed TLS baseline while this UI sequence was
  developed.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`  
**Commit:** [`69dce61`](https://github.com/bcarriveau/esp-aircraft-radar/commit/69dce612211326a4a41f0f66becc8eb7d46191f9)  
**Status:** First physically working native-TLS baseline

### Fixed

- Attached Espressif's full CA certificate bundle to the native ESP-IDF HTTPS client
  and kept hostname verification enabled.
- Corrected the Product 17 native-client configuration that failed locally before a
  network TLS handshake because no server-verification method was configured.
- Added a fallback HTTPS path and reduced unnecessary Wi-Fi reconnect churn while
  preserving core-0 HTTPS ownership, response limits, deadlines, PSRAM payload,
  generation rejection, and single-snapshot publication.

### Verification

- Compile/link and initial physical native-TLS testing passed, establishing Product 18
  as the working transport baseline for the following UI revisions.

## Product 17 - 2026-07-21

**Standalone commit/build marker:** Not preserved  
**Status:** Documented precursor inside the Product 18 commit

### Changed

- Replaced Arduino `NetworkClientSecure` plus the hand-written HTTP parser with
  ESP-IDF's native streaming HTTPS client.
- Re-resolved DNS and created a fresh native client for each retry while keeping all
  HTTPS work on the existing core-0 network task.
- Preserved response/body deadlines, response-size guards, PSRAM payload storage,
  generation rejection, and single-snapshot publication.
- Added native ESP-IDF/socket/RSSI and TCP-versus-TLS failure diagnostics.

### Known issue

- The first physical test failed before network TLS negotiation because the native
  client did not yet have a server-verification method configured; Product 18 added
  the CA certificate bundle.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`  
**Commit:** [`c81b34e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c81b34e5d640e734723964f895c9bb4ec49c1af8)  
**Status:** Superseded by native HTTPS Products 17-18

### Changed

- Used the already-resolved ADS-B server IP for TCP while retaining the hostname for
  TLS SNI.
- Increased the TLS handshake allowance from 10 to 20 seconds and logged exact
  mbedTLS error codes/text.
- Recycled Wi-Fi only for Wi-Fi, DNS, or TCP failures rather than deliberately
  disconnecting a healthy station after TLS/HTTP/body/JSON failures.

### Verification

- Complete compile/link passed; physical testing still encountered TLS timeouts,
  leading to the native HTTPS work documented in Products 17-18.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`  
**Commit:** [`b2a0a49`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b2a0a492c424cf192edf71eb7f5dd496ec0bbab8)  
**Tag:** `product-15-hardened`  
**Status:** Permanent hardened rollback baseline

### Established

- First hardened modular version-controlled baseline for the exact Waveshare
  ESP32-S3-Touch-LCD-7 project.
- Core-0 ADS-B network ownership, generation-safe publication, stale-result rejection,
  last-good aircraft retention, failure diagnostics, and bounded recovery behavior.
- Arduino-ESP32 3.0.7 high-performance XIP/OPI PSRAM configuration and the proven
  Waveshare RGB timing/DMA anti-rolling path with the 20-scanline bounce buffer.
- LVGL-based radar/System/Setup architecture and the reliability boundary from which
  later Product history is tracked.

### Verification at baseline creation

- The initial repository baseline recorded successful compile/link verification.
- The `product-15-hardened` tag remains the recommended permanent rollback point.
