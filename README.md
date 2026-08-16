# ESP AIRCRAFT RADAR

A dedicated 7-inch ESP32-S3 ADS-B aircraft radar display built for the
**Waveshare ESP32-S3-Touch-LCD-7**.

The radar shows nearby aircraft on a touch-screen 20 / 40 / 80 mile
display, supports stable aircraft selection and tracking, shows nearby
airports from an offline regional database, and includes local web tools
for firmware and airport-database maintenance.

This repository targets one exact hardware platform. It is **not** a
generic ESP32 display project.

## Hardware

Required hardware:

-   **Board:** Waveshare ESP32-S3-Touch-LCD-7
-   **Display:** 7-inch 800x480 RGB LCD with ST7262 controller
-   **Touch:** GT911 capacitive touchscreen
-   **I/O expander:** CH422G
-   **Processor:** ESP32-S3
-   **Flash:** 16 MB
-   **Memory:** OPI PSRAM
-   **USB:** data-capable USB connection for the initial factory install
    or recovery
-   **Network:** 2.4 GHz Wi-Fi with Internet access for live ADS-B data

Firmware stack:

-   PlatformIO
-   Arduino C++
-   Arduino-ESP32 3.0.7 high-performance build
-   LVGL 8.3.11

Do not use this firmware on Waveshare 7B/7C boards, ESP32-P4 boards,
Cheap Yellow Displays, generic 7-inch panels, ESPHome hardware, or
e-paper displays.

## Getting Started / First Setup

A new radar has two different stages:

1.  **Put the complete factory software on the blank device.**
2.  **Configure that device for its owner.**

After that, normal firmware updates use a separate, non-destructive
update path.

> **Important:** A factory install and a normal firmware update are not
> the same thing.
>
> **Factory install = full 16 MB flash erase.** It removes saved Wi-Fi,
> location, MQTT/Home Assistant settings, airport data, OTA state, and
> all other owner-specific flash state.
>
> **Normal public firmware update = application update.** It is designed
> to preserve owner settings in NVS and the separately stored airport
> database.
>
> Do **not** use the factory installer for routine updates.

### What a new owner needs

For the normal owner workflow:

-   the correct Waveshare ESP32-S3-Touch-LCD-7
-   a data-capable USB cable
-   a computer with current Chrome or Edge for the first factory
    installation
-   Internet access while using the browser factory installer
-   a 2.4 GHz Wi-Fi network for the radar
-   a phone or computer on the same local network for the radar's
    maintenance web pages

A normal owner does **not** need to understand ESP32 partitions, OTA
slots, NVS, or flash addresses.

A normal owner also does **not** need Python or PlatformIO to generate
their regional airport database.

## Factory Installation

Use a factory install only for:

-   a brand-new/blank radar
-   recovery from unknown or corrupt flash contents
-   a deliberate complete reset to new-owner state
-   a firmware transition that specifically requires a full factory
    reinstall

### Factory install warning

The factory installer deliberately erases the **entire 16 MB flash
chip** before installing the verified distribution image.

That destroys:

-   saved Wi-Fi credentials
-   saved home latitude/longitude
-   MQTT/Home Assistant settings
-   the installed regional airport database
-   OTA state
-   all other owner-specific flash values

If the radar is already working and you only want a newer firmware
version, see **Normal Firmware Updates** instead.

### Preferred browser factory installer

Public factory bundles contain an owner-facing file:

``` text
INSTALL_RADAR.html
```

The generated factory bundle also contains the verified firmware images
and manifest needed by that page.

For a local/extracted factory bundle:

1.  Extract the complete factory bundle to a folder.
2.  Open `INSTALL_RADAR.html` in a current Chrome or Edge browser.
3.  When the page asks for the bundle folder, choose the same extracted
    factory folder.
4.  Connect the radar to the computer with a data-capable USB cable.
5.  Follow the browser's connection prompt.
6.  Read the destructive-install warning carefully.
7.  Confirm the warning only if you really intend to erase the device.
8.  Allow the installer to verify the package and connected hardware.
9.  Keep USB power connected until flashing and verification finish and
    the radar restarts.

The browser installer verifies the factory manifest, hardware identity,
expected four-image flash layout, file sizes, SHA-256 hashes, the
distribution-build marker, build identity, ESP32-S3 chip identity, and
16 MB flash size before it performs the full erase.

The owner must explicitly acknowledge the destructive warning and type:

``` text
ERASE RADAR
```

The generated browser installer uses the pinned Espressif `esptool-js`
dependency over HTTPS. Opening the local installer therefore still
requires Internet access for that dependency.

### Offline/developer recovery installer

The same factory bundle also contains:

``` text
FLASH_RADAR_FACTORY.ps1
```

This is the offline/developer recovery path. It uses the same manifest
and verified factory images as the browser installer, but it requires a
suitable Python/esptool environment and serial-port access.

Most owners should use `INSTALL_RADAR.html`.

For deeper factory-install details, see `docs/FACTORY_INSTALL.md`.

## What Happens on First Boot

A public factory/distribution image contains neutral owner defaults. It
does not contain the developer's private Wi-Fi, location, or MQTT
credentials.

A true factory-installed device starts with:

-   blank Wi-Fi SSID
-   blank Wi-Fi password
-   neutral `0,0` location
-   MQTT disabled with no private broker/user/password
-   no owner-specific regional airport database

MQTT broker credentials are runtime owner state once they have been seeded into
NVS by a private development build. Public/distribution firmware reads those saved
values and does not contain private broker credentials of its own.

The radar then requires normal owner setup.

### Initial Wi-Fi and location setup

On the radar, use the normal setup/System interface to enter:

1.  the owner's 2.4 GHz Wi-Fi network name
2.  the owner's Wi-Fi password
3.  the radar's actual home latitude
4.  the radar's actual home longitude
5.  any other optional owner settings that are appropriate

Save the settings and allow the radar to connect.

The saved latitude/longitude becomes the radar center used for aircraft
distance/bearing and the nearby-airport cache.

The owner's location is runtime configuration. A normal owner does
**not** need a custom firmware build for their location.

## Airport Database Setup

Aircraft data and airport data are separate:

-   **Aircraft** are received live over the network.
-   **Airport data** is stored offline in a dedicated persistent flash
    partition.

The normal airport setup is performed in the user's browser after Wi-Fi
and the home location have been saved.

### Install airports for the owner's region

1.  On the radar, open **System**.
2.  Arm/enable the local firmware/maintenance window.
3.  The radar displays the local web address and a six-digit access
    code.
4.  On a phone or computer connected to the same local network, open the
    displayed address.
5.  Open **AIRPORT DATABASE**.
6.  Enter the six-digit code shown on the radar.
7.  After authentication, the page prefills the radar's saved home
    coordinates when they are valid.
8.  Review the center and coverage radius. **120 miles** is the
    recommended default.
9.  Select **BUILD & INSTALL AIRPORT DATABASE**.
10. Keep the radar powered while the package is generated, uploaded,
    verified, written, and read back.

The browser downloads the public OurAirports airport and runway CSV
datasets, filters them locally around the requested region, builds a
bounded `.radarapt` package, and uploads that package to the radar.

The ESP32 does **not** download and parse the worldwide CSV files
itself.

The radar validates the complete package before changing airport flash
storage, writes only the dedicated airport partition, reads the stored
package back, validates it again, and restarts after a successful
install.

### If the radar moves

Changing the saved home latitude/longitude changes the radar center. It
does not rewrite the installed airport package.

If the new location is still covered by the installed regional package,
changing the saved location may be all that is needed.

If the radar moves outside the installed region, use **AIRPORT
DATABASE** again to build and install a new regional package. No
firmware rebuild is required.

For developer/offline package generation, Windows users can run:

``` text
tools\Build Airport Database.bat
```

or run:

``` text
python tools/airport_database_setup.py
```

Both produce release\airports.radarapt; they do not rebuild firmware
and do not change the location saved on the radar. To install the
generated package, open the radar's Airport Database page and
select/upload the generated airports.radarapt file.

The lower-level generator is also available for scripted/developer use:

``` text
python tools/generate_airport_database.py airports.csv \
  --runways-csv runways.csv \
  --latitude YOUR_LATITUDE \
  --longitude YOUR_LONGITUDE \
  --radius 120 \
  --coverage "YOUR REGION"
```

See `docs/AIRPORT_DATABASE.md` for the full browser and PC-side workflows.

## How to Reach the Radar's Web Pages

The maintenance web server is not left open during normal operation.

To use local maintenance pages:

1.  Put the radar and the phone/computer on the same local network.
2.  Open **System** on the radar.
3.  Arm the local firmware/maintenance window.
4.  Note the web address and six-digit access code shown by the radar.
5.  Open the displayed address in a browser.
6.  Enter the code when requested.

The maintenance site provides the owner-facing firmware update and
Airport Database workflows.

The bounded maintenance window and access code are intentional security
boundaries. Do not treat the maintenance web server as a permanently
open administration page.

## Normal Firmware Updates

Once the device has been factory-installed and configured, routine
firmware updates should use the normal public update path.

**Do not factory-erase the radar for a routine update.**

A normal public update:

-   uses the project's validated `.radarota` application package
-   writes the inactive OTA application slot
-   verifies the package/image before selecting it for boot
-   does not intentionally erase NVS
-   does not intentionally erase the dedicated airport partition
-   therefore preserves the owner's Wi-Fi/location, saved MQTT broker
    credentials, and installed regional airport database

Airport data uses the same dedicated persistent `airports` partition in private
development builds and public/distribution firmware. Regional airport data is never
part of the application image. Once installed, normal VS Code firmware uploads and
normal `.radarota` updates leave that database alone.

### Update from the radar

The radar can check the repository's stable release metadata. When a
newer compatible release is available, the Software Update interface can
show the validated release notes under **WHAT'S NEW** and, after
explicit user confirmation, download and install the compatible release.

Firmware is not silently installed just because a release exists.

### Upload a local `.radarota` package

For a compatible package obtained separately:

1.  Open **System** on the radar.
2.  Arm Firmware / OTA maintenance.
3.  Open the displayed local web address.
4.  Enter the six-digit code.
5.  Open the firmware update page.
6.  Select the intended `.radarota` package.
7.  Start the update.
8.  Keep the radar powered through verification and restart.

The updater accepts the project's validated package format rather than
treating an arbitrary firmware binary as a routine owner update.

## Recovery / Reinstall

Use the destructive factory path when the normal update path is not
appropriate, such as:

-   a blank board
-   corrupted or unknown flash contents
-   recovery where the normal firmware updater cannot be reached
-   an intentional complete owner-data reset

Recovery uses the factory bundle described in **Factory Installation**.

Remember that recovery by full factory install removes the owner's saved
settings and airport database. After recovery, repeat Wi-Fi/location
setup and install the regional airport database again.

A normal PlatformIO upload of the distribution environment is **not**
equivalent to a clean factory reset because it does not issue a
whole-chip erase. With the current Product 97 layout, physical testing confirmed
that a normal `waveshare-s3-touch-lcd-7-factory` VS Code/PlatformIO upload preserves
saved owner state and an installed persistent airport database. It remains a
developer/test operation, not the normal owner update path. With the current Product 97 layout, physical testing confirmed
that a normal `waveshare-s3-touch-lcd-7-factory` VS Code/PlatformIO upload preserves
saved owner state and an installed persistent airport database. It remains a
developer/test operation, not the normal owner update path. With the current Product 97 partition layout, physical testing
confirmed that a normal `waveshare-s3-touch-lcd-7-factory` VS Code/PlatformIO
upload preserves existing NVS owner settings and an installed persistent airport
database. It still changes the running application to the credential-safe
distribution variant, so it remains a developer/test operation rather than the
normal owner update path.

## The Three Software Paths

These paths are intentionally separate.

  ----------------------------------------------------------------------------------
  Path           Intended user  What it does        Owner settings  Publicly
                                                    / airports      distributable?
  -------------- -------------- ------------------- --------------- ----------------
  **Factory      New owner or   Verifies            **Destroyed**   Yes, using the
  install**      recovery       hardware/package,                   verified factory
                                erases the complete                 bundle
                                16 MB flash, then                   
                                installs                            
                                bootloader,                         
                                partition table,                    
                                boot app, and                       
                                distribution                        
                                firmware                            

  **Public       Existing owner Installs a          **Preserved by  Yes
  firmware                      validated           design**        
  update**                      `.radarota`                         
                                application update                  
                                through the OTA                     
                                path                                

  **Private      Developer only Builds/flashes the  Normally        **No**
  development                   normal development  preserved by    
  build**                       environment and may ordinary        
                                use ignored private upload, but     
                                defaults            this is **not** 
                                                    a public        
                                                    release path    
  ----------------------------------------------------------------------------------

### 1. Factory install --- destructive provisioning/recovery

Use the generated factory bundle and `INSTALL_RADAR.html` for a true
clean installation.

This is the only path in this table that deliberately performs a
complete flash erase.

### 2. Public firmware update --- normal owner update

Public OTA packages are generated from the credential-safe distribution
environment. They contain no private `include/config.h` data.

This is the routine update path after the radar has been set up.

### 3. Private development build --- developer only

The private development environment is:

``` text
waveshare-s3-touch-lcd-7
```

For routine developer USB uploads, use this private environment. The dedicated
`waveshare-s3-touch-lcd-7-factory` environment exists to build credential-safe public
artifacts. Its PlatformIO upload writes bootloader/partition/app images but is **not**
the same operation as the destructive browser factory installer and is **not** the
owner OTA workflow.

It may use:

``` text
include/config.h
```

That file is intentionally ignored by Git and may contain developer-only
defaults.

**Never commit, upload, package, or distribute `include/config.h`.**

A private build must never be substituted for a public release artifact.

Private builds also seed missing MQTT broker URI/username/password NVS keys from
`include/config.h`. After that one-time migration, the credential-safe public build
uses the saved NVS values, so a normal `.radarota` update does not need private
credentials compiled into the release.

The private build has a guarded developer convenience: after a true
factory boot, if the complete neutral owner tuple is still unchanged, a
private build can seed private Wi-Fi/password/location and the private
MQTT enabled default from `include/config.h`. That reseed behavior is
compiled out of public distribution builds and does not run after the
owner has changed any of the neutral owner values.

## Development Setup

This section is for developers who want to build the project from
source. It is not required for an owner using a prepared factory bundle
and later public OTA updates.

### Tools

Install:

-   Visual Studio Code
-   PlatformIO
-   Git
-   Python 3 for repository tooling and tests

### Clone the repository

``` bash
git clone https://github.com/bcarriveau/esp-aircraft-radar.git
cd esp-aircraft-radar
```

Before developing, confirm that the branch above is still the intended
branch for the work being performed.

### Private configuration

For a private development build, create the ignored private
configuration from the example:

``` bash
cp include/config.example.h include/config.h
```

On Windows, copy the file using Explorer, PowerShell, or another normal
file-copy method if the shell command above is not available.

Keep all private Wi-Fi/location/MQTT defaults in `include/config.h`.

Never commit or distribute that file.

### Private development build

PlatformIO environment:

``` text
waveshare-s3-touch-lcd-7
```

Build:

``` bash
pio run -e waveshare-s3-touch-lcd-7
```

This environment is for development. It does not generate the public
release/factory artifacts.

Regional airport data is **not** compiled into this private firmware. Development
units use the same persistent airport database as public units. Install it once from
the radar's Airport Database web page, or generate `release/airports.radarapt` with
`tools\Build Airport Database.bat` and install that package from the same page.
Normal later private firmware uploads leave the airport partition unchanged.

### Public distribution build

PlatformIO environment:

``` text
waveshare-s3-touch-lcd-7-factory
```

Build:

``` bash
pio run -e waveshare-s3-touch-lcd-7-factory
```

Despite the historical environment name, simply building or uploading
this environment is **not** a factory reset.

This environment is the credential-safe distribution build. It defines
`RADAR_DISTRIBUTION_BUILD`, places `include/distribution` before the
private include directory, and is the only environment allowed to
generate public `.radarota`, manifest, and factory-bundle artifacts.

A successful distribution build runs the release generators for:

-   the normal public OTA package/manifest
-   the destructive factory-install bundle

The factory bundle is generated under:

``` text
release/factory/product-<version>/
```

and contains the verified flash images, manifest, browser installer, and
recovery installer.

For release-generation details, see `docs/GITHUB_RELEASES.md` and
`docs/FACTORY_INSTALL.md`.

## Features

### Live radar

-   20, 40, and 80 mile radar ranges
-   heading-aware aircraft symbols
-   stable ICAO-hex identity for selection and tracking
-   amber selected aircraft
-   red tracked aircraft
-   tracked tag with `TRACKED`, identifier, and MPH
-   outward auto-zoom to keep a tracked aircraft visible
-   collision-aware labels
-   coherent single-snapshot radar rendering
-   last-good aircraft retention through temporary transport failures

### Radar interaction

Idle:

-   left side shows aircraft count, nearest aircraft, and data status
-   right side shows the nearest-aircraft list
-   `20 / 40 / 80` is the radar range control

Selected:

-   selected-aircraft details take right-panel priority
-   `INFO / TRACK / CLEAR` are the primary actions

Tracked:

-   `STOP TRACK` takes right-panel priority
-   tracking remains tied to stable ICAO identity rather than an array
    position

### Aircraft and airport pages

-   live Aircraft Profile
-   Tracks page with live aircraft
-   Airspace totals/categories and radar handoff
-   Airports directory and profiles
-   airport `AUTO / SHOW / HIDE` display controls
-   `SHOW ON RADAR`
-   System diagnostics and owner settings
-   optional Home Assistant MQTT discovery/support

## Architecture and Reliability Notes

The project intentionally retains several hardware- and
reliability-specific protections.

### ADS-B networking

-   Core 0 owns ADS-B fetch and Wi-Fi recovery
-   ADS-B requests do not overlap
-   fixed 15-second start-to-start polling cadence
-   native ESP-IDF HTTPS is the preferred transport
-   hardened verified HTTPS fallback is limited to eligible transport
    failures
-   no blocking `HTTPClient::GET()`
-   no `setInsecure()` TLS path
-   bounded header, body, idle, and absolute deadlines
-   PSRAM-first/PSRAM-only response handling where designed
-   rejection of conflicting or ambiguous HTTP framing
-   stale generation results cannot overwrite newer range/location state
-   last-good aircraft are retained through temporary failures

### Display and memory

-   Arduino-ESP32 3.0.7 high-performance XIP/PSRAM stack
-   OPI PSRAM and `BOARD_HAS_PSRAM`
-   Waveshare panel timing
-   DMA/anti-rolling behavior
-   20-scanline RGB bounce buffer
-   128 KiB LVGL pool
-   measured 12 KiB Core-0 ADS-B task stack
-   bounded 200-target PSRAM architecture

These are deliberate parts of the known-good design. Do not casually
change framework versions, panel timing, DMA behavior, bounce-buffer
configuration, target capacity, or memory ownership while working on
unrelated features.

### Airport storage

The custom 16 MB partition table includes a dedicated 512 KiB
airport-data partition while preserving the OTA application layout.

The persistent airport package format is `.radarapt`.

The installer validates the package before destructive airport-partition
work, enforces the partition capacity, writes only the airport
partition, and validates the stored copy after write.

NVS settings, firmware OTA slots, airport storage, ADS-B response
storage, LVGL memory, and radar target capacity are separate concerns.

## Web Tools / Pages

The radar's local maintenance site is used for two owner workflows:

### Firmware update

Accepts the project's validated `.radarota` package during an armed
maintenance window and authenticated session.

### Airport Database

Builds a regional airport package in the user's browser from public
OurAirports data and installs it into the dedicated airport partition.

The Airport Database page also contains an Advanced option for
installing an existing `.radarapt` package.

Both workflows use the radar's bounded maintenance window and six-digit
access code.

## Troubleshooting

### I already configured the radar. Which installer should I use for an update?

Use the normal public firmware update path.

Do **not** use `INSTALL_RADAR.html` unless you intentionally want to
erase the complete device and start over.

### The browser factory installer cannot connect to the radar

-   use current Chrome or Edge
-   use a data-capable USB cable
-   connect the radar directly to the computer when possible
-   make sure another serial monitor is not holding the device
-   reconnect and follow the browser's serial-device prompt

The installer must positively identify an ESP32-S3 with 16 MB flash
before destructive work.

### The local factory installer opens but cannot proceed

The generated installer is a local HTML file, but its pinned
`esptool-js` dependency is loaded over HTTPS. Confirm that the computer
has Internet access.

### The radar has no Wi-Fi after a factory install

That is expected for a public factory image. Public distribution
firmware contains blank Wi-Fi credentials.

Enter the owner's Wi-Fi settings and home location through the radar's
normal setup/System interface.

### I changed the radar's location but the airport list is wrong or incomplete

The saved home location and the installed airport package are separate.

If the new location is outside the package's coverage, open **AIRPORT
DATABASE** and generate/install a new region.

### Saved coordinates do not prefill on the Airport Database page

Confirm that:

-   valid coordinates have already been saved on the radar
-   the maintenance window is armed
-   the six-digit code was accepted

Coordinates are exposed to the page only through the authenticated local
status path.

### Airport database generation cannot download data

The phone/computer needs Internet access to retrieve the public
OurAirports CSV files while also being able to reach the radar on the
local network.

### Airport upload reports a connection reset

Do not repeatedly force a partially transferred package.

The uploader permits a large-transfer retry only when both browser and
radar state prove that zero bytes were transferred. If the problem
persists, use the Advanced existing-package installer for diagnosis and
capture serial output.

### The radar reports no regional airport database

The dedicated persistent airport package is missing, unavailable, or invalid.

Open the Airport Database page and build/install the regional package. Private
development firmware and public/distribution firmware use this same persistent
database; there is no separate compiled development fallback.

### A normal PlatformIO upload did not erase old settings

That is expected. A normal PlatformIO upload is not a whole-chip factory
erase.

Use the verified factory installer only when a true destructive reset is
intended.

## Repository Structure

``` text
assets/                 Aircraft and UI artwork
docs/                   User, factory, airport, and release documentation
home-assistant/          MQTT dashboard/support files
include/                 Interfaces and build identity
include/distribution/    Neutral public distribution configuration
partitions/              Custom 16 MB partition table
release/                 Current generated public release artifacts
scripts/                 OTA and factory release generators
src/                     Firmware implementation
tests/                   Focused host/source regression tests
tools/                   Airport, aircraft, and factory tooling
platformio.ini           Pinned PlatformIO environments
README.md                Current project and getting-started documentation
CHANGELOG.md             Detailed confirmed project history
```

Important credential boundary:

``` text
include/config.h
```

is private, ignored, and must never be committed, uploaded, included in
a public package, or used to produce public release artifacts.

Use:

``` text
include/config.example.h
```

as the safe checked-in example.

## Current Known-Good Source

At the time of this README cleanup, the intended development/source
branch is:

``` text
main
```

Current firmware identity:

``` text
Product 97
7IN-20260816-PRODUCT97-UNIFIED-AIRPORT-STORAGE
```

The durable firmware identity is the build marker in
`include/build_info.h`. Repository HEAD can advance for
documentation-only commits without changing the firmware identity.

The permanent hardened rollback baseline remains:

``` text
product-15-hardened
7IN-20260721-PRODUCT15-HARDENED
```

## History

The README intentionally describes the current system and workflows
rather than serving as a Product-by-Product development diary.

Detailed confirmed version history is maintained in `CHANGELOG.md`.

## License and Data Sources

Repository licensing and third-party notices are maintained in:

-   `LICENSE`
-   `LICENSES/`
-   `THIRD_PARTY_NOTICES.md`

Airport information is derived from public OurAirports datasets and is
for visual awareness only, not navigation.

ADS-B data availability and permitted use remain subject to the selected
provider's terms and service availability.
