# Airport Database Setup

The aircraft radar receives **aircraft** from ADS-B over the network, but its
**airport** information is stored offline inside the firmware.

There are two separate location actions:

- **Moving nearby:** Change latitude and longitude on the radar's **System** page.
  The ESP32 immediately rebuilds its 90-mile nearby-airport cache from the airport
  records already compiled into the firmware.
- **Moving to another region:** Generate a new regional airport table on the
  computer, then build and upload the firmware again.

Changing coordinates on the display cannot download airports that were never
compiled into the firmware.

## Easiest Windows method

From the project folder, double-click:

```text
 tools\Build Airport Database.bat
```

The guided setup asks for:

1. The new home latitude.
2. The new home longitude.
3. A coverage radius. Press Enter to use the recommended **120 miles**.
4. A short region name, such as `CENTRAL IOWA`.
5. Whether to download current OurAirports data, use the previous download, or
   select CSV files already on the computer.

Before changing the project, it shows the number of major, public, private, and
heliport records plus approximate flash use. Nothing is replaced until you answer
`Y` at the final confirmation.

The tool then replaces only:

```text
include/generated_airport_database.h
```

It validates the finished header. If validation fails, the previous header is
restored automatically.

## After generation

1. Build the normal PlatformIO project.
2. Upload by USB, or upload `release/firmware.radarota` through the local OTA page.
3. Enter the same home coordinates on the radar's **System** page.

The setup tool does not build or upload firmware by itself.

## What belongs in Git

Commit these project files:

- `tools/Build Airport Database.bat`
- `tools/airport_database_setup.py`
- `tools/generate_airport_database.py`
- `tests/test_airport_database.py`
- `tests/test_airport_generator.py`
- `docs/AIRPORT_DATABASE.md`
- `include/generated_airport_database.h` after intentionally generating a new region

The generated header stays tracked because it is the exact regional database that
will be compiled into the firmware.

Do not commit downloaded `airports.csv` or `runways.csv`, Python cache files,
interrupted temporary files, or delivery-only package notes. Product 57 adds
matching `.gitignore` rules. The guided tool normally stores its downloaded CSV
cache outside the repository anyway.

## Why 120 miles?

The radar displays 20, 40, or 80 miles and keeps airports within 90 miles in its
bounded PSRAM cache. A 120-mile generated region gives a useful margin if the home
coordinates move locally later.

A wider generated radius uses more flash and takes longer to scan at startup. It
does not increase the radar's 80-mile display range or the 90-mile runtime cache.

## What the generator includes

The tool combines the public-domain OurAirports datasets:

- `airports.csv` for identifier, name, type, coordinates, and elevation.
- `runways.csv` for the longest open runway and its heading.

The downloaded public CSV files are cached outside the project folder so they do
not appear as Git changes. The entered home coordinates are used only while
selecting the regional records and are not written into generation metadata.

Airport categories are generated as follows:

- Large and medium airports: **Major**
- Heliports: **Heliport**
- Small airports with scheduled service, a US K-code, or a short local code:
  **Public**
- Other small airports: **Private**

OurAirports does not provide one universal worldwide public/private field, so the
small-airport split is an awareness-oriented heuristic. The radar's existing
`AUTO / SHOW / HIDE` controls can correct individual label preferences.

## Command-line method

Advanced users can run the lower-level generator directly:

```text
python tools/generate_airport_database.py airports.csv \
  --runways-csv runways.csv \
  --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE \
  --radius 120 \
  --coverage "CENTRAL IOWA"
```

Use `--dry-run` to preview counts without replacing the generated header.

## Troubleshooting

### The radar shows aircraft but no airports

The saved coordinates may be outside the region represented by the compiled
airport header. Run the setup tool for the new region, rebuild, upload, and enter
the same coordinates on the System page.

### Download fails

Run the tool again and choose the cached copy. If no cache exists, download
`airports.csv` and `runways.csv` from the official OurAirports data page and choose
option 3.

### Python is not found

Open a PlatformIO terminal in the project and run:

```text
python tools/airport_database_setup.py
```

### A private field is categorized incorrectly

The source data does not contain a universal public/private field. Use the
Airports page's individual `AUTO / SHOW / HIDE` control for its label.

## Safety and data source

Airport information is for visual awareness only and must not be used for
navigation. OurAirports releases the datasets to the public domain without a
guarantee of accuracy or fitness for use.
