Product 31 — corrected nearest-aircraft heading arrow

This package REPLACES the earlier Product 31 package.

Baseline
--------
Product 30 commit:
  1ccdc82ed379b969b60963190fb89a9e68f8498d

Product 31 keeps the side-panel aircraft bitmaps and adds the intended
nearest-aircraft heading arrow.

Build marker
------------
7IN-20260723-PRODUCT31-NEAREST-HEADING-ARROW

Replace these files
-------------------
src/ui.cpp
src/radar_renderer.cpp
include/radar_renderer.h
include/build_info.h

Correction
----------
- Adds a rotating heading arrow to the left idle nearest-aircraft card.
- Uses the same arrow geometry as the selected/tracked aircraft display.
- Shows:
    HDG
    273 W
- Shows HDG / -- and hides the arrow when track data is unavailable.
- Removes the redundant plain-text heading line from the left summary.
- Keeps the small aircraft-type bitmap.
- Arrow and heading-label taps select the same stable ICAO aircraft.
- Hides the complete left nearest block while selected/tracked details have
  priority on the right.

Retained Product 31 changes
---------------------------
- Right title remains NEAREST 5 AIRCRAFT.
- Left, right-list, and selected/tracked aircraft-type bitmaps remain.
- Seven fixed side-icon buffers remain in PSRAM.

Explicitly unchanged
--------------------
- MAX_TARGETS remains 200.
- Product 30 target retention, diagnostics, and PSRAM target buffers.
- ADS-B networking, native HTTPS/fallback, TLS, recovery, polling, deadlines,
  range generations, and stale-response rejection.
- Radar center, radius, projection, range math, dots, labels, and hit testing.
- Selected/tracked state behavior, INFO/BACK, STOP TRACK, and tab behavior.
- Panel driver, DMA, XIP/OPI PSRAM, and 20-scanline bounce buffer.
- include/config.h is not included or modified.

Verification performed
----------------------
- Strict comparison confirmed only the four listed files changed from the first
  Product 31 package.
- radar_renderer.cpp passed clang++ syntax checking.
- 200-target rendering with both independent heading-arrow point arrays passed
  AddressSanitizer and UndefinedBehaviorSanitizer.
- The exact new left-arrow construction/click-wiring block passed
  AddressSanitizer and UndefinedBehaviorSanitizer.
- Separate persistent point arrays are used so updating the left arrow cannot
  alter the selected/tracked arrow.

Still required
--------------
- Run the full PlatformIO compile/link.
- Confirm the Product 31 marker.
- Flash and visually check the left arrow at several headings.
- Confirm TLS and Product 30 memory behavior remain unchanged.
