#!/usr/bin/env python3
"""Focused Product 56 MQTT, Home Assistant, and resource-isolation checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_product_56_mqtt_is_optional_bounded_and_isolated() -> None:
    build = read("include/build_info.h")
    config = read("include/config.example.h")
    mqtt = read("src/mqtt_service.cpp")
    mqtt_header = read("include/mqtt_service.h")
    settings = read("src/settings.cpp")
    main = read("src/main.cpp")
    ota = read("src/ota_update.cpp")
    ui = read("src/ui.cpp")
    radar_control = read("src/radar_control.cpp")
    display_power = read("src/display_power.cpp")
    platformio = read("platformio.ini")

    assert "7IN-20260802-PRODUCT67-RADAR-GAP-ATTRIBUTION" in build
    assert "INACTIVE = 0" in mqtt_header
    assert "State::DISABLED" not in mqtt and "State::DISABLED" not in ui
    assert "#define MQTT_ENABLED_DEFAULT 0" in config
    assert 'constexpr const char* KEY_MQTT_ENABLED = "mqtt_on";' in settings
    assert "bool setMqttEnabled(bool enabled)" in settings
    assert "knolleary/PubSubClient@2.8" in platformio

    # Disabled mode must not construct the MQTT client or allocate target PSRAM.
    disabled_branch = mqtt[mqtt.index("if (!localMaintenanceRequested"):
                           mqtt.index("processCommands();")]
    assert "!localDesiredEnabled" in disabled_branch
    assert "!localClientPresent" in disabled_branch
    assert "!localBufferPresent" in disabled_branch
    assert "return;" in disabled_branch
    assert "new (std::nothrow) PubSubClient" not in disabled_branch
    assert "heap_caps_calloc" not in disabled_branch
    start_client = mqtt[mqtt.index("bool startClient"):
                        mqtt.index("void writeDevice")]
    assert "heap_caps_calloc" in start_client
    assert "heap_caps_malloc" in start_client
    assert "jsonBuffer" in start_client
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in mqtt
    assert "MQTT_PACKET_BUFFER_BYTES = 384" in mqtt
    assert "MQTT_WRITE_CHUNK_BYTES = 512" in mqtt
    assert "new (std::nothrow) PubSubClient" in mqtt
    assert "beginPublish(topic" in mqtt
    assert "mqttClient->write(" in mqtt
    assert "mqttClient->endPublish()" in mqtt
    assert "esp_mqtt_client_" not in mqtt
    assert "MQTT_TASK_STACK_BYTES" not in mqtt
    assert "MQTT_OUTBOX_BYTES" not in mqtt

    # MQTT is an optional task-free main-loop service and never owns station recovery.
    assert "mqtt_service::begin()" in main
    assert main.index("adsb::service();") < main.index("mqtt_service::service();")
    assert main.index("mqtt_service::service();") < main.index("ota_update::service();")
    forbidden = (
        "WiFi.begin(", "WiFi.disconnect(", "WiFi.mode(", "setInsecure(",
        "HTTPClient::GET()", "ESP.restart(", "xTaskCreatePinnedToCore(",
    )
    for token in forbidden:
        assert token not in mqtt

    # Commands are queued by the MQTT callback and applied from service context.
    assert "pendingCommands |= command" in mqtt
    assert "COMMAND_DISPLAY_ON" in mqtt and "COMMAND_DISPLAY_OFF" in mqtt
    assert "radar_control::setManualRangeMiles(20.0f)" in mqtt
    assert "radar_control::setManualRangeMiles(40.0f)" in mqtt
    assert "radar_control::setManualRangeMiles(80.0f)" in mqtt
    assert "adsb::requestRefresh();" in mqtt
    assert 'payloadEquals(payload, payloadLength, "PRESS")' in mqtt
    assert "mqttClient->setCallback(mqttMessageHandler)" in mqtt
    assert "if (fetchActive) return;" in mqtt
    assert "if (app_state::fetchInProgress()) return;" in mqtt
    assert "Publish at most one retained state message per service interval" in mqtt
    assert "app_state::setRadarRangeMiles(rangeMiles)" in radar_control
    assert "radar::clearAirportFocus();" in radar_control
    assert "toggle_backlight(backlightState);" in display_power

    # Home Assistant discovery is stable, bounded, and includes no remote OTA action.
    assert "DISCOVERY_COUNT = 12" in mqtt
    assert mqtt.count('writer.key("options");') == 1
    assert '"homeassistant/status"' in mqtt
    assert "COMMAND_REDISCOVER" in mqtt
    assert "sensor.bills_aircraft_radar_last_update_age" not in mqtt
    assert 'writer.key("update_age_s")' not in mqtt
    assert '"homeassistant/sensor/%s_update_age/config"' in mqtt
    assert "publish(topic, &EMPTY_PAYLOAD, 0, true)" in mqtt
    for entity_id in (
        "switch.bills_aircraft_radar_display",
        "select.bills_aircraft_radar_range",
        "button.bills_aircraft_radar_refresh",
        "sensor.bills_aircraft_radar_aircraft_count",
        "sensor.bills_aircraft_radar_data_status",
        "sensor.bills_aircraft_radar_tracked_aircraft",
        "sensor.bills_aircraft_radar_nearest_traffic",
        "sensor.bills_aircraft_radar_airspace_summary",
    ):
        assert entity_id in mqtt
    assert "NEAREST_COUNT = 5" in mqtt
    assert "MAX_TARGETS" in mqtt
    assert "command/ota" not in mqtt.lower()
    assert "command/reboot" not in mqtt.lower()

    # Product 54 OTA now waits for both independent network services.
    assert "mqtt_service::requestMaintenanceHold();" in ota
    assert "mqtt_service::maintenanceHoldActive()" in ota
    assert "mqtt_service::releaseMaintenanceHold();" in ota
    assert "Waiting for network services to become idle" in ota
    assert "requestMaintenanceHold" in mqtt_header

    # The physical UI can enable/disable MQTT but does not expose credentials.
    assert "HOME ASSISTANT MQTT" in ui
    assert "ENABLE MQTT" in ui
    assert "DISABLE MQTT" in ui
    assert "MQTT_PASSWORD" not in ui
    assert "MQTT_USERNAME" not in ui


def test_dashboard_uses_only_builtin_cards_and_discovered_entities() -> None:
    dashboard = read("home-assistant/aircraft-radar-view.yaml")
    instructions = read("home-assistant/README.md")

    assert "type: sections" in dashboard
    for card in ("type: glance", "type: tile", "type: button",
                 "type: markdown", "type: entities"):
        assert card in dashboard
    assert "custom:" not in dashboard
    assert "HACS" not in dashboard
    assert "sensor.bills_aircraft_radar_nearest_traffic" in dashboard
    assert "sensor.bills_aircraft_radar_tracked_aircraft" in dashboard
    assert "select.bills_aircraft_radar_range" in dashboard
    assert "switch.bills_aircraft_radar_display" in dashboard
    assert "button.bills_aircraft_radar_refresh" in dashboard
    assert "sensor.bills_aircraft_radar_last_update_age" not in dashboard
    assert "perform_action: button.press" in dashboard
    assert dashboard.count("content: |") == 3
    assert "No HACS components or custom cards are required" in instructions


def test_private_configuration_is_not_present() -> None:
    assert not (ROOT / "include" / "config.h").exists()
