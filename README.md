# Bill's Aircraft Radar

A dedicated 7-inch ESP32-S3 ADS-B aircraft radar display built with PlatformIO,
Arduino C++, and LVGL.

This repository targets one exact device:

- **Board:** Waveshare ESP32-S3-Touch-LCD-7
- **Display:** 7-inch 800x480 RGB LCD with ST7262 controller
- **Touch:** GT911 capacitive touchscreen
- **I/O expander:** CH422G
- **Processor:** ESP32-S3
- **Memory:** OPI PSRAM with XIP enabled
- **Framework:** Arduino-ESP32 3.0.7 high-performance build
- **UI:** LVGL 8.3.11

It is not compatible with the Waveshare 7B, 7C, P4, generic 7-inch panels,
Cheap Yellow Display hardware, ESPHome, or e-paper projects.

## Current status

Current source build marker:

```text
7IN-20260725-PRODUCT34-TRACK-LOSS-RECOVERY
```

Current source branch:

```text
main
```

The hardened version-controlled baseline is **Product 15**, with the recommended
tag:

```text
product-15-hardened
```

Product 34 is the current release candidate. It adds confirmed tracked-aircraft
loss recovery on top of Product 33 R5 and includes the local PlatformIO workspace
and cppcheck configuration. Product 34 remains a candidate until the complete
PlatformIO build, physical tracking-loss test, normal ADS-B operation, and soak
testing are confirmed.

## Features

### Live radar

- Displays all retained aircraft as radar contacts.
- Provides 20, 40, and 80 mile radar ranges.
- Uses the compact radar selector as the only range control.
- Shows themed contact tags at the 20-mile range.
- Uses stable ICAO hex identity for contact taps, selection, and tracking.
- Prioritizes tracked and selected tags during collision-aware label placement.
- Keeps selected aircraft visible with amber styling.
- Keeps tracked aircraft visible with red styling.
- Automatically zooms outward when a tracked aircraft approaches the edge.
- Presents aircraft speed in MPH.

### Radar interaction

Idle state:

- Left panel shows aircraft count, nearest-aircraft information, and data status.
- Right panel shows the nearest five aircraft.
- Tapping the nearest aircraft, a radar contact, or a radar tag selects it.

Selected state:

- Selected details take priority in the right panel.
- INFO opens aircraft details and returns to Radar.
- TRACK begins stable ICAO-based tracking.
- CLEAR removes the selection.
- The left panel shows up to three nearest other aircraft.
- The nearest-other heading reflects the number of populated rows:
  `NO OTHER`, `NEAREST 1`, `NEAREST 2`, or `NEAREST 3`.

Tracked state:

- Tracked details take priority in the right panel.
- STOP TRACK is contained in the tracked-aircraft card.
- The radar tag shows TRACKED, the aircraft identifier, and MPH.
- The left panel shows the nearest other aircraft without replacing the tracked
  aircraft.
- Stopping tracking preserves selection when practical.
- One or two successful snapshots that temporarily omit the tracked ICAO retain
  tracking.
- During that grace period, the right card reports `TRACK SIGNAL LOST` and checks
  the next update.
- Three consecutive successful current-generation snapshots without the tracked
  ICAO automatically clear tracking and return the radar to normal idle mode.
- Failed requests and stale discarded responses do not advance the lost-track
  counter.

### Additional pages

- **Tracks:** Aircraft table with details and explicit return-to-Tracks behavior.
- **Airspace:** Live totals, range metrics, aircraft-category cards, and traffic
  highlights.
- **Setup:** Display name, Wi-Fi, location, and device controls.
- **System:** Build, network, memory, diagnostics, and project identification.

## Architecture and reliability

### ADS-B networking

- All ADS-B requests and Wi-Fi recovery run in one core-0 network task.
- Requests cannot overlap.
- Polling uses a 15-second start-to-start cadence.
- The native ESP-IDF HTTPS path retains the configured secure fallback behavior.
- The firmware does not use blocking `HTTPClient::GET()`.
- Connect, header, body-idle, and total-response deadlines are bounded.
- Wi-Fi, DNS, TCP, TLS, HTTP, body, and JSON failures are classified separately.
- Recovery escalates from retry to reconnect, radio recycle, and last-resort
  restart.
- The last good aircraft snapshot remains visible during temporary failures.

### Request and state safety

- Range and location changes increment a request generation.
- Obsolete responses cannot overwrite a newer range or location selection.
- Radar rendering uses one coherent target snapshot per frame.
- LVGL labels and page content update only when their underlying versions change.
- Aircraft selection and tracking use stable ICAO hex values, never array
  positions.
- Tracked-aircraft loss is evaluated only when a successful current-generation
  target snapshot is published.
- A returned tracked ICAO resets the loss counter immediately.
- Manual STOP TRACK and starting a new tracked aircraft reset the loss counter.
- Tracking is cleared atomically under the existing app-state mutex after three
  consecutive confirmed misses.

### Target capacity

- Storage is bounded at 200 retained targets.
- Capacity-scaled target arrays and radar metadata use required PSRAM.
- Retention remains nearest-first while preserving the tracked aircraft when it
  is returned.
- Diagnostics distinguish received, eligible, stored, capacity-dropped, and
  visible aircraft counts.
- Array allocation sizes, bounds checks, counters, and indexes must remain tied
  to the same capacity constant.

### Display stability

The display configuration intentionally preserves:

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM framework
- OPI PSRAM and `BOARD_HAS_PSRAM`
- Existing Waveshare RGB timing
- DMA and anti-rolling protections
- 20-scanline RGB bounce buffer

Do not replace the framework, change panel timing, or reduce the bounce buffer
without a dedicated display-stability investigation.

## Repository layout

```text
assets/                 Aircraft artwork and display assets
include/                Shared interfaces and hardware configuration
src/                    Application, networking, state, radar, and UI sources
platformio.ini          Pinned PlatformIO environment, workspace, and checks
README.md               Current project documentation
CHANGELOG.md            Confirmed Product history
```

Private credentials belong only in `include/config.h`. That file must remain
ignored and must never be committed, uploaded, or included in replacement ZIPs.

## Setup

### 1. Install the tools

Install:

- Visual Studio Code
- PlatformIO extension
- Git

### 2. Clone the repository

```bash
git clone https://github.com/bcarriveau/esp-aircraft-radar.git
cd esp-aircraft-radar
```

### 3. Create the private configuration

Copy the example file:

```bash
cp include/config.example.h include/config.h
```

Edit `include/config.h` with the local Wi-Fi credentials and radar-center
coordinates.

Never commit this file.

### 4. Build

PlatformIO environment:

```text
waveshare-s3-touch-lcd-7
```

Command-line build:

```bash
pio run -e waveshare-s3-touch-lcd-7
```

The project pins:

- pioarduino platform-espressif32 51.03.07
- Arduino-ESP32 3.0.7 high-performance libraries
- ESP32_Display_Panel 0.1.4
- ESP32_IO_Expander 0.0.3
- LVGL 8.3.11
- ArduinoJson 7.3.1

Generated objects and downloaded project libraries are stored outside the
Google Drive project directory:

```text
~/.platformio/workspaces/bills_aircraft_radar
```

This avoids Drive File Stream locking or removing `.pio` files while SCons is
building.

### 5. Static inspection

Run:

```bash
pio check -e waveshare-s3-touch-lcd-7
```

The project passes `cppcheck: --skip-packages`, which keeps static analysis on
the application sources while skipping downloaded framework, toolchain, and
library packages. This avoids false syntax failures in package headers such as
`bits/c++config.h` and does not alter normal compilation, linking, or upload.

### 6. Upload and monitor

```bash
pio run -e waveshare-s3-touch-lcd-7 -t upload
pio device monitor -b 115200
```

The programming USB-C port is normally used for upload and Serial Monitor.

## Expected startup checks

Confirm the serial log reports:

- The intended Product build marker
- PSRAM detected
- App-state, radar, UI, and ADS-B buffers allocated in PSRAM
- The core-0 ADS-B task started
- Native HTTPS or the configured fallback connected successfully
- Aircraft received, eligible, stored, dropped, and published counts
- No immediate TLS memory-allocation failure
- No display rolling during HTTPS activity

## Verification checklist

Source review, compilation, physical testing, and soak testing are separate
verification stages.

Before calling a Product release complete:

1. Compile and link the full PlatformIO project.
2. Record flash and internal RAM usage.
3. Confirm the build marker, OPI PSRAM, and 20-scanline bounce buffer.
4. Test normal 15-second updates and 20/40/80 mile range changes.
5. Test selection, INFO, BACK, TRACK, STOP TRACK, CLEAR, and page navigation.
6. Track an aircraft and confirm one or two successful omitted snapshots retain
   tracking.
7. Confirm the third consecutive successful omitted snapshot clears tracking and
   restores the idle nearest-aircraft view.
8. Confirm failed requests and stale discarded responses do not clear tracking.
9. Interrupt Wi-Fi or the router and confirm recovery.
10. Change range during an active request and confirm stale-response rejection.
11. Check display stability during HTTPS traffic.
12. Check heap and PSRAM stability.
13. Complete an extended soak test.

## Screenshots

Current Product 33 UI photographs have been used for physical layout review but
are not yet committed under a documentation image directory. Add verified
current-display photos under `docs/images/` after Product 34 behavior is
physically confirmed.

## Major milestones

- **Product 15:** First hardened modular baseline with core-0 networking,
  generation-safe publishing, last-good retention, diagnostics, and the proven
  RGB anti-rolling configuration.
- **Products 16-18:** HTTPS transport and certificate-bundle corrections that
  established the native TLS baseline.
- **Products 19-21:** Tracking panel, rotating heading display, outward
  auto-zoom, keyboard, and radar UI refinements.
- **Products 22-25:** Large-response handling, heading crash correction,
  transport-recovery hardening, and quiet first-run NVS defaults.
- **Product 26:** Themed 20-mile radar tags, canvas hit testing, temporary
  selection, and compact radar range controls.
- **Products 27-29:** Idle/selected/tracked panel cleanup, stable ICAO list
  actions, detail origin, correct tab return behavior, and selection timeout
  fixes.
- **Product 30:** Bounded 200-target capacity with target and radar working
  buffers moved to PSRAM.
- **Product 31:** Aircraft-type side-panel bitmaps and corrected
  nearest-aircraft heading arrow.
- **Product 32:** Airspace dashboard, nearest-other aircraft rows, streamlined
  Setup page, and project credit card.
- **Product 33:** UI fit, spacing, heading, highlight-card, and dynamic nearest
  count cleanup.
- **Product 34:** Confirmed tracked-aircraft loss recovery with a three-successful-
  update grace period, temporary lost-signal messaging, and automatic return to
  idle mode. The accompanying PlatformIO update isolates generated files from
  Google Drive and skips third-party package headers during cppcheck.

Detailed Product-by-Product history is maintained in `CHANGELOG.md`. Unconfirmed
older history is not guessed.

## License and data source

Add the repository's chosen license before distributing the firmware.

ADS-B data availability and permitted use remain subject to the selected data
provider's terms and service availability.
