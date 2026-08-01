#include "ota_update.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_app_format.h>
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <cstring>

#include "adsb_network.h"
#include "build_info.h"
#include "mqtt_service.h"

namespace ota_update {
namespace {

constexpr uint16_t OTA_PORT = 80;
constexpr uint16_t PACKAGE_FORMAT_VERSION = 1;
constexpr uint16_t PACKAGE_HEADER_SIZE = 512;
constexpr uint32_t MINIMUM_FIRMWARE_SIZE = 64U * 1024U;
constexpr uint32_t PREPARE_TIMEOUT_MS = 45U * 1000U;
constexpr uint32_t RESTART_DELAY_MS = 1500U;
constexpr uint8_t ESP_APPLICATION_MAGIC = 0xE9;
constexpr uint16_t ESP32_S3_IMAGE_CHIP_ID = 9;
constexpr char PACKAGE_MAGIC[16] = "BILLS-RADAR-OTA";
constexpr char HARDWARE_ID[32] = "WAVESHARE-ESP32-S3-LCD-7";
constexpr char OTA_HOSTNAME[] = "bills-aircraft-radar";
constexpr char ACCESS_CODE_HEADER[] = "X-OTA-Code";

#pragma pack(push, 1)
struct PackageHeader {
  char magic[16];
  uint16_t formatVersion;
  uint16_t headerSize;
  char hardwareId[32];
  char buildId[96];
  uint32_t firmwareSize;
  uint8_t firmwareSha256[32];
  uint8_t reserved[328];
};
#pragma pack(pop)

static_assert(sizeof(PackageHeader) == PACKAGE_HEADER_SIZE,
              "Radar OTA package header must remain 512 bytes");
static_assert(sizeof(esp_image_header_t) == 24,
              "Unexpected ESP application image header size");

const char UPDATE_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bill's Aircraft Radar Update</title>
<style>
:root{color-scheme:dark;font-family:Arial,sans-serif;background:#041019;color:#e1ebf0}
body{margin:0;display:grid;place-items:center;min-height:100vh;padding:18px;box-sizing:border-box}
main{width:min(620px,100%);background:#0a1821;border:1px solid #23505b;border-radius:12px;padding:24px;box-sizing:border-box}
h1{margin:0 0 6px;color:#6edcff;font-size:26px}.sub{color:#64aab5;margin-bottom:22px}
label{display:block;margin:14px 0 6px;color:#8bddea}input{width:100%;box-sizing:border-box;padding:11px;border-radius:7px;border:1px solid #286672;background:#0c1c26;color:#fff}
button{margin-top:16px;padding:11px 16px;border:0;border-radius:7px;background:#188054;color:white;font-weight:700;cursor:pointer}button.secondary{background:#144452;margin-left:8px}button:disabled{opacity:.45;cursor:not-allowed}
progress{width:100%;height:20px;margin-top:18px}.status{min-height:54px;margin-top:16px;padding:12px;background:#07141c;border-radius:7px;white-space:pre-wrap}.warn{color:#ffd66a;font-size:13px;margin-top:18px}
</style>
</head>
<body><main>
<h1>BILL'S AIRCRAFT RADAR</h1><div class="sub">Local firmware update</div>
<label for="code">Six-digit access code shown on the radar</label><input id="code" inputmode="numeric" maxlength="6" autocomplete="one-time-code">
<label for="file">Radar OTA package</label><input id="file" type="file" accept=".radarota,application/octet-stream">
<button id="install">PREPARE &amp; INSTALL</button><button class="secondary" id="cancel">CANCEL OTA</button>
<progress id="progress" max="100" value="0"></progress><div class="status" id="status">Select the generated firmware.radarota file.</div>
<div class="warn">Do not remove power while firmware is being written. This page accepts only a Bill's 7-inch Radar .radarota package.</div>
<script>
const code=()=>document.getElementById('code').value.trim();
const status=document.getElementById('status'), progress=document.getElementById('progress'), install=document.getElementById('install');
async function call(path,options={}){options.headers=Object.assign({},options.headers||{}, {'X-OTA-Code':code()});const r=await fetch(path,options);const t=await r.text();let j={message:t};try{j=JSON.parse(t)}catch(e){}if(!r.ok)throw new Error(j.message||('HTTP '+r.status));return j}
async function waitReady(){for(let i=0;i<180;i++){const j=await call('/status');status.textContent=j.message||j.state;if(j.state==='READY')return;if(j.state==='ERROR')throw new Error(j.message);await new Promise(r=>setTimeout(r,250))}throw new Error('Radar did not enter update-ready state')}
function upload(file){return new Promise((resolve,reject)=>{const x=new XMLHttpRequest();x.open('POST','/upload');x.setRequestHeader('X-OTA-Code',code());x.upload.onprogress=e=>{if(e.lengthComputable)progress.value=Math.round(e.loaded*100/e.total)};x.onload=()=>{let j={message:x.responseText};try{j=JSON.parse(x.responseText)}catch(e){};x.status>=200&&x.status<300?resolve(j):reject(new Error(j.message||('HTTP '+x.status)))};x.onerror=()=>reject(new Error('Upload connection failed'));const f=new FormData();f.append('firmware',file,file.name);x.send(f)})}
install.onclick=async()=>{const file=document.getElementById('file').files[0];if(!/^\d{6}$/.test(code())){status.textContent='Enter the six-digit access code.';return}if(!file||!file.name.toLowerCase().endsWith('.radarota')){status.textContent='Choose firmware.radarota.';return}install.disabled=true;progress.value=0;try{status.textContent='Waiting for network services to become idle...';await call('/prepare',{method:'POST'});await waitReady();status.textContent='Uploading and validating firmware...';const j=await upload(file);status.textContent=j.message||'Update complete. Radar is restarting.';progress.value=100}catch(e){status.textContent=e.message}finally{install.disabled=false}};
document.getElementById('cancel').onclick=async()=>{try{const j=await call('/cancel',{method:'POST'});status.textContent=j.message}catch(e){status.textContent=e.message}};
</script></main></body></html>
)HTML";

WebServer server(OTA_PORT);
State currentState = State::UNAVAILABLE;
const esp_partition_t* updatePartition = nullptr;
bool routesConfigured = false;
bool serverRunning = false;
bool mdnsRunning = false;
bool stopServerPending = false;
uint32_t armedUntilMs = 0;
uint32_t prepareDeadlineMs = 0;
uint32_t restartAtMs = 0;
char accessCode[7]{};
char statusMessage[128] = "OTA service unavailable";

PackageHeader packageHeader{};
uint8_t packageHeaderBytes[PACKAGE_HEADER_SIZE]{};
size_t packageHeaderReceived = 0;
uint8_t imagePrefix[sizeof(esp_image_header_t)]{};
size_t imagePrefixReceived = 0;
uint32_t payloadReceived = 0;
uint32_t payloadWritten = 0;
esp_ota_handle_t otaHandle = 0;
bool otaHandleActive = false;
mbedtls_sha256_context shaContext;
bool shaActive = false;
bool uploadAccepted = false;
uint8_t buildMatchFailure[sizeof(packageHeader.buildId)]{};
size_t buildPatternLength = 0;
size_t buildMatchLength = 0;
bool buildIdSeen = false;
int uploadResponseCode = 500;
char uploadResponseMessage[128] = "Upload did not complete";

void setMessage(const char* message) {
  snprintf(statusMessage, sizeof(statusMessage), "%s",
           message ? message : "");
}

void setUploadResponse(int code, const char* message) {
  uploadResponseCode = code;
  snprintf(uploadResponseMessage, sizeof(uploadResponseMessage), "%s",
           message ? message : "");
}

bool codeMatches() {
  if (!server.hasHeader(ACCESS_CODE_HEADER)) return false;
  const String supplied = server.header(ACCESS_CODE_HEADER);
  return supplied.length() == 6 && supplied.equals(accessCode);
}

void sendJson(int code, const char* message) {
  char body[384];
  const uint8_t progress = packageHeader.firmwareSize
      ? static_cast<uint8_t>(std::min<uint32_t>(
            100U, (payloadWritten * 100U) / packageHeader.firmwareSize))
      : 0;
  snprintf(body, sizeof(body),
           "{\"state\":\"%s\",\"message\":\"%s\","
           "\"progress\":%u,\"build\":\"%s\"}",
           stateName(currentState), message ? message : statusMessage,
           static_cast<unsigned>(progress), BUILD_ID);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(code, "application/json", body);
}

void resetUploadSession() {
  if (otaHandleActive) {
    esp_ota_abort(otaHandle);
    otaHandleActive = false;
  }
  if (shaActive) {
    mbedtls_sha256_free(&shaContext);
    shaActive = false;
  }
  memset(&packageHeader, 0, sizeof(packageHeader));
  memset(packageHeaderBytes, 0, sizeof(packageHeaderBytes));
  memset(imagePrefix, 0, sizeof(imagePrefix));
  packageHeaderReceived = 0;
  imagePrefixReceived = 0;
  payloadReceived = 0;
  payloadWritten = 0;
  otaHandle = 0;
  uploadAccepted = false;
  memset(buildMatchFailure, 0, sizeof(buildMatchFailure));
  buildPatternLength = 0;
  buildMatchLength = 0;
  buildIdSeen = false;
  setUploadResponse(500, "Upload did not complete");
}

void releaseMaintenanceHold() {
  adsb::releaseMaintenanceHold();
  mqtt_service::releaseMaintenanceHold();
}

void failUpload(const char* message, int responseCode = 400) {
  if (otaHandleActive) {
    esp_ota_abort(otaHandle);
    otaHandleActive = false;
  }
  if (shaActive) {
    mbedtls_sha256_free(&shaContext);
    shaActive = false;
  }
  uploadAccepted = false;
  currentState = State::ERROR;
  setMessage(message);
  setUploadResponse(responseCode, message);
  releaseMaintenanceHold();
  Serial.printf("OTA update failed: %s\n", message ? message : "unknown");
}

void prepareBuildIdentityMatcher() {
  buildPatternLength = 0;
  while (buildPatternLength < sizeof(packageHeader.buildId) &&
         packageHeader.buildId[buildPatternLength] != 0) {
    ++buildPatternLength;
  }
  buildMatchLength = 0;
  buildIdSeen = false;
  memset(buildMatchFailure, 0, sizeof(buildMatchFailure));
  for (size_t index = 1, prefix = 0; index < buildPatternLength; ++index) {
    while (prefix > 0 &&
           packageHeader.buildId[index] != packageHeader.buildId[prefix]) {
      prefix = buildMatchFailure[prefix - 1];
    }
    if (packageHeader.buildId[index] == packageHeader.buildId[prefix]) {
      ++prefix;
    }
    buildMatchFailure[index] = static_cast<uint8_t>(prefix);
  }
}

void observeBuildIdentity(const uint8_t* data, size_t length) {
  if (buildIdSeen || buildPatternLength == 0) return;
  for (size_t index = 0; index < length; ++index) {
    const char current = static_cast<char>(data[index]);
    while (buildMatchLength > 0 &&
           current != packageHeader.buildId[buildMatchLength]) {
      buildMatchLength = buildMatchFailure[buildMatchLength - 1];
    }
    if (current == packageHeader.buildId[buildMatchLength]) {
      ++buildMatchLength;
      if (buildMatchLength == buildPatternLength) {
        buildIdSeen = true;
        return;
      }
    }
  }
}

bool validatePackageHeader() {
  memcpy(&packageHeader, packageHeaderBytes, sizeof(packageHeader));
  if (memcmp(packageHeader.magic, PACKAGE_MAGIC, sizeof(PACKAGE_MAGIC)) != 0) {
    failUpload("Not a Bill's Radar OTA package");
    return false;
  }
  if (packageHeader.formatVersion != PACKAGE_FORMAT_VERSION ||
      packageHeader.headerSize != PACKAGE_HEADER_SIZE) {
    failUpload("Unsupported radar OTA package version");
    return false;
  }
  if (packageHeader.hardwareId[sizeof(packageHeader.hardwareId) - 1] != 0 ||
      strcmp(packageHeader.hardwareId, HARDWARE_ID) != 0) {
    failUpload("Package is for different hardware");
    return false;
  }
  if (packageHeader.buildId[sizeof(packageHeader.buildId) - 1] != 0 ||
      strncmp(packageHeader.buildId, "7IN-", 4) != 0) {
    failUpload("Package build identity is invalid");
    return false;
  }
  prepareBuildIdentityMatcher();
  if (!updatePartition ||
      packageHeader.firmwareSize < MINIMUM_FIRMWARE_SIZE ||
      packageHeader.firmwareSize > updatePartition->size) {
    failUpload("Firmware does not fit the inactive OTA slot", 413);
    return false;
  }

  const esp_err_t beginResult =
      esp_ota_begin(updatePartition, packageHeader.firmwareSize, &otaHandle);
  if (beginResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "OTA partition begin failed: %s",
             esp_err_to_name(beginResult));
    failUpload(message, 500);
    return false;
  }
  otaHandleActive = true;
  mbedtls_sha256_init(&shaContext);
  if (mbedtls_sha256_starts(&shaContext, 0) != 0) {
    failUpload("SHA-256 initialization failed", 500);
    return false;
  }
  shaActive = true;
  Serial.printf("OTA package accepted: build=%s, firmware=%lu bytes\n",
                packageHeader.buildId,
                static_cast<unsigned long>(packageHeader.firmwareSize));
  return true;
}

bool validateImagePrefix() {
  esp_image_header_t imageHeader{};
  memcpy(&imageHeader, imagePrefix, sizeof(imageHeader));
  if (imageHeader.magic != ESP_APPLICATION_MAGIC) {
    failUpload("Firmware payload has invalid ESP image magic");
    return false;
  }
  if (imageHeader.chip_id != ESP32_S3_IMAGE_CHIP_ID) {
    failUpload("Firmware payload is not for ESP32-S3");
    return false;
  }
  return true;
}

bool writeOtaBytes(const uint8_t* data, size_t length) {
  if (!length) return true;
  const esp_err_t result = esp_ota_write(otaHandle, data, length);
  if (result != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Firmware write failed: %s",
             esp_err_to_name(result));
    failUpload(message, 500);
    return false;
  }
  payloadWritten += static_cast<uint32_t>(length);
  return true;
}

bool processPayload(const uint8_t* data, size_t length) {
  if (!length) return true;
  if (!shaActive || !otaHandleActive) {
    failUpload("OTA writer was not initialized", 500);
    return false;
  }
  if (payloadReceived > packageHeader.firmwareSize ||
      length > packageHeader.firmwareSize - payloadReceived) {
    failUpload("Package contains more firmware data than declared");
    return false;
  }
  observeBuildIdentity(data, length);
  if (mbedtls_sha256_update(&shaContext, data, length) != 0) {
    failUpload("SHA-256 update failed", 500);
    return false;
  }
  payloadReceived += static_cast<uint32_t>(length);

  if (imagePrefixReceived < sizeof(imagePrefix)) {
    const size_t needed = sizeof(imagePrefix) - imagePrefixReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(imagePrefix + imagePrefixReceived, data, copyLength);
    imagePrefixReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (imagePrefixReceived < sizeof(imagePrefix)) return true;
    if (!validateImagePrefix()) return false;
    if (!writeOtaBytes(imagePrefix, sizeof(imagePrefix))) return false;
  }
  return writeOtaBytes(data, length);
}

bool processUploadBytes(const uint8_t* data, size_t length) {
  if (!uploadAccepted) return false;
  if (packageHeaderReceived < PACKAGE_HEADER_SIZE) {
    const size_t needed = PACKAGE_HEADER_SIZE - packageHeaderReceived;
    const size_t copyLength = std::min(needed, length);
    memcpy(packageHeaderBytes + packageHeaderReceived, data, copyLength);
    packageHeaderReceived += copyLength;
    data += copyLength;
    length -= copyLength;
    if (packageHeaderReceived == PACKAGE_HEADER_SIZE &&
        !validatePackageHeader()) {
      return false;
    }
  }
  if (length && packageHeaderReceived == PACKAGE_HEADER_SIZE) {
    return processPayload(data, length);
  }
  return true;
}

void finishUpload(size_t totalPackageBytes) {
  if (!uploadAccepted) return;
  if (packageHeaderReceived != PACKAGE_HEADER_SIZE || !otaHandleActive ||
      !shaActive) {
    failUpload("Upload ended before the package header was complete");
    return;
  }
  const uint32_t expectedPackageSize =
      PACKAGE_HEADER_SIZE + packageHeader.firmwareSize;
  if (totalPackageBytes != expectedPackageSize ||
      payloadReceived != packageHeader.firmwareSize ||
      payloadWritten != packageHeader.firmwareSize) {
    failUpload("Firmware package is truncated or has an incorrect length");
    return;
  }
  if (!buildIdSeen) {
    failUpload("Firmware payload does not contain the declared build ID");
    return;
  }

  uint8_t actualSha256[32]{};
  if (mbedtls_sha256_finish(&shaContext, actualSha256) != 0) {
    failUpload("SHA-256 finalization failed", 500);
    return;
  }
  mbedtls_sha256_free(&shaContext);
  shaActive = false;
  if (memcmp(actualSha256, packageHeader.firmwareSha256,
             sizeof(actualSha256)) != 0) {
    failUpload("Firmware SHA-256 does not match the package header");
    return;
  }

  const esp_err_t endResult = esp_ota_end(otaHandle);
  otaHandleActive = false;
  if (endResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "ESP image validation failed: %s",
             esp_err_to_name(endResult));
    failUpload(message);
    return;
  }
  const esp_err_t bootResult = esp_ota_set_boot_partition(updatePartition);
  if (bootResult != ESP_OK) {
    char message[128];
    snprintf(message, sizeof(message), "Boot partition update failed: %s",
             esp_err_to_name(bootResult));
    failUpload(message, 500);
    return;
  }

  uploadAccepted = false;
  currentState = State::SUCCESS;
  setMessage("Firmware verified. Radar is restarting.");
  setUploadResponse(200, statusMessage);
  restartAtMs = millis() + RESTART_DELAY_MS;
  Serial.printf("OTA update verified: %s (%lu bytes); restart scheduled\n",
                packageHeader.buildId,
                static_cast<unsigned long>(packageHeader.firmwareSize));
}

void handleUploadData() {
  HTTPUpload& upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START:
      resetUploadSession();
      if (!codeMatches()) {
        setUploadResponse(403, "Access code rejected");
        return;
      }
      if (currentState != State::READY || !adsb::maintenanceHoldActive() ||
          !mqtt_service::maintenanceHoldActive()) {
        setUploadResponse(409, "Radar is not ready for firmware upload");
        return;
      }
      if (!upload.filename.endsWith(".radarota")) {
        setUploadResponse(415, "Select a .radarota package");
        return;
      }
      uploadAccepted = true;
      currentState = State::UPLOADING;
      setMessage("Receiving and validating firmware");
      setUploadResponse(500, "Upload did not complete");
      Serial.printf("OTA upload started: %s\n", upload.filename.c_str());
      break;

    case UPLOAD_FILE_WRITE:
      if (uploadAccepted && upload.currentSize) {
        processUploadBytes(upload.buf, upload.currentSize);
        delay(0);
      }
      break;

    case UPLOAD_FILE_END:
      finishUpload(upload.totalSize);
      break;

    case UPLOAD_FILE_ABORTED:
      if (uploadAccepted || currentState == State::UPLOADING) {
        failUpload("Firmware upload was interrupted");
      } else {
        setUploadResponse(400, "Firmware upload was interrupted");
      }
      break;
  }
}

void handlePrepare() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  if (currentState != State::ARMED && currentState != State::ERROR) {
    sendJson(409, "OTA service is not available for preparation");
    return;
  }
  resetUploadSession();
  adsb::requestMaintenanceHold();
  mqtt_service::requestMaintenanceHold();
  currentState = State::PREPARING;
  prepareDeadlineMs = millis() + PREPARE_TIMEOUT_MS;
  setMessage("Waiting for network services to become idle");
  sendJson(202, statusMessage);
}

void handleStatus() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  sendJson(200, statusMessage);
}

void handleCancel() {
  if (!codeMatches()) {
    sendJson(403, "Access code rejected");
    return;
  }
  if (currentState == State::UPLOADING || currentState == State::SUCCESS) {
    sendJson(409, "An active firmware operation cannot be cancelled");
    return;
  }
  releaseMaintenanceHold();
  currentState = State::INACTIVE;
  armedUntilMs = 0;
  prepareDeadlineMs = 0;
  accessCode[0] = 0;
  setMessage("Local OTA disabled");
  sendJson(200, statusMessage);
  stopServerPending = true;
}

void handleUploadComplete() {
  sendJson(uploadResponseCode, uploadResponseMessage);
}

void configureRoutes() {
  if (routesConfigured) return;
  const char* headerKeys[] = {ACCESS_CODE_HEADER};
  server.collectHeaders(headerKeys, 1);
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Location", "/update", true);
    server.send(302, "text/plain", "");
  });
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", UPDATE_PAGE);
  });
  server.on("/prepare", HTTP_POST, handlePrepare);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/cancel", HTTP_POST, handleCancel);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadData);
  server.onNotFound([]() { sendJson(404, "Not found"); });
  routesConfigured = true;
}

void stopServer() {
  if (serverRunning) {
    server.stop();
    serverRunning = false;
  }
  if (mdnsRunning) {
    MDNS.end();
    mdnsRunning = false;
  }
  stopServerPending = false;
}

}  // namespace

const char* stateName(State state) {
  switch (state) {
    case State::UNAVAILABLE: return "UNAVAILABLE";
    case State::INACTIVE: return "DISABLED";
    case State::ARMED: return "ARMED";
    case State::PREPARING: return "PREPARING";
    case State::READY: return "READY";
    case State::UPLOADING: return "UPLOADING";
    case State::ERROR: return "ERROR";
    case State::SUCCESS: return "SUCCESS";
    default: return "UNKNOWN";
  }
}

bool begin() {
  configureRoutes();
  updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (!updatePartition || updatePartition->size < MINIMUM_FIRMWARE_SIZE) {
    currentState = State::UNAVAILABLE;
    setMessage("No usable inactive OTA partition");
    Serial.println("OTA unavailable: no usable inactive OTA partition");
    return false;
  }
  currentState = State::INACTIVE;
  setMessage("Local OTA is disabled");
  Serial.printf("OTA ready: inactive partition %s, %lu bytes\n",
                updatePartition->label,
                static_cast<unsigned long>(updatePartition->size));
  return true;
}

bool enable() {
  if (!updatePartition || currentState == State::UNAVAILABLE || busy()) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    currentState = State::ERROR;
    setMessage("Wi-Fi must be connected before enabling OTA");
    return false;
  }

  resetUploadSession();
  snprintf(accessCode, sizeof(accessCode), "%06lu",
           static_cast<unsigned long>(esp_random() % 1000000U));
  armedUntilMs = millis() + ENABLE_WINDOW_MS;
  prepareDeadlineMs = 0;
  restartAtMs = 0;
  currentState = State::ARMED;
  setMessage("Open the local update page and enter the access code");

  if (!serverRunning) {
    server.begin();
    serverRunning = true;
  }
  if (!mdnsRunning && MDNS.begin(OTA_HOSTNAME)) {
    MDNS.addService("http", "tcp", OTA_PORT);
    mdnsRunning = true;
  }
  Serial.printf("OTA enabled for five minutes: http://%s/update\n",
                WiFi.localIP().toString().c_str());
  return true;
}

void disable() {
  if (currentState == State::UPLOADING || currentState == State::SUCCESS) return;
  resetUploadSession();
  releaseMaintenanceHold();
  stopServer();
  armedUntilMs = 0;
  prepareDeadlineMs = 0;
  accessCode[0] = 0;
  currentState = updatePartition ? State::INACTIVE : State::UNAVAILABLE;
  setMessage(updatePartition ? "Local OTA is disabled"
                             : "OTA service unavailable");
  Serial.println("OTA disabled");
}

bool busy() {
  return currentState == State::PREPARING || currentState == State::READY ||
         currentState == State::UPLOADING || currentState == State::SUCCESS;
}

void service() {
  if (serverRunning) server.handleClient();
  if (stopServerPending) stopServer();

  const uint32_t now = millis();
  if (currentState == State::PREPARING && adsb::maintenanceHoldActive() &&
      mqtt_service::maintenanceHoldActive()) {
    currentState = State::READY;
    setMessage("Radar ready. Upload may begin.");
    Serial.println("OTA network maintenance holds active; upload permitted");
  }
  if ((currentState == State::PREPARING || currentState == State::READY) &&
      prepareDeadlineMs && (int32_t)(now - prepareDeadlineMs) >= 0) {
    releaseMaintenanceHold();
    currentState = State::ARMED;
    prepareDeadlineMs = 0;
    setMessage("Upload preparation expired; try again");
  }
  if (serverRunning && currentState != State::UPLOADING &&
      currentState != State::SUCCESS && armedUntilMs &&
      (int32_t)(now - armedUntilMs) >= 0) {
    disable();
  }
  if (serverRunning && currentState != State::UPLOADING &&
      currentState != State::SUCCESS && WiFi.status() != WL_CONNECTED) {
    resetUploadSession();
    releaseMaintenanceHold();
    stopServer();
    armedUntilMs = 0;
    currentState = State::ERROR;
    setMessage("OTA disabled because Wi-Fi disconnected");
  }
  if (restartAtMs && (int32_t)(now - restartAtMs) >= 0) {
    Serial.println("Restarting into verified OTA firmware");
    Serial.flush();
    delay(50);
    ESP.restart();
  }
}

void copyStatus(Status& status) {
  status = Status{};
  status.state = currentState;
  status.available = updatePartition != nullptr;
  status.serverRunning = serverRunning;
  status.maintenanceActive = adsb::maintenanceHoldActive() &&
                             mqtt_service::maintenanceHoldActive();
  status.firmwareBytes = packageHeader.firmwareSize;
  status.writtenBytes = payloadWritten;
  status.progressPercent = packageHeader.firmwareSize
      ? static_cast<uint8_t>(std::min<uint32_t>(
            100U, (payloadWritten * 100U) / packageHeader.firmwareSize))
      : 0;
  if (armedUntilMs && currentState != State::INACTIVE &&
      currentState != State::UNAVAILABLE) {
    const int32_t remaining = static_cast<int32_t>(armedUntilMs - millis());
    status.secondsRemaining = remaining > 0
        ? static_cast<uint32_t>(remaining) / 1000U
        : 0;
  }
  snprintf(status.accessCode, sizeof(status.accessCode), "%s", accessCode);
  if (serverRunning && WiFi.status() == WL_CONNECTED) {
    snprintf(status.ipAddress, sizeof(status.ipAddress), "http://%s/update",
             WiFi.localIP().toString().c_str());
    if (mdnsRunning) {
      snprintf(status.mdnsAddress, sizeof(status.mdnsAddress),
               "http://%s.local/update", OTA_HOSTNAME);
    }
  }
  snprintf(status.message, sizeof(status.message), "%s", statusMessage);
}

}  // namespace ota_update
