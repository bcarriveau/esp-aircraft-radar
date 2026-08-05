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

It is not compatible with the Waveshare 7B, 7C, ESP32-P4, generic 7-inch
panels, Cheap Yellow Display hardware, ESPHome, or e-paper projects.

## Current status

Current replacement-source build marker:

```text
7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE
```

Source baseline:

```text
main
508144cf4599929505db077c64aec98d0cfdb406
```

Product 76 runtime commit retained by this update:

```text
817462c8a6f2cb8157c7223a1788c8ff34dfc621
```

The hardened permanent rollback baseline remains **Product 15**:

```text
product-15-hardened
7IN-20260721-PRODUCT15-HARDENED
```

Product 77 is a focused replacement-source candidate based on the current `main`
baseline. It keeps an open Aircraft Profile synchronized with newly published
aircraft snapshots by stable ICAO hex. Distance, bearing, altitude, speed, heading,
vertical rate, identity text, aircraft preview, and tracking action update together
only when target, range, or tracking versions change.

A fresh profile shows `CURRENT UPDATE`. When a selected aircraft is absent from the
latest snapshot, the profile explicitly shows `NOT IN CURRENT UPDATE / LAST KNOWN
VALUES` and prevents starting a new track from stale data. During the established
tracked-aircraft grace period, it shows `TRACK SIGNAL LOST / LAST KNOWN VALUES`
while preserving `STOP TRACKING`. The profile resumes current data automatically
when the same ICAO returns.

Product 76 changed the three secondary aircraft rows shown during selection or
tracking so they are ranked by horizontal separation from the selected or tracked
aircraft rather than by distance from the radar center. The calculation uses the
existing coherent aircraft snapshot, stable ICAO identity, one bounded pass, and
fixed three-entry storage. The rows show relative distance and direction, with
`NEAR SELECT`, `NEAR TRACK`, or `POSITION LOST` status as appropriate.

Product 75 refined the Software Update page for the 800x480 display, shortened
the System-page update button to prevent overlap, and changed update-state startup
handling. Reboot clears transient queued, checking, installing, progress, and
available-release state. Automatic checking waits five minutes after each boot,
and **CHECK NOW** remains immediately available.

Product 73 added explicit, user-confirmed installation of a validated GitHub
stable release directly on the radar. Product 74 was the versioned test release
used to prove that path on hardware. The confirmed flow downloaded the generated
`.radarota` package, verified the manifest and complete package, wrote the inactive
partition, performed the hardened cross-core restart, and booted the newer Product
marker. Local browser OTA remains available and retains priority as the recovery
and manual-installation path.

Products 69-72 established the bounded transport foundation used by that updater:

- Product 69 keeps a complete ADS-B fetch below the fixed 15-second cadence with
  a shared 10.5-second transport budget and reserved JSON headroom. An accepted
  OTA request can cancel active transport only between bounded calls without
  recording a false ADS-B failure.
- Product 70 restored a real queued GitHub stable-release check that runs only in
  a safe serialized network window after a successful current-generation ADS-B
  publication.
- Product 71 accepted real GitHub response-header sets and signed redirect URLs
  within explicit 16 KiB header and 4095-character URL limits.
- Product 72 sized the ESP-IDF transmit buffer from the validated URL length so
  long signed release redirects can be sent without removing any TLS, host, or
  redirect restrictions.

The Product 77 candidate preserves Product 63's PSRAM-only ADS-B JSON and
response-body policy, Product 64's measured 12 KiB core-0 ADS-B task stack,
Product 66's bounded dirty-region radar restoration, Product 68's reduced fetch
logging contention, Product 69's bounded transport, Product 73's verified remote
installer, the local browser updater, lightweight MQTT, offline airports, Product
76 relative-neighbor rows, stable ICAO selection and tracking, 200-target bounds,
panel timing, DMA, OPI PSRAM, and the 20-scanline RGB bounce buffer.

## Features

### Live radar

- Displays all retained aircraft as radar contacts.
- Provides 20, 40, and 80 mile radar ranges.
- Shares the same range state between the compact Radar selector and the green
  Airspace `CURRENT RANGE` card.
- Draws configured airport runway and heliport symbols beneath aircraft contacts.
- Supports independent airport symbol and label filters at 20, 40, and 80 miles.
- Shows themed contact tags at the 20-mile range.
- Uses stable ICAO hex identity for contact taps, selection, and tracking.
- Prioritizes tracked and selected tags during collision-aware label placement.
- Keeps selected aircraft visible with amber styling.
- Keeps tracked aircraft visible with red styling.
- Automatically zooms outward when a tracked aircraft approaches the edge.
- Presents aircraft speed in MPH.
- Uses elapsed time for radar sweep motion so delayed frames do not slow the sweep.
- Restores bounded dirty radar regions instead of copying the full cached canvas on
  every dense steady-state frame.

### Radar interaction

Idle state:

- Left panel shows aircraft count, nearest-aircraft information, and data status.
- Right panel shows the nearest five aircraft relative to the radar center.
- Tapping the nearest aircraft, a radar contact, or a radar tag selects it.

Selected state:

- Selected details take priority in the right panel.
- INFO opens aircraft details and returns to Radar.
- TRACK begins stable ICAO-based tracking.
- CLEAR removes the selection.
- The secondary rows show up to three aircraft nearest to the selected aircraft.
- Each row reports separation and relative compass direction from the selection.
- The secondary heading reads `NEAR SELECT` or `NO OTHER`.

Tracked state:

- Tracked details take priority in the right panel.
- STOP TRACK is contained in the tracked-aircraft card.
- The radar tag shows TRACKED, the aircraft identifier, and MPH.
- The secondary rows show up to three aircraft nearest to the tracked aircraft.
- Each row reports separation and relative compass direction from the tracked
  aircraft rather than distance and bearing from home.
- The secondary heading reads `NEAR TRACK`, `NO OTHER`, or `POSITION LOST`.
- Stopping tracking preserves selection when practical.
- One or two successful snapshots that temporarily omit the tracked ICAO retain
  tracking.
- During that grace period, the right card reports `TRACK SIGNAL LOST` and checks
  the next update.
- Three consecutive successful current-generation snapshots without the tracked
  ICAO automatically clear tracking and return the radar to normal idle mode.
- Failed requests and stale discarded responses do not advance the lost-track
  counter.

Aircraft Profile:

- Profiles opened from Radar or Tracks remain keyed to the aircraft's stable ICAO
  hex rather than its current array position.
- The open profile refreshes only after target, range, or tracking versions change.
- One coherent bounded target snapshot updates the title, values, preview, freshness
  status, and tracking action together; LVGL objects are not rebuilt.
- Fresh data shows `CURRENT UPDATE`.
- A missing selected aircraft shows `NOT IN CURRENT UPDATE / LAST KNOWN VALUES`;
  starting a new track is disabled until current data returns.
- A temporarily missing tracked aircraft shows
  `TRACK SIGNAL LOST / LAST KNOWN VALUES` and retains `STOP TRACKING` through the
  established grace period.
- When the same ICAO returns, the profile resumes current values automatically.

### Additional pages

- **Tracks:** Aircraft table with a live stable-ICAO Aircraft Profile, scrolling
  protection after row-count reductions, and explicit return-to-Tracks behavior.
- **Airspace:** Live totals, a green 20/40/80-mile range toggle, aircraft-category
  cards, and tappable nearest, fastest, lowest-airborne, and highest-airborne
  shortcuts that select aircraft on Radar by stable ICAO hex.
- **Airports:** Nearby-airport awareness, directory and profile views, per-category
  20/40/80-mile symbol and label controls, `AUTO / SHOW / HIDE` preferences,
  current-label eye indicators retained for every airport label actually rendered,
  and `SHOW ON RADAR` range selection.
- **System:** Build, memory, connectivity, airport, radar-cadence, MQTT, local OTA,
  and GitHub update status; Device & Network settings; and bounded maintenance
  controls.
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
- Airport identifiers use deterministic positions for each radar range and are drawn
  before airport symbols, aircraft contacts, and aircraft tags.
- The radar grid and configured airport layer use an optional PSRAM static cache.
  Cache allocation failure preserves the established full-render fallback.
- Airport symbols and labels remain below aircraft; aircraft contacts and all
  aircraft labels retain priority.
- The Airports directory remains deterministically bounded at 64 rows. It retains
  every airport identifier reported as visibly labeled by the latest completed
  radar frame, replacing only farther non-visible rows when required and then
  restoring distance order.
- `AUTO / SHOW / HIDE` uses stable airport identifiers and checked NVS storage.
- Directory editing is explicitly locked behind `EDIT / DONE`, and row actions are
  cancelled by scrolling, excessive pointer movement, lost presses, or long holds.
- `SHOW ON RADAR` chooses the smallest useful 20/40/80-mile range and temporarily
  highlights the airport without changing saved preferences.
- The checked-in generated header is the exact regional airport table compiled into
  the firmware and remains tracked in Git.
- `tools/Build Airport Database.bat` provides the guided Windows flow.
- `tools/generate_airport_database.py` provides advanced command-line generation.
- The generator combines OurAirports airport and runway data, selects the longest
  open runway, previews size/category counts, validates output, and restores the
  previous header if replacement validation fails.
- See `docs/AIRPORT_DATABASE.md` for nearby-move versus new-region instructions.
- Airport information is awareness-only and must not be used for navigation.

### ADS-B networking

- All ADS-B requests and Wi-Fi recovery run in one core-0 network task.
- Requests cannot overlap.
- Polling uses a fixed 15-second start-to-start cadence.
- Native ESP-IDF HTTPS remains the preferred transport.
- The independent fallback verifies TLS and hostname and is used only for eligible
  native TCP, TLS, and HTTP-header transport failures.
- The firmware does not use blocking `HTTPClient::GET()` or `setInsecure()`.
- The shared Product 69 policy reserves 12 seconds for the complete fetch, including
  10.5 seconds for transport and 1.5 seconds for JSON work.
- Connect, header, body-read, idle, retry, release, fallback, and total work are
  reduced by the remaining shared budget.
- Local OTA, remote install, range refresh, and reconnect requests can cancel active
  transport at bounded boundaries without manufacturing an ADS-B failure.
- The fallback reader uses bounded streaming parsing, PSRAM-first payload storage,
  strict response-size limits, and explicit support for valid `Content-Length`,
  chunked, and required close-delimited bodies.
- Conflicting or ambiguous framing is rejected.
- Wi-Fi, DNS, TCP, TLS, HTTP, body, JSON, stale, cancellation, and budget failures
  remain distinguishable.
- Recovery escalates from retry to reconnect, radio recycle, and last-resort restart.
- The last good aircraft snapshot remains visible during temporary failures.

### Request and state safety

- Range and location changes increment a request generation.
- Obsolete responses cannot overwrite a newer range or location selection.
- A fully completed stale response counts as a transport success while remaining
  separate from published-data freshness.
- Radar rendering uses one coherent target snapshot per frame.
- The renderer copies that snapshot only when target, range, or tracking versions
  change and shares it with tracked-aircraft auto-zoom.
- Sweep angle advances from elapsed milliseconds rather than completed-frame count.
- Sparse and dense steady-state frames restore bounded prior dynamic regions from
  the cached static layer.
- LVGL labels and page content update only when their underlying versions change.
- Aircraft selection and tracking use stable ICAO hex values, never array positions.
- Radar hit-test priority remains tracked, selected, then closest.
- Tracked-aircraft loss is evaluated only when a successful current-generation
  target snapshot is published.
- Tracking is cleared atomically after three consecutive confirmed misses.
- Product 76 calculates relative neighbor rows from the same coherent snapshot in
  one bounded pass with fixed storage; it does not create a second target snapshot.
- Product 77 version-gates an open Aircraft Profile by target, range, and tracking
  versions and resolves the same stable ICAO from one coherent snapshot.
- Profile refresh creates no target buffer, dynamic container, timer, or replacement
  LVGL object and does not run during ordinary unchanged 80 ms frames.
- Missing-current-data states retain clearly marked last-known values instead of
  silently presenting them as live.

### Home Assistant MQTT

- MQTT is disabled by default and its enabled state is stored independently in NVS.
- Disabled mode creates no MQTT task, makes no broker connection attempts, and
  allocates no aircraft snapshot or JSON buffer.
- When enabled, task-free PubSubClient 2.8 publishes Home Assistant discovery,
  retained availability, bounded state topics, tracked-aircraft data, five nearest
  aircraft, and Airspace highlights.
- The raw Last Update Age entity is intentionally absent. Home Assistant receives
  useful `LIVE`, `UPDATING`, `STALE`, and `OFFLINE` status instead.
- Home Assistant can switch the physical LCD backlight, select the shared
  20/40/80-mile range, and queue the existing non-overlapping ADS-B refresh command.
- MQTT does not own Wi-Fi, recycle the station, change ADS-B recovery, or trigger a
  restart. Broker failure remains nonfatal to the radar.
- MQTT uses a 384-byte internal packet buffer and streams larger retained payloads
  from bounded PSRAM storage.
- MQTT network work is skipped while ADS-B, a GitHub check/install, local OTA, or
  hard Wi-Fi recovery owns the serialized network window.
- Hard radio recovery requires acknowledged MQTT socket/client/workspace teardown
  before the station is recycled.
- `home-assistant/aircraft-radar-view.yaml` provides a ready-to-paste dashboard view
  using only standard Home Assistant cards.

### Local browser firmware updates

- The local HTTP updater is disabled during normal operation and can be armed from
  the System page for five minutes.
- Arming immediately claims an exclusive OTA maintenance window.
- Each arming generates a temporary six-digit access code and displays both the
  numeric-IP address and `bills-aircraft-radar.local/update` when mDNS starts.
- PlatformIO creates `firmware.radarota` beside `firmware.bin` and copies the same
  package to `release/firmware.radarota`.
- USB flashing uses `firmware.bin`; browser updates accept only `.radarota`.
- Upload data is streamed directly to the inactive OTA partition. No complete
  firmware copy is allocated in heap or PSRAM.
- ADS-B parks and MQTT closes/releases its resources for the complete armed,
  preparation, retry, upload, verification, and restart lifecycle.
- Browser request pacing and bounded retries accommodate the single-client Arduino
  `WebServer` without permitting ambiguous duplicate writes.
- Upload retry is allowed only when authenticated status proves zero bytes were
  received and written.
- The next boot partition is selected only after hardware, build-ID, ESP32-S3 image,
  exact-length, SHA-256, and `esp_ota_end()` checks succeed.
- Automatic restart uses a bounded internal-stack Core-0 task and an atomic Core-1
  acknowledgement from an interrupt-masked IRAM park.

### GitHub stable-release updates

- No firmware is downloaded or installed automatically.
- A metadata check can run only after a successful current-generation ADS-B fetch
  while the existing serialized network ownership is still active.
- Automatic checks wait at least five minutes after boot and approximately 24 hours
  after a completed attempt.
- **CHECK NOW** bypasses only the five-minute and 24-hour timers; it still waits for
  Wi-Fi, cadence slack, ADS-B success, MQTT, command, recovery, and OTA safety gates.
- A metadata check has a six-second absolute ceiling, a 2048-byte manifest limit,
  a 16 KiB aggregate header limit, a 4095-character URL limit, at most three approved
  HTTPS redirects, and a URL-sized 1024-4607-byte ESP-IDF transmit buffer.
- Redirects are restricted to `github.com`, `objects.githubusercontent.com`, and
  `release-assets.githubusercontent.com`.
- The manifest must match schema, exact hardware, stable channel, numeric version,
  updater support, Product label, build ID, asset name, sizes, and SHA-256 fields.
- A newer compatible release enables a two-tap **DOWNLOAD & INSTALL** flow.
- The first tap arms a 15-second confirmation; the second queues installation for
  the next safe network window.
- The manifest is downloaded and validated again immediately before installation.
  A changed version, build ID, size, or digest cancels installation and requires
  fresh confirmation.
- Remote package transport uses verified native ESP-IDF HTTPS, at most three approved
  redirects, a 4096-byte PSRAM receive buffer, a 1024-byte internal-RAM flash staging
  buffer, an eight-second connect/header ceiling per request, a fifteen-second body
  idle ceiling, and a three-minute absolute install ceiling.
- The complete package is never held in memory.
- Every flash write source is staged through internal RAM before `esp_ota_write()`.
- The package header, hardware, build ID, package size, firmware size, package SHA-256,
  firmware SHA-256, ESP application magic, ESP32-S3 image identity, embedded build ID,
  exact byte counts, and `esp_ota_end()` must all pass before boot selection.
- Local browser OTA has priority and can cancel the remote operation at the next
  bounded point.
- Product 74 physically proved the Product 73 remote installation and restart path.
- Product 75 clears transient update/install state after reboot, then starts a fresh
  five-minute automatic-check delay while leaving **CHECK NOW** ready.
- TLS plus hashes protect against corruption and accidental mismatch, but the manifest
  and package share the same publishing account. Public-key package signing and
  automatic first-boot rollback remain separate future hardening work.

### Target capacity

- Storage is bounded at 200 retained targets.
- Capacity-scaled target arrays and radar metadata use required PSRAM.
- Retention remains nearest-first while preserving the tracked aircraft when returned.
- Diagnostics distinguish received, eligible, stored, capacity-dropped, and visible
  aircraft counts.
- Array allocation sizes, bounds checks, counters, and indexes remain tied to the
  same source capacity.

### Memory diagnostics

- ADS-B response payloads and both ArduinoJson documents use PSRAM-only allocation.
- Per-fetch request URL/path construction uses bounded character arrays rather than
  hot-path Arduino `String` allocation.
- The 64-row airport directory and optional radar caches use PSRAM.
- System diagnostics include free/minimum heap, current/minimum largest internal
  block, free/minimum PSRAM, fetch-only lows and stage attribution, ADS-B task stack
  headroom, LVGL pool use/free/largest block/fragmentation, radar render duration,
  radar frame gaps, fetch duration, and activity-stage attribution.
- The core-0 ADS-B task stack remains 12 KiB based on measured hardware headroom.
- The LVGL pool remains 128 KiB.

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
scripts/                PlatformIO post-build OTA and release-asset generation
release/                Generated local and versioned OTA assets plus manifest
home-assistant/          Built-in-card dashboard view and installation notes
include/                 Shared interfaces, build identity, and hardware configuration
src/                     Application, networking, state, radar, UI, MQTT, and OTA
platformio.ini           Pinned PlatformIO environment, workspace, and checks
README.md                Current project documentation
CHANGELOG.md             Confirmed Product history
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

Edit `include/config.h` with local Wi-Fi credentials and radar-center coordinates.
To use Home Assistant, also replace the MQTT placeholders with local broker values.
MQTT remains disabled until enabled from the radar's System page.

Never commit this file.

### 4. Generate another airport region when needed

A nearby move inside the compiled region needs only a coordinate change on the
System page. A move to another region requires a new generated header and build.

Windows guided setup:

```text
tools\Build Airport Database.bat
```

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

Generated objects and downloaded libraries are stored outside the Google Drive
project directory:

```text
~/.platformio/workspaces/bills_aircraft_radar
```

A successful build creates:

```text
~/.platformio/workspaces/bills_aircraft_radar/build/waveshare-s3-touch-lcd-7/firmware.bin
~/.platformio/workspaces/bills_aircraft_radar/build/waveshare-s3-touch-lcd-7/firmware.radarota
<project>/release/firmware.radarota
<project>/release/waveshare-esp32-s3-touch-lcd-7-product-77.radarota
<project>/release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

Use `firmware.bin` for USB upload, `release/firmware.radarota` for local browser
OTA, and the versioned package plus fixed-name manifest for a normal stable GitHub
Release. Do not hand-edit generated assets.

### 6. Static inspection

Run the repository checks appropriate to the changed subsystem. The documentation
catch-up itself does not require a PlatformIO build because it changes no firmware.
For source work, the repository includes focused tests for airports, classification,
capacity, ADS-B transport, radar rendering, MQTT, local OTA, GitHub release checking,
and remote installation.

The project uses `cppcheck: --skip-packages` so application sources are checked
without treating downloaded framework headers as project code.

### 7. Local browser update

An OTA-capable build must be installed by USB once. For later local updates:

1. Build and locate `release/firmware.radarota`.
2. Open **System**, tap **Firmware / OTA**, and enable the five-minute window.
3. Open the displayed address from another device on the same network.
4. Enter the six-digit code, select `firmware.radarota`, and start the update.
5. Keep power connected through verification and restart.

A partial, rejected, or interrupted package is not selected as the next boot image.

### 8. GitHub release update

To publish a stable release for the on-device updater:

1. Build the exact intended Product source.
2. Confirm the expected Product marker on hardware.
3. Run the relevant checked-in tests and normal radar regression checks.
4. Create a normal, published, non-draft, non-prerelease release.
5. Use the matching tag and release name, such as `product-77` / `Product 77`.
6. Attach the generated versioned `.radarota` package and the fixed-name manifest.
7. Keep `release/firmware.radarota` as the local browser-install fallback.

On the radar, **CHECK NOW** queues a bounded check. A newer compatible release can
then be installed only after the two-tap confirmation flow.

### 9. Home Assistant MQTT

1. Configure broker URI and credentials only in private `include/config.h`.
2. Confirm Home Assistant MQTT discovery is enabled.
3. Open **System**, tap **HA MQTT**, and enable MQTT.
4. Confirm the radar appears as one MQTT device.
5. Follow `home-assistant/README.md` to install the supplied dashboard view.

Disabling MQTT stops and destroys the client and releases its PSRAM work buffers
without disconnecting Wi-Fi or changing ADS-B behavior.

### 10. Upload and monitor

```bash
pio run -e waveshare-s3-touch-lcd-7 -t upload
pio device monitor -b 115200
```

The programming USB-C port is normally used for upload and Serial Monitor.

## Expected startup checks

Confirm the serial log reports:

- `7IN-20260805-PRODUCT77-LIVE-AIRCRAFT-PROFILE`
- PSRAM detected
- unused BLE controller memory released
- app-state, radar, UI, and ADS-B buffers allocated in PSRAM
- airport database and nearby-cache counts
- core-0 ADS-B task started with the intended stack
- update manager initialized with transient state cleared for the new boot
- native HTTPS or the configured verified fallback connected successfully
- aircraft received, eligible, stored, dropped, and published counts
- no immediate TLS memory-allocation failure
- no display rolling during HTTPS activity

## Verification checklist

Source review, compilation, physical testing, and soak testing are separate stages.
Before calling a Product release complete:

1. Compile and link the full PlatformIO project.
2. Record flash and internal RAM use.
3. Confirm the Product 77 marker, OPI PSRAM, 128 KiB LVGL pool, 12 KiB ADS-B
   stack, and 20-scanline bounce buffer.
4. Test normal 15-second updates and 20/40/80-mile range changes.
5. Select an aircraft, open **INFO**, and leave the profile open through several
   ADS-B publications. Confirm distance, bearing, altitude, speed, heading, and
   vertical rate update without closing or rebuilding the page.
6. Open a profile from Tracks, allow the list to reorder, and confirm the profile
   continues following the same stable ICAO.
7. Confirm fresh profiles show `CURRENT UPDATE`.
8. Confirm an absent selected aircraft shows
   `NOT IN CURRENT UPDATE / LAST KNOWN VALUES`, disables starting a stale track,
   and resumes current values automatically if the ICAO returns.
9. Track an aircraft, open its profile, and confirm a temporary omission shows
   `TRACK SIGNAL LOST / LAST KNOWN VALUES` while `STOP TRACKING` remains available.
10. Confirm one or two successful omitted snapshots retain tracking and the third
    consecutive confirmed miss clears it.
11. Confirm failed requests and stale discarded responses do not clear tracking.
12. Test BACK, TRACK, STOP TRACK, CLEAR, tab changes, and selected timeout behavior.
13. Select and track aircraft; confirm the three secondary rows remain ranked by
    separation from the priority aircraft and show relative direction.
14. Confirm `NEAR SELECT`, `NEAR TRACK`, `NO OTHER`, and `POSITION LOST` states.
15. Interrupt Wi-Fi or the router and confirm recovery.
16. Change range during an active request and confirm stale-response rejection.
17. Check display stability during HTTPS, MQTT, update checks, and page switching.
18. Check heap, largest internal block, PSRAM, LVGL pool, and ADS-B stack stability.
19. Confirm local browser OTA failure and success paths.
20. Confirm a manual GitHub check waits for a safe ADS-B window and reports its state.
21. Confirm reboot clears transient update/install state and starts a fresh
    five-minute automatic-check delay.
22. Publish a numerically newer test release and confirm two-tap remote installation,
    fresh manifest recheck, progress, verification, restart, and new marker.
23. Confirm changed-manifest, cancellation, truncation, hash mismatch, wrong hardware,
    and wrong image cases leave the current boot partition active.
24. Confirm MQTT disabled and enabled behavior, Home Assistant controls, and network
    serialization around ADS-B and OTA.
25. Confirm every visible radar airport label has a matching directory eye indicator.
26. Complete an extended soak test.

## Screenshots

Current display photographs have been used during physical layout review, but a
curated repository-facing screenshot set is not yet committed under `docs/images/`.

## Major milestones

- **Product 15:** First hardened modular baseline with core-0 networking,
  generation-safe publishing, last-good retention, diagnostics, and the proven RGB
  anti-rolling configuration.
- **Products 16-18:** HTTPS transport and certificate-bundle corrections that
  established the working native TLS baseline.
- **Products 19-25:** Tracking UI, outward auto-zoom, keyboard, large-response and
  heading-crash fixes, transport recovery, and quiet first-run NVS defaults.
- **Products 26-29:** Themed radar tags, stable ICAO selection, compact range control,
  idle/selected/tracked panel flow, detail-origin handling, and UI-state fixes.
- **Products 30-34:** Bounded 200-target PSRAM architecture, aircraft side bitmaps,
  Airspace dashboard, UI fit cleanup, and confirmed tracked-aircraft loss recovery.
- **Products 35-49:** Safer aircraft classification, bitmap/heading contacts,
  fallback HTTPS hardening, startup and NVS reliability, response recovery,
  vertical-state artwork, Tracks scroll clamping, and exact 20-mile label selection.
- **Products 50-53:** Offline airport awareness, deterministic labels, directory and
  profile views, per-airport controls, touch safety, and radar focus.
- **Products 54-61:** Hardware-bound local browser OTA, Home Assistant MQTT,
  region-independent airport tooling, exclusive network ownership, IRAM-safe restart,
  Wi-Fi recovery coordination, idempotent preparation, and socket pacing.
- **Products 62-64:** Complete airport-eye coverage, PSRAM-only ADS-B parsing,
  expanded memory diagnostics, and a measured 12 KiB ADS-B task stack.
- **Products 65-68:** Elapsed-time sweep, cached static radar layer, bounded dirty
  restoration, activity-stage gap attribution, and reduced fetch contention.
- **Product 69:** Shared bounded ADS-B transport budget with safe OTA preemption.
- **Products 70-72:** Restored GitHub stable-release checking and corrected real-world
  response headers, redirects, and request transmit sizing.
- **Products 73-74:** Explicit verified on-device GitHub installation and its first
  physically working remote update.
- **Product 75:** Software Update layout cleanup and reboot-time transient-state reset.
- **Product 76:** Three bounded nearest-aircraft rows relative to the selected or
  tracked aircraft.
- **Product 77:** Live stable-ICAO Aircraft Profiles with version-gated coherent
  refresh, explicit current/last-known states, and safe tracked-signal-loss actions.

Detailed confirmed Product history is maintained in `CHANGELOG.md`. Products 1-14
are not reconstructed because the repository does not preserve authoritative
numbered history for them.

## License and data source

Add the repository's chosen license before distributing the firmware.

Airport information is derived from public OurAirports datasets and is for visual
awareness only, without a guarantee of accuracy or fitness for navigation.

ADS-B data availability and permitted use remain subject to the selected provider's
terms and service availability.
