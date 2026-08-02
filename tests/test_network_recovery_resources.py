#!/usr/bin/env python3
"""Focused Product 59 network-recovery ownership and memory checks."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


BUILD = read("include/build_info.h")
MAIN = read("src/main.cpp")
NETWORK = read("src/adsb_network.cpp")
NETWORK_HEADER = read("include/adsb_network.h")
MQTT = read("src/mqtt_service.cpp")
MQTT_HEADER = read("include/mqtt_service.h")
OTA = read("src/ota_update.cpp")


class NetworkRecoveryResourceTests(unittest.TestCase):
    def test_product_59_releases_unused_ble_controller_memory(self):
        self.assertIn(
            "7IN-20260802-PRODUCT60-OTA-PREPARE-IDEMPOTENT", BUILD
        )
        self.assertIn("esp_bt_controller_mem_release(ESP_BT_MODE_BLE)", MAIN)
        self.assertIn("MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT", MAIN)
        self.assertIn("Unused BLE controller memory released", MAIN)
        release = MAIN.index("releaseUnusedBluetoothControllerMemory();")
        settings = MAIN.index("settings::initialize()")
        self.assertLess(release, settings)

    def test_hard_recovery_reserves_network_before_mqtt_teardown(self):
        reserve = NETWORK[
            NETWORK.index("bool reserveHardWifiRecovery()") :
            NETWORK.index("bool prepareHardWifiRecovery()")
        ]
        self.assertIn("!maintenanceRequested && !wifiOperationPending", reserve)
        self.assertIn("wifiOperationPending = true;", reserve)

        prepare = NETWORK[
            NETWORK.index("bool prepareHardWifiRecovery()") :
            NETWORK.index("void finishHardWifiRecovery()")
        ]
        self.assertLess(
            prepare.index("reserveHardWifiRecovery()"),
            prepare.index("mqtt_service::requestNetworkRecoveryHold();"),
        )
        self.assertIn("mqtt_service::networkRecoveryHoldActive()", prepare)
        self.assertIn("MQTT quiesce timed out; deferring hard radio recycle", prepare)
        self.assertIn("NETWORK_QUIESCE_SETTLE_MS", prepare)

    def test_mqtt_resources_are_destroyed_before_recovery_ack(self):
        destroy = MQTT[
            MQTT.index("void destroyClient(bool graceful = true)") :
            MQTT.index("bool publishMessage(")
        ]
        self.assertLess(
            destroy.index("if (!graceful && networkClient) networkClient->stop();"),
            destroy.index("delete mqttClient;"),
        )
        self.assertIn("free(targetBuffer);", destroy)
        self.assertIn("free(jsonBuffer);", destroy)

        recovery = MQTT[
            MQTT.index("if (localNetworkRecoveryRequested)") :
            MQTT.index("// The normal disabled state")
        ]
        self.assertLess(
            recovery.index("destroyClient(false);"),
            recovery.index("networkRecoveryActive = true;"),
        )
        for api in (
            "requestNetworkRecoveryHold",
            "networkRecoveryHoldActive",
            "releaseNetworkRecoveryHold",
        ):
            self.assertIn(api, MQTT_HEADER)

    def test_hard_radio_recycle_runs_only_after_quiescence(self):
        begin = NETWORK[
            NETWORK.index("bool beginWifiConnection(") :
            NETWORK.index("bool waitForWifi(")
        ]
        self.assertLess(
            begin.index("prepareHardWifiRecovery()"),
            begin.index("WiFi.disconnect(true, false)"),
        )

        failure = NETWORK[
            NETWORK.index("const bool recoveryDue") :
            NETWORK.index("nextPollAt = recoveryConnected")
        ]
        self.assertIn("const bool recoveryStarted = beginWifiConnection", failure)
        self.assertIn("if (recoveryStarted)", failure)
        self.assertIn("if (restartRadio) finishHardWifiRecovery();", failure)

    def test_ota_and_hard_recovery_use_mutually_exclusive_reservation(self):
        request = NETWORK[
            NETWORK.index("bool requestMaintenanceHold()") :
            NETWORK.index("bool maintenanceHoldActive()")
        ]
        self.assertIn("if (maintenanceRequested)", request)
        self.assertIn("else if (!wifiOperationPending)", request)
        self.assertIn("return accepted;", request)
        self.assertIn("bool requestMaintenanceHold();", NETWORK_HEADER)

        enable = OTA[OTA.index("bool enable()") : OTA.index("void disable()")]
        self.assertLess(
            enable.index("if (!adsb::requestMaintenanceHold())"),
            enable.index("server.begin();"),
        )
        self.assertIn("Wi-Fi recovery in progress; try enabling OTA again", enable)

        prepare = OTA[OTA.index("void handlePrepare()") : OTA.index("void handleStatus()")]
        self.assertIn("if (!adsb::requestMaintenanceHold())", prepare)
        self.assertIn("try preparation again", prepare)

    def test_core_one_avoids_ota_calls_during_hard_radio_transition(self):
        self.assertIn("if (isWifiOperationPending()) return;", NETWORK)
        self.assertIn("wifiOperationInProgress", NETWORK_HEADER)
        self.assertIn(
            "if (!adsb::wifiOperationInProgress()) ota_update::service();", MAIN
        )

    def test_no_forbidden_transport_regression(self):
        combined = MAIN + NETWORK + MQTT + OTA
        self.assertNotIn("setInsecure()", combined)
        self.assertNotIn("HTTPClient::GET()", combined)
        self.assertNotIn("/upload-raw", OTA)
        self.assertNotIn("/upload-chunk", OTA)


if __name__ == "__main__":
    unittest.main()
