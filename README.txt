Product 29 — UI state fixes

Baseline:
Product 28 files previously supplied and flashed by Bill.

Copy these replacement files into the matching repository paths:

  src/ui.cpp
  src/radar_renderer.cpp
  include/radar_renderer.h
  include/build_info.h

Build marker:
  7IN-20260723-PRODUCT29-UI-STATE-FIXES

Changes:
- Corrects the radar label-box bound so allocation and bounds checks both use
  MAX_TARGETS + 2.
- Shows nearest-aircraft information on the left only while idle.
- Hides the left nearest block while a selected or tracked aircraft has priority
  in the right panel.
- Keeps STOP TRACK in the right tracked-aircraft card.
- Uses stable rendered ICAO hex values for the left nearest card and right
  nearest-aircraft list taps.
- Makes only the nearest-aircraft text area clickable on the left; aircraft
  count and data status no longer trigger selection.
- Adds an explicit Radar/Tracks detail origin.
- INFO opened from Radar keeps the Radar tab active and BACK returns to Radar.
- Details opened from Tracks keep the Tracks tab active and BACK returns to
  Tracks.
- Pauses radar rendering and selected-aircraft timeout while the detail page is
  open, then refreshes the selection timer on return.
- Closes the detail overlay cleanly when another navigation tab is selected.

Explicitly unchanged:
- MAX_TARGETS remains 100.
- ADS-B parsing, retention, request radius, range filtering, networking, TLS,
  Wi-Fi recovery, and 15-second polling are unchanged.
- Radar center, radius, projection, bearing math, contact rendering, and hit-test
  priority are unchanged.
- PSRAM/XIP, panel timing, DMA, and the 20-scanline bounce buffer are unchanged.
- include/config.h is not included or modified.

Review performed:
1. Scope/non-regression review:
   drawRadarBackground(), drawContacts(), render(), hitTest(), and
   updateHeadingDisplay() were verified unchanged from Product 28.
2. Bounds/identity review:
   label allocation/check constants, 7-byte ICAO buffers, null termination, and
   five-entry list indexes were checked.
3. State-flow review:
   Radar/Tracks detail origin, BACK behavior, tab state, timeout pause/refresh,
   and left/right visibility transitions were checked.
4. Syntax-only C++ review:
   src/ui.cpp and src/radar_renderer.cpp passed clang++ syntax checking against
   local interface stubs. This is not a full PlatformIO firmware build.

Still required:
- Run the complete PlatformIO compile/link locally.
- Confirm flash/internal RAM usage, Product marker, PSRAM, and 20-line bounce
  buffer.
- Flash and physically test idle/selected/tracked states, INFO/BACK from Radar,
  Details/BACK from Tracks, STOP TRACK, range controls, and HTTPS screen
  stability.
