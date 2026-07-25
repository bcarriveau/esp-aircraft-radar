Product 32 — radar UI dashboard refinement

Baseline
--------
GitHub repository:
  bcarriveau/esp-aircraft-radar

Branch:
  main

Product 31 commit:
  8ec682663805b7b947f48ce9b743418f28f9a4ac

The Product 31 file blob hashes used as the baseline match GitHub:
  src/ui.cpp                  e310b86308085cc555d4cf35fcdf389ee5b0042f
  src/radar_renderer.cpp      c8f1392871a80ac926fbc9031b057ee7b252646b
  include/radar_renderer.h    b47542808bd2b8e3e780e3b950fbc88c5cf28b89
  include/build_info.h        5e10d8fca26f184b30f30c5792e5003364137903

The uploaded "New folder.zip" contained serial logs only, not newer source
files. No local Git working-tree status was available to inspect.

Build marker
------------
7IN-20260724-PRODUCT32-UI-DASHBOARD

Replace these files
-------------------
src/ui.cpp
src/radar_renderer.cpp
include/radar_renderer.h
include/build_info.h

Changes
-------
SETUP
- Removes the duplicate Setup-page 20/40/80 range controls completely.
- Renames the page header to SETUP // DEVICE & NETWORK.
- The radar-page 20/40/80 selector remains the only range control.

RADAR RANGE CONTROL
- Moves the compact selector 4 pixels right and 4 pixels down.
- Adds a muted MILES caption immediately above its upper-right corner.
- Updates the radar-label exclusion rectangle to match the visible control and
  caption.
- Does not change radar center, radius, projection, bearing, request radius, or
  range filtering.

SELECTED / TRACKED LEFT PANEL
- Uses the formerly empty left-panel space for three nearest other aircraft.
- Excludes the selected or tracked aircraft from this compact list.
- Each row shows an aircraft-type bitmap, stable identifier, distance, and
  compass direction.
- Rows use stable ICAO hex identity.
- While selected, tapping a row selects that aircraft.
- While tracking, tapping a row opens its details without changing tracking.
- Aircraft count and DATA STATUS remain in their existing positions.

AIRSPACE
- Replaces the paragraph-only page with a visual dashboard.
- Adds top metrics for total aircraft, within 20 miles, within 40 miles, and
  current range.
- Adds bitmap cards for airliners, business jets, turboprops, piston aircraft,
  helicopters, and military/other traffic.
- Each category card shows count and percentage.
- Adds live highlights for nearest, fastest, lowest airborne, and dominant
  category.

SYSTEM CREDIT
- Adds a restrained project-identification card:
    DESIGNED & BUILT BY
    BILL CARRIVEAU
    ESP32-S3 ADS-B AIRCRAFT RADAR
    PLATFORMIO / C++ / LVGL

MEMORY
- Expands fixed side-icon PSRAM storage from 7 buffers to 16 buffers.
- Product 32 expected side-icon allocation:
    17,024 bytes PSRAM
- Increase over Product 31:
    9,576 bytes PSRAM
- Additional LVGL dashboard objects use runtime UI memory; the physical startup
  log remains authoritative for internal heap and largest-block headroom.

Explicitly unchanged
--------------------
- MAX_TARGETS remains 200.
- Product 30/31 PSRAM target-buffer ownership.
- ADS-B parsing, nearest retention, tracked retention, and diagnostics.
- Native HTTPS/fallback, Wi-Fi recovery, core-0 ownership, deadlines, polling,
  stale-generation rejection, and last-good retention.
- Radar center, radius, projection, bearing, request radius, and range math.
- Radar dots, selected/tracked tag priority, and canvas hit-test priority.
- Product 29/31 INFO/BACK, selection timeout, tracking, STOP TRACK, and tab
  behavior.
- Arduino-ESP32 3.0.7 XIP/OPI PSRAM framework.
- Waveshare panel timing, DMA, anti-rolling protections, and 20-scanline bounce
  buffer.
- include/config.h is not included or modified.

Verification performed
----------------------
- Strict diff review confirmed only the four listed UI/rendering files changed.
- Removed Setup range objects and all references to them.
- Checked all 16 icon-buffer indexes and compile-time allocation bounds.
- Checked three compact-row ICAO buffers for copy size and null termination.
- ui.cpp and radar_renderer.cpp passed clang++ C++17 syntax checks with warnings
  treated as errors.
- UI allocation/build/index test passed under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Radar tests passed under AddressSanitizer and UndefinedBehaviorSanitizer for:
    0 targets
    1 target
    200 targets
    selected aircraft outside the nearest three
    tracked aircraft at the nearest position
    nearest-other exclusion and ordering
- No blocking HTTPClient::GET() path was introduced.
- No networking, capacity, transport, panel-driver, or PSRAM target-storage
  source was changed.

Still required
--------------
1. Run the complete PlatformIO compile and link.
2. Report flash and internal RAM usage.
3. Confirm the Product 32 build marker.
4. Confirm startup reports:
     Radar side-icon buffers in PSRAM: 17024 bytes
5. Compare ADSB memory ready / Heap before request against Product 31.
6. Flash and visually check:
   - Setup has no duplicate range controls
   - selector spacing and MILES caption
   - selected and tracked nearest-other rows
   - stable row taps
   - Airspace card spacing and bitmap clarity
   - System credit-card placement
7. Confirm normal TLS polling, range switching, stale-response rejection,
   tracking auto-zoom, screen stability, and soak behavior.
