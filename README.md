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
7IN-20260801-PRODUCT56-R4-HA-SYSTEM-UI
```

Current intended repository branch:

```text
main
```

The hardened version-controlled baseline is **Product 15**, with the recommended
tag:

```text
product-15-hardened
```

Product 56 R4 is the current host-verified candidate. It retains the optional Home
Assistant device, display/range/refresh controls, bounded aircraft telemetry, and the
lightweight MQTT transport proven stable in the Product 56 R3 hardware soak. MQTT remains
disabled by default and creates no client, broker traffic, aircraft snapshot buffer, or
JSON buffer until configured and enabled from the System page.

Product 56 R1 and R2 hardware logs proved that the native ESP-MQTT task, socket state,
and persistent allocations fragmented internal RAM enough that later preferred ADS-B
TLS handshakes failed even when total free heap remained above 50 KB. R3 replaced only
that optional transport with task-free PubSubClient 2.8 and then completed repeated
native ADS-B TLS cycles without allocation failures or fragmentation buildup. R4 removes
the redundant Home Assistant last-update-age entity and reorganizes the System page so
the status card is taller and the four maintenance actions use a compact two-row layout.
ADS-B HTTPS, TLS verification, deadlines, recovery, 15-second cadence, OTA, display
framework, and Product 55 interaction behavior are unchanged. Product 56 R4 PlatformIO
compile/link and physical UI validation remain user-side.

## Features

### Live radar

- Displays all retained aircraft as radar contacts.
- Provides 20, 40, and 80 mile radar ranges.
- Shares the same range state between the compact Radar selector and the green
  Airspace `CURRENT RANGE` card.
- Draws configured airport runway/heliport symbols beneath aircraft contacts.
- Supports independent airport symbol and label filters at 20, 40, and 80 miles.
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
- **Airspace:** Live totals, a green 20/40/80-mile range toggle, aircraft-category
  cards, and tappable nearest, fastest, lowest-airborne, and highest-airborne
  shortcuts that select the aircraft on Radar by stable ICAO hex.
- **Airports:** Nearby-airport awareness plus per-category 20/40/80-mile symbol
  and label controls.
- **System:** A tall status card plus compact device/network and two-row maintenance
  cards for build, memory, connectivity, saved settings, retry/reconnect, Home Assistant,
  reset controls, and a dedicated local firmware-update overlay.
- **Home Assistant:** Optional MQTT discovery, backlight/range/refresh controls,
  bounded tracked/nearest/airspace telemetry, diagnostics, and a supplied dashboard
  view using built-in Home Assistant cards only.

## Architecture and reliability

### Airport awareness

- Airport data is offline and does not share the ADS-B transport path.
- A bounded PSRAM cache is built at startup and rebuilt only after a confirmed
  location change.
- Airport settings are persisted with checked NVS writes and cached in RAM for
  rendering.
- Airport identifiers use fixed positions for each radar range and are drawn before
  airport symbols, aircraft contacts, and aircraft tags. Moving traffic can cover
  them but cannot make them jump or blink.
- Airport symbols are drawn below aircraft; aircraft contacts and all aircraft
  labels retain priority.
- Product 50 ships with a southeastern Wisconsin/northern Illinois starter table
  for hardware validation plus `tools/generate_airport_database.py` for generating
  another regional table from the public-domain OurAirports CSV.
- Airport information is awareness-only and must not be used for navigation.

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


### Home Assistant MQTT

- MQTT is disabled by default and its enabled state is stored independently in NVS.
- Disabled mode creates no MQTT client task, makes no broker connection attempts, and
  allocates no aircraft snapshot or JSON buffer.
- When enabled, a task-free PubSubClient 2.8 connection publishes Home Assistant
  discovery, retained availability, bounded state topics, a tracked-aircraft snapshot,
  the five nearest aircraft, and the Product 55 Airspace highlights.
- Home Assistant can switch the physical LCD backlight, select the shared 20/40/80-mile
  radar range, and queue the existing non-overlapping ADS-B refresh command.
- MQTT does not own Wi-Fi, reconnect the station, recycle the radio, alter ADS-B
  recovery, or trigger a restart. Broker failure is nonfatal to the radar.
- MQTT uses a 384-byte internal packet buffer and streams larger retained payloads
  directly from the bounded PSRAM JSON workspace. It has no separate MQTT task or
  persistent outbox.
- MQTT connect, loop, discovery, and state publication are skipped while the ADS-B
  HTTPS task reports a fetch in progress. State publication is limited to one message
  per 250 ms service interval.
- The service allocates one capacity-sized aircraft snapshot buffer and one bounded
  JSON buffer in PSRAM only while enabled and configured.
- `home-assistant/aircraft-radar-view.yaml` provides a ready-to-paste dashboard view
  using only standard Home Assistant cards.

### Local firmware updates

- The local HTTP updater is disabled during normal operation and can be armed from
  the System page for five minutes.
- Each arming generates a temporary six-digit access code and displays both the
  numeric-IP update address and `bills-aircraft-radar.local/update` when mDNS starts.
- PlatformIO automatically creates `firmware.radarota` beside `firmware.bin` after a
  successful build and copies the same package to `release/firmware.radarota` in the
  project folder. USB flashing continues to use `firmware.bin`; browser updates use
  only `firmware.radarota`.
- The package identifies the exact Waveshare ESP32-S3 7-inch hardware, carries the
  Product build marker, declares the firmware length, and includes the firmware
  SHA-256 digest.
- Upload data is streamed directly to the inactive OTA partition. No complete
  firmware copy is allocated in heap or PSRAM.
- The core-0 ADS-B task finishes any active request and acknowledges a maintenance
  hold before the browser can upload. When MQTT is active, its client is also stopped
  and its PSRAM snapshot and JSON buffers are released before OTA becomes ready. A
  failed or cancelled preparation releases both holds and resumes normal operation.
- The next boot partition is selected only after package checks, confirmation that
  the declared Product build ID is embedded in the firmware, ESP32-S3 image validation,
  exact-length validation, SHA-256 verification, and `esp_ota_end()` all succeed.
- Product 54 does not claim signed-firmware authenticity or automatic first-boot
  rollback; those remain separate reliability features.

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
tools/                  Generated-data utilities, including airport CSV conversion
scripts/                PlatformIO post-build OTA package generation
release/                Latest generated OTA package after a successful build
home-assistant/          Built-in-card dashboard view and installation notes
include/                Shared interfaces and hardware configuration
src/                    Application, networking, state, radar, UI, and OTA sources
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
coordinates. To use Home Assistant, also replace the MQTT placeholders with the
local broker URI and credentials. MQTT remains disabled until enabled from the
radar's **System > HA MQTT** panel.

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

A successful build creates the OTA package in the PlatformIO workspace and copies an
identical upload-ready package into the project folder:

```text
~/.platformio/workspaces/bills_aircraft_radar/build/waveshare-s3-touch-lcd-7/firmware.radarota
<project>/release/firmware.radarota
```

Use `firmware.bin` for USB upload and `release/firmware.radarota` for the radar's
local browser update page or as a GitHub Release asset. Attaching the package to a
GitHub Release avoids adding a new multi-megabyte binary revision to normal source
commit history.

### 5. Static inspection

Run:

```bash
pio check -e waveshare-s3-touch-lcd-7
```

The project passes `cppcheck: --skip-packages`, which keeps static analysis on
the application sources while skipping downloaded framework, toolchain, and
library packages. This avoids false syntax failures in package headers such as
`bits/c++config.h` and does not alter normal compilation, linking, or upload.

### 6. Local browser update

Product 54 must first be installed by USB. For later updates:

1. Build the new source and locate `release/firmware.radarota` in the project folder.
2. On the radar, open **System**, tap **Firmware / OTA**, and enable the five-minute
   update window.
3. From another device on the same network, open the displayed address.
4. Enter the six-digit access code, select `firmware.radarota`, and start the update.
5. Keep power connected until the browser reports verification and the radar restarts.

A partial, rejected, or interrupted package is not selected as the next boot image.

### 7. Home Assistant MQTT

1. Configure `MQTT_BROKER_URI`, `MQTT_USERNAME`, and `MQTT_PASSWORD` in private
   `include/config.h`.
2. Confirm Home Assistant's MQTT integration and discovery are enabled.
3. On the radar, open **System**, tap **HA MQTT**, and enable MQTT.
4. Confirm the radar appears as one MQTT device in Home Assistant.
5. Follow `home-assistant/README.md` to install the supplied dashboard view.

Disabling MQTT from the radar stops and destroys the client and releases its PSRAM
snapshot and JSON buffers without disconnecting Wi-Fi or changing ADS-B behavior.

### 8. Upload and monitor

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
13. Confirm both generated `firmware.radarota` files are identical and each is 512
    bytes larger than `firmware.bin`.
14. Test a successful local browser update and confirm the new build marker after
    restart.
15. Confirm wrong-code, wrong-extension, mismatched-build, truncated-package, and
    SHA-mismatch failures leave the current firmware selected and release the ADS-B
    maintenance hold.
16. With MQTT disabled, confirm no broker attempts, MQTT client, or MQTT aircraft
    buffer are reported and normal Product 55 behavior is unchanged.
17. Enable MQTT and confirm Home Assistant discovery, availability, display power,
    shared range control, refresh, tracked/nearest telemetry, and dashboard rendering.
18. Start OTA with MQTT connected and confirm OTA does not become ready until both
    ADS-B and MQTT maintenance holds are active; confirm cancellation resumes both.
19. Complete an extended soak test.

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
- **Product 54:** Temporarily armed local browser OTA with generated hardware-bound
  packages, streamed inactive-slot writes, SHA-256 and ESP image validation, and an
  acknowledged core-0 ADS-B maintenance hold.
- **Product 55:** Shared Airspace range toggle and stable-ICAO live-highlight
  shortcuts, with highest airborne replacing the redundant dominant-category entry.
- **Product 56:** Optional Home Assistant MQTT discovery and controls, bounded
  aircraft telemetry, dependency-free dashboard YAML, disabled-mode zero-allocation
  behavior, and MQTT-aware OTA maintenance coordination.

Detailed Product-by-Product history is maintained in `CHANGELOG.md`. Unconfirmed
older history is not guessed.

## License and data source

Add the repository's chosen license before distributing the firmware.

ADS-B data availability and permitted use remain subject to the selected data
provider's terms and service availability.
