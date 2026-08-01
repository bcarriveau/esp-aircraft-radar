#include "mqtt_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <mqtt_client.h>

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "adsb_network.h"
#include "aircraft_data.h"
#include "app_state.h"
#include "build_info.h"
#include "config.h"
#include "display_power.h"
#include "ota_update.h"
#include "radar_control.h"
#include "settings.h"
#include "vertical_state.h"

#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI ""
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

namespace mqtt_service {
namespace {

constexpr uint32_t SERVICE_INTERVAL_MS = 250;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 60000;
constexpr uint32_t START_RETRY_MS = 60000;
constexpr uint32_t STOP_GRACE_MS = 500;
constexpr uint16_t MQTT_BUFFER_BYTES = 1024;
constexpr uint16_t MQTT_TASK_STACK_BYTES = 4096;
constexpr uint32_t MQTT_OUTBOX_BYTES = 16U * 1024U;
constexpr uint16_t STATE_BUFFER_BYTES = 4096;
constexpr uint8_t NEAREST_COUNT = 5;
constexpr uint8_t DISCOVERY_COUNT = 13;
constexpr uint32_t COMMAND_DISPLAY_ON = 1U << 0;
constexpr uint32_t COMMAND_DISPLAY_OFF = 1U << 1;
constexpr uint32_t COMMAND_RANGE_20 = 1U << 2;
constexpr uint32_t COMMAND_RANGE_40 = 1U << 3;
constexpr uint32_t COMMAND_RANGE_80 = 1U << 4;
constexpr uint32_t COMMAND_REFRESH = 1U << 5;
constexpr uint32_t COMMAND_REDISCOVER = 1U << 6;
constexpr uint32_t COMMAND_DISPLAY_MASK =
    COMMAND_DISPLAY_ON | COMMAND_DISPLAY_OFF;
constexpr uint32_t COMMAND_RANGE_MASK =
    COMMAND_RANGE_20 | COMMAND_RANGE_40 | COMMAND_RANGE_80;
constexpr uint8_t PUBLISH_STATUS = 1U << 0;
constexpr uint8_t PUBLISH_TRAFFIC = 1U << 1;
constexpr uint8_t PUBLISH_TRACKED = 1U << 2;
constexpr uint8_t PUBLISH_AIRSPACE = 1U << 3;
constexpr uint8_t PUBLISH_ALL =
    PUBLISH_STATUS | PUBLISH_TRAFFIC | PUBLISH_TRACKED | PUBLISH_AIRSPACE;

void logMemoryStage(const char* stage) {
  app_state::observeMemory();
  Serial.printf(
      "MEM MQTT %-18s heap=%u block=%u psram=%u\n",
      stage ? stage : "unknown", ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      ESP.getFreePsram());
}

class JsonWriter {
 public:
  JsonWriter(char* buffer, size_t capacity)
      : buffer_(buffer), capacity_(capacity) {
    if (buffer_ && capacity_) buffer_[0] = 0;
  }

  bool valid() const { return valid_; }
  size_t size() const { return position_; }
  const char* c_str() const { return buffer_ ? buffer_ : ""; }

  void beginObject() {
    prefixValue();
    put('{');
    push(true);
  }

  void endObject() {
    put('}');
    pop();
  }

  void beginArray() {
    prefixValue();
    put('[');
    push(false);
  }

  void endArray() {
    put(']');
    pop();
  }

  void key(const char* name) {
    if (!valid_ || depth_ == 0 || !object_[depth_ - 1] || afterKey_) {
      valid_ = false;
      return;
    }
    if (!first_[depth_ - 1]) put(',');
    first_[depth_ - 1] = false;
    writeString(name ? name : "");
    put(':');
    afterKey_ = true;
  }

  void value(const char* text) {
    prefixValue();
    writeString(text ? text : "");
  }

  void value(bool value) {
    prefixValue();
    raw(value ? "true" : "false");
  }

  void value(int value) {
    char text[24];
    snprintf(text, sizeof(text), "%d", value);
    prefixValue();
    raw(text);
  }

  void value(unsigned value) {
    char text[24];
    snprintf(text, sizeof(text), "%u", value);
    prefixValue();
    raw(text);
  }

  void value(unsigned long value) {
    char text[32];
    snprintf(text, sizeof(text), "%lu", value);
    prefixValue();
    raw(text);
  }

  void value(float value, uint8_t decimals = 1) {
    if (!isfinite(value)) {
      nullValue();
      return;
    }
    char format[8];
    snprintf(format, sizeof(format), "%%.%uf", decimals);
    char text[40];
    snprintf(text, sizeof(text), format, value);
    prefixValue();
    raw(text);
  }

  void nullValue() {
    prefixValue();
    raw("null");
  }

 private:
  void prefixValue() {
    if (!valid_) return;
    if (afterKey_) {
      afterKey_ = false;
      return;
    }
    if (depth_ == 0) return;
    if (object_[depth_ - 1]) {
      valid_ = false;
      return;
    }
    if (!first_[depth_ - 1]) put(',');
    first_[depth_ - 1] = false;
  }

  void push(bool object) {
    if (depth_ >= MAX_DEPTH) {
      valid_ = false;
      return;
    }
    object_[depth_] = object;
    first_[depth_] = true;
    ++depth_;
  }

  void pop() {
    if (depth_ == 0 || afterKey_) {
      valid_ = false;
      return;
    }
    --depth_;
  }

  void writeString(const char* text) {
    put('"');
    for (const unsigned char* current =
             reinterpret_cast<const unsigned char*>(text);
         valid_ && current && *current; ++current) {
      switch (*current) {
        case '"': raw("\\\""); break;
        case '\\': raw("\\\\"); break;
        case '\b': raw("\\b"); break;
        case '\f': raw("\\f"); break;
        case '\n': raw("\\n"); break;
        case '\r': raw("\\r"); break;
        case '\t': raw("\\t"); break;
        default:
          if (*current < 0x20) {
            char escaped[7];
            snprintf(escaped, sizeof(escaped), "\\u%04x", *current);
            raw(escaped);
          } else {
            put(static_cast<char>(*current));
          }
          break;
      }
    }
    put('"');
  }

  void raw(const char* text) {
    if (!text) return;
    while (valid_ && *text) put(*text++);
  }

  void put(char value) {
    if (!buffer_ || capacity_ == 0 || position_ + 1 >= capacity_) {
      valid_ = false;
      return;
    }
    buffer_[position_++] = value;
    buffer_[position_] = 0;
  }

  static constexpr uint8_t MAX_DEPTH = 8;
  char* buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t position_ = 0;
  bool valid_ = true;
  bool afterKey_ = false;
  uint8_t depth_ = 0;
  bool object_[MAX_DEPTH]{};
  bool first_[MAX_DEPTH]{};
};

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
esp_mqtt_client_handle_t client = nullptr;
aircraft::Target* targetBuffer = nullptr;
char* jsonBuffer = nullptr;
State currentState = State::INACTIVE;
bool configured = false;
bool desiredEnabled = false;
bool clientConnected = false;
bool maintenanceRequested = false;
bool maintenanceActive = false;
bool stopRequested = false;
bool offlineQueued = false;
bool availabilityPublished = false;
uint32_t stopAfterMs = 0;
uint32_t nextStartAttemptMs = 0;
uint32_t lastServiceMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t pendingCommands = 0;
uint8_t pendingPublishMask = 0;
uint8_t discoveryIndex = DISCOVERY_COUNT;
uint32_t observedTargetVersion = UINT32_MAX;
uint32_t observedRangeGeneration = UINT32_MAX;
uint32_t observedTrackingVersion = UINT32_MAX;
bool observedDisplayEnabled = true;
ota_update::State observedOtaState = ota_update::State::UNAVAILABLE;
char deviceId[40]{};
char clientId[48]{};
char baseTopic[96]{};
char availabilityTopic[128]{};
char statusTopic[128]{};
char trafficTopic[128]{};
char trackedTopic[128]{};
char airspaceTopic[128]{};
char displayCommandTopic[128]{};
char rangeCommandTopic[128]{};
char refreshCommandTopic[128]{};
char statusMessage[128] = "MQTT disabled";

void setState(State state, const char* message) {
  const char* safeMessage = message ? message : "";
  portENTER_CRITICAL(&stateMux);
  if (currentState != state ||
      strncmp(statusMessage, safeMessage, sizeof(statusMessage)) != 0) {
    currentState = state;
    strncpy(statusMessage, safeMessage, sizeof(statusMessage) - 1);
    statusMessage[sizeof(statusMessage) - 1] = 0;
  }
  portEXIT_CRITICAL(&stateMux);
}

void queueCommand(uint32_t command) {
  portENTER_CRITICAL(&stateMux);
  if (command & COMMAND_DISPLAY_MASK) pendingCommands &= ~COMMAND_DISPLAY_MASK;
  if (command & COMMAND_RANGE_MASK) pendingCommands &= ~COMMAND_RANGE_MASK;
  pendingCommands |= command;
  portEXIT_CRITICAL(&stateMux);
}

uint32_t takeCommands() {
  portENTER_CRITICAL(&stateMux);
  const uint32_t commands = pendingCommands;
  pendingCommands = 0;
  portEXIT_CRITICAL(&stateMux);
  return commands;
}

bool payloadEquals(const esp_mqtt_event_handle_t event, const char* expected) {
  if (!event || !expected || event->current_data_offset != 0 ||
      event->data_len != event->total_data_len) {
    return false;
  }
  const size_t length = strlen(expected);
  return length == static_cast<size_t>(event->data_len) &&
         memcmp(event->data, expected, length) == 0;
}

bool topicEquals(const esp_mqtt_event_handle_t event, const char* expected) {
  if (!event || !expected || !event->topic || event->topic_len < 0) return false;
  const size_t length = strlen(expected);
  return length == static_cast<size_t>(event->topic_len) &&
         memcmp(event->topic, expected, length) == 0;
}

void mqttEventHandler(void*, esp_event_base_t, int32_t eventId,
                      void* eventData) {
  auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);
  if (!event) return;
  bool currentClient = false;
  portENTER_CRITICAL(&stateMux);
  currentClient = event->client == client;
  portEXIT_CRITICAL(&stateMux);
  if (!currentClient) return;

  switch (eventId) {
    case MQTT_EVENT_CONNECTED: {
      logMemoryStage("broker-connected");
      bool active = false;
      portENTER_CRITICAL(&stateMux);
      clientConnected = true;
      active = desiredEnabled && !maintenanceRequested;
      if (active) {
        pendingCommands |= COMMAND_REDISCOVER;
        currentState = State::CONNECTED;
        snprintf(statusMessage, sizeof(statusMessage),
                 "Connected to MQTT broker");
      }
      portEXIT_CRITICAL(&stateMux);
      if (active) {
        esp_mqtt_client_subscribe(event->client, displayCommandTopic, 0);
        esp_mqtt_client_subscribe(event->client, rangeCommandTopic, 0);
        esp_mqtt_client_subscribe(event->client, refreshCommandTopic, 0);
        esp_mqtt_client_subscribe(event->client, "homeassistant/status", 0);
      }
      break;
    }

    case MQTT_EVENT_DISCONNECTED:
      portENTER_CRITICAL(&stateMux);
      clientConnected = false;
      if (!maintenanceRequested && desiredEnabled) {
        currentState = State::CONNECTING;
        snprintf(statusMessage, sizeof(statusMessage),
                 "Waiting for MQTT broker");
      }
      portEXIT_CRITICAL(&stateMux);
      break;

    case MQTT_EVENT_DATA:
      if (topicEquals(event, displayCommandTopic)) {
        if (payloadEquals(event, "ON")) queueCommand(COMMAND_DISPLAY_ON);
        else if (payloadEquals(event, "OFF")) queueCommand(COMMAND_DISPLAY_OFF);
      } else if (topicEquals(event, rangeCommandTopic)) {
        if (payloadEquals(event, "20")) queueCommand(COMMAND_RANGE_20);
        else if (payloadEquals(event, "40")) queueCommand(COMMAND_RANGE_40);
        else if (payloadEquals(event, "80")) queueCommand(COMMAND_RANGE_80);
      } else if (topicEquals(event, refreshCommandTopic) &&
                 payloadEquals(event, "PRESS")) {
        queueCommand(COMMAND_REFRESH);
      } else if (topicEquals(event, "homeassistant/status") &&
                 payloadEquals(event, "online")) {
        queueCommand(COMMAND_REDISCOVER);
      }
      break;

    case MQTT_EVENT_ERROR:
      portENTER_CRITICAL(&stateMux);
      if (!maintenanceRequested && desiredEnabled) {
        currentState = State::ERROR;
        snprintf(statusMessage, sizeof(statusMessage),
                 "MQTT transport error; retrying");
      }
      portEXIT_CRITICAL(&stateMux);
      break;

    default:
      break;
  }
}

bool brokerConfigured() {
  return MQTT_BROKER_URI[0] != 0 &&
         strncmp(MQTT_BROKER_URI, "mqtt://", 7) == 0;
}

void initializeIdentity() {
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceId, sizeof(deviceId), "bills_aircraft_radar_%012llx",
           static_cast<unsigned long long>(mac & 0xFFFFFFFFFFFFULL));
  snprintf(clientId, sizeof(clientId), "bills-radar-%012llx",
           static_cast<unsigned long long>(mac & 0xFFFFFFFFFFFFULL));
  snprintf(baseTopic, sizeof(baseTopic), "bills-aircraft-radar/%012llx",
           static_cast<unsigned long long>(mac & 0xFFFFFFFFFFFFULL));
  snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability",
           baseTopic);
  snprintf(statusTopic, sizeof(statusTopic), "%s/state", baseTopic);
  snprintf(trafficTopic, sizeof(trafficTopic), "%s/traffic", baseTopic);
  snprintf(trackedTopic, sizeof(trackedTopic), "%s/tracked", baseTopic);
  snprintf(airspaceTopic, sizeof(airspaceTopic), "%s/airspace", baseTopic);
  snprintf(displayCommandTopic, sizeof(displayCommandTopic),
           "%s/command/display", baseTopic);
  snprintf(rangeCommandTopic, sizeof(rangeCommandTopic), "%s/command/range",
           baseTopic);
  snprintf(refreshCommandTopic, sizeof(refreshCommandTopic),
           "%s/command/refresh", baseTopic);
}

void resetObservedState() {
  observedTargetVersion = UINT32_MAX;
  observedRangeGeneration = UINT32_MAX;
  observedTrackingVersion = UINT32_MAX;
  observedDisplayEnabled = !display_power::enabled();
  observedOtaState = ota_update::State::UNAVAILABLE;
  pendingPublishMask = PUBLISH_ALL;
  discoveryIndex = 0;
}

void destroyClient() {
  logMemoryStage("destroy-begin");
  esp_mqtt_client_handle_t localClient = nullptr;
  portENTER_CRITICAL(&stateMux);
  localClient = client;
  client = nullptr;
  clientConnected = false;
  portEXIT_CRITICAL(&stateMux);

  if (localClient) {
    esp_mqtt_client_stop(localClient);
    esp_mqtt_client_destroy(localClient);
  }
  if (targetBuffer) {
    free(targetBuffer);
    targetBuffer = nullptr;
  }
  if (jsonBuffer) {
    free(jsonBuffer);
    jsonBuffer = nullptr;
  }
  portENTER_CRITICAL(&stateMux);
  pendingCommands = 0;
  portEXIT_CRITICAL(&stateMux);
  offlineQueued = false;
  availabilityPublished = false;
  stopAfterMs = 0;
  stopRequested = false;
  logMemoryStage("destroy-complete");
}

bool enqueueMessage(const char* topic, const char* payload, size_t length,
                    bool retain) {
  esp_mqtt_client_handle_t localClient = nullptr;
  bool connected = false;
  portENTER_CRITICAL(&stateMux);
  localClient = client;
  connected = clientConnected;
  portEXIT_CRITICAL(&stateMux);
  if (!localClient || !connected || !topic || !payload || length == 0) {
    return false;
  }
  return esp_mqtt_client_enqueue(localClient, topic, payload,
                                 static_cast<int>(length), 0, retain, true) >= 0;
}

bool enqueueText(const char* topic, const char* payload, bool retain) {
  return enqueueMessage(topic, payload, strlen(payload), retain);
}

void requestStop() {
  if (!client) {
    destroyClient();
    return;
  }
  stopRequested = true;
  setState(State::STOPPING, "Stopping MQTT service");
}

void serviceStop(uint32_t now) {
  if (!client) {
    destroyClient();
    return;
  }
  bool connected = false;
  portENTER_CRITICAL(&stateMux);
  connected = clientConnected;
  portEXIT_CRITICAL(&stateMux);
  if (!offlineQueued && connected) {
    if (enqueueText(availabilityTopic, "offline", true)) {
      offlineQueued = true;
      stopAfterMs = now + STOP_GRACE_MS;
      return;
    }
    if (stopAfterMs == 0) stopAfterMs = now + STOP_GRACE_MS;
    if (static_cast<int32_t>(now - stopAfterMs) < 0) return;
  }
  if (offlineQueued && static_cast<int32_t>(now - stopAfterMs) < 0) return;
  destroyClient();
}

bool startClient() {
  logMemoryStage("start-begin");
  if (!targetBuffer) {
    targetBuffer = static_cast<aircraft::Target*>(heap_caps_calloc(
        aircraft::MAX_TARGETS, sizeof(aircraft::Target),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!targetBuffer) {
      setState(State::ERROR, "MQTT aircraft buffer allocation failed");
      return false;
    }
  }
  if (!jsonBuffer) {
    jsonBuffer = static_cast<char*>(heap_caps_malloc(
        STATE_BUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!jsonBuffer) {
      free(targetBuffer);
      targetBuffer = nullptr;
      setState(State::ERROR, "MQTT JSON buffer allocation failed");
      return false;
    }
    jsonBuffer[0] = 0;
  }
  logMemoryStage("psram-ready");

  esp_mqtt_client_config_t mqttConfig{};
  mqttConfig.broker.address.uri = MQTT_BROKER_URI;
  mqttConfig.credentials.client_id = clientId;
  if (MQTT_USERNAME[0]) mqttConfig.credentials.username = MQTT_USERNAME;
  if (MQTT_PASSWORD[0]) {
    mqttConfig.credentials.authentication.password = MQTT_PASSWORD;
  }
  mqttConfig.session.keepalive = 60;
  mqttConfig.session.last_will.topic = availabilityTopic;
  mqttConfig.session.last_will.msg = "offline";
  mqttConfig.session.last_will.msg_len = 7;
  mqttConfig.session.last_will.qos = 0;
  mqttConfig.session.last_will.retain = true;
  mqttConfig.network.timeout_ms = 10000;
  mqttConfig.network.reconnect_timeout_ms = 30000;
  mqttConfig.task.stack_size = MQTT_TASK_STACK_BYTES;
  mqttConfig.buffer.size = MQTT_BUFFER_BYTES;
  mqttConfig.buffer.out_size = MQTT_BUFFER_BYTES;
  mqttConfig.outbox.limit = MQTT_OUTBOX_BYTES;

  setState(State::STARTING, "Starting MQTT client");
  esp_mqtt_client_handle_t newClient = esp_mqtt_client_init(&mqttConfig);
  if (!newClient) {
    free(targetBuffer);
    targetBuffer = nullptr;
    free(jsonBuffer);
    jsonBuffer = nullptr;
    setState(State::ERROR, "MQTT client initialization failed");
    return false;
  }
  logMemoryStage("client-init");
  portENTER_CRITICAL(&stateMux);
  client = newClient;
  clientConnected = false;
  portEXIT_CRITICAL(&stateMux);
  if (esp_mqtt_client_register_event(
          newClient, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
          mqttEventHandler, nullptr) != ESP_OK) {
    portENTER_CRITICAL(&stateMux);
    if (client == newClient) client = nullptr;
    portEXIT_CRITICAL(&stateMux);
    esp_mqtt_client_destroy(newClient);
    free(targetBuffer);
    targetBuffer = nullptr;
    free(jsonBuffer);
    jsonBuffer = nullptr;
    setState(State::ERROR, "MQTT event registration failed");
    return false;
  }
  resetObservedState();
  setState(State::CONNECTING, "Connecting to MQTT broker");
  if (esp_mqtt_client_start(newClient) != ESP_OK) {
    portENTER_CRITICAL(&stateMux);
    if (client == newClient) client = nullptr;
    clientConnected = false;
    portEXIT_CRITICAL(&stateMux);
    esp_mqtt_client_destroy(newClient);
    free(targetBuffer);
    targetBuffer = nullptr;
    free(jsonBuffer);
    jsonBuffer = nullptr;
    setState(State::ERROR, "MQTT client start failed");
    return false;
  }
  Serial.printf("MQTT client started: %s\n", deviceId);
  logMemoryStage("client-started");
  return true;
}

void writeDevice(JsonWriter& writer) {
  writer.key("device");
  writer.beginObject();
  writer.key("identifiers");
  writer.beginArray();
  writer.value(deviceId);
  writer.endArray();
  writer.key("name");
  writer.value(settings::deviceTitle().c_str());
  writer.key("manufacturer");
  writer.value("Bill's Aircraft Radar");
  writer.key("model");
  writer.value("Waveshare ESP32-S3-Touch-LCD-7");
  writer.key("sw_version");
  writer.value(BUILD_ID);
  writer.endObject();

  writer.key("origin");
  writer.beginObject();
  writer.key("name");
  writer.value("Bill's Aircraft Radar");
  writer.key("sw_version");
  writer.value(BUILD_ID);
  writer.endObject();
}

void writeDiscoveryCommon(JsonWriter& writer, const char* name,
                          const char* uniqueSuffix,
                          const char* defaultEntityId) {
  char uniqueId[80];
  snprintf(uniqueId, sizeof(uniqueId), "%s_%s", deviceId, uniqueSuffix);
  writer.key("name");
  writer.value(name);
  writer.key("unique_id");
  writer.value(uniqueId);
  writer.key("default_entity_id");
  writer.value(defaultEntityId);
  writer.key("availability_topic");
  writer.value(availabilityTopic);
  writer.key("payload_available");
  writer.value("online");
  writer.key("payload_not_available");
  writer.value("offline");
  writeDevice(writer);
}

bool publishDiscovery(uint8_t index) {
  const char* component = nullptr;
  const char* object = nullptr;
  if (!jsonBuffer) return false;
  JsonWriter writer(jsonBuffer, STATE_BUFFER_BYTES);
  writer.beginObject();

  switch (index) {
    case 0:
      component = "switch";
      object = "display";
      writeDiscoveryCommon(writer, "Display Backlight", "display",
                           "switch.bills_aircraft_radar_display");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.display }}");
      writer.key("command_topic"); writer.value(displayCommandTopic);
      writer.key("payload_on"); writer.value("ON");
      writer.key("payload_off"); writer.value("OFF");
      writer.key("icon"); writer.value("mdi:monitor");
      break;

    case 1:
      component = "select";
      object = "range";
      writeDiscoveryCommon(writer, "Radar Range", "range",
                           "select.bills_aircraft_radar_range");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.range_miles }}");
      writer.key("command_topic"); writer.value(rangeCommandTopic);
      writer.key("options");
      writer.beginArray(); writer.value("20"); writer.value("40"); writer.value("80"); writer.endArray();
      writer.key("icon"); writer.value("mdi:radar");
      break;

    case 2:
      component = "button";
      object = "refresh";
      writeDiscoveryCommon(writer, "Refresh ADS-B", "refresh",
                           "button.bills_aircraft_radar_refresh");
      writer.key("command_topic"); writer.value(refreshCommandTopic);
      writer.key("payload_press"); writer.value("PRESS");
      writer.key("icon"); writer.value("mdi:refresh");
      break;

    case 3:
      component = "sensor";
      object = "aircraft_count";
      writeDiscoveryCommon(writer, "Aircraft Count", "aircraft_count",
                           "sensor.bills_aircraft_radar_aircraft_count");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.aircraft_count }}");
      writer.key("icon"); writer.value("mdi:airplane");
      break;

    case 4:
      component = "sensor";
      object = "data_status";
      writeDiscoveryCommon(writer, "Data Status", "data_status",
                           "sensor.bills_aircraft_radar_data_status");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.data_status }}");
      writer.key("json_attributes_topic"); writer.value(statusTopic);
      writer.key("icon"); writer.value("mdi:access-point-network");
      break;

    case 5:
      component = "sensor";
      object = "update_age";
      writeDiscoveryCommon(writer, "Last Update Age", "update_age",
                           "sensor.bills_aircraft_radar_last_update_age");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.update_age_s }}");
      writer.key("unit_of_measurement"); writer.value("s");
      writer.key("state_class"); writer.value("measurement");
      writer.key("icon"); writer.value("mdi:timer-outline");
      break;

    case 6:
      component = "sensor";
      object = "tracked";
      writeDiscoveryCommon(writer, "Tracked Aircraft", "tracked",
                           "sensor.bills_aircraft_radar_tracked_aircraft");
      writer.key("state_topic"); writer.value(trackedTopic);
      writer.key("value_template"); writer.value("{{ value_json.id }}");
      writer.key("json_attributes_topic"); writer.value(trackedTopic);
      writer.key("icon"); writer.value("mdi:airplane-marker");
      break;

    case 7:
      component = "sensor";
      object = "nearest_traffic";
      writeDiscoveryCommon(writer, "Nearest Traffic", "nearest_traffic",
                           "sensor.bills_aircraft_radar_nearest_traffic");
      writer.key("state_topic"); writer.value(trafficTopic);
      writer.key("value_template"); writer.value("{{ value_json.id }}");
      writer.key("json_attributes_topic"); writer.value(trafficTopic);
      writer.key("icon"); writer.value("mdi:airplane-search");
      break;

    case 8:
      component = "sensor";
      object = "airspace_summary";
      writeDiscoveryCommon(writer, "Airspace Summary", "airspace_summary",
                           "sensor.bills_aircraft_radar_airspace_summary");
      writer.key("state_topic"); writer.value(airspaceTopic);
      writer.key("value_template"); writer.value("{{ value_json.count }}");
      writer.key("json_attributes_topic"); writer.value(airspaceTopic);
      writer.key("icon"); writer.value("mdi:chart-donut");
      break;

    case 9:
      component = "sensor";
      object = "wifi_signal";
      writeDiscoveryCommon(writer, "Wi-Fi Signal", "wifi_signal",
                           "sensor.bills_aircraft_radar_wifi_signal");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.wifi_rssi }}");
      writer.key("device_class"); writer.value("signal_strength");
      writer.key("unit_of_measurement"); writer.value("dBm");
      writer.key("state_class"); writer.value("measurement");
      writer.key("entity_category"); writer.value("diagnostic");
      break;

    case 10:
      component = "sensor";
      object = "build";
      writeDiscoveryCommon(writer, "Build", "build",
                           "sensor.bills_aircraft_radar_build");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.build }}");
      writer.key("entity_category"); writer.value("diagnostic");
      writer.key("icon"); writer.value("mdi:chip");
      break;

    case 11:
      component = "sensor";
      object = "ota_status";
      writeDiscoveryCommon(writer, "OTA Status", "ota_status",
                           "sensor.bills_aircraft_radar_ota_status");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.ota_status }}");
      writer.key("entity_category"); writer.value("diagnostic");
      writer.key("icon"); writer.value("mdi:update");
      break;

    case 12:
      component = "sensor";
      object = "mqtt_status";
      writeDiscoveryCommon(writer, "MQTT Status", "mqtt_status",
                           "sensor.bills_aircraft_radar_mqtt_status");
      writer.key("state_topic"); writer.value(statusTopic);
      writer.key("value_template"); writer.value("{{ value_json.mqtt_status }}");
      writer.key("entity_category"); writer.value("diagnostic");
      writer.key("icon"); writer.value("mdi:home-assistant");
      break;

    default:
      return false;
  }

  writer.endObject();
  if (!writer.valid() || !component || !object) return false;

  char topic[160];
  snprintf(topic, sizeof(topic), "homeassistant/%s/%s_%s/config", component,
           deviceId, object);
  return enqueueMessage(topic, writer.c_str(), writer.size(), true);
}

const char* dataStatusName(const app_state::Snapshot& snapshot,
                           const app_state::Diagnostics& diagnostics,
                           bool wifiConnected, bool fetchInProgress,
                           uint32_t now) {
  const uint32_t ageSeconds = snapshot.lastUpdateMs
      ? (now - snapshot.lastUpdateMs) / 1000U : 0;
  if (snapshot.locationUpdatePending) return "UPDATING";
  if (!wifiConnected) return "OFFLINE";
  if (snapshot.lastUpdateMs == 0) return fetchInProgress ? "UPDATING" : "OFFLINE";
  if (ageSeconds >= 60U && diagnostics.consecutiveFailures >= 3U) return "OFFLINE";
  if (ageSeconds >= 60U) return "STALE";
  if (fetchInProgress) return "UPDATING";
  return "LIVE";
}

void writeTarget(JsonWriter& writer, const aircraft::Target& target) {
  const vertical_state::State vertical =
      vertical_state::initialState(target.verticalRateFpm);

  writer.beginObject();
  writer.key("id"); writer.value(aircraft::primaryIdentifier(target));
  writer.key("hex"); writer.value(target.hex);
  writer.key("registration"); writer.value(target.registration);
  writer.key("type"); writer.value(target.typeCode);
  writer.key("operator"); writer.value(target.operatorName);
  writer.key("description"); writer.value(target.description);
  writer.key("distance_mi"); writer.value(target.distanceMiles, 1);
  writer.key("direction"); writer.value(aircraft::compassDirection(target.bearing));
  writer.key("bearing_deg"); writer.value(static_cast<int>(lroundf(target.bearing)));
  writer.key("altitude_ft"); writer.value(static_cast<int>(lroundf(target.altitudeFt)));
  writer.key("speed_mph"); writer.value(static_cast<int>(lroundf(target.speedKt * 1.15077945f)));
  writer.key("vertical_state"); writer.value(vertical_state::stateName(vertical));
  writer.key("vertical_rate_fpm"); writer.value(vertical_state::roundedRateFpm(target.verticalRateFpm));
  writer.key("track_deg");
  if (target.hasTrack) writer.value(static_cast<int>(lroundf(target.track)));
  else writer.nullValue();
  writer.endObject();
}

const aircraft::Target* findTracked(const app_state::Snapshot& snapshot) {
  if (!snapshot.manualTracking || !snapshot.trackedHex[0]) return nullptr;
  for (uint8_t index = 0; index < snapshot.count; ++index) {
    if (strcmp(targetBuffer[index].hex, snapshot.trackedHex) == 0) {
      return &targetBuffer[index];
    }
  }
  return nullptr;
}

uint8_t nearestTargets(const app_state::Snapshot& snapshot,
                       const aircraft::Target* out[NEAREST_COUNT]) {
  for (uint8_t i = 0; i < NEAREST_COUNT; ++i) out[i] = nullptr;
  uint8_t count = 0;
  for (uint8_t index = 0; index < snapshot.count; ++index) {
    const aircraft::Target* candidate = &targetBuffer[index];
    uint8_t insertAt = count;
    while (insertAt > 0 &&
           out[insertAt - 1]->distanceMiles > candidate->distanceMiles) {
      --insertAt;
    }
    if (insertAt >= NEAREST_COUNT) continue;
    const uint8_t end = std::min<uint8_t>(count, NEAREST_COUNT - 1);
    for (uint8_t move = end; move > insertAt; --move) {
      out[move] = out[move - 1];
    }
    out[insertAt] = candidate;
    if (count < NEAREST_COUNT) ++count;
  }
  return count;
}

bool publishStatus(const app_state::Snapshot& snapshot,
                   const app_state::Diagnostics& diagnostics, uint32_t now) {
  if (!jsonBuffer) return false;
  JsonWriter writer(jsonBuffer, STATE_BUFFER_BYTES);
  ota_update::Status otaStatus;
  ota_update::copyStatus(otaStatus);
  const bool wifiConnected = app_state::wifiStatus() == WL_CONNECTED;
  const uint32_t ageSeconds = snapshot.lastUpdateMs
      ? (now - snapshot.lastUpdateMs) / 1000U : 0;

  writer.beginObject();
  writer.key("display"); writer.value(display_power::enabled() ? "ON" : "OFF");
  writer.key("range_miles"); writer.value(static_cast<unsigned>(lroundf(snapshot.rangeMiles)));
  writer.key("aircraft_count"); writer.value(static_cast<unsigned>(snapshot.count));
  writer.key("data_status");
  writer.value(dataStatusName(snapshot, diagnostics, wifiConnected,
                              app_state::fetchInProgress(), now));
  writer.key("update_age_s"); writer.value(static_cast<unsigned long>(ageSeconds));
  writer.key("wifi_rssi"); writer.value(wifiConnected ? WiFi.RSSI() : 0);
  writer.key("failure_stage"); writer.value(app_state::failureStageName(diagnostics.lastFailureStage));
  writer.key("consecutive_failures"); writer.value(static_cast<unsigned>(diagnostics.consecutiveFailures));
  writer.key("fetch_duration_ms"); writer.value(static_cast<unsigned long>(diagnostics.lastDurationMs));
  writer.key("response_bytes"); writer.value(static_cast<unsigned long>(diagnostics.lastResponseBytes));
  writer.key("free_heap"); writer.value(static_cast<unsigned long>(ESP.getFreeHeap()));
  writer.key("free_psram"); writer.value(static_cast<unsigned long>(ESP.getFreePsram()));
  writer.key("build"); writer.value(BUILD_ID);
  writer.key("ota_status"); writer.value(ota_update::stateName(otaStatus.state));
  State mqttState;
  portENTER_CRITICAL(&stateMux);
  mqttState = currentState;
  portEXIT_CRITICAL(&stateMux);
  writer.key("mqtt_status"); writer.value(stateName(mqttState));
  writer.endObject();
  return writer.valid() &&
         enqueueMessage(statusTopic, writer.c_str(), writer.size(), true);
}

bool publishTraffic(const app_state::Snapshot& snapshot) {
  if (!jsonBuffer) return false;
  JsonWriter writer(jsonBuffer, STATE_BUFFER_BYTES);
  const aircraft::Target* nearest[NEAREST_COUNT]{};
  const uint8_t nearestCount = nearestTargets(snapshot, nearest);

  writer.beginObject();
  writer.key("id");
  writer.value(nearestCount ? aircraft::primaryIdentifier(*nearest[0]) : "NONE");
  writer.key("nearest");
  writer.beginArray();
  for (uint8_t index = 0; index < nearestCount; ++index) {
    writeTarget(writer, *nearest[index]);
  }
  writer.endArray();
  writer.endObject();
  return writer.valid() &&
         enqueueMessage(trafficTopic, writer.c_str(), writer.size(), true);
}

bool publishTracked(const app_state::Snapshot& snapshot) {
  if (!jsonBuffer) return false;
  JsonWriter writer(jsonBuffer, STATE_BUFFER_BYTES);
  const aircraft::Target* tracked = findTracked(snapshot);

  writer.beginObject();
  writer.key("id");
  if (tracked) writer.value(aircraft::primaryIdentifier(*tracked));
  else writer.value(snapshot.manualTracking && snapshot.trackedHex[0]
                        ? snapshot.trackedHex : "NONE");
  writer.key("active"); writer.value(snapshot.manualTracking);
  writer.key("present"); writer.value(tracked != nullptr);
  writer.key("aircraft");
  if (tracked) writeTarget(writer, *tracked);
  else writer.nullValue();
  writer.endObject();
  return writer.valid() &&
         enqueueMessage(trackedTopic, writer.c_str(), writer.size(), true);
}

uint8_t categoryIndex(const aircraft::Target& target) {
  switch (aircraft::categoryForTarget(target)) {
    case aircraft::Category::AIRLINER: return 0;
    case aircraft::Category::BUSINESS_JET: return 1;
    case aircraft::Category::TURBOPROP: return 2;
    case aircraft::Category::PISTON: return 3;
    case aircraft::Category::HELICOPTER: return 4;
    default: return 5;
  }
}

void writeOptionalTarget(JsonWriter& writer, const aircraft::Target* target) {
  if (target) writeTarget(writer, *target);
  else writer.nullValue();
}

bool publishAirspace(const app_state::Snapshot& snapshot) {
  if (!jsonBuffer) return false;
  JsonWriter writer(jsonBuffer, STATE_BUFFER_BYTES);
  uint16_t categories[6]{};
  const aircraft::Target* nearest = nullptr;
  const aircraft::Target* fastest = nullptr;
  const aircraft::Target* lowest = nullptr;
  const aircraft::Target* highest = nullptr;

  for (uint8_t index = 0; index < snapshot.count; ++index) {
    const aircraft::Target& target = targetBuffer[index];
    ++categories[categoryIndex(target)];
    if (!nearest || target.distanceMiles < nearest->distanceMiles) nearest = &target;
    if (!fastest || target.speedKt > fastest->speedKt) fastest = &target;
    if (target.altitudeFt > 0.0f &&
        (!lowest || target.altitudeFt < lowest->altitudeFt)) {
      lowest = &target;
    }
    if (target.altitudeFt > 0.0f &&
        (!highest || target.altitudeFt > highest->altitudeFt)) {
      highest = &target;
    }
  }

  writer.beginObject();
  writer.key("count"); writer.value(static_cast<unsigned>(snapshot.count));
  writer.key("nearest"); writeOptionalTarget(writer, nearest);
  writer.key("fastest"); writeOptionalTarget(writer, fastest);
  writer.key("lowest_airborne"); writeOptionalTarget(writer, lowest);
  writer.key("highest_airborne"); writeOptionalTarget(writer, highest);
  writer.key("categories");
  writer.beginObject();
  writer.key("airliners"); writer.value(static_cast<unsigned>(categories[0]));
  writer.key("business_jets"); writer.value(static_cast<unsigned>(categories[1]));
  writer.key("turboprops"); writer.value(static_cast<unsigned>(categories[2]));
  writer.key("piston"); writer.value(static_cast<unsigned>(categories[3]));
  writer.key("helicopters"); writer.value(static_cast<unsigned>(categories[4]));
  writer.key("military_other"); writer.value(static_cast<unsigned>(categories[5]));
  writer.endObject();
  writer.endObject();
  return writer.valid() &&
         enqueueMessage(airspaceTopic, writer.c_str(), writer.size(), true);
}

void observeChanges(uint32_t now) {
  const uint32_t targetVersion = app_state::targetVersion();
  const uint32_t rangeGeneration = app_state::rangeGeneration();
  const uint32_t trackingVersion = app_state::trackingVersion();
  const bool displayEnabled = display_power::enabled();
  ota_update::Status otaStatus;
  ota_update::copyStatus(otaStatus);

  if (targetVersion != observedTargetVersion) {
    observedTargetVersion = targetVersion;
    pendingPublishMask |= PUBLISH_ALL;
  }
  if (rangeGeneration != observedRangeGeneration) {
    observedRangeGeneration = rangeGeneration;
    pendingPublishMask |= PUBLISH_STATUS | PUBLISH_TRAFFIC | PUBLISH_AIRSPACE;
  }
  if (trackingVersion != observedTrackingVersion) {
    observedTrackingVersion = trackingVersion;
    pendingPublishMask |= PUBLISH_STATUS | PUBLISH_TRACKED;
  }
  if (displayEnabled != observedDisplayEnabled) {
    observedDisplayEnabled = displayEnabled;
    pendingPublishMask |= PUBLISH_STATUS;
  }
  if (otaStatus.state != observedOtaState) {
    observedOtaState = otaStatus.state;
    pendingPublishMask |= PUBLISH_STATUS;
  }
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    pendingPublishMask |= PUBLISH_STATUS;
  }
}

void publishPending(uint32_t now) {
  bool connected = false;
  portENTER_CRITICAL(&stateMux);
  connected = clientConnected;
  portEXIT_CRITICAL(&stateMux);
  if (!connected || !targetBuffer || pendingPublishMask == 0) return;

  app_state::Snapshot snapshot;
  app_state::copySnapshot(targetBuffer, snapshot);
  app_state::Diagnostics diagnostics;
  app_state::copyDiagnostics(diagnostics);

  if ((pendingPublishMask & PUBLISH_STATUS) &&
      publishStatus(snapshot, diagnostics, now)) {
    pendingPublishMask &= static_cast<uint8_t>(~PUBLISH_STATUS);
  }
  if ((pendingPublishMask & PUBLISH_TRAFFIC) && publishTraffic(snapshot)) {
    pendingPublishMask &= static_cast<uint8_t>(~PUBLISH_TRAFFIC);
  }
  if ((pendingPublishMask & PUBLISH_TRACKED) && publishTracked(snapshot)) {
    pendingPublishMask &= static_cast<uint8_t>(~PUBLISH_TRACKED);
  }
  if ((pendingPublishMask & PUBLISH_AIRSPACE) && publishAirspace(snapshot)) {
    pendingPublishMask &= static_cast<uint8_t>(~PUBLISH_AIRSPACE);
  }
}

void processCommands() {
  const uint32_t commands = takeCommands();
  if (commands & COMMAND_DISPLAY_ON) display_power::setEnabled(true);
  if (commands & COMMAND_DISPLAY_OFF) display_power::setEnabled(false);
  if (commands & COMMAND_RANGE_20) radar_control::setManualRangeMiles(20.0f);
  if (commands & COMMAND_RANGE_40) radar_control::setManualRangeMiles(40.0f);
  if (commands & COMMAND_RANGE_80) radar_control::setManualRangeMiles(80.0f);
  if (commands & COMMAND_REFRESH) adsb::requestRefresh();
  if (commands & COMMAND_REDISCOVER) {
    availabilityPublished = false;
    discoveryIndex = 0;
    pendingPublishMask |= PUBLISH_ALL;
  }
}

}  // namespace

const char* stateName(State state) {
  switch (state) {
    case State::INACTIVE: return "DISABLED";
    case State::NOT_CONFIGURED: return "NOT CONFIGURED";
    case State::WAITING_FOR_WIFI: return "WAITING FOR WI-FI";
    case State::STARTING: return "STARTING";
    case State::CONNECTING: return "CONNECTING";
    case State::CONNECTED: return "CONNECTED";
    case State::STOPPING: return "STOPPING";
    case State::MAINTENANCE: return "MAINTENANCE";
    case State::ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

bool begin() {
  initializeIdentity();
  configured = brokerConfigured();
  desiredEnabled = settings::mqttEnabled();
  maintenanceRequested = false;
  maintenanceActive = false;
  if (!desiredEnabled) {
    setState(State::INACTIVE, "MQTT disabled; no task or buffer allocated");
  } else if (!configured) {
    setState(State::NOT_CONFIGURED,
             "Add MQTT_BROKER_URI to private include/config.h");
  } else {
    setState(State::WAITING_FOR_WIFI, "Waiting for Wi-Fi");
  }
  Serial.printf("MQTT: %s, configured=%s\n", desiredEnabled ? "enabled" : "disabled",
                configured ? "yes" : "no");
  return true;
}

bool setEnabled(bool enabledValue) {
  if (!settings::setMqttEnabled(enabledValue)) return false;
  portENTER_CRITICAL(&stateMux);
  desiredEnabled = enabledValue;
  pendingCommands = 0;
  portEXIT_CRITICAL(&stateMux);
  nextStartAttemptMs = 0;
  return true;
}

void requestDiscoveryRefresh() {
  portENTER_CRITICAL(&stateMux);
  if (desiredEnabled) pendingCommands |= COMMAND_REDISCOVER;
  portEXIT_CRITICAL(&stateMux);
}

void requestMaintenanceHold() {
  portENTER_CRITICAL(&stateMux);
  maintenanceRequested = true;
  portEXIT_CRITICAL(&stateMux);
}

bool maintenanceHoldActive() {
  portENTER_CRITICAL(&stateMux);
  const bool active = maintenanceActive;
  portEXIT_CRITICAL(&stateMux);
  return active;
}

void releaseMaintenanceHold() {
  portENTER_CRITICAL(&stateMux);
  maintenanceRequested = false;
  maintenanceActive = false;
  portEXIT_CRITICAL(&stateMux);
}

void service() {
  const uint32_t now = millis();

  bool localMaintenanceRequested = false;
  bool localDesiredEnabled = false;
  bool localClientPresent = false;
  bool localBufferPresent = false;
  portENTER_CRITICAL(&stateMux);
  localMaintenanceRequested = maintenanceRequested;
  localDesiredEnabled = desiredEnabled;
  localClientPresent = client != nullptr;
  localBufferPresent = targetBuffer != nullptr || jsonBuffer != nullptr;
  portEXIT_CRITICAL(&stateMux);

  // The normal disabled state is deliberately almost free: no MQTT task,
  // no broker traffic, no aircraft buffer, and no command-processing work.
  if (!localMaintenanceRequested && !localDesiredEnabled &&
      !localClientPresent && !localBufferPresent) {
    return;
  }

  if (now - lastServiceMs < SERVICE_INTERVAL_MS) return;
  lastServiceMs = now;

  if (localMaintenanceRequested || !localDesiredEnabled) {
    takeCommands();
  } else {
    processCommands();
  }

  if (localMaintenanceRequested) {
    if (client || targetBuffer || jsonBuffer) {
      if (!stopRequested) requestStop();
      serviceStop(now);
      if (client || targetBuffer || jsonBuffer) return;
    }
    portENTER_CRITICAL(&stateMux);
    maintenanceActive = true;
    portEXIT_CRITICAL(&stateMux);
    setState(State::MAINTENANCE, "MQTT idle for firmware update");
    return;
  }

  portENTER_CRITICAL(&stateMux);
  maintenanceActive = false;
  portEXIT_CRITICAL(&stateMux);

  if (!localDesiredEnabled) {
    if (client || targetBuffer || jsonBuffer) {
      if (!stopRequested) requestStop();
      serviceStop(now);
      if (client || targetBuffer || jsonBuffer) return;
    }
    setState(State::INACTIVE, "MQTT disabled; no task or buffer allocated");
    return;
  }

  if (!configured) {
    if (client || targetBuffer || jsonBuffer) destroyClient();
    setState(State::NOT_CONFIGURED,
             "Add MQTT_BROKER_URI to private include/config.h");
    return;
  }

  if (!client) {
    if (WiFi.status() != WL_CONNECTED) {
      setState(State::WAITING_FOR_WIFI, "Waiting for Wi-Fi");
      return;
    }
    if (nextStartAttemptMs && static_cast<int32_t>(now - nextStartAttemptMs) < 0) {
      return;
    }
    if (!startClient()) {
      nextStartAttemptMs = now + START_RETRY_MS;
      return;
    }
    nextStartAttemptMs = 0;
  }

  if (stopRequested) {
    serviceStop(now);
    return;
  }

  bool connected = false;
  portENTER_CRITICAL(&stateMux);
  connected = clientConnected;
  portEXIT_CRITICAL(&stateMux);
  if (!connected) return;
  if (!availabilityPublished) {
    if (enqueueText(availabilityTopic, "online", true)) {
      availabilityPublished = true;
    }
    return;
  }
  if (discoveryIndex < DISCOVERY_COUNT) {
    if (publishDiscovery(discoveryIndex)) {
      ++discoveryIndex;
      if (discoveryIndex == DISCOVERY_COUNT) {
        logMemoryStage("discovery-complete");
      }
    }
    return;
  }

  if (lastHeartbeatMs == 0) lastHeartbeatMs = now;
  observeChanges(now);
  publishPending(now);
}

void copyStatus(Status& status) {
  status = Status{};
  portENTER_CRITICAL(&stateMux);
  status.state = currentState;
  status.configured = configured;
  status.enabled = desiredEnabled;
  status.clientRunning = client != nullptr;
  status.connected = clientConnected;
  status.maintenanceActive = maintenanceActive;
  memcpy(status.deviceId, deviceId, sizeof(status.deviceId));
  memcpy(status.message, statusMessage, sizeof(status.message));
  portEXIT_CRITICAL(&stateMux);
  status.deviceId[sizeof(status.deviceId) - 1] = 0;
  status.message[sizeof(status.message) - 1] = 0;
}

}  // namespace mqtt_service
