# Airport Database Setup

Bill's Aircraft Radar keeps aircraft and airport data on separate paths:

- **Aircraft** are received from ADS-B over the network.
- **Airport data** is stored offline in the dedicated persistent `airports`
  flash partition.
- **All firmware variants use that same persistent airport partition.** Regional
  airport data is not compiled into private or public application firmware.

Airport information is for visual awareness only and must not be used for
navigation.

## Normal user workflow

A normal user does **not** need Python, PlatformIO, or a firmware rebuild to
create airport data for their location.

1. Save the radar's normal home latitude and longitude from the **System** page.
2. Open **System** and enable the local firmware/maintenance window.
3. From a phone or computer on the same network, open the displayed radar web
   address.
4. Open **AIRPORT DATABASE**.
5. Enter the six-digit access code shown by the radar.
6. After authentication, the page prefills the currently saved radar latitude
   and longitude.
7. Review the package center and choose a coverage radius. **120 miles** is the
   recommended default.
8. Tap **BUILD & INSTALL AIRPORT DATABASE**.

The browser then:

1. Downloads the public OurAirports airport and runway CSV datasets.
2. Filters them locally around the requested package center.
3. Builds the bounded `airports.radarapt` package in the browser.
4. Uploads the generated package to the radar.
5. Lets the ESP32 validate the complete package in PSRAM before changing flash.
6. Writes only the dedicated airport-data partition.
7. Re-reads and validates the persistent package.
8. Returns a success response and automatically restarts the radar.
9. Boots using the persistent airport database.

The ESP32 does not parse the worldwide CSV files and does not build the regional
database itself.

## Location changes

There are two independent concepts:

### Radar home location

The latitude and longitude saved on the radar determine the current radar center
used for aircraft distance/bearing and the nearby-airport cache.

Changing the saved location does **not** rewrite the persistent airport package.

### Airport package coverage

The persistent package contains a regional set of airports selected when the
browser builds it.

A nearby move that remains inside the installed package coverage generally needs
only the normal System-page coordinate change.

For a move outside the installed region, open the Airport Database web page and
build/install a new package for the new location. No firmware rebuild is
required.

## Why 120 miles?

The radar displays 20, 40, or 80 miles and maintains a bounded nearby-airport
cache around the current home position.

A 120-mile package gives useful margin around the maximum 80-mile display range
and the nearby cache while keeping the persistent package compact.

The browser also allows larger coverage regions when needed. A larger package
stores more airport records but does not increase the radar's 80-mile display
range.

## Browser package builder

The Airport Database page is mobile-friendly and is the primary setup method.

The package center coordinates are used only to select the regional records.
They are deliberately **not stored inside `airports.radarapt`**.

The generated package contains bounded metadata plus fixed-size airport records
for:

- airport identifier and name
- latitude and longitude
- elevation
- longest open runway length
- runway heading
- radar airport category

The package has a fixed header and payload SHA-256. The firmware validates
metadata, exact package length, SHA-256, and every airport record before erasing
any existing persistent data.

## Safe installation behavior

Airport upload uses the same authenticated local maintenance window used by the
firmware updater.

The complete `.radarapt` upload is first buffered in PSRAM and bounded by the
dedicated airport partition size.

Before flash is changed, the firmware rejects:

- invalid package magic or format
- invalid header or record size
- malformed metadata
- impossible record counts or fields
- package-length mismatch
- payload SHA-256 mismatch
- packages larger than the airport partition

Only a completely validated package reaches the destructive install step.

After writing, the radar reopens and validates the persistent copy again before
reporting success.

If an upload is interrupted before installation starts, the existing persistent
database remains unchanged.

If persistent storage is unavailable, empty, or invalid at boot, the radar runs
without regional airport data until a valid package is installed.

## Upload connection pacing

The local web server is the Arduino single-client `WebServer`, so airport uploads
retain the proven firmware-OTA handoff sequence:

```text
PREPARE -> READY -> bounded settle -> multipart upload
```

Short control/status requests use bounded retries.

A large airport upload may retry once only when both sides prove the transfer
never started: the browser observed no uploaded bytes and the radar still reports
`READY` with zero airport bytes received.

A partial or ambiguous large upload is never blindly retried.

## Advanced: install an existing package

The Airport Database page keeps an **Advanced** section that can install a saved
`.radarapt` file directly.

This is useful for:

- reinstalling a previously generated region
- developer testing
- comparing browser and Python-generated packages
- debugging the upload path without rebuilding the package

Normal users do not need to download or manually select a package.

## Developer / PC package builder

The repository retains a PC-side builder as a development and recovery tool:

```text
tools\Build Airport Database.bat
```

or:

```text
python tools/airport_database_setup.py
```

This tooling creates:

```text
release\airports.radarapt
```

It does **not** rebuild firmware and does **not** change the location saved on the
radar. The generated package is installed separately into persistent airport flash.

The guided builder can download fresh `airports.csv` and `runways.csv`, reuse its
local cache, or use CSV files already on the computer. It asks for the package center,
coverage radius, and region label, validates the generated package by parsing it back,
and writes only:

```text
release\airports.radarapt
```

The lower-level generator remains available for tests, automation, and developer
workflows when the CSV files are already available:

```text
python tools/generate_airport_database.py airports.csv \
  --runways-csv runways.csv \
  --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE \
  --radius 120 \
  --coverage "YOUR REGION"
```

Its default output is also `release/airports.radarapt`. Use `--output` only when a
different package path is intentionally needed. The lower-level generator does not
download the CSV files and does not flash the radar.

After either PC-side method finishes, open the radar's authenticated **AIRPORT
DATABASE** page and use the **Advanced** existing-package installer to select the
generated `.radarapt` file. The radar performs the same PSRAM-first validation,
dedicated-partition write, read-back verification, and automatic restart used for a
browser-built package.

The Python package implementation is also used as the reference implementation for
browser-builder parity tests.

## One airport-data path

Private development firmware and public/distribution firmware now use exactly the
same airport source: a validated `airports.radarapt` package in the dedicated
`airports` partition.

`include/generated_airport_database.h` is retained only as a small legacy placeholder
so old references fail visibly during development; it contains no regional records
and is not used by the runtime.

For a development unit, install the regional package once through the same Airport
Database web workflow used by an owner. Normal later VS Code application uploads do
not rewrite that partition. Regenerate/reinstall airport data only when the intended
region or source data changes.

## Persistent partition

The custom 16 MB partition layout reserves a dedicated `airports` data partition
for `airports.radarapt`.

The airport package is independent of:

- NVS settings
- both OTA application slots
- ADS-B response storage
- radar target capacity
- LVGL memory
- the firmware GitHub-release package

Product 97 physical testing confirmed that an installed persistent airport package
survives all ordinary application-update paths currently used by the project:

- private `waveshare-s3-touch-lcd-7` VS Code/PlatformIO upload
- distribution `waveshare-s3-touch-lcd-7-factory` VS Code/PlatformIO upload
- normal `.radarota` OTA update

Those operations are not factory resets. The explicit factory installer is different:
it performs a whole-chip erase and therefore removes the airport database, NVS owner
settings, MQTT state, and other saved flash state before provisioning the device.

## OurAirports source data

The browser and developer tools combine the public OurAirports datasets:

- `airports.csv` for identifier, name, type, coordinates, and elevation
- `runways.csv` for longest open runway and heading

Airport categories are generated as follows:

- Large and medium airports: **Major**
- Heliports: **Heliport**
- Small airports with scheduled service, a US K-code, or a short local code:
  **Public**
- Other small airports: **Private**

OurAirports does not provide one universal worldwide public/private field, so
the small-airport split is an awareness-oriented heuristic.

The radar's existing `AUTO / SHOW / HIDE` controls can correct individual label
preferences.

## Troubleshooting

### Saved coordinates do not appear after entering the code

Confirm the six-digit code is correct and that valid latitude/longitude have
already been saved on the radar's System page.

The authenticated airport status response exposes only the radar's current saved
home coordinates; the page does not use a hard-coded user location.

### Build fails while downloading airport data

The phone/computer needs Internet access to retrieve the public OurAirports CSV
files while still being able to reach the radar on the local network.

Try the build again from a device/network arrangement that can reach both.

### Upload reports a connection reset

Do not repeatedly force partial uploads.

The current airport uploader uses the same bounded READY/settle/retry discipline
as the proven firmware uploader. A retry is allowed automatically only when both
browser and radar state prove zero bytes were transferred.

If a reset persists, use the downloaded/generated package with the Advanced
manual installer for diagnosis and capture the radar serial output.

### Radar boots without a regional database

The persistent airport package is missing, unavailable, or failed validation.

Open the Airport Database page and build/install a regional package, or generate
`release\airports.radarapt` with the PC-side builder and install it from the
**Advanced** existing-package section. Product 97 has no compiled regional airport
fallback; both private and distribution firmware require the same persistent package.

### An airport category is imperfect

The source data does not contain one universal worldwide public/private field.

Use the Airports page's stable-identifier `AUTO / SHOW / HIDE` controls for label
preference corrections.

## Files involved

Runtime:

```text
include/airport_package_format.h
include/airport_store.h
src/airport_package_format.cpp
src/airport_store.cpp
src/airport_data.cpp
src/ota_update.cpp
partitions/airport_separation_16MB.csv
```

Browser and developer generation:

```text
tools/airport_package.py
tools/generate_airport_database.py
tools/airport_database_setup.py
tools\Build Airport Database.bat
```

Focused tests include package-format, persistent-storage, installer,
browser-builder, upload-handoff, automatic-restart, saved-location-prefill, and
existing airport-rendering/directory regressions.

## Safety and data source

Airport information is for visual awareness only and is not suitable for
navigation.

OurAirports publishes its airport/runway datasets as public data without a
guarantee of accuracy or fitness for a particular use.
