# BILLS Aircraft Radar — Waveshare ESP32-S3-Touch-LCD-7

PlatformIO firmware for the exact 7-inch 800×480 Waveshare ST7262 + GT911
board.

## Product 15

Current firmware marker:

`7IN-20260721-PRODUCT15-HARDENED`

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
- Shows a compact radar status line such as
  `18 AIRCRAFT | 20 MI | LIVE 6s`.
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
  `~/.platformio/workspaces/bills_aircraft_radar`, outside Google Drive, to
  prevent Drive File Stream locking generated `.pio` files.

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

`include/config.h` and `*.env` are ignored by Git. The private `config.h` may
remain in this personal Google Drive project so it continues to build normally;
do not intentionally commit or share it.

## Expected first-boot output

Serial should show:

- `Build: 7IN-20260721-PRODUCT15-HARDENED`
- `PSRAM: YES`
- display initialization messages
- Wi-Fi IP and RSSI
- `ADSB fetch task started on core 0`
- `Published N aircraft in N ms`

The display still loads and remains usable if Wi-Fi or adsb.fi is temporarily
unavailable.

## Verified build

Product 15 was compiled and linked with the pinned PlatformIO environment and a
placeholder local `config.h`:

- Flash: 1,928,921 / 6,553,600 bytes (29.4%)
- Internal RAM: 196,620 / 327,680 bytes (60.0%)

The test build did not use or overwrite the private Wi-Fi credentials.

## Important

This project is for the exact **Waveshare ESP32-S3-Touch-LCD-7, 800×480,
ST7262 + GT911** board. It is not for the 7B, 7C-BOX, P4, or a generic 7-inch
ESP32-S3 panel.

If an older Drive-synced `.pio` folder remains in the project, it is no longer
used and can be ignored or deleted after PlatformIO is closed. Do not delete
`src`, `include`, `assets`, `platformio.ini`, or `README.md`.
