# Changelog

All notable confirmed changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
Dates below follow the firmware build marker when one is preserved. Commit links
refer to the authoritative GitHub history after the Product 26-33 commit-message
cleanup.

The changelog starts with **Product 15**, the first hardened
version-controlled baseline. Earlier Product history is not reconstructed where
GitHub or preserved evidence does not confirm the changes.

## Current status

- **Current source:** Product 40
- **Current build marker:** `7IN-20260726-PRODUCT40-AIRCRAFT-TYPE-DATABASE`
- **Current branch:** `main`
- **Hardened rollback baseline:** Product 15
- **Recommended baseline tag:** `product-15-hardened`

## Product 40 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT40-AIRCRAFT-TYPE-DATABASE`  
**Status:** Host-verified aircraft-classification candidate; full PlatformIO and physical verification pending

### Added

- Added a generated, sorted aircraft-type database containing 2,697 unique
  designators from a pinned ICAO Doc 8643-derived snapshot.
- Added 11,716 collision-checked description aliases for use only when an exact
  aircraft type designator is absent or unresolved.
- Added a deterministic offline generator that validates the source schema,
  row count, duplicate designators, source SHA-256, alias conflicts, and hash
  collisions before emitting the firmware table.
- Recorded the pinned source revision, source date, source checksum, generated
  entry counts, and generated-header checksum in repository tooling metadata.

### Classification behavior

- Replaced the fragile hand-written type-prefix classifier with an exact,
  case-insensitive designator lookup.
- Kept the authoritative type-code result ahead of description fallback.
- Classified fixed-wing piston, turboprop, helicopter, and special aircraft from
  source engine and aircraft-form fields.
- Applied explicit, reviewed role rules for airliners, business jets, and
  military/heavy aircraft where engine type alone cannot determine the display
  category.
- Kept gliders, gyroplanes, balloons, airships, powered parachutes, electric
  aircraft, experimental jets, and other ambiguous types as `UNKNOWN` when the
  existing display categories cannot represent them honestly.
- Corrected the confirmed Cessna 190 gap: `C190` and `CESSNA 190` now resolve to
  `PISTON`.
- Preserved collision-sensitive distinctions including C-17 versus Cessna 170,
  E190 versus C190/B190, PC-24 versus PC-12, and C25A versus C208.

### Storage and performance

- Stored the generated database as read-only parallel arrays in firmware flash.
- Used bounded binary searches with no heap allocation, PSRAM allocation,
  Arduino `String`, regular expression, or additional runtime network request.
- Kept `Target`, `MAX_TARGETS`, all capacity-scaled buffers, and all 200-target
  bounds unchanged.
- Host object inspection measured approximately 131,128 bytes of read-only data
  for the generated type and description tables; no `.data` or `.bss` growth was
  introduced in the host object.
- The complete PlatformIO flash and internal-RAM change remains to be measured
  with the target toolchain.

### Preserved

- Product 39 startup initialization failure propagation and stable fatal-status
  screen.
- Product 38 location invalidation and pending-new-center behavior.
- Product 37 verified fallback HTTPS reader and native HTTPS preference.
- Product 36 R4 20-mile heading sprites, overlap priority, hit testing, and
  40/80-mile dot behavior.
- Stable ICAO selection/tracking, tracked-loss recovery, outward auto-zoom, MPH
  display, single-snapshot rendering, and dirty/version-driven LVGL updates.
- Core-0 serialized ADS-B ownership, non-overlapping requests, 15-second cadence,
  deadlines, recovery, stale-response rejection, and last-good retention.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA, anti-rolling
  protections, and the 20-scanline RGB bounce buffer.
- `include/config.h` privacy and ignore behavior.

### Verification

- Validated the source CSV at 2,697 rows with 2,697 unique designators and no
  duplicate codes.
- Verified deterministic generation byte-for-byte; the generated header SHA-256
  is `5cb411c1923209972da74477cbc52ec88f9468dd9b6301916b490a0f938c5737`.
- Strict host C++17 compilation passed with `-Wall -Wextra -Werror -pedantic`.
- AddressSanitizer and UndefinedBehaviorSanitizer tests passed.
- Exhaustive runtime tests passed for all 2,697 exact type records and all 11,716
  generated description aliases.
- Regression tests passed for C190, C170, C17, E190, B190, PC12, PC24, C208,
  C25A, H64, V22, P8, BT7, GLID, GYRO, UHEL, type-code priority, bitmap selection, and
  description fallback.
- Confirmed no networking, TLS, Wi-Fi, display-driver, radar-renderer, touch,
  target-capacity, PSRAM-ownership, or credential file was changed.

### Pending verification

- Full PlatformIO compile and link.
- Product 40 build-marker confirmation, target flash usage, and target
  internal-RAM usage.
- Physical C190 and other formerly unknown-aircraft bitmap/category validation.
- Normal 15-second ADS-B updates, native/fallback TLS, Wi-Fi/TLS recovery,
  location changes, 20/40/80 range changes, selection, tracking, STOP TRACK,
  touch, page switching, no screen rolling, heap/PSRAM stability, and soak test.

## Product 39 - 2026-07-26

**Build:** `7IN-20260726-PRODUCT39-STARTUP-FAILURE-PROPAGATION`  
**Status:** Host-verified startup-reliability candidate; full PlatformIO and physical verification pending

### Fixed

- Changed `ui::buildUi()` to return success only after all required radar,
  navigation, page-shell, and detail-panel construction completes.
- Prevented `main.cpp` from starting networking after required UI construction
  fails.
- Changed `adsb::begin()` to report initialization failure instead of allowing
  the main loop to treat a missing ADS-B task as ready.
- Moved allocation of the core-0 incoming target buffer ahead of task creation,
  so required PSRAM failure is visible synchronously during startup.
- Checked `xTaskCreatePinnedToCore()` against `pdPASS` and released the incoming
  PSRAM buffer when task creation fails.
- Set `startupComplete` only after target storage, required UI buffers, complete
  UI construction, ADS-B incoming storage, and core-0 task creation all succeed.
- Added a stable `STARTUP HALTED` LVGL status screen for fatal UI or ADS-B
  initialization failures that occur after display initialization.

### Startup behavior

- An initial Wi-Fi connection timeout remains recoverable and does not fail
  `adsb::begin()` after required memory and task creation have succeeded.
- Required target-buffer failures that occur before `lcd_init()` remain
  serial-fatal and do not start the operational loop.
- The core-0 ADS-B task now receives the preallocated incoming buffer and no
  longer allocates required target storage after asynchronous task startup.

### Preserved

- Core-0 serialized ADS-B ownership, non-overlapping requests, and 15-second
  start-to-start cadence.
- Native ESP-IDF HTTPS preference, hardened verified fallback, bounded framing,
  deadlines, recovery, stale-response rejection, and last-good retention.
- Product 30 200-target capacity, PSRAM ownership, and deterministic bounds.
- Product 38 location invalidation and pending-new-center behavior.
- Stable ICAO selection/tracking, outward auto-zoom, MPH display, radar
  rendering, touch behavior, and existing page layout.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM, Waveshare panel timing, DMA,
  anti-rolling protections, and the 20-scanline RGB bounce buffer.

### Verification

- Confirmed all edited source and header files against exact Product 38 Git
  blobs before modification.
- Full `src/main.cpp` and `src/adsb_network.cpp` host C++17 syntax checks passed
  with `-Wall -Wextra -Werror` using focused interface stubs.
- Changed UI startup and fatal-screen functions passed focused host C++17
  syntax checking with warnings treated as errors.
- Static checks confirmed required startup ordering, boolean failure
  propagation, PSRAM-first incoming allocation, `pdPASS` handling, cleanup after
  task-creation failure, and the Product 39 build marker.
- Confirmed no `setInsecure()`, blocking `HTTPClient::GET()`, whole-response
  `readString()`, capacity change, display-driver change, or credential file was
  introduced.

### Pending verification

- Full PlatformIO compile and link, flash usage, and internal-RAM usage.
- Physical normal startup and Product 39 marker confirmation.
- Recoverable initial Wi-Fi timeout and later reconnect.
- Native and fallback TLS, normal 15-second ADS-B updates, stale-response
  rejection, and Wi-Fi/TLS recovery.
- 20/40/80 range changes, selection, tracking, STOP TRACK, touch, page
  switching, no screen rolling, heap/PSRAM stability, and extended soak testing.

## Product 38 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT38-LOCATION-INVALIDATION`
**Status:** Host-verified state/UI candidate; full PlatformIO and physical verification pending

### Fixed

- A saved radar-center change now invalidates the published aircraft snapshot
  immediately instead of leaving aircraft from the previous location visible.
- Location invalidation increments the existing request generation, clears the
  visible target count and publication age, advances the target version, and marks
  the shared snapshot as awaiting new-center data.
- The radar data-status panel displays `LOCATION CHANGED / UPDATING` until a
  successful current-generation ADS-B snapshot is published.
- Restoring Setup defaults invalidates aircraft only when the restored latitude or
  longitude actually differs from the active location.

### Tracking and state behavior

- The stable tracked ICAO and its confirmed-miss count remain stored internally
  while location data is pending.
- Location invalidation itself does not count as a missing tracked-aircraft update.
- Pending snapshots suppress the old aircraft list and stale tracked-aircraft
  presentation while retaining the internal tracking identity.
- The first successful current-generation snapshot for the new center clears the
  pending state and becomes the first publication that can advance or reset the
  tracked-aircraft miss count.
- Ordinary range changes and same-location network failures retain the existing
  last-good snapshot behavior.

### Preserved

- Existing current-generation comparison and stale-response rejection before
  publication.
- Core-0 ADS-B ownership, non-overlapping requests, native and fallback HTTPS,
  15-second cadence, deadlines, Wi-Fi/TLS recovery, and transport classification.
- Product 30 target capacity and PSRAM ownership.
- Radar rendering, touch behavior, stable ICAO selection/tracking, outward
  auto-zoom, MPH display, Waveshare panel timing, DMA, anti-rolling protections,
  and the 20-scanline bounce buffer.

### Verification

- Confirmed the edited app-state files are minimal changes from the exact GitHub
  Product 37 blobs.
- App-state C++17 compilation passed with `-Wall -Wextra -Werror`.
- AddressSanitizer and UndefinedBehaviorSanitizer state tests passed for immediate
  invalidation, pending-snapshot suppression, tracked-ICAO retention, successful
  republish, three actual post-change misses, range changes, and generic request
  invalidation.
- Static UI checks confirmed saved-coordinate and changed-default paths use the
  location-specific invalidation API and the pending state has status priority.
- Confirmed no networking, TLS, display-driver, capacity, or configuration files
  changed.

### Pending verification

- Full PlatformIO compile and link, flash usage, and internal-RAM usage.
- Physical saved-location change, immediate old-aircraft removal, new-center
  publication, stale old-center response rejection, tracked-aircraft continuity,
  range changes, normal 15-second updates, native/fallback TLS, Wi-Fi/TLS recovery,
  touch, page switching, no screen rolling, heap/PSRAM stability, and soak testing.

## Product 37 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT37-FALLBACK-HTTPS-HARDENED`  
**Status:** Host-verified networking candidate; full PlatformIO and physical verification pending

### Fixed

- Removed `setInsecure()` from the `WiFiClientSecure` fallback and attached the
  same built-in Espressif CA bundle used by the native HTTPS path.
- Removed whole-response Arduino `String` buffering and the duplicate header and
  body `String` copies.
- Added a bounded streaming fallback reader that places response payload storage
  in PSRAM first and enforces the 250,000-byte limit before allocation or copy.
- Added a 15-second header deadline, 12-second no-progress deadline, and
  45-second absolute response deadline after response reading begins.
- Added bounded request writes so partial TLS writes cannot silently truncate the
  HTTP request.

### Framing and bounds

- Added strict bounded parsing for `Content-Length`, chunked transfer framing,
  and required close-delimited response bodies.
- Added 512-byte line, 8,192-byte total-header, and 16,384-byte chunk-framing
  limits.
- Added chunk-extension and bounded trailer support.
- Rejects conflicting `Content-Length` values, duplicate or unsupported
  `Transfer-Encoding`, mixed `Content-Length` plus chunked framing, malformed
  header names, invalid CRLF framing, forbidden framing trailers, and oversized
  decoded bodies.
- Keeps one PSRAM-backed decoded body buffer and does not retain raw chunk
  framing or duplicate response copies.

### Fallback policy

- Native ESP-IDF HTTPS remains the preferred transport.
- Fallback is limited to transport-specific native TCP, TLS, header, or
  incomplete-body failures.
- Valid native non-200 HTTP responses such as 429 or 500 do not invoke fallback.
- Oversized native responses, local payload-allocation failures, Wi-Fi loss, and
  JSON-format failures do not invoke fallback.
- Preserves retry serialization, generation checks, stale-response rejection,
  Wi-Fi/TLS recovery, and last-good snapshot retention.

### Preserved

- Core-0 ADS-B task and non-overlapping requests.
- 15-second start-to-start cadence and existing native HTTPS implementation.
- Product 30 200-target PSRAM ownership and deterministic retention.
- Stable ICAO selection/tracking, outward auto-zoom, MPH display, radar
  rendering, and touch behavior.
- Arduino-ESP32 3.0.7 high-performance XIP/OPI PSRAM, Waveshare panel timing,
  DMA, anti-rolling protections, and the 20-scanline RGB bounce buffer.

### Verification

- Confirmed the source was based on GitHub `main` at Product 36 R4 commit
  `9de21e3`.
- Confirmed Arduino-ESP32 3.0.7 requires both the CA-bundle flag and
  `attach_ssl_certificate_bundle()` callback for verified
  `WiFiClientSecure` connections.
- Host C++17 fallback compilation passed with `-Wall -Wextra -Werror`.
- Parser tests passed for Content-Length, chunked bodies, chunk extensions,
  trailers, close-delimited bodies, partial request writes, oversized bodies,
  conflicting framing, malformed headers, and invalid CRLF.
- AddressSanitizer and UndefinedBehaviorSanitizer parser tests passed.
- Final source inspection confirmed no `setInsecure()`, whole-response
  `readString()`, or fallback invocation after native HTTP status, oversize,
  allocation, Wi-Fi, or JSON failures.

### Pending verification

- Full PlatformIO compile and link.
- Product 37 build-marker confirmation, flash usage, and internal-RAM usage.
- Physical native/fallback TLS behavior, 15-second updates, Wi-Fi/TLS recovery,
  stale-response rejection, 20/40/80 range changes, selection/tracking,
  STOP TRACK, touch, page switching, no screen rolling, heap/PSRAM stability,
  and extended soak testing.

## Product 36 R4 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-OVERLAP-PRIORITY-R4`  
**Status:** Host-verified release candidate; full PlatformIO and physical verification pending

### Changed

- Added 20-mile radar contact overlap suppression so multiple 25 x 25 heading
  sprites no longer combine into unreadable cyan blobs.
- Applied deterministic visual priority in the existing project order:
  tracked aircraft first, selected aircraft second, then the closest aircraft.
- Suppressed both the lower-priority overlapping icon and its radar tag while
  retaining that aircraft in the snapshot, nearest-aircraft list, and contact
  hit data.
- Increased icon-to-tag clearance from the old dot-oriented spacing to 16 pixels
  for 20-mile bitmap contacts.
- Kept the existing compact tag spacing for 40-mile and 80-mile dot contacts.
- Updated hit evaluation so tracked and selected contact priority is preserved
  even when a lower-priority tag overlaps the contact touch area.

### Bounds and performance

- Overlap handling uses a fixed 16-pixel center-distance threshold and a bounded
  200-entry stack order table.
- Priority ordering is deterministic and uses tracked state, selected state,
  aircraft distance, then target index as a final stable tie breaker.
- No render-loop heap allocation, PSRAM allocation, temporary image buffer,
  runtime image rotation, or per-contact LVGL object was added.
- Overlap suppression remains limited to the 20-mile bitmap view; 40-mile and
  80-mile dots retain their previous rendering behavior.

### Preserved

- Six asset-derived categories and sixteen 22.5-degree heading positions.
- Product 35 type-code-first and description-fallback classification through
  `bitmapForTarget()`.
- Stable ICAO selection/tracking, tracked red and selected amber state, rings,
  tags, MPH display, STOP TRACK, auto-zoom, and nearest-aircraft panels.
- Core-0 ADS-B networking, native HTTPS and fallback, 15-second cadence,
  deadlines, stale-response rejection, last-good retention, display timing, DMA,
  XIP/OPI PSRAM, and the 20-scanline bounce buffer.

### Verification

- Host C++17 syntax check passed with warnings treated as errors.
- ASan/UBSan overlap-priority tests passed for normal-nearest, selected, tracked,
  separated contacts, 40-mile behavior, retained hit data, and 200 targets.
- Existing host renderer tests passed for zero, one, selected, tracked, and
  200-target scenarios after the hit-priority adjustment.

### Pending verification

- Full PlatformIO compile and link.
- Flash and internal-RAM usage report.
- Physical validation of dense 20-mile overlap scenes, tag clearance, touch
  priority, selection/tracking, 20/40/80 range changes, page switching, no screen
  rolling, heap/PSRAM stability, and normal ADS-B/TLS/Wi-Fi recovery.

## Product 36 R3 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-HEADING-SPRITES-R3`  
**Status:** Host-verified release candidate; full PlatformIO and physical verification pending

### Added

- Added compact 25 x 25 radar contact sprites derived from the repository's
  existing `assets/aircraft_sprites_96x64.png` artwork.
- Added sixteen precomputed clockwise heading variants per aircraft category at
  22.5-degree spacing.
- Added heading selection from each target's existing ADS-B `track` value so
  20-mile aircraft contacts point in their reported travel direction.
- Added a north-facing fallback when valid track data is unavailable.

### Changed

- Replaced Product 36's runtime downscaling of the 96 x 64 panel artwork with
  fixed flash-resident radar contact masks designed for the radar view.
- Preserved clear category differences for airliner, business jet, turboprop,
  piston, helicopter, and unknown contacts.
- Preserved a recognizable helicopter silhouette with its rotor and tail form.
- Kept bitmap contacts limited to the 20-mile radar view; 40-mile and 80-mile
  contacts remain compact dots.

### Performance and bounds

- Heading rotation is precomputed in the asset data; no runtime image rotation,
  temporary image buffer, heap allocation, PSRAM allocation, or per-contact LVGL
  object is introduced.
- The six-category, sixteen-heading sprite table occupies 9,600 bytes of fixed
  bitmap data before compiler/linker metadata.
- Rendering remains bounded to 25 x 25 pixels per visible 20-mile contact.
- Existing 18-pixel contact hit radius still covers the 25 x 25 square contact
  footprint.

### Preserved

- Product 35 type-code-first and description-fallback classification through
  `bitmapForTarget()`.
- Selected amber and tracked red contact styling, rings, collision-aware tags,
  stable ICAO identity, MPH display, auto-zoom, and STOP TRACK behavior.
- Single-snapshot rendering, 200-target PSRAM working buffers, deterministic
  bounds, and the lower-right range-control exclusion area.
- Networking, native HTTPS and fallback, 15-second cadence, stale-response
  rejection, last-good retention, display timing, DMA, XIP/OPI PSRAM, and the
  20-scanline bounce buffer.

### Verification

- Inspected the uploaded repository, branch `main`, commit `3ea59bb`, and the
  existing dirty working tree before editing.
- Host C++17 renderer syntax check passed.
- ASan/UBSan radar renderer test passed for zero, one, selected, tracked,
  overlapping, and 200-target scenarios.
- Heading bucket tests passed for cardinal directions, diagonal boundaries,
  wraparound, and negative input normalization.
- Confirmed all 96 category/heading masks are non-empty and the preview sheet
  visually retains category separation.

### Pending verification

- Full PlatformIO compile and link.
- Flash and internal-RAM usage report.
- Physical checks for heading orientation, dense 20-mile traffic, selection,
  tracking, hit testing, range changes, page switching, touch responsiveness,
  no screen rolling, heap/PSRAM stability, and normal ADS-B/TLS/Wi-Fi recovery.

## Product 36 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS`  
**Status:** Host-verified release candidate; full PlatformIO and physical verification pending

### Added

- Replaced normal radar contact dots with compact 20 x 14 aircraft bitmap
  silhouettes when the radar is set to 20 miles.
- Used the existing flash-resident airliner, business-jet, turboprop, piston,
  helicopter, and unknown bitmap assets.
- Added a fixed bounded nearest-neighbor contact renderer that draws directly
  into the existing radar canvas and preserves transparent source pixels.

### Changed

- Selected aircraft retain an amber bitmap plus the existing amber selection
  rings and selected tag.
- Tracked aircraft retain a red bitmap plus the existing tracked ring,
  `TRACKED` tag, stable ICAO identity, MPH display, and STOP TRACK behavior.
- Normal 20-mile contacts use restrained cyan or green radar-theme coloring.
- 40-mile and 80-mile views retain the existing compact dot rendering.
- Contact bitmaps use `bitmapForTarget()` so the Product 35 classification path
  remains type code first, description-keyword fallback second, and unknown only
  when neither method classifies the target.
- Contact pixels are clipped from the existing lower-right range-control
  exclusion area; the 20/40/80 control and `MILES` caption are unchanged.

### Performance and bounds

- Contact rendering uses fixed 20 x 14 dimensions and bounded loops.
- No temporary image, heap, PSRAM, Arduino `String`, or LVGL object is allocated
  inside the radar render loop.
- The existing single-snapshot render path, PSRAM working buffers, 200-target
  capacity, and `uint8_t` count/index safety assertion remain unchanged.
- Existing 18-pixel contact touch radius already covers the bitmap dimensions,
  so hit testing remains bounded without enlarging overlap between targets.

### Preserved

- Stable ICAO selection and tracking identity and hit-test priority: tracked,
  selected, then closest.
- Collision-aware label limits and selected/tracked label priority.
- Product 34 tracking-loss recovery and Product 35 description classifier.
- Core-0 networking, native HTTPS and fallback, non-overlapping requests,
  15-second cadence, deadlines, recovery, range-generation and stale-response
  rejection, and last-good retention.
- Radar center, radius, projection, bearing, distance, request-radius, and
  outward auto-zoom math.
- Product 30 PSRAM ownership and 200-target capacity.
- Waveshare panel timing, DMA, XIP/OPI PSRAM, anti-rolling configuration, GT911
  touch driver, and 20-scanline RGB bounce buffer.
- `include/config.h` privacy and ignore behavior.

### Completed verification

- Reviewed zero-, one-, dense-, overlap-, selected-, tracked-, and 200-target
  bounds in the contact render and hit-test paths.
- Host C++17 bitmap-scaling tests passed with warnings treated as errors,
  including transparent pixels, canvas bounds, range-control clipping, and 200
  retained contacts.
- Static checks confirmed the new render helper contains no heap allocation,
  PSRAM allocation, Arduino `String`, or per-aircraft LVGL object creation.
- Confirmed Product 36 changes are limited to radar rendering, the build marker,
  and repository documentation.

### Pending verification

- Full PlatformIO compile and link in a PlatformIO-capable checkout.
- Flash and internal-RAM usage report from that build.
- Physical validation of every bitmap category and Product 35 description
  fallback, including unknown fallback.
- Dense 20-mile traffic, selection, tracking, tracking-loss recovery, overlapping
  aircraft, tag and icon touch, 20/40/80 switching, page switching, touch
  responsiveness, normal 15-second updates, TLS/Wi-Fi recovery, no screen
  rolling, heap/PSRAM stability, and extended soak testing.

## Product 35 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT35-DESCRIPTION-TYPE-FALLBACK`  
**Status:** Superseded by Product 36

### Added

- Added a conservative aircraft-description classification fallback for targets
  whose ADS-B ICAO type code is empty, unknown, or not present in the existing
  type-code tables.
- Added compact flash-resident keyword references for:
  - Airliners and regional jets
  - Business and corporate jets
  - Military and heavy aircraft
  - Turboprops
  - Piston aircraft
  - Helicopters and rotorcraft
- Added target-aware classification helpers so existing radar, Tracks, details,
  bitmap, and Airspace category call sites can use the stored ADS-B description
  without changing their source files.

### Changed

- Kept the existing ICAO type-code classifier authoritative.
- Consulted `Target::description` only when type-code classification returns
  `UNKNOWN`.
- Normalized description text for case-insensitive keyword matching without
  heap allocation, Arduino `String`, regular expressions, or additional network
  requests.
- Kept ambiguous or unsupported descriptions classified as `UNKNOWN` rather
  than guessing from weak words such as `JET` alone.

### Preserved

- Product 34 tracked-aircraft-loss recovery and three-successful-update grace
  period.
- Stable ICAO selection and tracking identity.
- Product 30 200-target bounded capacity and PSRAM ownership.
- Native HTTPS and fallback transport behavior.
- Core-0 request ownership, non-overlapping requests, 15-second cadence,
  deadlines, recovery, stale-response rejection, and last-good retention.
- Radar range, projection, hit testing, label priority, and outward auto-zoom.
- Waveshare panel timing, DMA, XIP/OPI PSRAM, and the 20-scanline bounce buffer.
- Existing `Target` size and all capacity-scaled array bounds.
- `include/config.h` privacy and ignore behavior.

### Verification

- Host C++17 aircraft-classifier tests passed with warnings treated as errors.
- Tests covered type-code priority and description collisions including:
  - C-17 versus Cessna 172
  - Cessna Citation versus piston Cessna models
  - Pilatus PC-24 versus PC-12
  - AH-64 Apache versus Piper Apache
  - Airbus helicopters versus Airbus airliners
  - Dornier 328 turboprop versus jet variants
- Confirmed plain type-code inputs remain type-code-only.
- Confirmed the `Target` structure and `MAX_TARGETS` remain unchanged.

### Pending verification

- Full PlatformIO compile and link.
- Confirm Product 35 build marker, OPI PSRAM, and 20-scanline bounce buffer.
- Record flash and internal-RAM usage.
- Physical validation that previously unknown aircraft receive sensible
  categories and bitmaps.
- Normal 15-second updates, selection, tracking, STOP TRACK, range changes,
  touch, TLS/Wi-Fi recovery, no screen rolling, heap/PSRAM stability, and soak.

## Product 34 - 2026-07-25

**Build:** `7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY`  
**Status:** Superseded by Product 35

### Fixed

- Corrected manual tracking that could remain active indefinitely after the
  tracked ICAO stopped appearing in successful ADS-B snapshots.
- Added a bounded three-successful-update grace period:
  - A returned tracked ICAO resets the miss counter immediately.
  - The first and second consecutive successful misses retain tracking.
  - The third consecutive successful miss clears manual tracking and the tracked
    ICAO atomically.
- Returned the radar to normal idle nearest-aircraft mode after automatic clear.
- Prevented failed requests from advancing the tracked-aircraft miss counter.
- Prevented stale discarded range responses from advancing the counter.
- Reset the counter when STOP TRACK is used or a new aircraft is tracked.

### Changed

- Replaced the indefinite `Waiting for tracked aircraft` state during the grace
  period with:
  - `TRACK SIGNAL LOST`
  - `Checking next update`
- Added a serial message when confirmed loss clears tracking:
  `Tracked aircraft <hex> absent from 3 consecutive updates; tracking cleared`.

### Build tooling

- Added a global PlatformIO `workspace_dir` outside the Google Drive project:
  `~/.platformio/workspaces/bills_aircraft_radar`.
- Kept generated objects and downloaded project libraries away from Drive File
  Stream locking or removal during SCons builds.
- Added `cppcheck: --skip-packages` so Project Inspection analyzes application
  sources without reporting false syntax failures in downloaded toolchain and
  library package headers.
- The cppcheck option does not alter normal compilation, linking, upload, target
  framework, or runtime behavior.

### Preserved

- Product 33 R5 UI, longitude spacing, and dynamic nearest-other count.
- Stable ICAO selection and tracking identity.
- Tracked-aircraft outward auto-zoom.
- Native HTTPS and fallback behavior.
- Core-0 request ownership and non-overlapping requests.
- Recovery escalation, deadlines, generation rejection, and last-good retention.
- Product 30 target capacity and PSRAM ownership.
- Radar center, radius, projection, bearing, request radius, and range math.
- Waveshare panel timing, DMA, XIP/OPI PSRAM, and the 20-scanline bounce buffer.

### Verification

- App-state compilation passed with warnings treated as errors.
- State-transition tests covered tracked presence, one and two misses, third-miss
  auto-clear, target return, empty successful snapshots, manual STOP TRACK, a
  newly tracked aircraft, repeated fetch failures, and stale discarded
  responses.
- Logging occurs after releasing the app-state mutex.

### Pending verification

- Full PlatformIO compile and link.
- Flash and internal-RAM usage report.
- Physical tracked-aircraft-loss test.
- Normal ADS-B and TLS operation.
- Clean Project Inspection run with package headers skipped.
- Extended soak test.

## Product 33 R5 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`  
**Commit:** [`14fe9f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/14fe9f46b8c131ed1a22f5adb93e898f3188e3b1)  
**Status:** Superseded by Product 34

### Changed

- Corrected the longitude label and input spacing on Setup.
- Changed the selected/tracked left-panel heading to reflect the actual number
  of populated other-aircraft rows:
  - `NO OTHER`
  - `NEAREST 1`
  - `NEAREST 2`
  - `NEAREST 3`
- Kept the bounded list at three entries when more aircraft are available.
- Hid unused labels, icons, and ICAO slots.
- Increased the nearest-other heading size.
- Completed the static Airspace live-highlights fit cleanup.
- Preserved stable ICAO identity for nearest-other selection and detail actions.

### Preserved

- Product 30 target capacity and PSRAM ownership.
- Native HTTPS and fallback behavior.
- Core-0 request ownership and non-overlapping requests.
- Recovery escalation, deadlines, generation rejection, and last-good retention.
- Radar center, radius, projection, bearing, request radius, and range math.
- Waveshare panel timing, DMA, XIP/OPI PSRAM, and the 20-scanline bounce buffer.

### Verification

- The README identified Product 33 R3 as the physically tested source from which
  the later R4 and R5 refinements were derived.
- Product 33 R4 was not retained as a standalone Git commit; its
  longitude-spacing correction is included in R5.
- Product 33 R5 still required full compile/link, physical UI and touch testing,
  normal ADS-B verification, and soak testing when superseded.

## Product 33 R2 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R2`  
**Commit:** [`52367df`](https://github.com/bcarriveau/esp-aircraft-radar/commit/52367dfa5f85491f2813a3d7103c0aa4a5cf6953)  
**Status:** Superseded by Product 33 R5

### Changed

- Improved Setup field styling and spacing.
- Enlarged and repositioned the nearest-other heading.
- Expanded the Airspace live-highlights card.
- Removed Airspace highlight scrolling and its timer, offset, and direction
  state.
- Kept all live-highlight sections visible at once.
- Repositioned and simplified the project credit card.

### Verification

- Strict source comparison limited the revision to UI and build-marker files.
- UI syntax checking passed with warnings treated as errors.
- Later Product 33 revisions refined the fit and dynamic nearest count.

## Product 32 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT32-UI-DASHBOARD`  
**Commit:** [`a4cc594`](https://github.com/bcarriveau/esp-aircraft-radar/commit/a4cc594f52c8d8e562cd751dbb528a744d72d00e)  
**Status:** Superseded

### Added

- A visual Airspace dashboard with total aircraft, aircraft within 20 and 40
  miles, current range, category counts and percentages, and nearest, fastest,
  lowest-airborne, and dominant-category highlights.
- Category cards for airliners, business jets, turboprops, piston aircraft,
  helicopters, and military/other traffic.
- Three nearest-other aircraft rows during selected or tracked operation.
- Stable ICAO identity for the nearest-other rows.
- A restrained project identification card on System.

### Changed

- Removed duplicate Setup-page range controls and kept the compact radar selector
  as the only 20/40/80 control.
- Added a `MILES` caption and updated label exclusions.
- Expanded fixed side-icon storage in PSRAM from 7 to 16 buffers.

### Verification

- UI and radar sources passed C++17 syntax checks.
- Bounds and null termination were checked for icon and ICAO arrays.
- AddressSanitizer and UndefinedBehaviorSanitizer tests covered zero, one, and
  200 targets plus selected/tracked ordering cases.
- No networking, capacity, transport, panel-driver, or target-storage sources
  changed.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`  
**Commit:** [`1747cb1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1747cb169969dff12f3a5d794f561b56dc2acc83)  
**Status:** Superseded

### Added

- Aircraft-type bitmap icons to the idle nearest-aircraft card,
  selected/tracked priority details, and nearest-five list.
- A rotating heading arrow and heading value for the idle nearest aircraft.
- Independent persistent point arrays for the idle and selected/tracked arrows.

### Changed

- Removed the redundant plain-text idle heading line.
- Made the arrow and heading label select the same stable ICAO aircraft.

### Verification

- Radar syntax checks passed.
- AddressSanitizer and UndefinedBehaviorSanitizer tests covered independent arrow
  storage and 200-target rendering.
- Product 30 memory ownership and networking behavior remained unchanged.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`  
**Commit:** [`50821ba`](https://github.com/bcarriveau/esp-aircraft-radar/commit/50821badc68efb2e0c8dab597d1db5f1267628ab)  
**Status:** Capacity and memory architecture retained by current firmware

### Added

- Bounded storage for up to 200 retained targets.
- Diagnostics for received, eligible, stored, capacity-dropped, and visible
  aircraft.
- Required PSRAM allocation for shared targets, incoming ADS-B targets, UI
  snapshots, radar hit regions, screen contacts, and label collision boxes.
- Clean startup failure when required PSRAM is unavailable.
- Largest-free internal-memory logging before ADS-B networking starts.

### Changed

- Preserved tracked aircraft first, then filled remaining capacity nearest-first.
- Removed internal-RAM fallback paths for capacity-scaled buffers.
- Prevented UI and network servicing after incomplete startup.

### Fixed

- Corrected an earlier 200-target attempt that left too little contiguous
  internal RAM for TLS and produced mbedTLS `-0x7F00` allocation failures.

### Verification

- App-state publication, snapshot, and tracking tests passed at 200 targets.
- Radar render and hit testing passed under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Allocation-failure and cleanup paths were tested.
- Physical startup logs recorded required PSRAM allocations and repeated native
  TLS success with large snapshots.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`  
**Commit:** [`237d06a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/237d06a2b79b0ca76960ad594c435ee0947716a2)  
**Status:** UI-state behavior retained by current firmware

### Fixed

- Corrected radar label-box bounds to use `MAX_TARGETS + 2` consistently.
- Used stable ICAO hex values for the left nearest card and right nearest list.
- Limited left-panel clicks to nearest-aircraft content.
- Hid idle nearest details when selected or tracked details had priority.
- Added explicit Radar and Tracks detail origins and correct return behavior.
- Paused selected-aircraft timeout while details were open.
- Closed detail overlays cleanly when another tab was selected.

### Preserved

- 100-target capacity at that stage.
- Networking, TLS, Wi-Fi recovery, polling, filtering, radar projection, and
  display configuration.

### Verification

- Bounds, ICAO buffers, list indexes, and state transitions were reviewed.
- UI and radar sources passed syntax checking against local interface stubs.

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
- Moved selected and tracked information into the right aircraft card.
- Added INFO, TRACK, CLEAR, and right-panel STOP TRACK actions.
- Kept the compact range selector at the radar lower-right.
- Improved nearest-aircraft presentation and selected/tracked heading colors.
- Removed redundant left-side range text.
- Changed nearest-list taps to select instead of immediately changing pages.

### Preserved

- Radar projection and range math.
- Stable tracking identity.
- Networking, TLS, PSRAM/XIP, DMA, panel timing, and bounce buffering.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`  
**Commit:** [`298c87a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/298c87ab43d83ae51e0276fb152789e05ad7423e)  
**Status:** First radar interaction and themed-tag candidate

### Added

- Compact dark navy 20-mile tags with cyan identifiers and teal borders.
- Stable ICAO-based hit regions for contacts and tags.
- Hit testing priority: tracked, selected, then closest contact.
- Temporary amber selected-aircraft state with INFO and TRACK actions.
- Compact 20/40/80 range selector inside the radar.

### Changed

- Made the nearest-aircraft panel select its aircraft while idle.
- Kept tracked labels and outward auto-zoom at higher priority.
- Preserved Product 25 networking and display reliability.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`  
**Commit:** [`1c1d555`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee)  
**Status:** First-soak cleanup retained by later firmware

### Fixed

- Eliminated expected first-run Preferences errors for missing NVS keys.
- Initialized missing title, Wi-Fi, latitude, and longitude keys with defaults.
- Kept existing saved values unchanged.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`  
**Commit:** [`dec570e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/dec570eab7324ae6cc00747a037af2090cdf94bd)  
**Status:** Transport recovery architecture retained by current firmware

### Fixed

- Added bounded retries for stalled or incomplete ADS-B response bodies.
- Treated early zero-byte reads as incomplete responses.
- Closed native HTTP connections cleanly before fallback or retry.
- Preserved the most advanced failure stage across attempts.
- Reconnected Wi-Fi after incomplete body downloads.
- Escalated repeated failures to a complete station-radio recycle.
- Retried ADS-B promptly after recovery.
- Moved last-resort restart execution to the main loop.
- Preserved the last valid snapshot during transport failures.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`  
**Commit:** [`faa9bc2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/faa9bc28b8524ff9fb850636e1bf356f508d71da)  
**Status:** Crash fix confirmed in physical updates

### Fixed

- Replaced unsupported floating-point LVGL formatting with integer heading
  formatting.
- Fixed the core-1 `LoadProhibited` crash after aircraft data populated.
- Normalized headings from 000 through 359.
- Removed remaining floating-point LVGL format calls from range text.

### Verification

- Complete compile and link passed.
- Repeated physical updates confirmed the heading crash was fixed.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`  
**Commit:** [`3203bd3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3203bd3a4263b7c1f65a839466a083d7b9cd8c90)  
**Status:** Large-response handling retained by later firmware

### Fixed

- Retried temporary `EAGAIN`, `EWOULDBLOCK`, and timeout conditions during
  native HTTPS body reads.
- Prevented valid large ADS-B responses from being discarded prematurely.
- Retained independent no-progress and total-response deadlines.

### Verification

- Runtime testing confirmed a complete 105,690-byte response containing 189
  parsed aircraft and the then-bounded 100 published targets.

## Products 19-21 - 2026-07-21

**Final build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`  
**Commit:** [`5d5b0b6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9)  
**Status:** Combined GitHub record; retained and refined by later firmware

Git history preserves Products 19-21 as one combined Product 21 commit. Exact
individual Product boundaries are not reconstructed.

### Confirmed changes

- Removed 160 miles and limited ranges to 20, 40, and 80 miles.
- Added predictive outward auto-zoom for tracked aircraft.
- Added a full-width popup keyboard for Setup fields.
- Prevented aircraft identifiers from wrapping.
- Removed the redundant upper-left radar status overlay.
- Changed the left aircraft panel to tracked-aircraft information while tracking.
- Added the rotating heading arrow and heading value.
- Kept the tracked panel active through temporary target omissions.
- Opened details for whichever aircraft the left panel represented.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`  
**Commit:** [`69dce61`](https://github.com/bcarriveau/esp-aircraft-radar/commit/69dce612211326a4a41f0f66becc8eb7d46191f9)  
**Status:** Physically working native TLS baseline

### Fixed

- Attached Espressif's CA certificate bundle to native HTTPS.
- Kept hostname verification enabled.
- Corrected the Product 17 missing server-verification configuration.
- Added a secure fallback HTTPS path.
- Reduced unnecessary Wi-Fi reconnect churn.

### Verification

- Compile and link passed.
- Initial physical TLS testing passed.

## Product 17 - 2026-07-21

**Standalone commit:** Not preserved  
**Status:** Documented precursor to Product 18

### Changed

- Replaced Arduino `NetworkClientSecure` plus the hand-written HTTP parser with
  ESP-IDF native streaming HTTPS.
- Re-resolved DNS and created a fresh native client for each retry.
- Kept HTTPS work on the core-0 network task.
- Preserved deadlines, size guards, PSRAM payload storage, generation rejection,
  and single-snapshot publication.
- Added detailed native error, socket errno, RSSI, and TCP-versus-TLS diagnostics.

### Known issue

- The first physical test failed before the handshake because no
  server-verification method was configured; Product 18 fixed it.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`  
**Commit:** [`c81b34e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c81b34e5d640e734723964f895c9bb4ec49c1af8)  
**Status:** Superseded by native HTTPS Products 17-18

### Changed

- Used the resolved server IP for TCP while retaining hostname TLS SNI.
- Increased TLS handshake allowance from 10 to 20 seconds.
- Logged exact mbedTLS error codes and descriptions.
- Recycled Wi-Fi only for Wi-Fi, DNS, or TCP failures.
- Retried TLS, HTTP, body, and JSON failures without deliberately disconnecting
  a healthy Wi-Fi station.

### Verification

- Complete compile and link passed.
- Physical testing still encountered TLS timeouts, leading to Product 17.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`  
**Commit:** [`b2a0a49`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b2a0a492c424cf192edf71eb7f5dd496ec0bbab8)  
**Tag:** `product-15-hardened`  
**Status:** First hardened version-controlled and rollback baseline

### Added

- Modular PlatformIO/C++ and LVGL firmware for the exact Waveshare
  ESP32-S3-Touch-LCD-7.
- Core-0 ADS-B networking and Wi-Fi recovery ownership.
- True 15-second start-to-start polling and non-overlapping requests.
- Explicit connect, header, body-idle, and total-response deadlines.
- Separate Wi-Fi, DNS, TCP, TLS, HTTP, response-body, JSON, and stale-response
  failure classification.
- Request generations, obsolete-response rejection, and last-good retention.
- Thread-safe single-snapshot publication and radar rendering.
- Collision-aware 20-mile labels and stable ICAO tracking.
- Setup validation, protected reset behavior, and system diagnostics.
- Explicit aircraft categories and unknown-artwork fallback.
- Private configuration example with `include/config.h` excluded from Git.

### Display and memory baseline

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM framework.
- OPI PSRAM with `BOARD_HAS_PSRAM`.
- Waveshare RGB timing, DMA, anti-rolling protections, and 20-scanline bounce
  buffer.

### Verification

- Compile and link passed.
- Product 15 remains the permanent hardened rollback baseline.

---

## History boundary

The GitHub Product history begins at Product 15. Product 15 states that it
preserved the proven RGB anti-rolling configuration from Product 14, but the
repository does not contain authoritative Product 1-14 history. Those releases
are intentionally omitted rather than reconstructed from memory or uncertain
files.
