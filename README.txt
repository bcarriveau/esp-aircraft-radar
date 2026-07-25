Product 33 R5 — longitude spacing and dynamic nearest count

This package REPLACES Product 33 R4. Bill had not applied R4 yet, so this ZIP
contains both the R4 longitude-spacing correction and the new dynamic nearest
count.

Baseline
--------
Exact Product 33 R4 files previously supplied to Bill, derived from the
physically tested Product 33 R3 source.

Build marker
------------
7IN-20260724-PRODUCT33-UI-POLISH-R5

Replace these files
-------------------
src/ui.cpp
src/radar_renderer.cpp
include/build_info.h

Changes
-------
SETUP
- Keeps the R4 correction:
  - LONGITUDE label remains in place.
  - Longitude input moves 12 pixels right.
  - Input width is reduced by 12 pixels.
  - The input right edge remains aligned.
  - The label-to-input gap now resembles the LATITUDE row.

SELECTED / TRACKED LEFT PANEL
- Changes the heading to match the actual number of other aircraft displayed:
    NO OTHER
    NEAREST 1
    NEAREST 2
    NEAREST 3
- Only populated aircraft rows are shown.
- Empty labels, icons, and ICAO slots remain hidden/cleared.
- With more than three other aircraft, the bounded list remains NEAREST 3.
- Stable ICAO selection/detail behavior is unchanged.

Explicitly unchanged
--------------------
- Product 33 R3 NEAREST heading font and placement.
- Product 33 R3 taller static Airspace LIVE HIGHLIGHTS card.
- All other Setup styling, padding, buttons, and keyboard behavior.
- Product 32 radar selector position and MILES caption.
- System credit card.
- Networking, TLS, Wi-Fi recovery, polling, deadlines, stale-response
  rejection, and last-good retention.
- 200-target capacity and Product 30 PSRAM target-buffer ownership.
- Radar center, radius, projection, bearing, request radius, range math, dots,
  tags, and hit-test priority.
- Waveshare panel timing, DMA, XIP/OPI PSRAM, and 20-scanline bounce buffer.
- include/config.h is not included or modified.

Verification performed
----------------------
- Strict R4-to-R5 diff confirms:
  - ui.cpp is unchanged from R4.
  - radar_renderer.cpp contains only the dynamic heading-count logic.
  - build_info.h contains only the marker update.
- ui.cpp and radar_renderer.cpp passed C++17 syntax checking.
- AddressSanitizer and UndefinedBehaviorSanitizer test passed for:
    0 other aircraft -> NO OTHER
    1 other aircraft -> NEAREST 1
    2 other aircraft -> NEAREST 2
    3 other aircraft -> NEAREST 3
- Existing row bounds remain fixed at three.
- Existing ICAO buffers remain six characters plus null termination.

Still required
--------------
1. Run the complete PlatformIO compile/link.
2. Confirm the Product 33 R5 build marker.
3. Flash and visually check:
   - LONGITUDE label/input spacing.
   - NO OTHER / NEAREST 1 / NEAREST 2 / NEAREST 3 behavior.
   - empty rows remain hidden.
4. Confirm normal ADS-B/TLS operation.
