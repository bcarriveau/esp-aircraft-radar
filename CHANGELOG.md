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

- **Current source:** Product 36
- **Current build marker:** `7IN-20260725-PRODUCT36-RADAR-BITMAP-CONTACTS`
- **Current branch:** `main`
- **Hardened rollback baseline:** Product 15
- **Recommended baseline tag:** `product-15-hardened`

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
