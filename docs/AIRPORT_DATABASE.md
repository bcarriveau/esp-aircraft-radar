# Airport Database Setup

Bill's Aircraft Radar keeps aircraft and airport data on separate paths:

- **Aircraft** are received from ADS-B over the network.
- **Airport data** is stored offline in the dedicated persistent `airports`
  flash partition.
- A checked-in compiled airport table remains in firmware only as the safe
  fallback when the persistent partition is empty, unavailable, or invalid.

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

If persistent storage is unavailable or invalid at boot, the runtime uses the
compiled fallback table instead.

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

It does **not** rebuild firmware, does **not** modify the checked-in compiled
fallback header, and does **not** change the location saved on the radar.

The lower-level generator remains available for tests and developer workflows:

```text
python tools/generate_airport_database.py airports.csv \
  --runways-csv runways.csv \
  --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE \
  --radius 120 \
  --coverage "YOUR REGION"
```

The Python package implementation is also used as the reference implementation
for browser-builder parity tests.

## Compiled fallback database

`include/generated_airport_database.h` remains intentionally tracked.

It is no longer the normal per-user regional database. It is the firmware's
known-good fallback so the Airports feature can still operate when the dedicated
persistent package is missing or invalid.

Do not remove the compiled fallback merely because a persistent package is
installed.

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

Updating firmware does not intentionally erase the user's saved airport package
or their saved NVS home coordinates.

A partition-table-changing USB flash operation is different from an ordinary
firmware OTA update and should be treated accordingly.

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

### Radar boots with compiled fallback

The persistent airport package is missing, unavailable, or failed validation.

Open the Airport Database page and install a newly generated package. The compiled
fallback remains available by design.

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
