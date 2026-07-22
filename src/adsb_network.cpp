#include "adsb_network.h"

#include <esp_heap_caps.h>

#include "adsb_fetch.h"
#include "app_state.h"
#include "config.h"
#include "network_reconnect_logic.h"
#include "settings.h"

namespace adsb {
namespace {

constexpr uint32_t WIFI_CONNECT_WINDOW_MS = 12000;
constexpr uint32_t LAST_RESORT_RESTART_MS = 30UL * 60UL * 1000UL;
constexpr uint16_t LAST_RESORT_FAILURE_COUNT = 20;
constexpr uint32_t POST_RECOVERY_RETRY_MS = 1000;
constexpr uint32_t COMMAND_REFRESH = 1U << 0;
constexpr uint32_t COMMAND_WIFI_RECONNECT = 1U << 1;

TaskHandle_t fetchTaskHandle = nullptr;
portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t pendingCommands = 0;
bool controlledRestartPending = false;
volatile uint32_t lastWifiAttempt = 0;
uint32_t wifiAttempts = 0;
wl_status_t lastLoggedWifiStatus = WL_IDLE_STATUS;

void configureTimeSync() {
  configTzTime("CST6CDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org",
               "time.google.com");
}

void queueCommand(uint32_t command) {
  portENTER_CRITICAL(&commandMux);
  pendingCommands |= command;
  portEXIT_CRITICAL(&commandMux);
  if (fetchTaskHandle) xTaskNotifyGive(fetchTaskHandle);
}

uint32_t takeCommands() {
  portENTER_CRITICAL(&commandMux);
  uint32_t commands = pendingCommands;
  pendingCommands = 0;
  portEXIT_CRITICAL(&commandMux);
  return commands;
}

void requestControlledRestart() {
  portENTER_CRITICAL(&commandMux);
  controlledRestartPending = true;
  portEXIT_CRITICAL(&commandMux);
}

bool isControlledRestartPending() {
  portENTER_CRITICAL(&commandMux);
  const bool pending = controlledRestartPending;
  portEXIT_CRITICAL(&commandMux);
  return pending;
}

void beginWifiConnection(const char* reason, bool restartRadio = false) {
  ++wifiAttempts;
  lastWifiAttempt = millis();
  const String ssid = settings::wifiSsid();
  const String password = settings::wifiPassword();
  Serial.printf("WiFi attempt %lu (%s): %s\n",
                (unsigned long)wifiAttempts, reason, ssid.c_str());
  WiFi.setAutoReconnect(false);
  if (restartRadio) {
    Serial.println("WiFi recovery: restarting station radio");
    WiFi.disconnect(true, false);
    delay(250);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
  } else {
    WiFi.disconnect(false, false);
    delay(100);
  }
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  app_state::setWifiStatus(WiFi.status());
}

bool waitForWifi(uint32_t timeoutMs) {
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) {
    delay(100);
    yield();
  }
  app_state::setWifiStatus(WiFi.status());
  return WiFi.status() == WL_CONNECTED;
}

uint32_t failureBackoffMs(uint16_t failures) {
  if (failures < 3) return FETCH_INTERVAL_MS;
  if (failures < 6) return 30000;
  if (failures < 9) return 60000;
  return 120000;
}

void waitUntilOrCommand(uint32_t deadlineMs) {
  for (;;) {
    const uint32_t now = millis();
    if ((int32_t)(now - deadlineMs) >= 0) return;
    const uint32_t remainingMs = deadlineMs - now;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remainingMs)) > 0) return;
  }
}

void fetchTask(void*) {
  aircraft::Target* incoming = static_cast<aircraft::Target*>(heap_caps_calloc(
      aircraft::MAX_TARGETS, sizeof(aircraft::Target),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!incoming) {
    incoming = static_cast<aircraft::Target*>(
        calloc(aircraft::MAX_TARGETS, sizeof(aircraft::Target)));
  }
  if (!incoming) {
    Serial.println("FATAL: ADSB target buffer allocation failed");
    vTaskDelete(nullptr);
    return;
  }

  Serial.println("ADSB fetch task started on core 0");
  uint32_t nextPollAt = millis();
  uint32_t outageStartedAt = 0;
  uint8_t outageRecoveries = 0;
  for (;;) {
    uint32_t commands = takeCommands();
    if (commands & COMMAND_WIFI_RECONNECT) {
      Serial.println("Network task processing WiFi reconnect");
      beginWifiConnection("requested");
      app_state::recordNetworkRecovery();
      waitForWifi(WIFI_CONNECT_WINDOW_MS);
      nextPollAt = millis();
      commands |= COMMAND_REFRESH;
    }

    const uint32_t now = millis();
    const bool pollDue = (int32_t)(now - nextPollAt) >= 0;
    if (!pollDue && !(commands & COMMAND_REFRESH)) {
      waitUntilOrCommand(nextPollAt);
      continue;
    }

    const uint32_t pollStartedAt = millis();
    app_state::beginFetch();
    adsb_fetch::Result result = adsb_fetch::fetchAircraft(incoming);
    bool immediateFollowup = false;

    if (result.success) {
      if (result.requestGeneration != app_state::rangeGeneration()) {
        Serial.printf(
            "Discarded ADSB generation %lu; current generation is %lu\n",
            (unsigned long)result.requestGeneration,
            (unsigned long)app_state::rangeGeneration());
        app_state::recordDiscardedResponse(
            result.durationMs, result.responseBytes, result.receivedCount,
            result.acceptedCount);
        immediateFollowup = true;
      } else {
        app_state::publishTargets(incoming, result.acceptedCount, millis());
        app_state::recordFetchSuccess(
            result.durationMs, result.responseBytes, result.receivedCount,
            result.acceptedCount);
        Serial.printf("Published %u aircraft in %lu ms\n",
                      result.acceptedCount,
                      (unsigned long)result.durationMs);
        outageStartedAt = 0;
        outageRecoveries = 0;
      }
    } else {
      app_state::recordFetchFailure(result.failureStage, result.durationMs,
                                    result.responseBytes);
      app_state::Diagnostics diagnostics;
      app_state::copyDiagnostics(diagnostics);
      if (outageStartedAt == 0) outageStartedAt = millis();
      Serial.printf("ADSB fetch failed at %s; consecutive failures=%u\n",
                    app_state::failureStageName(result.failureStage),
                    diagnostics.consecutiveFailures);

      // A single TLS error does not justify dropping a healthy station link.
      // A stalled response body is different: the physical log shows that it
      // can leave both HTTPS implementations unable to open another session.
      // Escalate from an association reconnect to one station-radio restart,
      // then continue using the normal outage backoff.
      const bool linkFailure =
          result.failureStage == app_state::FetchFailureStage::WIFI ||
          result.failureStage == app_state::FetchFailureStage::DNS ||
          result.failureStage == app_state::FetchFailureStage::TCP;
      const bool transportFailure =
          result.failureStage == app_state::FetchFailureStage::TLS ||
          result.failureStage == app_state::FetchFailureStage::HTTP_HEADERS;
      const bool bodyFailure =
          result.failureStage == app_state::FetchFailureStage::RESPONSE_BODY;
      const bool linkRecoveryDue =
          linkFailure && diagnostics.consecutiveFailures >= 3 &&
          (diagnostics.consecutiveFailures == 3 ||
           diagnostics.consecutiveFailures % 6 == 0);
      const bool transportRecoveryDue =
          transportFailure && diagnostics.consecutiveFailures >= 2 &&
          (diagnostics.consecutiveFailures == 2 ||
           diagnostics.consecutiveFailures % 3 == 0);
      const bool recoveryDue =
          bodyFailure || linkRecoveryDue || transportRecoveryDue;
      bool recoveryConnected = false;
      if (recoveryDue) {
        const bool restartRadio = outageRecoveries > 0;
        Serial.printf(
            "ADSB recovery ladder: %s WiFi after %s failure\n",
            restartRadio ? "hard-recycling" : "reconnecting",
            app_state::failureStageName(result.failureStage));
        beginWifiConnection(restartRadio ? "ADSB hard recovery"
                                         : "ADSB recovery",
                            restartRadio);
        app_state::recordNetworkRecovery();
        ++outageRecoveries;
        recoveryConnected = waitForWifi(WIFI_CONNECT_WINDOW_MS);
        Serial.printf("ADSB recovery result: WiFi %s\n",
                      recoveryConnected ? "connected" : "not connected");
      }

      if (outageStartedAt != 0 &&
          millis() - outageStartedAt >= LAST_RESORT_RESTART_MS &&
          diagnostics.consecutiveFailures >= LAST_RESORT_FAILURE_COUNT) {
        Serial.println(
            "ADSB recovery ladder exhausted for 30 minutes; requesting "
            "controlled restart");
        app_state::setFetchInProgress(false);
        requestControlledRestart();
        // The main loop on core 1 performs the restart before calling
        // WiFi.status() again. Suspending here prevents a cross-core restart
        // collision with the network task.
        vTaskSuspend(nullptr);
      }
      nextPollAt = recoveryConnected
                       ? millis() + POST_RECOVERY_RETRY_MS
                       : millis() +
                             failureBackoffMs(diagnostics.consecutiveFailures);
    }

    // A normal successful poll is scheduled from its start time, giving a
    // true 15-second cadence. A request never overlaps because this task owns
    // the entire transport.
    if (result.success && !immediateFollowup) {
      nextPollAt = pollStartedAt + FETCH_INTERVAL_MS;
      if ((int32_t)(millis() - nextPollAt) >= 0) nextPollAt = millis();
    } else if (immediateFollowup) {
      nextPollAt = millis();
    }

    // Preserve button/range commands that arrived while TLS was active.
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
      uint32_t lateCommands = takeCommands();
      if (lateCommands & COMMAND_WIFI_RECONNECT) {
        queueCommand(COMMAND_WIFI_RECONNECT);
      }
      if (lateCommands & COMMAND_REFRESH) nextPollAt = millis();
    }
  }
}

}  // namespace

const char* wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "connected";
    case WL_NO_SSID_AVAIL: return "SSID not found";
    case WL_CONNECT_FAILED: return "authentication failed";
    case WL_CONNECTION_LOST: return "connection lost";
    case WL_DISCONNECTED: return "disconnected";
    case WL_IDLE_STATUS: return "idle";
    default: return "unknown";
  }
}

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      const int reason = info.wifi_sta_disconnected.reason;
      app_state::setWifiStatus(WL_DISCONNECTED);
      app_state::setLastDisconnectReason(reason);
      Serial.printf("WiFi disconnected, reason=%d\n", reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      app_state::setWifiStatus(WL_CONNECTED);
      Serial.printf("WiFi connected: %s, RSSI=%d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      configureTimeSync();
    }
  });

  beginWifiConnection("startup");
  Serial.printf("Connecting to %s", settings::wifiSsid().c_str());
  const bool connected = waitForWifi(20000);
  Serial.println();
  if (connected) {
    Serial.printf("Initial WiFi connection complete: %s\n",
                  WiFi.localIP().toString().c_str());
    configureTimeSync();
  } else {
    Serial.println("WiFi timeout; UI will still run");
  }

  xTaskCreatePinnedToCore(fetchTask, "ADSB", 16384, nullptr, 1,
                          &fetchTaskHandle, 0);
}

void service() {
  if (isControlledRestartPending()) {
    Serial.println("Main loop performing controlled ESP32 restart");
    Serial.flush();
    delay(100);
    ESP.restart();
    return;
  }
  const uint32_t now = millis();
  const wl_status_t status = WiFi.status();
  if (status != lastLoggedWifiStatus) {
    app_state::setWifiStatus(status);
    Serial.printf("WiFi state: %s (%d)\n", wifiStatusName(status), status);
    lastLoggedWifiStatus = status;
  }
  static uint32_t lastMemorySample = 0;
  if (now - lastMemorySample >= 1000) {
    lastMemorySample = now;
    app_state::observeMemory();
  }
  const uint32_t retryDelayMs =
      (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED)
          ? 60000U
          : WIFI_RETRY_INTERVAL_MS;
  if (adsb::shouldScheduleWifiReconnect(status, now, lastWifiAttempt,
                                        retryDelayMs)) {
    queueCommand(COMMAND_WIFI_RECONNECT);
    lastWifiAttempt = now;
  }
}

void reconnectOrRefresh() {
  if (app_state::wifiStatus() == WL_CONNECTED) {
    requestRefresh();
  } else {
    requestWifiReconnect();
  }
}

void requestRefresh() {
  Serial.println("ADSB refresh queued");
  queueCommand(COMMAND_REFRESH);
}

void requestWifiReconnect() {
  Serial.println("WiFi reconnect queued for network task");
  queueCommand(COMMAND_WIFI_RECONNECT);
}

}  // namespace adsb
