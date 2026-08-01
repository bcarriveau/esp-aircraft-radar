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
7IN-20260801-PRODUCT57-AIRPORT-DATABASE-SETUP
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

Product 57 retains the Product 56 R6 runtime firmware and adds a guided,
region-independent airport-database setup workflow. It combines the public
OurAirports airport and runway datasets, previews the generated region, replaces
the compiled header atomically, restores the previous header after validation
failure, and documents clearly when a coordinate change does or does not require a
new firmware build.

Product 56 R5 added an optional Home Assistant MQTT device, dashboard,
display/range/refresh controls, bounded aircraft telemetry, diagnostics, and
MQTT-aware OTA coordination. MQTT remains disabled by default and creates no
client, broker traffic, aircraft snapshot buffer, or JSON buffer until configured
and enabled from the System page.

Product 56 R1 and R2 hardware logs proved that the native ESP-MQTT task, socket state,
and persistent allocations fragmented internal RAM enough that later preferred ADS-B
TLS handshakes failed even when total free heap remained above 50 KB. Product 56 R3
replaced only that optional MQTT transport with task-free PubSubClient 2.8, streams
retained payloads from the PSRAM JSON buffer through a 384-byte MQTT packet buffer,
publishes at most one state message per service interval, and pauses MQTT socket work
while an ADS-B fetch is active. Extended hardware logging then completed repeated
preferred native HTTPS/TLS ADS-B cycles with MQTT connected, stable post-request heap
recovery, and no fallback use, allocation failures, or Wi-Fi recovery.

Product 56 R5 removed the redundant Home Assistant Last Update Age entity while
retaining the internal LIVE/UPDATING/STALE/OFFLINE state logic, and finalized the
System Status, Device & Network, and maintenance-control layout for the physical
800x480 display. The checked-in Product 56 R6 build identity retained that reviewed
layout. ADS-B HTTPS, TLS verification, deadlines, recovery, 15-second cadence,
stable-ICAO interaction, target bounds, OTA behavior, panel timing, DMA, OPI PSRAM,
and the 20-scanline RGB bounce buffer remain unchanged.

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
- **Airports:** Nearby-airport awareness, directory/profile views, per-category
  20/40/80-mile symbol and label controls, `AUTO / SHOW / HIDE` preferences,
  current-label eye indicators, and `SHOW ON RADAR` range selection.
- **System:** A tall status card for build, memory, connectivity, and airport
  diagnostics; a separate Device & Network form; a two-row retry/reconnect/MQTT/reset
  control card; and a dedicated local firmware-update overlay.
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
- The checked-in generated header is the exact regional airport table compiled into
  the firmware and remains tracked in Git.
- Product 57 provides `tools/Build Airport Database.bat` for a guided Windows flow
  and `tools/generate_airport_database.py` for advanced command-line use.
- The generator combines `airports.csv` and `runways.csv`, selects the longest open
  runway, validates the result, and does not store the private generation-center
  coordinates in the header.
- Downloaded source CSV files are cached outside the repository. Python caches,
  accidental in-project CSV copies, interrupted temporary files, and delivery-only
  package notes are ignored by `.gitignore`.
- See `docs/AIRPORT_DATABASE.md` for nearby-move versus new-region instructions.
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
- The raw Last Update Age sensor is intentionally not exposed. Home Assistant receives
  the useful data-status state while the radar retains its internal age calculation for
  `LIVE`, `UPDATING`, `STALE`, and `OFFLINE` classification.
- Product 56 R5 clears the legacy retained update-age discovery topic once after MQTT
  reconnects so an older Home Assistant entity can be removed automatically.
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
docs/                   User-facing project guides
tools/                  Generated-data utilities, including airport CSV conversion
tests/                  Focused host and source regression checks
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
- Python 3 for airport generation and host tests

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

### 4. Generate another airport region when needed

A nearby move within the compiled region needs only a coordinate change on the
radar's System page. A move to another region requires a new generated header and
firmware build.

Windows guided setup:

```text
tools\Build Airport Database.bat
```

The tool downloads or reuses OurAirports data, previews the region, and replaces
only `include/generated_airport_database.h` after confirmation. The previous
header is restored if validation fails.

Advanced command-line generation:

```bash
python tools/generate_airport_database.py airports.csv \
  --runways-csv runways.csv \
  --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE \
  --radius 120 \
  --coverage "YOUR REGION"
```

Use `--dry-run` to preview without replacing the generated header. Full directions
are in `docs/AIRPORT_DATABASE.md`.

### 5. Build

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
- PubSubClient 2.8

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

### 6. Static inspection

Run:

```bash
pio check -e waveshare-s3-touch-lcd-7
python tests/test_airport_database.py
python tests/test_airport_generator.py
```

The project passes `cppcheck: --skip-packages`, which keeps static analysis on
the application sources while skipping downloaded framework, toolchain, and
library packages. This avoids false syntax failures in package headers such as
`bits/c++config.h` and does not alter normal compilation, linking, or upload.

### 7. Local browser update

Product 54 must first be installed by USB. For later updates:

1. Build the new source and locate `release/firmware.radarota` in the project folder.
2. On the radar, open **System**, tap **Firmware / OTA**, and enable the five-minute
   update window.
3. From another device on the same network, open the displayed address.
4. Enter the six-digit access code, select `firmware.radarota`, and start the update.
5. Keep power connected until the browser reports verification and the radar restarts.

A partial, rejected, or interrupted package is not selected as the next boot image.

### 8. Home Assistant MQTT

1. Configure `MQTT_BROKER_URI`, `MQTT_USERNAME`, and `MQTT_PASSWORD` in private
   `include/config.h`.
2. Confirm Home Assistant's MQTT integration and discovery are enabled.
3. On the radar, open **System**, tap **HA MQTT**, and enable MQTT.
4. Confirm the radar appears as one MQTT device in Home Assistant.
5. Follow `home-assistant/README.md` to install the supplied dashboard view.

Disabling MQTT from the radar stops and destroys the client and releases its PSRAM
snapshot and JSON buffers without disconnecting Wi-Fi or changing ADS-B behavior.

### 9. Upload and monitor

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
- The airport database and nearby-cache counts
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
    shared range control, refresh, tracked/nearest telemetry, dashboard rendering, and
    absence of the removed Last Update Age entity after retained-topic cleanup.
18. Confirm repeated preferred native HTTPS/TLS ADS-B cycles continue with MQTT
    connected and that free heap and the largest internal block recover after each
    request.
19. Start OTA with MQTT connected and confirm OTA does not become ready until both
    ADS-B and MQTT maintenance holds are active; confirm cancellation resumes both.
20. Generate a different airport region with synthetic or downloaded data, confirm
    the generic tests pass, then verify the intended regional airports appear.
21. Complete an extended soak test.

## Screenshots

Current Product 56 R6 display photographs have been used for physical layout review
but are not yet committed under a documentation image directory. Selected current-
display photographs can be added under `docs/images/` when repository-facing
screenshots are prepared.

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
- **Products 50-53:** Offline airport awareness, deterministic labels, directory
  and profile views, per-airport label controls, touch safety, and radar focus.
- **Product 54:** Temporarily armed local browser OTA with generated hardware-bound
  packages, streamed inactive-slot writes, SHA-256 and ESP image validation, and an
  acknowledged core-0 ADS-B maintenance hold.
- **Product 55:** Shared Airspace range toggle and stable-ICAO live-highlight
  shortcuts, with highest airborne replacing the redundant dominant-category entry.
- **Product 56:** Optional Home Assistant MQTT discovery and controls, bounded
  aircraft telemetry, built-in-card dashboard YAML, disabled-mode zero-allocation
  behavior, MQTT-aware OTA coordination, the lightweight PubSubClient transport that
  preserves native ADS-B TLS memory, removal of the redundant update-age entity, and
  the finalized System-page layout.
- **Product 57:** Guided regional airport and runway generation, atomic validation
  and rollback, location-independent tests, plain-language setup documentation, and
  Git exclusions for local tooling artifacts.

Detailed Product-by-Product history is maintained in `CHANGELOG.md`. Unconfirmed
older history is not guessed.

## License and data source

Add the repository's chosen license before distributing the firmware.

Airport information is derived from public OurAirports datasets and is for visual
awareness only, without a guarantee of accuracy or fitness for navigation.

ADS-B data availability and permitted use remain subject to the selected data
provider's terms and service availability.
