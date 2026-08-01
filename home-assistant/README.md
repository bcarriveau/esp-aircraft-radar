# Home Assistant dashboard

Product 56 publishes Home Assistant MQTT discovery for the radar device and its
entities. MQTT is disabled by default on the radar and must be configured in the
private `include/config.h` file before it can be enabled from the System page.

## Requirements

- A working Home Assistant MQTT integration and broker.
- MQTT discovery enabled in Home Assistant.
- Product 56 connected to the same broker.
- No HACS components or custom cards are required.

The dashboard uses the combined **Data Status** entity rather than exposing a separate
last-update-age sensor. Product 56 R4 clears the older retained discovery configuration
for that sensor automatically after MQTT reconnects.

## Install the view

1. Enable MQTT on the radar's **System > HA MQTT** panel.
2. Confirm that the device appears under **Settings > Devices & services > MQTT**.
3. Open the target dashboard in Home Assistant and choose **Edit dashboard**.
4. Add a new view, open that view's YAML editor, and replace the view YAML with
   the contents of `aircraft-radar-view.yaml`.
5. Save the view.

The supplied YAML expects the default entity IDs created by Product 56. If an
entity ID already existed when discovery first ran, Home Assistant may append a
number. In that case, update the corresponding entity IDs in the view YAML.

## Behavior

- The dashboard controls the physical display backlight, the shared 20/40/80-mile
  radar range, and the normal non-overlapping ADS-B refresh command.
- Aircraft selection and tracking remain local to the radar display.
- OTA remains a physically armed local-browser workflow; Home Assistant receives
  read-only OTA status only.
- Disabling MQTT on the radar stops and destroys the MQTT client and releases its
  PSRAM snapshot and JSON buffers. It does not disconnect Wi-Fi or alter ADS-B
  operation.
