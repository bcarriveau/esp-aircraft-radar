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
7IN-20260802-PRODUCT66-RADAR-DIRTY-REGIONS
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

Product 66 is the current source candidate for `main`, built from the exact
hardware-tested Product 65 replacement source. It keeps Product 63's PSRAM-only ADS-B
JSON and response-body protections, Product 64's measured 12 KiB core-0 ADS-B task
stack, the unchanged 128 KiB LVGL pool, and Product 65's elapsed-time radar sweep and
optional 309,600-byte PSRAM static radar/airport cache.

Product 65 made the sweep and aircraft motion visibly better, but dense 40/80-mile
frames still restored the complete 430 x 360 cached layer every 80 ms. With roughly
138-143 retained aircraft at 80 miles, that meant a 309,600-byte PSRAM-to-PSRAM copy
on every frame while TLS, display DMA, contact drawing, and label work shared memory
bandwidth.

Product 66 replaces that dense steady-state copy with a deterministic PSRAM list of
merged dirty rectangles. The previous sweep is restored by exact pixels, while prior
contact and tag rectangles are clipped, merged, and restored by bounded rows. The
complete cached-layer copy remains a safe fallback if the optional dirty-region
allocation is unavailable or its deterministic capacity is ever exceeded.

The coherent radar target snapshot is now recopied only after a published target
version, range generation, or tracking-state change. Aircraft projection, contact
bitmaps/dots, selected and tracked styling, and all labels still update every radar
frame. Throttled `RADAR PERF` logs report render time, frame gap, contacts, dirty
regions, restored bytes, snapshot copies, cache rebuilds, fallback count, heap, and
largest block. `RADAR CACHE` logs bracket static-layer rebuilds to help locate the
observed range-change largest-block fragmentation.

Product 66 does not alter ADS-B cadence, HTTPS behavior, MQTT, target capacity,
selection/tracking, airport capacities, panel timing, DMA, or the 20-scanline RGB
bounce buffer.

Product 66 retains Product 62's bounded 64-row airport eye-coverage fix, Product
61's OTA socket pacing, Product 60's idempotent `/prepare`, Product 59's unused
BLE-memory release and acknowledged MQTT teardown before hard Wi-Fi recovery, and
Product 58's network-exclusive OTA window and IRAM-safe restart.

Product 57 added the guided, region-independent airport and runway generator with
preview, atomic replacement, validation rollback, and relocation documentation.
Product 56 retains the optional lightweight Home Assistant MQTT integration, bounded
telemetry, finalized System-page layout, native preferred ADS-B HTTPS transport,
15-second cadence, stale-response rejection, last-good retention, stable ICAO
interaction, 200-target bounds, panel timing, DMA, OPI PSRAM, and the 20-scanline
RGB bounce buffer.

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
  current-label eye indicators retained for every airport label actually rendered,
  and `SHOW ON RADAR` range selection.
- **System:** A tall status card for build, memory, connectivity, airport, and radar
  frame-cadence diagnostics; a separate Device & Network form; a two-row
  retry/reconnect/MQTT/reset
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
- The radar grid and configured airport layer use an optional PSRAM static cache.
  The cache is invalidated only by range, location, airport-setting, or temporary
  airport-focus changes; allocation failure preserves the full-frame fallback.
- Airport symbols are drawn below aircraft; aircraft contacts and all aircraft
  labels retain priority.
- The Airports directory remains deterministically bounded at 64 rows. It retains
  every airport identifier reported as visibly labeled by the latest completed radar
  frame, replacing only farther non-visible rows when required and then restoring
  distance order.
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
- Sweep angle advances from elapsed milliseconds rather than completed-frame count,
  preserving rotational speed when a frame is delayed.
- Sparse frames restore only prior dynamic radar regions; dense frames use one
  bounded cached-base copy instead of recalculating static airports every frame.
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
  the System page for five minutes. Arming immediately claims an exclusive OTA
  maintenance window rather than waiting for the browser upload to begin.
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
- As soon as OTA is armed, the core-0 ADS-B task finishes only a request already in
  flight and then parks. MQTT closes its connection and releases its client, aircraft
  snapshot, and JSON work buffers. Pending refresh/reconnect work and ADS-B Wi-Fi
  recovery are suppressed for the complete armed, preparation, retry, upload,
  verification, and restart lifecycle.
- Upload or preparation errors keep the exclusive hold active for a bounded retry.
  Normal ADS-B and MQTT operation resumes only after OTA is cancelled, disabled,
  expires, or loses Wi-Fi externally; HTTP and mDNS stop before the hold is released.
- The next boot partition is selected only after package checks, confirmation that
  the declared Product build ID is embedded in the firmware, ESP32-S3 image validation,
  exact-length validation, SHA-256 verification, and `esp_ota_end()` all succeed.
- Automatic restart uses a bounded internal-stack task on Core 0 and an atomic Core-1
  acknowledgement from an interrupt-masked IRAM park, preventing Core 1 from fetching
  flash code after the restart path disables caches. Hardware testing completed two
  consecutive clean software restarts with `Reset reason: 3`.
- The updater does not claim signed-firmware authenticity or automatic first-boot
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
python tests/test_radar_ota_package.py
python tests/test_ota_restart_shutdown.py
python tests/test_ota_exclusive_window.py
```

The project passes `cppcheck: --skip-packages`, which keeps static analysis on
the application sources while skipping downloaded framework, toolchain, and
library packages. This avoids false syntax failures in package headers such as
`bits/c++config.h` and does not alter normal compilation, linking, or upload.

### 7. Local browser update

An OTA-capable build must be installed by USB once. For later updates:

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
    SHA-mismatch failures leave the current firmware selected while the exclusive
    OTA hold remains available for retry until cancellation or expiry.
16. With MQTT disabled, confirm no broker attempts, MQTT client, or MQTT aircraft
    buffer are reported and normal Product 55 behavior is unchanged.
17. Enable MQTT and confirm Home Assistant discovery, availability, display power,
    shared range control, refresh, tracked/nearest telemetry, dashboard rendering, and
    absence of the removed Last Update Age entity after retained-topic cleanup.
18. Confirm repeated preferred native HTTPS/TLS ADS-B cycles continue with MQTT
    connected and that free heap and the largest internal block recover after each
    request.
19. Enable OTA with MQTT connected and confirm ADS-B parks and MQTT releases its
    resources immediately for the complete countdown. Confirm cancellation resumes
    both, then complete two OTA cycles and verify clean `Reset reason: 3` restarts.
20. Generate a different airport region with synthetic or downloaded data, confirm
    the generic tests pass, then verify the intended regional airports appear.
21. Confirm boot logs report the unused BLE-memory release, then fault-inject a hard
    ADS-B recovery and verify MQTT resources are released before the station radio
    restarts and reconnects.
22. Confirm enabling OTA during a hard recovery is rejected with a retry message,
    while enabling OTA first prevents the hard recovery from starting.
23. From Chrome or Edge, select the package and press **PREPARE & INSTALL** once.
    Confirm the page reaches upload without manual retries, the serial log reports one
    upload start, and no `/status` or `/upload` connection-reset error appears.
24. At 20, 40, and 80 miles, note every airport label visible on Radar, then scroll
    the complete Airports directory and confirm each visible label has exactly one eye
    indicator, including visible airports outside the nearest subset.
25. Complete an extended soak test.

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
- **Product 58:** Physically verified network-exclusive OTA window, immediate ADS-B
  and MQTT maintenance ownership, suppression of competing Wi-Fi recovery, preserved
  Product 57 multipart/package writer, and a clean IRAM-safe dual-core restart proven
  across consecutive alternating-partition updates.
- **Product 59:** Unused BLE-controller memory release, acknowledged MQTT teardown
  before hard station-radio recovery, bounded recovery deferral, and atomic exclusion
  between hard Wi-Fi recovery and the Product 58 OTA window.
- **Product 60:** Idempotent OTA preparation requests plus bounded browser recovery
  when the first `/prepare` response is lost, while preserving the Product 57
  multipart upload writer and Product 58 restart implementation.
- **Product 61:** HAR-confirmed single-client socket pacing, bounded control-request
  retries, safe zero-byte-only upload retry, transfer counters, and removal of the
  duplicate `Connection: close` header.
- **Product 62:** Retained every radar-visible airport in the bounded 64-row
  Airports directory so scrolling exposes an eye indicator for each label actually
  rendered, with stable identifiers, deterministic replacement, distance ordering,
  and a nearest-row fallback if optional PSRAM is unavailable.
- **Product 63:** Moved ADS-B JSON and response payload allocation to PSRAM-only
  storage, removed hot-path `String` construction, moved the 64-entry airport
  directory array to optional PSRAM, and added fetch, LVGL-pool, and ADS-B stack
  low-water diagnostics without changing pool or task sizes.
- **Product 64:** Reduced the hardware-measured ADS-B task stack to 12 KiB,
  attributed handshake low-water samples to `tls-handshake`, and moved Firmware /
  OTA into the System-page header without reducing the LVGL pool.
- **Product 65:** Made the sweep elapsed-time based, cached the static grid/airport
  layer in optional PSRAM, restored bounded dynamic regions between sparse frames,
  and added radar render/gap diagnostics without touching networking or display DMA.

Detailed Product-by-Product history is maintained in `CHANGELOG.md`. Unconfirmed
older history is not guessed.

## License and data source

Add the repository's chosen license before distributing the firmware.

Airport information is derived from public OurAirports datasets and is for visual
awareness only, without a guarantee of accuracy or fitness for navigation.

ADS-B data availability and permitted use remain subject to the selected data
provider's terms and service availability.
