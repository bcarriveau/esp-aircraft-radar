# BILLS Aircraft Radar — Waveshare ESP32-S3-Touch-LCD-7

PlatformIO firmware for the exact 7-inch 800×480 Waveshare ST7262 + GT911
board.

## Product 21 tracked-heading candidate

Current firmware marker:

`7IN-20260721-PRODUCT21-TRACKED-HEADING`

Product 21 builds on the physically working Product 18 TLS baseline with the
following radar and Setup improvements:

- Removes the 160-mile selection; available radar ranges are 20, 40, and 80
  miles.
- Predictively steps the radar outward from 20 to 40 or 40 to 80 miles before
  the tracked aircraft leaves the current view. Tracking never automatically
  zooms back in.
- Uses a screen-level popup keyboard for the Setup entry fields.
- Removes the redundant status overlay from the radar's upper-left corner.
- Measures identifier text with LVGL so aircraft labels stay on one line.
- Changes the left aircraft panel from `NEAREST` to `TRACKED` while tracking
  and shows the tracked aircraft's live information.
- Adds a rotating travel-heading arrow and heading value above Data Status for
  both nearest and tracked aircraft.
- Keeps the panel in tracked mode if the selected aircraft temporarily drops
  out of a response instead of silently substituting the nearest aircraft.
- Opens details for whichever aircraft the left panel currently represents.

Product 21 has not yet completed a compile/link or physical-display test. The
latest physically confirmed TLS baseline is Product 18 at commit `69dce612`.
The permanent rollback baseline remains tagged `product-15-hardened`.

## Product 18 certificate-bundle TLS baseline

Product 18 corrects the native HTTPS configuration rejected during the first
Product 17 physical test. It attaches Espressif's full CA certificate bundle
and keeps hostname verification enabled. Product 17 failed locally before any
network TLS handshake because no server-verification method was configured.

Product 17 introduced the native transport while keeping the Product 15
display, UI, data-publication, and recovery
architecture while replacing the TLS transport that continued to time out in
Product 16:

- Uses ESP-IDF's native streaming HTTPS client instead of Arduino's
  `NetworkClientSecure` handshake plus the hand-written HTTP parser.
- Re-resolves DNS and creates a completely fresh native client for each retry;
  both attempts are no longer pinned to one Cloudflare address.
- Keeps all HTTPS work on the existing core-0 network task and does not
  reintroduce `HTTPClient::GET()` on the UI task.
- Preserves the request/body deadlines, response-size guard, PSRAM payload,
  generation rejection, and single-snapshot publication architecture.
- Logs RSSI, the native ESP-IDF error, socket errno, and TCP-vs-TLS diagnosis
  when a connection fails.
- Recycles Wi-Fi only for Wi-Fi, DNS, or TCP failures. TLS, HTTP, response-body,
  and JSON failures now retry without deliberately disconnecting a healthy
  station link.

Product 18 passed compile/link verification and the initial physical TLS test.
It remains subject to long-term soak testing. The known Product 15 rollback
baseline remains tagged as `product-15-hardened`.

## Product 15 hardened baseline

Product 15 combines the reliability, workload, interface, Setup, aircraft
classification, and diagnostics work into one release. It preserves the
proven RGB anti-rolling configuration from Product 14.

### Live ADS-B reliability

- Polls adsb.fi on a true 15-second start-to-start schedule.
- Runs all TLS requests and Wi-Fi recovery on one core-0 network task, so
  requests cannot overlap.
- Uses explicit TLS connect, header, body-idle, and total-response deadlines.
- Classifies Wi-Fi, DNS, TCP, TLS, HTTP status, HTTP header, response body, and
  JSON failures separately.
- Retries normally first, recycles Wi-Fi after repeated failures, progressively
  backs off during a prolonged outage, and restarts only as a last resort after
  30 minutes and at least 20 consecutive failures.
- Gives every range or location change a generation number. An old 80-mile
  response cannot overwrite a newer 20-mile selection.
- Keeps the last good aircraft visible while status changes between `LIVE`,
  `UPDATING`, `STALE`, and `OFFLINE`.
- Shows response duration, response size, received/accepted/visible counts,
  failure stage, recovery count, and discarded-response count on System.

### Display and radar

- Keeps radar animation at 12.5 FPS while data labels and page content update
  only when their underlying version changes.
- Copies targets, selected range, and tracking state under one mutex per radar
  frame instead of locking once per aircraft.
- Product 15 originally showed a compact upper-left radar status line. Product
  20 removed that overlay because it duplicated other status information and
  interfered with aircraft-label placement.
- Shows one collision-aware identifier per 20-mile contact. Airline/charter
  traffic uses flight/callsign; GA/private traffic without a separate flight
  identifier falls back to registration.
- Always gives the tracked aircraft first label priority and displays only:

  ```text
  TRACKED N123AB
  358 MPH
  ```

- Provides a conditional `STOP TRACK` button in the radar's lower-right corner.
- Retains the dedicated Tracks icon column and the three-line nearest-aircraft
  cards.
- Stops radar rendering when another page is active.

### Setup and diagnostics

- Shows the currently saved SSID rather than the compile-time fallback.
- Masks the saved Wi-Fi password with a `SHOW` / `HIDE` control.
- Strictly validates latitude and longitude; invalid text can no longer become
  `0,0` silently.
- Reconnects through the network task after Wi-Fi credentials change.
- Separates `RETRY ADSB NOW` from `RECONNECT WIFI` so neither button tears down
  an active TLS request.
- Requires a second tap within five seconds before resetting defaults.
- Restores the default title as `BILLS AIRCRAFT RADAR`.
- Shows build ID, uptime, Wi-Fi/IP/RSSI, data age, last fetch duration and size,
  failure stage/count, memory low-water marks, and aircraft counts on System.

### Aircraft categories

Aircraft types now use an explicit category table instead of loose prefixes:

- Airliner
- Business jet
- Military/heavy
- Turboprop
- Piston
- Helicopter
- Unknown

This specifically prevents `PC24`, `BE40`, and `C17` from receiving piston or
propeller artwork. Unknown ICAO types use the unknown graphic instead of being
guessed as an airliner.

## Hardware and build configuration

- ST7262 RGB LCD, GT911 capacitive touch, and CH422G I/O expander.
- Espressif Arduino 3.0.7 high-performance XIP-on-PSRAM framework.
- 64-byte ESP32-S3 cache lines and `-O2` from the high-performance framework.
- 20-scanline RGB bounce buffer in `waveshare_panel_board.h`.
- Exact display, I/O expander, LVGL, and ArduinoJson versions are pinned in
  `platformio.ini`.
- PlatformIO build objects and dependencies live under
  `~/.platformio/workspaces/bills_aircraft_radar`, outside the repository, to
  keep generated `.pio` files out of version control.

Do not replace the high-performance framework, reduce the bounce buffer, or
change the Waveshare panel timing unless specifically testing the RGB rolling
fix.

## Before building

1. Keep your private `include/config.h` in place. For a new checkout, copy
   `include/config.example.h` to `include/config.h` and enter Wi-Fi credentials
   plus the radar center coordinates.
2. Open the project folder in VS Code with PlatformIO.
3. Connect the board's programming USB-C port.
4. Build, upload, and open Serial Monitor at 115200.

`include/config.h` and `*.env` are ignored by Git. Keep the private
`include/config.h` only in the local working copy; never commit or share it.

## Expected first-boot output

Serial should show:

- `Build: 7IN-20260721-PRODUCT21-TRACKED-HEADING`
- `PSRAM: YES`
- display initialization messages
- Wi-Fi IP and RSSI
- `ADSB fetch task started on core 0`
- `Published N aircraft in N ms`

The display still loads and remains usable if Wi-Fi or adsb.fi is temporarily
unavailable.

## Build-verification status

Product 18 was compiled and linked with the pinned PlatformIO environment and a
placeholder local `config.h`:

- Flash: 2,011,253 / 6,553,600 bytes (30.7%)
- Internal RAM: 197,060 / 327,680 bytes (60.1%)

The test build did not use or overwrite the private Wi-Fi credentials.

Product 21 has not yet been compile-verified. Build and upload it locally, then
confirm the marker, PSRAM detection, the 20-scanline bounce buffer, tracking,
automatic range changes, label layout, popup keyboard, page switching, and
screen stability during HTTPS activity.

## Important

This project is for the exact **Waveshare ESP32-S3-Touch-LCD-7, 800×480,
ST7262 + GT911** board. It is not for the 7B, 7C-BOX, P4, or a generic 7-inch
ESP32-S3 panel.

Do not commit generated `.pio` content. Do not delete `src`, `include`,
`assets`, `platformio.ini`, or `README.md`.
