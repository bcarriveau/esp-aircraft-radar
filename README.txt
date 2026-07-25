Product 33 R2 — nearest heading and Airspace fit correction

This package REPLACES the first Product 33 package.

Baseline
--------
Exact Product 33 files previously supplied to Bill:
  src/ui.cpp
  include/build_info.h

GitHub main was checked before packaging and currently contains Product 32:
  e289b839528a2aced7d4b9b18fa430cb9faf9d17

Build marker
------------
7IN-20260724-PRODUCT33-UI-POLISH-R2

Replace these files
-------------------
src/ui.cpp
include/build_info.h

Corrections
-----------
- Enlarges NEAREST 3 from Montserrat 12 to Montserrat 14.
- Moves the heading up 2 pixels and gives it an explicit clipped width.
- Enlarges the Airspace LIVE HIGHLIGHTS card from 224x176 to 224x200.
- Enlarges its content viewport from 200x136 to 200x160.
- Removes the up/down scrolling behavior completely.
- Removes all Airspace scroll timers, offsets, direction state, and update work.
- Keeps all four highlight sections visible at once.
- Leaves the improved Product 33 Setup layout unchanged.

Bounds checked
--------------
- Airspace dashboard height: 270 pixels.
- Highlights card: y=68, height=200, bottom=268.
- Highlights viewport: y=28, height=160, bottom=188 inside a 200-pixel card.

Explicitly unchanged
--------------------
- Setup field positions, padding, colors, buttons, and keyboard behavior.
- Product 32 radar selector position and MILES caption.
- Selected/tracked nearest-other ICAO behavior.
- System credit card.
- Airspace metrics and aircraft-category cards.
- Networking, TLS, Wi-Fi recovery, polling, and stale-response rejection.
- 200-target capacity and PSRAM target-buffer ownership.
- Radar center, radius, projection, range math, dots, and hit testing.
- Panel driver, DMA, XIP/OPI PSRAM, and 20-scanline bounce buffer.
- include/config.h is not included or modified.

Verification performed
----------------------
- Strict diff confirms only ui.cpp and build_info.h changed from Product 33.
- Setup construction and styling code is byte-for-byte unchanged.
- ui.cpp passed clang++ C++17 syntax checking with warnings treated as errors.
- Verified no Airspace scroll function, timer, offset, or direction reference
  remains.
- Verified the enlarged card remains inside the existing dashboard bounds.

Still required
--------------
1. Run the complete PlatformIO compile/link.
2. Confirm the Product 33 R2 marker.
3. Compare flash and internal RAM with Product 33.
4. Flash and visually confirm:
   - NEAREST 3 is larger but not clipped.
   - all LIVE HIGHLIGHTS content is visible at once.
   - no Airspace movement occurs.
   - Setup remains unchanged.
5. Confirm normal ADS-B/TLS operation.
