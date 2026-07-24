Product 30 — 200-target PSRAM correction

Baseline
--------
GitHub main commit:
  190b056737e7cc9c8e6da412dd6585e8e74c5f76 (Product 29)

The Product 29 ui.cpp, radar_renderer.cpp, radar_renderer.h, and build_info.h
used here match that GitHub commit. The rejected Product 30 files are the exact
files previously supplied and flashed by Bill.

Build marker
------------
7IN-20260723-PRODUCT30-200-TARGET-PSRAM

Replace these files
-------------------
include/aircraft_data.h
include/adsb_fetch.h
include/app_state.h
include/build_info.h
include/radar_renderer.h
src/adsb_fetch.cpp
src/adsb_network.cpp
src/app_state.cpp
src/main.cpp
src/radar_renderer.cpp
src/ui.cpp

Confirmed Product 30 regression
-------------------------------
The rejected Product 30 booted with approximately 49–51 KB free internal heap.
Both native mbedTLS and WiFiClientSecure immediately failed with -0x7F00:
SSL memory allocation failed. PSRAM still had about 4.44 MB free.

Correction
----------
- Keeps the bounded 200-target capacity.
- Keeps received, eligible, stored, dropped, and visible diagnostics.
- Keeps nearest-first retention and forced tracked-ICAO retention.
- Moves the authoritative shared target array to required PSRAM.
- Keeps UI and core-0 incoming target arrays in required PSRAM and removes
  their internal-RAM fallbacks.
- Moves radar hit regions, screen contacts, and label collision boxes to
  required PSRAM.
- Frees partial radar allocations and fails startup cleanly if any required
  PSRAM allocation fails.
- Prevents loop() from running UI/network services after an incomplete startup.
- Logs the largest free internal block before the ADS-B task begins.

Estimated storage effect
------------------------
Using the current structure sizes:
- Removes about 32,016 bytes of capacity-scaled static/internal storage compared
  with the rejected Product 30.
- Uses about 80,016 bytes of PSRAM for the three 200-target arrays and radar
  metadata.
The PlatformIO link report and physical startup log remain authoritative.

Explicitly unchanged
--------------------
- MAX_TARGETS remains 200.
- ADS-B parsing, filtering, sorting, tracked retention, and diagnostics.
- Radar center, radius, projection, bearing, request radius, and range math.
- Native HTTPS/fallback behavior, deadlines, recovery ladder, and 15-second
  polling cadence.
- Range generations, stale-response rejection, and last-good retention.
- Product 29 UI state/navigation behavior.
- XIP/OPI PSRAM framework, panel timing, DMA, and 20-scanline bounce buffer.
- include/config.h is not included or modified.

Review and verification performed
---------------------------------
- Verified current GitHub main is Product 29.
- Verified local Product 29 UI/radar file blob hashes match GitHub.
- Verified no MAX_TARGETS-sized fixed arrays remain in the changed source.
- Verified no internal-RAM fallback remains for target/radar capacity buffers.
- Verified adsb_fetch.cpp is byte-for-byte unchanged from rejected Product 30.
- Verified no HTTPClient::GET() path was introduced.
- App-state 200-target publish/snapshot/tracking test passed.
- Radar 200-contact render/hit test passed under AddressSanitizer and UBSan.
- Partial radar PSRAM-allocation failure and retry test passed.
- App-state PSRAM-allocation failure test passed.
- Syntax checks passed for app_state.cpp, radar_renderer.cpp, main.cpp, and the
  exact new UI/network allocation blocks.

Still required before calling it complete
-----------------------------------------
1. Run the full PlatformIO compile/link.
2. Confirm the Product 30 marker, OPI PSRAM, and 20-line bounce buffer.
3. Record flash and internal RAM usage.
4. Flash and confirm the startup log includes all four PSRAM allocations.
5. Check "ADSB memory ready" and "Heap before request" values.
6. Confirm TLS connects without immediate -0x7F00 allocation failure.
7. Verify received/eligible/stored/dropped/visible counts at 20/40/80 miles.
8. Test tracked-aircraft retention, recovery behavior, screen rolling, and soak.
