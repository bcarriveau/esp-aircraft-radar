# Changelog

All notable confirmed changes to Bill's 7-inch ESP32-S3 Aircraft Radar are
documented here.

This project uses numbered **Product** builds rather than semantic versioning.
Dates below follow the firmware build marker when one is preserved. Commit links
refer to the authoritative GitHub history after the Product 26–33 commit-message
cleanup.

The changelog starts with **Product 15**, the first hardened
version-controlled baseline. Earlier Product history is not reconstructed where
GitHub or preserved evidence does not confirm the changes.

## Current status

- **Current source:** Product 33 R5
- **Current build marker:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`
- **Current branch:** `main`
- **Hardened rollback baseline:** Product 15
- **Recommended baseline tag:** `product-15-hardened`

Product 33 R5 remains a candidate until its complete PlatformIO build, physical
UI and touch checks, normal ADS-B operation, and extended soak testing are
confirmed.

## Unreleased

### Documentation

- Reorganized `README.md` around the current project, exact hardware, features,
  setup, architecture, reliability protections, verification, screenshots, and
  major milestones.
- Added this confirmed-history `CHANGELOG.md`.
- Kept detailed Product history out of the main README.
- Made no firmware, networking, display, capacity, or credential changes.

---

## Product 33 R5 - 2026-07-24

**Build:** `7IN-20260724-PRODUCT33-UI-POLISH-R5`  
**Commit:** [`14fe9f4`](https://github.com/bcarriveau/esp-aircraft-radar/commit/14fe9f46b8c131ed1a22f5adb93e898f3188e3b1)  
**Status:** Current UI-cleanup candidate

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

- The current README identifies Product 33 R3 as the physically tested source
  from which the later R4 and R5 refinements were derived.
- Product 33 R4 is not retained as a standalone Git commit; its longitude-spacing
  correction is included in R5.
- Product 33 R5 still requires full compile/link, physical UI and touch testing,
  normal ADS-B verification, and soak testing.

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

- A visual Airspace dashboard with:
  - Total aircraft
  - Aircraft within 20 miles
  - Aircraft within 40 miles
  - Current selected range
  - Category counts and percentages
  - Nearest, fastest, lowest-airborne, and dominant-category highlights
- Aircraft-category cards for airliners, business jets, turboprops, piston
  aircraft, helicopters, and military/other traffic.
- Three nearest-other aircraft rows while an aircraft is selected or tracked.
- Stable ICAO identity for the nearest-other rows.
- A restrained project identification card on System.

### Changed

- Removed the duplicate Setup-page 20/40/80 controls.
- Kept the compact radar selector as the only range control.
- Added a `MILES` caption to the radar selector.
- Updated radar label exclusions to match the visible selector and caption.
- Expanded fixed side-icon storage in PSRAM from 7 to 16 buffers.

### Verification

- UI and radar sources passed C++17 syntax checks.
- Bounds and null termination were checked for the icon and ICAO arrays.
- AddressSanitizer and UndefinedBehaviorSanitizer tests covered zero, one, and
  200 targets plus selected/tracked ordering cases.
- No networking, capacity, transport, panel-driver, or target-storage sources
  changed.

## Product 31 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW`  
**Commit:** [`1747cb1`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1747cb169969dff12f3a5d794f561b56dc2acc83)  
**Status:** Superseded

### Added

- Aircraft-type bitmap icons to:
  - The idle nearest-aircraft card
  - Selected/tracked priority details
  - The nearest-five aircraft list
- A rotating heading arrow and heading value for the idle nearest aircraft.
- Independent persistent point arrays for the idle and selected/tracked heading
  arrows.

### Changed

- Removed the redundant plain-text heading line from the idle summary.
- Made the heading arrow and heading label select the same stable ICAO aircraft.

### Verification

- Radar syntax checks passed.
- AddressSanitizer and UndefinedBehaviorSanitizer tests covered independent arrow
  storage and 200-target rendering.
- Product 30 memory ownership and networking behavior were intentionally
  unchanged.

## Product 30 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT30-200-TARGET-PSRAM`  
**Commit:** [`50821ba`](https://github.com/bcarriveau/esp-aircraft-radar/commit/50821badc68efb2e0c8dab597d1db5f1267628ab)  
**Status:** Capacity and memory architecture retained by current firmware

### Added

- Bounded storage for up to 200 retained targets.
- Diagnostics for:
  - Received aircraft
  - Eligible aircraft
  - Stored aircraft
  - Capacity-dropped aircraft
  - Visible aircraft
- Required PSRAM allocation for:
  - Authoritative shared target storage
  - Core-0 incoming ADS-B storage
  - UI target snapshots
  - Radar hit regions
  - Screen contacts
  - Label collision boxes
- Clean startup failure when required PSRAM allocation is unavailable.
- Logging of the largest free internal-memory block before ADS-B networking
  starts.

### Changed

- Preserved the tracked aircraft when returned, then filled remaining capacity
  nearest-first.
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
- Physical startup logs recorded required PSRAM allocations and repeated
  successful native TLS requests with large aircraft snapshots.

## Product 29 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT29-UI-STATE-FIXES`  
**Commit:** [`237d06a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/237d06a2b79b0ca76960ad594c435ee0947716a2)  
**Status:** UI-state behavior retained by current firmware

### Fixed

- Corrected the radar label-box allocation and bounds check to use the same
  `MAX_TARGETS + 2` limit.
- Used stable rendered ICAO hex values for:
  - The left nearest-aircraft card
  - The right nearest-aircraft list
- Limited left-panel click handling to the nearest-aircraft content instead of
  the entire panel.
- Hid idle nearest-aircraft details when selected or tracked details had
  priority.
- Added explicit Radar and Tracks detail origins.
- Kept the correct tab active while details were open.
- Returned INFO from Radar to Radar and Details from Tracks to Tracks.
- Paused the selected-aircraft timeout while details were open and refreshed it
  on return.
- Closed the detail overlay cleanly when another navigation tab was selected.

### Preserved

- 100-target capacity at this stage.
- Networking, TLS, Wi-Fi recovery, polling, filtering, radar projection, and
  display configuration.

### Verification

- Bounds, ICAO buffers, list indexes, and state transitions were reviewed.
- UI and radar sources passed syntax checking against local interface stubs.
- A complete PlatformIO build and physical test were still required at the time
  of this commit.

## Product 28 - 2026-07-23

**Build:** `7IN-20260723-PRODUCT28-RADAR-STATE-FLOW`  
**Commit:** [`61f104c`](https://github.com/bcarriveau/esp-aircraft-radar/commit/61f104ca4a8adb6bf7d9ffb6caf3509ddb05797c)  
**Status:** Superseded by Product 29

### Changed

- Restored nearest-aircraft information to the left panel while idle.
- Gave selected and tracked details priority in the right panel.
- Moved STOP TRACK into the right tracked-aircraft card.
- Added explicit detail return behavior for Radar and Tracks.
- Kept the active tab accurate while details were open.
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
- Improved the nearest-aircraft list presentation.
- Added selected/tracked color treatment to heading elements.
- Removed redundant left-side range text.
- Changed nearest-list taps to select an aircraft instead of immediately changing
  pages.

### Preserved

- Radar projection and range math.
- Stable tracking identity.
- Networking, TLS, PSRAM/XIP, DMA, panel timing, and bounce buffering.

## Product 26 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT26-RADAR-INTERACTION`  
**Commit:** [`298c87a`](https://github.com/bcarriveau/esp-aircraft-radar/commit/298c87ab43d83ae51e0276fb152789e05ad7423e)  
**Status:** First radar interaction and themed-tag candidate

### Added

- Compact dark navy 20-mile aircraft tags with cyan identifiers and teal borders.
- Stable ICAO-based hit regions for radar contacts and tags.
- Tag-first hit testing with priority for:
  1. Tracked aircraft
  2. Selected aircraft
  3. Closest contact
- A temporary selected-aircraft state with amber rings and tag styling.
- INFO and TRACK actions for the selected aircraft.
- A compact 20/40/80 range selector inside the radar.

### Changed

- Made the nearest-aircraft panel select its aircraft while idle.
- Kept tracked labels and outward auto-zoom at higher priority.
- Preserved Product 25 networking and display-reliability behavior.

### Verification

- Physical touch, layout, rolling, and soak verification were required.

## Product 25 - 2026-07-22

**Build:** `7IN-20260722-PRODUCT25-NVS-DEFAULTS`  
**Commit:** [`1c1d555`](https://github.com/bcarriveau/esp-aircraft-radar/commit/1c1d5557dd1ae34ba9ea47a39e381c0a86bbbeee)  
**Status:** First-soak cleanup retained by later firmware

### Fixed

- Eliminated expected first-run Preferences errors for missing NVS keys.
- Initialized missing title, Wi-Fi SSID, Wi-Fi password, latitude, and longitude
  keys with configured defaults.
- Kept existing saved values unchanged.

### Verification

- The issue was identified during the first soak test.

## Product 24 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT24-TRANSPORT-RECOVERY`  
**Commit:** [`dec570e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/dec570eab7324ae6cc00747a037af2090cdf94bd)  
**Status:** Transport recovery architecture retained by current firmware

### Fixed

- Added bounded retries for stalled or incomplete ADS-B response bodies.
- Treated early zero-byte reads as incomplete responses.
- Closed native HTTP connections cleanly before fallback or retry.
- Preserved the most advanced failure stage across attempts.
- Reconnected Wi-Fi immediately after incomplete body downloads.
- Escalated repeated failures to a complete station-radio recycle.
- Retried ADS-B promptly after successful recovery.
- Moved last-resort restart execution from the network task to the main loop.
- Preserved the last valid aircraft snapshot during transport failures.

### Verification

- Networking translation units were compile-verified.
- Full build, physical recovery tests, router interruption tests, and soak testing
  were still required at the time of the commit.

## Product 23 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT23-HEADING-CRASH-FIX`  
**Commit:** [`faa9bc2`](https://github.com/bcarriveau/esp-aircraft-radar/commit/faa9bc28b8524ff9fb850636e1bf356f508d71da)  
**Status:** Crash fix confirmed in physical updates

### Fixed

- Replaced unsupported floating-point LVGL formatting with integer heading
  formatting.
- Fixed the core-1 `LoadProhibited` crash that appeared after aircraft data
  populated.
- Normalized headings to integer values from 000 through 359.
- Removed remaining floating-point LVGL format calls from range text.

### Preserved

- Rotating heading arrow.
- Tracked-aircraft panel.
- Product 22 large-response handling.
- Networking, panel timing, PSRAM/XIP, and bounce buffering.

### Verification

- Complete compile and link passed.
- Repeated physical aircraft updates confirmed the heading crash was fixed.
- The physical log then exposed the separate stalled-response recovery problem
  addressed by Product 24.

## Product 22 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT22-LARGE-RESPONSE`  
**Commit:** [`3203bd3`](https://github.com/bcarriveau/esp-aircraft-radar/commit/3203bd3a4263b7c1f65a839466a083d7b9cd8c90)  
**Status:** Large-response handling retained by later firmware

### Fixed

- Retried temporary `EAGAIN`, `EWOULDBLOCK`, and timeout conditions during
  native HTTPS body reads.
- Prevented valid large ADS-B responses from being discarded prematurely.
- Retained independent no-progress and total-response deadlines.

### Preserved

- TLS verification.
- Wi-Fi recovery.
- Product 21 tracking and UI behavior.
- PSRAM/XIP and display buffering.

### Verification

- Runtime testing confirmed a complete 105,690-byte response.
- The response contained 189 parsed aircraft and published the then-bounded 100
  targets.
- A separate heading-format crash discovered afterward was fixed in Product 23.

## Products 19-21 - 2026-07-21

**Final build:** `7IN-20260721-PRODUCT21-TRACKED-HEADING`  
**Commit:** [`5d5b0b6`](https://github.com/bcarriveau/esp-aircraft-radar/commit/5d5b0b62cd828349b1120b1be9e24c8bb98cd6e9)  
**Status:** Combined GitHub record; retained and refined by later firmware

Git history preserves Products 19-21 as one combined Product 21 commit. The
exact boundary of every individual Product is therefore not reconstructed.

### Confirmed changes

- Removed the 160-mile range and limited choices to 20, 40, and 80 miles.
- Added predictive outward auto-zoom for tracked aircraft.
- Added a full-width popup keyboard for Setup fields.
- Prevented aircraft identifiers from wrapping.
- Removed the redundant upper-left radar status overlay.
- Changed the left aircraft panel to tracked-aircraft information while tracking.
- Added the rotating heading arrow and heading value.
- Kept the tracked panel active while fresh data temporarily omitted the tracked
  aircraft.
- Opened details for whichever aircraft the left panel represented.

### Product-specific evidence

- Repository documentation explicitly identifies Product 20 as the revision that
  removed the redundant radar status overlay.
- The preserved build marker and commit identify Product 21 as the
  tracked-heading candidate.
- Other exact Product 19/20 boundaries are not claimed without a standalone
  authoritative commit.

### Verification

- Product 18 remained the physically confirmed TLS baseline when this combined
  UI commit was created.
- These UI features were inherited by Products 22 and 23, which later completed
  compile and physical update testing.

## Product 18 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT18-CERT-BUNDLE`  
**Commit:** [`69dce61`](https://github.com/bcarriveau/esp-aircraft-radar/commit/69dce612211326a4a41f0f66becc8eb7d46191f9)  
**Status:** Physically working native TLS baseline

### Fixed

- Attached Espressif's full CA certificate bundle to the native HTTPS client.
- Kept hostname verification enabled.
- Corrected the Product 17 configuration that failed locally before a network
  TLS handshake because no server-verification method was configured.
- Added a secure fallback HTTPS path.
- Reduced unnecessary Wi-Fi reconnect churn.

### Preserved

- Core-0 HTTPS ownership.
- Response-size guard and PSRAM payload.
- Request deadlines and range-generation rejection.
- Single-snapshot publication.
- Failure-stage diagnostics.

### Verification

- Compile and link passed.
- Initial physical TLS testing passed.
- Product 18 became the confirmed TLS baseline used for Products 19-21.

## Product 17 - 2026-07-21

**Standalone commit:** Not preserved  
**Status:** Documented precursor to Product 18

The Product 18 commit explicitly records the Product 17 work:

### Changed

- Replaced Arduino `NetworkClientSecure` plus the hand-written HTTP parser with
  ESP-IDF's native streaming HTTPS client.
- Re-resolved DNS and created a fresh native client for each retry.
- Kept HTTPS work on the existing core-0 network task.
- Preserved deadlines, response-size guards, PSRAM payload storage,
  generation rejection, and single-snapshot publication.
- Added detailed native error, socket errno, RSSI, and TCP-versus-TLS
  diagnostics.

### Known issue

- The first physical test failed before the network handshake because the native
  client had no configured server-verification method.
- Product 18 corrected that configuration with the CA certificate bundle.

No standalone Product 17 commit or final Product 17 build marker is claimed.

## Product 16 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT16-TLS-STABLE`  
**Commit:** [`c81b34e`](https://github.com/bcarriveau/esp-aircraft-radar/commit/c81b34e5d640e734723964f895c9bb4ec49c1af8)  
**Status:** Superseded by native HTTPS Products 17-18

### Changed

- Used the already resolved server IP for TCP while retaining the hostname for
  TLS SNI.
- Increased the TLS handshake allowance from 10 to 20 seconds.
- Logged exact mbedTLS error codes and descriptions.
- Recycled Wi-Fi only for Wi-Fi, DNS, or TCP failures.
- Retried TLS, HTTP, response-body, and JSON failures without deliberately
  disconnecting a healthy Wi-Fi station.

### Verification

- Complete compile and link passed.
- Physical testing still encountered TLS timeouts, leading to the native HTTPS
  work documented as Product 17.

## Product 15 - 2026-07-21

**Build:** `7IN-20260721-PRODUCT15-HARDENED`  
**Commit:** [`b2a0a49`](https://github.com/bcarriveau/esp-aircraft-radar/commit/b2a0a492c424cf192edf71eb7f5dd496ec0bbab8)  
**Tag:** `product-15-hardened`  
**Status:** First hardened version-controlled and rollback baseline

### Added

- Modular PlatformIO/C++ and LVGL firmware for the exact Waveshare
  ESP32-S3-Touch-LCD-7.
- Core-0 ADS-B networking and Wi-Fi recovery ownership.
- True 15-second start-to-start polling.
- Non-overlapping requests.
- Explicit connect, header, body-idle, and total-response deadlines.
- Separate Wi-Fi, DNS, TCP, TLS, HTTP, response-body, JSON, and stale-response
  failure classification.
- Request generations for range and location changes.
- Rejection of obsolete responses.
- Last-good aircraft retention through temporary failures.
- Thread-safe single-snapshot publication and radar rendering.
- Collision-aware 20-mile aircraft labels.
- Stable ICAO-based manual tracking.
- Conditional STOP TRACK control.
- Setup validation and protected reset behavior.
- System diagnostics for build, network, fetch, memory, and aircraft counts.
- Explicit aircraft categories and correct unknown-artwork fallback.
- Private configuration example with `include/config.h` excluded from Git.

### Display and memory baseline

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM framework.
- OPI PSRAM with `BOARD_HAS_PSRAM`.
- Existing Waveshare RGB timing.
- DMA and anti-rolling protections.
- 20-scanline RGB bounce buffer.

### Verification

- Compile and link passed.
- The initial commit still listed physical display and long-term soak testing as
  pending.
- Product 15 was retained as the permanent hardened rollback baseline while
  later transport and UI work proceeded.

---

## History boundary

The GitHub Product history begins at Product 15.

The Product 15 source states that it preserved the proven RGB anti-rolling
configuration from Product 14, but the repository does not contain an
authoritative Product 1-14 history. Those releases are intentionally omitted
rather than reconstructed from memory, old chats, or uncertain files.
