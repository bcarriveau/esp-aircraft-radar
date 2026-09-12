#include "adsb_network.h"

#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>

#include "adsb_fetch.h"
#include "adsb_diagnostics.h"
#include "app_state.h"
#include "config.h"
#include "network_reconnect_logic.h"
#include "mqtt_service.h"
#include "settings.h"
#include "update_manager.h"

namespace adsb {
namespace {

constexpr uint32_t WIFI_CONNECT_WINDOW_MS = 12000;
constexpr uint32_t LAST_RESORT_RESTART_MS = 30UL * 60UL * 1000UL;
constexpr uint16_t LAST_RESORT_FAILURE_COUNT = 20;
constexpr uint32_t POST_RECOVERY_RETRY_MS = 1000;
constexpr uint32_t NETWORK_QUIESCE_SETTLE_MS = 100;
constexpr uint32_t ADSB_TASK_STACK_BYTES = 12U * 1024U;
constexpr uint32_t COMMAND_REFRESH = 1U << 0;
constexpr uint32_t COMMAND_WIFI_RECONNECT = 1U << 1;
constexpr uint32_t TIME_SYNC_RETRY_MS = 15000;
constexpr uint32_t TIME_PERSIST_INTERVAL_SECONDS = 6UL * 60UL * 60UL;
constexpr uint32_t TIME_SEED_MIN_EPOCH = 1704067200UL;  // 2024-01-01 UTC
constexpr uint32_t TIME_SEED_MAX_EPOCH = 4102444800UL;  // 2100-01-01 UTC
constexpr char TIME_PREF_NAMESPACE[] = "adsb_time";
constexpr char TIME_PREF_KEY[] = "last_ntp";
constexpr char LOCAL_TIME_ZONE[] = "CST6CDT,M3.2.0/2,M11.1.0/2";

TaskHandle_t fetchTaskHandle = nullptr;
portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t pendingCommands = 0;
bool controlledRestartPending = false;
bool maintenanceRequested = false;
bool maintenanceActive = false;
bool wifiOperationPending = false;
uint32_t lastWifiAttempt = 0;
uint32_t wifiAttempts = 0;
wl_status_t lastLoggedWifiStatus = WL_IDLE_STATUS;

portMUX_TYPE timeMux = portMUX_INITIALIZER_UNLOCKED;
bool ntpSynchronized = false;
bool timePersistPending = false;
bool timeRefreshPending = false;
uint32_t pendingNtpEpoch = 0;
uint32_t lastPersistedNtpEpoch = 0;
uint32_t lastTimeSyncKickMs = 0;

void logFetchMemory(const char* stage) {
  app_state::observeFetchMemory(stage);
#if ADSB_VERBOSE_FETCH_LOGGING
  Serial.printf(
      "MEM ADSB %-18s heap=%u block=%u psram=%u\n",
      stage ? stage : "unknown", ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      ESP.getFreePsram());
#endif
}

#if ADSB_VERBOSE_FETCH_LOGGING
void printFetchLowWater(const app_state::Diagnostics& diagnostics) {
  Serial.printf(
      "MEM ADSB FETCH LOW heap=%u block=%u block-stage=%s "
      "lifetime-block=%u lifetime-stage=%s task-stack-free=%u\n",
      diagnostics.lastFetchMinimumFreeHeap,
      diagnostics.lastFetchMinimumLargestInternalBlock,
      diagnostics.lastFetchMinimumBlockStage[0]
          ? diagnostics.lastFetchMinimumBlockStage
          : "unknown",
      diagnostics.minimumLargestInternalBlock,
      diagnostics.minimumBlockStage[0] ? diagnostics.minimumBlockStage
                                       : "unknown",
      diagnostics.minimumAdsbTaskStackFreeBytes);
}
#endif

void logFetchLowWater() {
#if ADSB_VERBOSE_FETCH_LOGGING
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);
  printFetchLowWater(diagnostics);
#endif
}

void printFetchSummary(const char* outcome, const adsb_fetch::Result& result,
                       const app_state::Diagnostics& diagnostics) {
  const uint32_t logStartedMs = millis();
  Serial.printf(
      "ADSB %s accepted=%u received=%u eligible=%u dropped=%u "
      "bytes=%lu total=%lums json=%lu/%luus yields=%u "
      "low=%lu/%lu@%s stack=%lu vlog=%luus\n",
      outcome ? outcome : "UNKNOWN", (unsigned)result.acceptedCount,
      (unsigned)result.receivedCount, (unsigned)result.eligibleCount,
      (unsigned)result.capacityDroppedCount,
      (unsigned long)result.responseBytes,
      (unsigned long)result.durationMs,
      (unsigned long)result.jsonDeserializeUs,
      (unsigned long)result.jsonExtractUs,
      (unsigned)result.extractionYieldCount,
      (unsigned long)diagnostics.lastFetchMinimumFreeHeap,
      (unsigned long)diagnostics.lastFetchMinimumLargestInternalBlock,
      diagnostics.lastFetchMinimumBlockStage[0]
          ? diagnostics.lastFetchMinimumBlockStage : "unknown",
      (unsigned long)diagnostics.minimumAdsbTaskStackFreeBytes,
      (unsigned long)result.verboseDiagnosticUs);
  app_state::recordActivityWindow(app_state::ActivityStage::DIAGNOSTICS,
                                  logStartedMs, millis());
}

bool timeEpochSane(uint32_t epoch) {
  return epoch >= TIME_SEED_MIN_EPOCH && epoch <= TIME_SEED_MAX_EPOCH;
}

bool systemTimeUsable() {
  const time_t current = time(nullptr);
  return current >= 0 && static_cast<uint64_t>(current) <= UINT32_MAX &&
         timeEpochSane(static_cast<uint32_t>(current));
}

bool isTimeSynchronized() {
  portENTER_CRITICAL(&timeMux);
  const bool synchronized = ntpSynchronized;
  portEXIT_CRITICAL(&timeMux);
  return synchronized;
}

void noteTimeSyncKick() {
  portENTER_CRITICAL(&timeMux);
  lastTimeSyncKickMs = millis();
  portEXIT_CRITICAL(&timeMux);
}

uint32_t lastTimeSyncKick() {
  portENTER_CRITICAL(&timeMux);
  const uint32_t kickedAt = lastTimeSyncKickMs;
  portEXIT_CRITICAL(&timeMux);
  return kickedAt;
}

void onTimeSynchronized(struct timeval* tv) {
  if (!tv || tv->tv_sec < 0 ||
      static_cast<uint64_t>(tv->tv_sec) > UINT32_MAX) {
    return;
  }

  const uint32_t epoch = static_cast<uint32_t>(tv->tv_sec);
  if (!timeEpochSane(epoch)) return;

  portENTER_CRITICAL(&timeMux);
  const bool firstSyncThisBoot = !ntpSynchronized;
  ntpSynchronized = true;
  pendingNtpEpoch = epoch;
  timePersistPending = true;
  if (firstSyncThisBoot) timeRefreshPending = true;
  portEXIT_CRITICAL(&timeMux);
}

void configureTimeSync(const char* reason) {
  noteTimeSyncKick();
  sntp_set_time_sync_notification_cb(onTimeSynchronized);
  configTzTime(LOCAL_TIME_ZONE, "pool.ntp.org", "time.google.com");
  Serial.printf("SNTP started%s%s\n", reason ? ": " : "",
                reason ? reason : "");
}

bool restoreTimeSeed() {
  Preferences preferences;
  if (!preferences.begin(TIME_PREF_NAMESPACE, false)) {
    Serial.println("Time seed: NVS namespace unavailable");
    return false;
  }

  if (!preferences.isKey(TIME_PREF_KEY)) {
    preferences.end();
    Serial.println("Time seed: no saved NTP epoch yet");
    return false;
  }

  const uint32_t epoch = preferences.getULong(TIME_PREF_KEY, 0);
  preferences.end();
  if (!timeEpochSane(epoch)) {
    Serial.println("Time seed: saved NTP epoch is outside sanity bounds");
    return false;
  }

  portENTER_CRITICAL(&timeMux);
  lastPersistedNtpEpoch = epoch;
  portEXIT_CRITICAL(&timeMux);

  if (systemTimeUsable()) {
    Serial.println("Time seed: system clock already usable");
    return true;
  }

  struct timeval seededTime{};
  seededTime.tv_sec = static_cast<time_t>(epoch);
  if (settimeofday(&seededTime, nullptr) != 0) {
    Serial.println("Time seed: settimeofday failed");
    return false;
  }

  Serial.printf("Time seed: restored last successful NTP epoch %lu\n",
                (unsigned long)epoch);
  return true;
}

bool shouldPersistTime(uint32_t epoch) {
  portENTER_CRITICAL(&timeMux);
  const uint32_t previous = lastPersistedNtpEpoch;
  portEXIT_CRITICAL(&timeMux);
  if (previous == 0) return true;
  const uint32_t delta = epoch >= previous ? epoch - previous : previous - epoch;
  return delta >= TIME_PERSIST_INTERVAL_SECONDS;
}

void persistTimeSeed(uint32_t epoch) {
  if (!timeEpochSane(epoch) || !shouldPersistTime(epoch)) return;

  Preferences preferences;
  if (!preferences.begin(TIME_PREF_NAMESPACE, false)) {
    Serial.println("Time seed: NVS write namespace unavailable");
    return;
  }
  const size_t written = preferences.putULong(TIME_PREF_KEY, epoch);
  preferences.end();
  if (written != sizeof(uint32_t)) {
    Serial.println("Time seed: NVS write failed");
    return;
  }

  portENTER_CRITICAL(&timeMux);
  lastPersistedNtpEpoch = epoch;
  portEXIT_CRITICAL(&timeMux);
  Serial.printf("Time seed: saved NTP epoch %lu\n", (unsigned long)epoch);
}

void queueCommand(uint32_t command) {
  update_manager::requestAbort();
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

bool isMaintenanceRequested() {
  portENTER_CRITICAL(&commandMux);
  const bool requested = maintenanceRequested;
  portEXIT_CRITICAL(&commandMux);
  return requested;
}

void setMaintenanceActive(bool active) {
  portENTER_CRITICAL(&commandMux);
  maintenanceActive = active;
  portEXIT_CRITICAL(&commandMux);
}

void setWifiOperationPending(bool pending) {
  portENTER_CRITICAL(&commandMux);
  wifiOperationPending = pending;
  portEXIT_CRITICAL(&commandMux);
}

bool isWifiOperationPending() {
  portENTER_CRITICAL(&commandMux);
  const bool pending = wifiOperationPending;
  portEXIT_CRITICAL(&commandMux);
  return pending;
}

bool reserveHardWifiRecovery() {
  portENTER_CRITICAL(&commandMux);
  const bool reserved = !maintenanceRequested && !wifiOperationPending;
  if (reserved) wifiOperationPending = true;
  portEXIT_CRITICAL(&commandMux);
  return reserved;
}

bool prepareHardWifiRecovery() {
  if (!reserveHardWifiRecovery()) {
    Serial.println(
        "WiFi recovery: hard radio recycle deferred for OTA or another recovery");
    return false;
  }

  mqtt_service::requestNetworkRecoveryHold();

  const uint32_t startedAt = millis();
  while (!mqtt_service::networkRecoveryHoldActive() &&
         millis() - startedAt < 2000U) {
    delay(10);
    yield();
  }

  if (!mqtt_service::networkRecoveryHoldActive()) {
    Serial.println(
        "WiFi recovery: MQTT quiesce timed out; deferring hard radio recycle");
    mqtt_service::releaseNetworkRecoveryHold();
    setWifiOperationPending(false);
    return false;
  }

  // WiFiClient::stop() releases the application socket synchronously, but
  // lwIP completes some buffer callbacks on its own task. Give that bounded
  // cleanup a short window before the station driver is stopped.
  delay(NETWORK_QUIESCE_SETTLE_MS);
  yield();

  Serial.printf(
      "WiFi recovery: dependent network resources released, heap=%u, "
      "largest internal=%u, PSRAM=%u\n",
      ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      ESP.getFreePsram());
  return true;
}

void finishHardWifiRecovery() {
  mqtt_service::releaseNetworkRecoveryHold();
  setWifiOperationPending(false);
}

void recordWifiAttempt() {
  portENTER_CRITICAL(&commandMux);
  lastWifiAttempt = millis();
  portEXIT_CRITICAL(&commandMux);
}

bool reserveWifiReconnect(wl_status_t status, uint32_t retryDelayMs) {
  portENTER_CRITICAL(&commandMux);
  const uint32_t now = millis();
  const bool reconnectDue =
      adsb::shouldScheduleWifiReconnect(status, now, lastWifiAttempt,
                                        retryDelayMs);
  if (reconnectDue) lastWifiAttempt = now;
  portEXIT_CRITICAL(&commandMux);
  return reconnectDue;
}

bool beginWifiConnection(const char* reason, bool restartRadio = false,
                         bool quiesceDependents = true) {
  if (restartRadio && quiesceDependents && !prepareHardWifiRecovery()) {
    return false;
  }

  ++wifiAttempts;
  recordWifiAttempt();
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
  return true;
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

void fetchTask(void* parameter) {
  aircraft::Target* incoming = static_cast<aircraft::Target*>(parameter);

  Serial.printf("ADSB target buffer in PSRAM: %u bytes\n",
                (unsigned)(aircraft::MAX_TARGETS *
                           sizeof(aircraft::Target)));
  Serial.printf(
      "ADSB memory ready: heap=%u, largest internal=%u, free PSRAM=%u\n",
      ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      ESP.getFreePsram());
  Serial.println("ADSB fetch task started on core 0");
  uint32_t nextPollAt = millis();
  uint32_t outageStartedAt = 0;
  uint8_t outageRecoveries = 0;
  bool unsyncedTlsRecoveryAttempted = false;
  for (;;) {
    if (isMaintenanceRequested()) {
      setMaintenanceActive(true);
      app_state::setFetchInProgress(false);
      Serial.println("ADSB task entered OTA maintenance hold");
      while (isMaintenanceRequested()) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      }
      setMaintenanceActive(false);
      nextPollAt = millis();
      Serial.println("ADSB task released from OTA maintenance hold");
      continue;
    }

    uint32_t commands = takeCommands();
    if (isMaintenanceRequested()) {
      // Commands taken concurrently with the hold request are stale for the
      // exclusive OTA window and must not disconnect Wi-Fi or start a fetch.
      continue;
    }
    if (commands & COMMAND_WIFI_RECONNECT) {
      Serial.println("Network task processing WiFi reconnect");
      beginWifiConnection("requested");
      app_state::recordNetworkRecovery();
      waitForWifi(WIFI_CONNECT_WINDOW_MS);
      nextPollAt = millis();
      commands |= COMMAND_REFRESH;
    }

    if (isMaintenanceRequested()) continue;

    const uint32_t now = millis();
    const bool pollDue = (int32_t)(now - nextPollAt) >= 0;
    if (!pollDue && !(commands & COMMAND_REFRESH)) {
      waitUntilOrCommand(nextPollAt);
      continue;
    }

    const uint32_t pollStartedAt = millis();
    app_state::beginFetch();
    logFetchMemory("task-before-fetch");
    adsb_fetch::Result result = adsb_fetch::fetchAircraft(incoming);
    logFetchMemory("task-after-fetch");
    app_state::recordAdsbTaskStackFreeBytes(
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)));
    bool immediateFollowup = false;

    if (result.cancelled) {
      // Cancellation is an OTA ownership handoff, not an ADS-B transport
      // failure. Preserve last-good data, failure counters, and recovery state.
      app_state::setFetchInProgress(false);
      Serial.printf(
          "ADSB fetch cancelled for OTA maintenance after %lu ms\n",
          (unsigned long)result.durationMs);
      continue;
    }

    if (result.success) {
      if (result.requestGeneration != app_state::rangeGeneration()) {
        Serial.printf(
            "Discarded ADSB generation %lu; current generation is %lu\n",
            (unsigned long)result.requestGeneration,
            (unsigned long)app_state::rangeGeneration());
        app_state::recordDiscardedResponse(
            result.durationMs, result.responseBytes, result.receivedCount,
            result.eligibleCount, result.acceptedCount,
            result.capacityDroppedCount);
        logFetchLowWater();
        app_state::Diagnostics diagnostics;
        app_state::copyDiagnostics(diagnostics);
        printFetchSummary("STALE", result, diagnostics);
        outageStartedAt = 0;
        outageRecoveries = 0;
        unsyncedTlsRecoveryAttempted = false;
        immediateFollowup = true;
      } else {
        app_state::publishTargets(incoming, result.acceptedCount, millis());
        // Claim an optional manual or daily release check while the established
        // ADS-B/MQTT exclusion is still active. The disposable GitHub request
        // runs only after publication and ADS-B diagnostics complete.
        const bool updateCheckPrepared =
            update_manager::prepareAfterSuccessfulAdsb(
                pollStartedAt + FETCH_INTERVAL_MS);
        app_state::recordFetchSuccess(
            result.durationMs, result.responseBytes, result.receivedCount,
            result.eligibleCount, result.acceptedCount,
            result.capacityDroppedCount);
        logFetchLowWater();
        app_state::Diagnostics diagnostics;
        app_state::copyDiagnostics(diagnostics);
        printFetchSummary("OK", result, diagnostics);
        outageStartedAt = 0;
        outageRecoveries = 0;
        unsyncedTlsRecoveryAttempted = false;
        if (updateCheckPrepared) update_manager::performPreparedCheck();
      }
    } else {
      app_state::recordFetchFailure(result.failureStage, result.durationMs,
                                    result.responseBytes);
      app_state::Diagnostics diagnostics;
      app_state::copyDiagnostics(diagnostics);
#if ADSB_VERBOSE_FETCH_LOGGING
      printFetchLowWater(diagnostics);
#endif
      printFetchSummary("FAIL", result, diagnostics);
      if (outageStartedAt == 0) outageStartedAt = millis();
      Serial.printf("ADSB fetch failed at %s; consecutive failures=%u\n",
                    app_state::failureStageName(result.failureStage),
                    diagnostics.consecutiveFailures);

      // A TLS failure while this boot is still unsynchronized, or while the
      // system clock is outside sane bounds, gets one bounded hard-radio
      // recovery. The GOT_IP event starts SNTP immediately after the recycle;
      // later unsynchronized TLS failures wait for that background sync instead
      // of entering a soft-reconnect death spiral.
      const bool linkFailure =
          result.failureStage == app_state::FetchFailureStage::WIFI ||
          result.failureStage == app_state::FetchFailureStage::DNS ||
          result.failureStage == app_state::FetchFailureStage::TCP;
      const bool transportFailure =
          result.failureStage == app_state::FetchFailureStage::TLS ||
          result.failureStage == app_state::FetchFailureStage::HTTP_HEADERS;
      const bool bodyFailure =
          result.failureStage == app_state::FetchFailureStage::RESPONSE_BODY;
      const bool unsyncedTlsFailure =
          result.failureStage == app_state::FetchFailureStage::TLS &&
          (!isTimeSynchronized() || !systemTimeUsable());
      const bool unsyncedTlsRecoveryDue =
          unsyncedTlsFailure && !unsyncedTlsRecoveryAttempted;
      const bool linkRecoveryDue =
          linkFailure && diagnostics.consecutiveFailures >= 3 &&
          (diagnostics.consecutiveFailures == 3 ||
           diagnostics.consecutiveFailures % 6 == 0);
      const bool transportRecoveryDue =
          !unsyncedTlsFailure && transportFailure &&
          diagnostics.consecutiveFailures >= 2 &&
          (diagnostics.consecutiveFailures == 2 ||
           diagnostics.consecutiveFailures % 3 == 0);
      const bool recoveryDue = bodyFailure || unsyncedTlsRecoveryDue ||
                               linkRecoveryDue || transportRecoveryDue;
      const bool maintenanceRequestedAfterFetch = isMaintenanceRequested();
      const bool recoveryDeferredForOta =
          recoveryDue && maintenanceRequestedAfterFetch;
      bool recoveryConnected = false;
      if (recoveryDeferredForOta) {
        Serial.println(
            "ADSB recovery deferred: OTA exclusive hold was requested");
      }
      if (recoveryDue && !recoveryDeferredForOta) {
        if (unsyncedTlsRecoveryDue) {
          unsyncedTlsRecoveryAttempted = true;
          Serial.println(
              "ADSB TLS failed before SNTP sync; hard radio recycle and "
              "background time kick selected");
        }
        const bool restartRadio = unsyncedTlsRecoveryDue || bodyFailure ||
                                  outageRecoveries > 0;
        Serial.printf(
            "ADSB recovery ladder: %s WiFi after %s failure\n",
            restartRadio ? "hard-recycling" : "reconnecting",
            app_state::failureStageName(result.failureStage));
        const bool recoveryStarted = beginWifiConnection(
            restartRadio ? "ADSB hard recovery" : "ADSB recovery",
            restartRadio);
        if (recoveryStarted) {
          app_state::recordNetworkRecovery();
          ++outageRecoveries;
          recoveryConnected = waitForWifi(WIFI_CONNECT_WINDOW_MS);
          if (restartRadio) finishHardWifiRecovery();
          Serial.printf("ADSB recovery result: WiFi %s\n",
                        recoveryConnected ? "connected" : "not connected");
        }
      }

      if (outageStartedAt != 0 &&
          millis() - outageStartedAt >= LAST_RESORT_RESTART_MS &&
          diagnostics.consecutiveFailures >= LAST_RESORT_FAILURE_COUNT &&
          !maintenanceRequestedAfterFetch) {
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

void serviceTimeSync(uint32_t now, wl_status_t wifiStatus) {
  const bool maintenance = isMaintenanceRequested();
  const bool fetchBusy = app_state::fetchInProgress();
  uint32_t syncedEpoch = 0;
  bool persistPending = false;
  bool refreshPending = false;
  portENTER_CRITICAL(&timeMux);
  if (timePersistPending && !maintenance && !fetchBusy) {
    syncedEpoch = pendingNtpEpoch;
    timePersistPending = false;
    persistPending = true;
  }
  if (timeRefreshPending && !maintenance) {
    timeRefreshPending = false;
    refreshPending = true;
  }
  portEXIT_CRITICAL(&timeMux);

  if (persistPending) persistTimeSeed(syncedEpoch);
  if (refreshPending) {
    Serial.println("SNTP synchronized; ADSB refresh queued");
    queueCommand(COMMAND_REFRESH);
  }

  if (wifiStatus != WL_CONNECTED ||
      (isTimeSynchronized() && systemTimeUsable()) || maintenance) {
    return;
  }

  const uint32_t lastKick = lastTimeSyncKick();
  if (lastKick != 0 && now - lastKick < TIME_SYNC_RETRY_MS) return;
  configureTimeSync("background retry");
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

bool begin() {
  aircraft::Target* incoming = static_cast<aircraft::Target*>(heap_caps_calloc(
      aircraft::MAX_TARGETS, sizeof(aircraft::Target),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!incoming) {
    Serial.println("FATAL: ADSB target-buffer PSRAM allocation failed");
    return false;
  }

  sntp_set_time_sync_notification_cb(onTimeSynchronized);
  restoreTimeSeed();

  const esp_reset_reason_t resetReason = esp_reset_reason();
  const bool coldRadioReset = resetReason == ESP_RST_POWERON ||
                              resetReason == ESP_RST_BROWNOUT;
  if (!coldRadioReset) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
  }
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
      configureTimeSync("WiFi got IP");
    }
  });

  if (coldRadioReset) {
    // Power-on/brownout starts with the same station-radio recycle already
    // proven to clear poisoned TLS/socket state. MQTT is not started yet, so
    // there are no dependent runtime sockets to quiesce at this point.
    beginWifiConnection("startup hard bring-up", true, false);
  } else {
    beginWifiConnection("startup");
  }
  Serial.printf("Connecting to %s", settings::wifiSsid().c_str());
  const bool connected = waitForWifi(20000);
  Serial.println();
  if (connected) {
    Serial.printf("Initial WiFi connection complete: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi timeout; UI will still run");
  }

  fetchTaskHandle = nullptr;
  const BaseType_t taskResult = xTaskCreatePinnedToCore(
      fetchTask, "ADSB", ADSB_TASK_STACK_BYTES, incoming, 1,
      &fetchTaskHandle, 0);
  if (taskResult != pdPASS) {
    fetchTaskHandle = nullptr;
    free(incoming);
    Serial.printf("FATAL: ADSB task creation failed, result=%ld\n",
                  (long)taskResult);
    return false;
  }

  return true;
}

void service() {
  if (isControlledRestartPending()) {
    Serial.println("Main loop performing controlled ESP32 restart");
    Serial.flush();
    delay(100);
    ESP.restart();
    return;
  }
  if (isWifiOperationPending()) return;

  const uint32_t now = millis();
  const wl_status_t status = WiFi.status();
  if (status != lastLoggedWifiStatus) {
    app_state::setWifiStatus(status);
    Serial.printf("WiFi state: %s (%d)\n", wifiStatusName(status), status);
    lastLoggedWifiStatus = status;
  }
  serviceTimeSync(now, status);
  static uint32_t lastMemorySample = 0;
  if (now - lastMemorySample >= 1000) {
    lastMemorySample = now;
    app_state::observeMemory();
  }
  if (isMaintenanceRequested()) return;

  const uint32_t retryDelayMs =
      (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED)
          ? 60000U
          : WIFI_RETRY_INTERVAL_MS;
  if (reserveWifiReconnect(status, retryDelayMs)) {
    queueCommand(COMMAND_WIFI_RECONNECT);
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
  if (isMaintenanceRequested()) {
    Serial.println("ADSB refresh ignored during OTA exclusive hold");
    return;
  }
  Serial.println("ADSB refresh queued");
  queueCommand(COMMAND_REFRESH);
}

void requestWifiReconnect() {
  if (isMaintenanceRequested()) {
    Serial.println("WiFi reconnect ignored during OTA exclusive hold");
    return;
  }
  Serial.println("WiFi reconnect queued for network task");
  queueCommand(COMMAND_WIFI_RECONNECT);
}

bool wifiOperationInProgress() {
  return isWifiOperationPending();
}

bool fetchAbortRequested() {
  return isMaintenanceRequested();
}

bool timeSynchronized() {
  return isTimeSynchronized() && systemTimeUsable();
}

bool requestMaintenanceHold() {
  // A browser OTA request takes priority over the disposable GitHub check.
  update_manager::requestAbort();
  bool accepted = false;
  portENTER_CRITICAL(&commandMux);
  if (maintenanceRequested) {
    accepted = true;
  } else if (!wifiOperationPending) {
    maintenanceRequested = true;
    pendingCommands = 0;
    accepted = true;
  }
  portEXIT_CRITICAL(&commandMux);

  if (accepted && fetchTaskHandle) xTaskNotifyGive(fetchTaskHandle);
  if (!accepted) {
    Serial.println(
        "OTA hold deferred: hard Wi-Fi recovery already owns the network");
  }
  return accepted;
}

bool maintenanceHoldActive() {
  portENTER_CRITICAL(&commandMux);
  const bool active = maintenanceActive;
  portEXIT_CRITICAL(&commandMux);
  return active;
}

void releaseMaintenanceHold() {
  portENTER_CRITICAL(&commandMux);
  maintenanceRequested = false;
  portEXIT_CRITICAL(&commandMux);
  if (fetchTaskHandle) xTaskNotifyGive(fetchTaskHandle);
}

}  // namespace adsb
