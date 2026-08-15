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

It is not compatible with Waveshare 7B/7C, ESP32-P4, generic 7-inch panels,
Cheap Yellow Display hardware, ESPHome, or e-paper projects.

## Current status

Current development/source branch for the completed airport-separation work:

```text
airport-seperation
```

Current committed Product:

```text
Product 94
7IN-20260814-PRODUCT94-FACTORY-DISTRIBUTION
```

The Product marker is the durable firmware identity. Repository HEAD naturally
advances for documentation and housekeeping commits, so README does not pin a
"current commit" SHA.

Product 94 adds a separate credential-safe factory/distribution build for blank/new-owner
hardware while preserving the normal private development build. The factory build uses neutral
Wi-Fi/location/MQTT defaults and contains no compiled regional airport fallback; the owner
installs a regional airport database after setup.

After the local maintenance window is armed and the six-digit code is accepted,
the Airport Database page can prefill the radar's already-saved home coordinates,
download public OurAirports data in the user's browser, build a bounded regional
`.radarapt` package locally, upload it through the validated persistent-storage
installer, and restart automatically only after write/readback verification succeeds.

The permanent hardened rollback baseline remains:

```text
product-15-hardened
7IN-20260721-PRODUCT15-HARDENED
```

## Core features

### Live radar

- Displays retained ADS-B aircraft on 20, 40, and 80 mile radar ranges.
- Uses heading-aware aircraft symbols at all three ranges.
- Uses stable ICAO hex for selection, tracking, row actions, and profile identity.
- Selected aircraft are amber; tracked aircraft are red.
- Tracked tags show `TRACKED`, identifier, and MPH.
- Outward auto-zoom keeps a tracked aircraft visible as it approaches the edge.
- Hit-test priority remains tracked, selected, then closest.
- Uses one coherent aircraft snapshot per radar update.
- Uses version/dirty-region updates rather than rebuilding the whole LVGL UI.
- Retains last-good aircraft through temporary transport failures.

### Radar interaction

Idle:

- Left side shows count, nearest aircraft, and data status.
- Right side shows nearest aircraft.
- Radar `20 / 40 / 80` is the range control, and the last manual choice is restored after restart.

Selected:

- Selected details take right-panel priority.
- `INFO / TRACK / CLEAR` are the primary actions.
- Nearby rows are ranked relative to the selected aircraft.

Tracked:

- `STOP TRACK` has right-panel priority.
- Nearby rows are ranked relative to the tracked aircraft.
- Tracking uses stable ICAO identity and a confirmed-miss grace period.
- Failed requests and stale discarded responses do not falsely advance track loss.

### Aircraft profiles and pages

- Aircraft Profile remains tied to stable ICAO and can update while open.
- Tracks preserves scroll during live refresh but returns to top when re-entered.
- Airspace provides totals, category cards, shared range, and live shortcuts.
- Airports provides directory/profile views, per-category display settings,
  `AUTO / SHOW / HIDE`, label-eye indicators, and `SHOW ON RADAR`.
- System provides build, memory, networking, radar, airport, MQTT, update, and
  settings diagnostics.
- Optional Home Assistant MQTT discovery and controls remain isolated from ADS-B
  network ownership.

## Airport architecture

Airport data is now deliberately separated from per-user firmware configuration.

### Runtime sources

The radar can use one of two airport sources:

1. **Persistent regional package** — the normal Product 92 end-user source.
2. **Compiled fallback table** — retained in firmware as a known-good fallback if
   the persistent airport partition is empty, unavailable, or invalid.

The compiled fallback is intentional and should not be removed merely because
persistent storage exists.

### Persistent storage

The custom 16 MB partition table preserves the two OTA application slots and
reserves a dedicated 512 KiB airport-data partition.

The persistent package format is `.radarapt`. The installer:

- accepts a complete bounded package from PSRAM
- verifies package structure and exact record sizing
- verifies SHA-256 before destructive work
- enforces the dedicated partition capacity
- erases only the aligned span required by the package
- writes only the airport partition
- re-reads and fully validates the stored copy
- reports success only after readback verification

NVS settings, both firmware OTA slots, ADS-B storage, LVGL memory, and radar target
capacity are separate from the airport partition.

### Normal new-user airport setup

A normal user does not need Python or a custom firmware build for their location.

1. Flash/install the Product firmware.
2. Save normal home latitude/longitude on the radar's System page.
3. Arm the local firmware/maintenance window.
4. Open the radar web page from a phone or computer on the same network.
5. Open **AIRPORT DATABASE**.
6. Enter the radar's six-digit access code.
7. The browser prefills the currently saved radar coordinates when valid.
8. Review the center and coverage radius; 120 miles is the recommended default.
9. Tap **BUILD & INSTALL AIRPORT DATABASE**.

The browser downloads the current public OurAirports airport/runway CSV datasets,
filters them locally, creates the exact bounded `.radarapt` package, and uploads it
to the ESP.

The ESP does not parse the worldwide CSV files.

After a verified install, Product 92 uses the established hardened restart path so
the new persistent airport source becomes active automatically on the next boot.

### Moving the radar

Changing the radar's saved latitude/longitude changes the current aircraft/radar
center and rebuilds the nearby airport cache.

It does not rewrite the regional airport package.

A nearby move still covered by the installed package generally needs only the
System-page coordinate change. A move outside the installed region should use the
Airport Database browser page to build/install another region. No firmware rebuild
is required.

### Developer airport tooling

The PC/Python builder remains intentionally checked in as a reference, recovery,
and regression tool:

```text
tools\Build Airport Database.bat
python tools/airport_database_setup.py
python tools/generate_airport_database.py ...
```

It can generate:

```text
release\airports.radarapt
```

without changing firmware or the radar's saved location. The Python package code
also serves as the reference implementation used to validate browser-generated
package bytes.

See `docs/AIRPORT_DATABASE.md` for the full workflow.

## ADS-B networking and reliability

- Core-0 owns ADS-B fetch and Wi-Fi recovery.
- ADS-B requests do not overlap.
- Polling retains the fixed 15-second start-to-start cadence.
- Native ESP-IDF HTTPS remains preferred.
- Hardened verified fallback remains restricted to eligible transport failures.
- No blocking `HTTPClient::GET()` is used.
- No `setInsecure()` TLS path is permitted.
- Header, body, idle, and absolute budgets remain bounded.
- Response payload and JSON parsing use PSRAM-first/PSRAM-only policy where designed.
- Conflicting or ambiguous HTTP framing is rejected.
- Stale generation results cannot overwrite newer range/location state.
- Fully successful stale responses still count as transport successes.
- Wi-Fi/TLS recovery and last-good aircraft retention remain intact.

## Memory and display protections

The project intentionally retains:

- Arduino-ESP32 3.0.7 high-performance XIP/PSRAM
- OPI PSRAM and `BOARD_HAS_PSRAM`
- Waveshare panel timing
- DMA/anti-rolling behavior
- 20-scanline RGB bounce buffer
- 128 KiB LVGL pool
- measured 12 KiB core-0 ADS-B task stack
- bounded 200-target PSRAM architecture

Do not casually change framework, panel timing, DMA, bounce buffer, target capacity,
or memory ownership while working on unrelated features.

## Firmware updates

### Local browser firmware update

The local HTTP updater is disabled during normal operation and is armed from System
for a bounded maintenance window.

The on-device Software Update panel shows the installed Product/build and, when a newer release is available, labels the validated manifest release notes as **WHAT'S NEW**.

The user receives a six-digit code. The firmware page accepts only the project's
validated `.radarota` package format, performs bounded handoff/retry behavior for
the single-client WebServer, verifies the image/package before selecting the inactive
OTA slot, and uses the hardened restart sequence.

### GitHub stable-release update

The radar can check the repository's stable release metadata and, after explicit
user confirmation, download/install a newer compatible GitHub release through the
bounded verified installer.

Firmware is not silently installed merely because files exist in `release/`.

## Active `release/` policy

The active branch keeps only the current Product-numbered `.radarota` package and
its matching fixed-name manifest in `release/`.

For Product 92 that means:

```text
release/waveshare-esp32-s3-touch-lcd-7-product-92.radarota
release/waveshare-esp32-s3-touch-lcd-7.manifest.json
```

There is no redundant tracked `release/firmware.radarota` copy. The ESP's local
browser updater accepts the Product-numbered package directly.

Older Product packages remain available from the Git history/tag/release that
belongs to that Product instead of accumulating in the current working tree.

A PlatformIO build may create a temporary `firmware.radarota` under `.pio/build/`;
that temporary build output is not a tracked release asset and is not authoritative
until the versioned package/manifest have been generated for the intended Product.

## Repository layout

```text
assets/                 Aircraft and UI artwork
docs/                   Repository/user guides
home-assistant/          MQTT dashboard/support files
include/                 Interfaces, build identity, generated fallback data
partitions/              Custom partition table including persistent airport storage
release/                 Current Product OTA package and matching manifest
scripts/                 OTA/release post-build tooling
src/                     Firmware implementation
tests/                   Focused host/source regression tests
tools/                   Airport/aircraft generation and developer tooling
platformio.ini           Pinned PlatformIO environment
README.md                Current project documentation
CHANGELOG.md             Confirmed Product history
```

Private credentials belong only in `include/config.h`. That file must remain ignored
and must never be committed, uploaded, or included in distribution ZIPs.

## Initial setup

### 1. Tools

Install:

- Visual Studio Code
- PlatformIO
- Git
- Python 3 for host/developer tooling

### 2. Clone

```bash
git clone https://github.com/bcarriveau/esp-aircraft-radar.git
cd esp-aircraft-radar
```

Use the intended branch for the work being tested.

### 3. Private configuration

Copy:

```bash
cp include/config.example.h include/config.h
```

Keep credentials/private defaults in `include/config.h` only. Never commit it.

Normal users can later edit Wi-Fi, home coordinates, and display name through the
radar's System page.

### 4. First USB flash and partition-table requirement

The persistent airport architecture introduced a custom partition table. A device
coming from a pre-separation layout needs one appropriate USB/PlatformIO flash that
installs the intended partition table.

Ordinary later firmware OTA updates do not intentionally erase NVS or the dedicated
airport partition.

### 5. Build

PlatformIO environment:

```text
waveshare-s3-touch-lcd-7
```

Build:

```bash
pio run -e waveshare-s3-touch-lcd-7
```

The project pins the established Arduino-ESP32 3.0.7 high-performance stack and
LVGL 8.3.11.

### 6. Install regional airports

After firmware is running, use the browser Airport Database workflow described
above. Do not rebuild firmware simply to customize the normal user's region.

### 7. Local browser firmware update

1. Build the exact intended source.
2. Use the newly generated local `.radarota`.
3. Open System and arm Firmware / OTA.
4. Open the displayed address.
5. Enter the six-digit code.
6. Upload the newly generated package.
7. Keep power connected through verification/restart.

### 8. GitHub release publishing

Before publishing a stable release:

1. Build the exact intended Product source.
2. Confirm the Product marker.
3. Run relevant focused tests.
4. Perform required physical regression tests.
5. Publish the matching tag/release.
6. Attach only the matching generated versioned `.radarota` and manifest expected
   by the updater.

Use the current Product-numbered package generated from the exact intended source;
older packages belong to their historical Git commit/tag/release.

## Expected Product 94 checks

For Product 94, confirm:

- build marker `7IN-20260814-PRODUCT94-FACTORY-DISTRIBUTION`
- factory build uses the neutral distribution config and no compiled regional airport fallback
- select 20, 40, and 80 miles and confirm the last manual choice survives restart
- confirm an invalid/missing saved range safely defaults to 80 miles
- when a newer release is available, confirm its validated manifest note appears under WHAT'S NEW
- OPI PSRAM detected
- 20-scanline display bounce buffer retained
- core-0 ADS-B task and 15-second cadence retained
- native/fallback HTTPS remains stable
- persistent airport source is reported after successful browser install
- compiled fallback is used safely when no valid persistent package exists
- six-digit Airport Database page access works
- saved coordinates prefill only after authenticated status succeeds
- browser package generation succeeds from phone/PC
- airport upload uses READY/settle pacing without connection reset
- verified airport install restarts automatically
- new boot reports persistent airport records
- selection/tracking, 20/40/80, touch, page switching, and display stability remain
  normal
- heap/PSRAM remain stable through airport generation/upload/restart

## Major milestones

- **Product 15:** Hardened modular rollback baseline.
- **Products 16-18:** Native HTTPS/certificate baseline.
- **Products 19-29:** Tracking, range, themed tags, stable ICAO interaction, and UI
  state hardening.
- **Products 30-34:** 200-target PSRAM architecture, aircraft imagery, Airspace, and
  confirmed track-loss recovery.
- **Products 35-49:** Classification safety, bitmap contacts, HTTPS fallback hardening,
  NVS/recovery fixes, vertical-state display, Tracks fixes, and label hit testing.
- **Products 50-53:** Offline airport rendering, directory/profile, controls, and
  collision-aware labels.
- **Products 54-61:** Hardware-bound local OTA, MQTT, airport tooling, exclusive
  maintenance ownership, restart hardening, and socket pacing.
- **Products 62-69:** Airport directory completeness, PSRAM parsing, diagnostics,
  radar dirty-region rendering, and bounded ADS-B transport.
- **Products 70-75:** GitHub stable-release checking/install and update UI/state.
- **Products 76-85:** Relative neighbor rows, live profiles, page-entry scroll,
  multi-range aircraft symbols, boot splash, Airspace handoff, keyboard visibility,
  enlarged priority icon, and range-control clipping correction.
- **Product 86:** Dedicated persistent airport partition with compiled fallback.
- **Product 87:** Validated persistent `.radarapt` installer and readback verification.
- **Product 88:** Mobile airport upload page on the existing maintenance WebServer.
- **Product 89:** Browser-side regional airport package generation.
- **Product 90:** Proven READY/settle WebServer upload pacing and safe retry rules.
- **Product 91:** Automatic restart after verified airport install and clearer web
  navigation.
- **Product 92:** Authenticated prefill from the radar's saved home coordinates and
  removal of location-specific examples.
- **Product 93:** Clear on-device WHAT'S NEW release notes plus persisted last-used
  20/40/80-mile radar range.

Detailed confirmed history is maintained in `CHANGELOG.md`.

## License and data source

Repository licensing and third-party notices are maintained in `LICENSE`,
`LICENSES/`, and `THIRD_PARTY_NOTICES.md`.

Airport information is derived from public OurAirports datasets and is for visual
awareness only, not navigation.

ADS-B data availability and permitted use remain subject to the selected provider's
terms and service availability.

## Factory / new-owner build

Use the dedicated environment when producing firmware for a blank unit:

```text
pio run -e waveshare-s3-touch-lcd-7-factory
```

The factory environment deliberately places `include/distribution` before the
normal private include directory and defines `RADAR_DISTRIBUTION_BUILD`.

That build therefore:

- does **not** compile the private `include/config.h`
- starts with blank Wi-Fi credentials and neutral `0,0` coordinates
- starts with MQTT disabled and no broker/user/password
- does **not** compile the generated regional airport fallback
- uses the same 16 MB custom partition table, Arduino-ESP32 3.0.7,
  OPI PSRAM/XIP settings, display timing, DMA, and 20-scanline bounce buffer
- expects the owner to enter Wi-Fi/location on the System page and then install
  a regional airport database from the Airport Database web page

The normal `waveshare-s3-touch-lcd-7` environment remains the private development
build and continues to use `include/config.h`.

The factory environment intentionally disables the normal OTA post-build release
copy so a factory test cannot overwrite the active private Product package in
`release/`.
