#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cmath>
#include <esp_system.h>
#include <time.h>
#include <Wire.h>
#include <WiFi.h>
#include <cstring>

#include "button_manager.h"
#include "battery_manager.h"
#include "config.h"
#include "display_manager.h"
#include "mqtt_manager.h"
#include "pins.h"
#include "payload_builder.h"
#include "rtc_manager.h"
#include "sensor_manager.h"
#include "spool_manager.h"
#include "storage_manager.h"
#include "sync_manager.h"
#include "topics.h"
#include "types.h"
#include "wifi_manager_ext.h"
#include "zone_profiles.h"

#ifndef WM_BATTERY_ENABLED
#define WM_BATTERY_ENABLED 0
#endif

namespace {
RuntimeConfig gConfig;
StorageManager gStorage;
SensorManager gSensor;
BatteryManager gBattery;
RTCManager gRtc;
DisplayManager gDisplay;
ButtonManager gButton(Pins::BUTTON);
WiFiManagerExt gWifi;
MqttManager gMqtt;
SpoolManager gSpool;
SyncManager gSync;

enum class PortableSyncState {
  Idle,
  Ready,
  Syncing
};

SensorReading gLastReading;
SensorReading gLastValidReading;
bool gHasLastValidReading = false;
DeviceStatus gStatus;

AlarmState gTempHigh{"TH"};
AlarmState gTempLow{"TL"};
AlarmState gHumHigh{"HH"};
AlarmState gHumLow{"HL"};

bool gBootPublishPending = true;
bool gCalMetadataPublishPending = true;
bool gStatusPublishPending = true;
bool gForcePortalRequested = false;
bool gSensorFailActive = false;
bool gRtcFailActive = false;
bool gWifiFailActive = false;
bool gMqttFailActive = false;
uint32_t gSensorFailStartEpoch = 0;
uint32_t gRtcFailStartEpoch = 0;
uint32_t gWifiFailStartEpoch = 0;
uint32_t gMqttFailStartEpoch = 0;
uint32_t gLastSampleMs = 0;
uint32_t gLastTelemetryMs = 0;
uint32_t gLastStatusMs = 0;
uint32_t gLastCommissioningLogMs = 0;
uint32_t gLastTelemetrySpoolMs = 0;
uint32_t gLastPortableSyncMs = 0;
uint32_t gBootCount = 0;
uint32_t gLastKnownEpoch = 0;
uint32_t gLastKnownEpochMs = 0;
uint32_t gLastGoodSampleEpoch = 0;
uint32_t gLastWifiOkEpoch = 0;
uint32_t gLastMqttOkEpoch = 0;
uint32_t gSensorFailCount = 0;
uint32_t gSensorFailStreak = 0;
uint32_t gWifiReconnectCount = 0;
uint32_t gMqttReconnectCount = 0;
const char* gResetReason = "UNKNOWN";
bool gHasSeenWifiOk = false;
bool gHasSeenMqttOk = false;
bool gPrevWifiOk = false;
bool gPrevMqttOk = false;
bool gPrevDisplayWifiOk = false;
bool gPrevDisplayMqttOk = false;
bool gPrevDisplayRtcOk = false;
bool gPrevDisplaySensorOk = false;
BackendState gPrevDisplayBackendState = BackendState::Unknown;
int32_t gPrevDisplayRssi = -127;
uint32_t gLastNtpSyncMs = 0;
uint32_t gLastNtpAttemptMs = 0;
bool gHasSyncedNtp = false;
bool gNtpSyncInProgress = false;
uint32_t gNtpSyncStartedMs = 0;
uint32_t gNtpSyncTimeoutMs = 0;
const char* gNtpSyncReason = "";
bool gExternalAlarmActive = false;
bool gAlarmOutputsActive = false;
bool gAlarmBuzzerMuted = false;
bool gAlarmBuzzerActive = false;
bool gBuzzerPatternActive = false;
bool gBuzzerOutputHigh = false;
uint8_t gBuzzerPendingBeeps = 0;
uint16_t gBuzzerOnMs = 80;
uint16_t gBuzzerOffMs = 70;
uint32_t gBuzzerNextToggleMs = 0;
uint8_t gShortPressCount = 0;
uint32_t gShortPressWindowStartMs = 0;
uint32_t gLastShortPressMs = 0;
bool gShortPressActionPending = false;
bool gTechnicalScreenVisible = false;
uint32_t gLastBackendHealthCheckMs = 0;
uint32_t gLastBackendAckCheckMs = 0;
uint32_t gLastRuntimeDiagLogMs = 0;
uint32_t gLoopMaxDurationMs = 0;
uint8_t gBackendHealthFailCount = 0;
uint8_t gBackendAckFailCount = 0;
long gBackendLastAckSequence = -1;
uint32_t gBackendLastAckEpoch = 0;
uint32_t gBackendAckStaleSeconds = 0;
bool gBackendPathBlockedLatched = false;
uint8_t gBackendHealthyStreak = 0;
uint32_t gBackendBlockedSinceMs = 0;
PortableSyncState gPortableSyncState = PortableSyncState::Idle;
size_t gPortablePendingCount = 0;
size_t gPortableSyncSent = 0;
size_t gPortableSyncTotal = 0;
CalibrationState gCalState;
constexpr uint8_t kBackendUnblockMinHealthySamples = 2;
constexpr uint32_t kBackendUnblockDebounceMs = 30000;

bool isBackendPathBlocked();

const char* resetReasonLabel(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_SW:
      return "SW";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
}

uint32_t uptimeSeconds() { return millis() / 1000UL; }

void externalAlarmOn() {
  digitalWrite(Pins::RELAY_ALARM, Pins::RELAY_ACTIVE_LOW ? LOW : HIGH);
}

void externalAlarmOff() {
  digitalWrite(Pins::RELAY_ALARM, Pins::RELAY_ACTIVE_LOW ? HIGH : LOW);
}

void setExternalAlarm(bool active) {
  if (gExternalAlarmActive == active) {
    return;
  }

  gExternalAlarmActive = active;
  if (active) {
    externalAlarmOn();
  } else {
    externalAlarmOff();
  }
}

void buzzerOn() {
  digitalWrite(Pins::BUZZER, HIGH);
  gBuzzerOutputHigh = true;
}

void buzzerOff() {
  digitalWrite(Pins::BUZZER, LOW);
  gBuzzerOutputHigh = false;
}

void startBeepPattern(uint8_t beeps, uint16_t onMs, uint16_t offMs) {
  if (beeps == 0 || gAlarmBuzzerActive) {
    return;
  }

  gBuzzerPatternActive = true;
  gBuzzerPendingBeeps = beeps;
  gBuzzerOnMs = onMs;
  gBuzzerOffMs = offMs;
  buzzerOn();
  gBuzzerNextToggleMs = millis() + gBuzzerOnMs;
}

void beepOnce(uint16_t durationMs) { startBeepPattern(1, durationMs, 0); }

void setAlarmBuzzer(bool active) {
  if (gAlarmBuzzerActive == active) {
    return;
  }

  gAlarmBuzzerActive = active;
  if (active) {
    gBuzzerPatternActive = false;
    gBuzzerPendingBeeps = 0;
    buzzerOn();
  } else {
    buzzerOff();
  }
}

void syncAlarmOutputs(bool alarmActive) {
  bool shouldSoundBuzzer = alarmActive && gConfig.buzzerEnabled && !gAlarmBuzzerMuted;
  if (gAlarmOutputsActive == alarmActive && gAlarmBuzzerActive == shouldSoundBuzzer) {
    return;
  }

  gAlarmOutputsActive = alarmActive;
  setExternalAlarm(alarmActive);
  setAlarmBuzzer(shouldSoundBuzzer);
}

void updateBuzzer(uint32_t nowMs) {
  if (gAlarmBuzzerActive) {
    if (!gBuzzerOutputHigh) {
      buzzerOn();
    }
    return;
  }

  if (!gBuzzerPatternActive) {
    return;
  }

  if (static_cast<int32_t>(nowMs - gBuzzerNextToggleMs) < 0) {
    return;
  }

  if (gBuzzerOutputHigh) {
    buzzerOff();
    if (gBuzzerPendingBeeps > 0) {
      gBuzzerPendingBeeps--;
    }

    if (gBuzzerPendingBeeps == 0) {
      gBuzzerPatternActive = false;
      return;
    }

    gBuzzerNextToggleMs = nowMs + gBuzzerOffMs;
    return;
  }

  buzzerOn();
  gBuzzerNextToggleMs = nowMs + gBuzzerOnMs;
}

void updateLastKnownEpoch(uint32_t epoch) {
  gLastKnownEpoch = epoch;
  gLastKnownEpochMs = millis();
}

void applyCalibration(SensorReading& reading) {
  if (!reading.valid) {
    return;
  }

  int16_t rawT = reading.rawTemperatureX10;
  int16_t rawH = reading.rawHumidityX10;

  int16_t corrT;
  int16_t corrH;

  if (gConfig.calVer >= 2) {
    // v2 formula: round(raw * slope/1000 + offset)
    float ft = rawT * gConfig.calTempSlopeX1000 / 1000.0f + gConfig.calTempOffsetX10;
    float fh = rawH * gConfig.calHumSlopeX1000 / 1000.0f + gConfig.calHumOffsetX10;
    corrT = static_cast<int16_t>(round(ft));
    corrH = static_cast<int16_t>(round(fh));
  } else {
    // v1 / offset-only: use v2 offset slots for simulator parity.
    // Legacy "to"/"ho" commands are already mapped into cal*Offset in applySetCommand/loadConfig.
    corrT = rawT + gConfig.calTempOffsetX10;
    corrH = rawH + gConfig.calHumOffsetX10;
  }

  // Clamp humidity to valid range
  if (corrH < 0) {
    corrH = 0;
  } else if (corrH > 1000) {
    corrH = 1000;
  }

  reading.temperatureX10 = corrT;
  reading.humidityX10 = corrH;
  reading.temperatureC = reading.temperatureX10 / 10.0f;
  reading.humidityPct = reading.humidityX10 / 10.0f;
}

bool isI2CDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanI2CBus() {
  Serial.printf("[I2C] Scan start on SDA=%u SCL=%u clock=%luHz\n", Pins::I2C_SDA, Pins::I2C_SCL,
                static_cast<unsigned long>(Config::I2C_CLOCK_HZ));

  uint8_t foundCount = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("[I2C] Found device at 0x%02X\n", address);
      foundCount++;
    } else if (error == 4) {
      Serial.printf("[I2C] Unknown error at 0x%02X\n", address);
    }
  }

  Serial.printf("[I2C] Scan done, devices=%u\n", foundCount);
  Serial.printf("[I2C] OLED 0x3C: %s\n", isI2CDevicePresent(0x3C) ? "present" : "missing");
  Serial.printf("[I2C] RTC  0x68: %s\n", isI2CDevicePresent(0x68) ? "present" : "missing");
  Serial.printf("[I2C] SHT20 0x40: %s\n", isI2CDevicePresent(0x40) ? "present" : "missing");
}

void normalizeRuntimeConfig() { gStorage.normalizeConfig(gConfig); }

bool isSameConfig(const RuntimeConfig& lhs, const RuntimeConfig& rhs) {
  return lhs.siteId == rhs.siteId && lhs.deviceId == rhs.deviceId && lhs.mqttHost == rhs.mqttHost &&
         lhs.mqttPort == rhs.mqttPort && lhs.mqttUsername == rhs.mqttUsername &&
         lhs.mqttPassword == rhs.mqttPassword && lhs.mqttUseTls == rhs.mqttUseTls &&
         lhs.mqttTlsInsecure == rhs.mqttTlsInsecure && lhs.tempLowX10 == rhs.tempLowX10 &&
         lhs.tempHighX10 == rhs.tempHighX10 && lhs.humLowX10 == rhs.humLowX10 &&
         lhs.humHighX10 == rhs.humHighX10 && lhs.alarmDelayS == rhs.alarmDelayS &&
         lhs.sampleIntervalS == rhs.sampleIntervalS &&
         lhs.reportIntervalS == rhs.reportIntervalS &&
         lhs.statusIntervalS == rhs.statusIntervalS &&
         lhs.portalTimeoutS == rhs.portalTimeoutS &&
         lhs.commissioningMode == rhs.commissioningMode &&
         lhs.commissioningLogIntervalS == rhs.commissioningLogIntervalS &&
         lhs.buzzerEnabled == rhs.buzzerEnabled &&
         lhs.zoneProfile == rhs.zoneProfile && lhs.spoolEnabled == rhs.spoolEnabled &&
         lhs.spoolIntervalS == rhs.spoolIntervalS && lhs.syncBatchSize == rhs.syncBatchSize &&
         lhs.syncIntervalMs == rhs.syncIntervalMs && lhs.tempOffsetX10 == rhs.tempOffsetX10 &&
         lhs.humOffsetX10 == rhs.humOffsetX10 && lhs.calVer == rhs.calVer &&
         lhs.calStatus == rhs.calStatus &&
         lhs.calTempSlopeX1000 == rhs.calTempSlopeX1000 &&
         lhs.calTempOffsetX10 == rhs.calTempOffsetX10 &&
         lhs.calHumSlopeX1000 == rhs.calHumSlopeX1000 &&
         lhs.calHumOffsetX10 == rhs.calHumOffsetX10 &&
         lhs.operationMode == rhs.operationMode &&
         lhs.backendHttpBaseUrl == rhs.backendHttpBaseUrl &&
         lhs.backendIngestBearerToken == rhs.backendIngestBearerToken &&
         lhs.backendDeviceSecret == rhs.backendDeviceSecret &&
         lhs.backendTokenExpiresEpoch == rhs.backendTokenExpiresEpoch &&
         lhs.portableBatchSize == rhs.portableBatchSize;
}

void logConfigSummary(const char* prefix) {
  Serial.printf(
      "%s site=%s device=%s broker=%s:%u tls=%s zone=%s sample=%us report=%us status=%us alarmDelay=%us cm=%d cli=%us buz=%d spool=%d/%us sync=%u@%lums pm=%d pb=%u psec=%d tok=%d\n",
      prefix, gConfig.siteId.c_str(), gConfig.deviceId.c_str(), gConfig.mqttHost.c_str(),
      gConfig.mqttPort, gMqtt.tlsModeLabel(), ZoneProfiles::label(gConfig.zoneProfile),
      gConfig.sampleIntervalS, gConfig.reportIntervalS, gConfig.statusIntervalS,
      gConfig.alarmDelayS, gConfig.commissioningMode ? 1 : 0,
      gConfig.commissioningLogIntervalS, gConfig.buzzerEnabled ? 1 : 0,
      gConfig.spoolEnabled ? 1 : 0, gConfig.spoolIntervalS,
      gConfig.syncBatchSize, static_cast<unsigned long>(gConfig.syncIntervalMs),
      gConfig.operationMode == OperationMode::Portable ? 1 : 0, gConfig.portableBatchSize,
      gConfig.backendDeviceSecret.length() > 0 ? 1 : 0,
      gConfig.backendIngestBearerToken.length() > 0 ? 1 : 0);
  Serial.printf(
      "  cal: ver=%u status=%u ts=%d to=%d hs=%d ho=%d | legacy: to=%d ho=%d\n",
      gConfig.calVer, gConfig.calStatus,
      gConfig.calTempSlopeX1000, gConfig.calTempOffsetX10,
      gConfig.calHumSlopeX1000, gConfig.calHumOffsetX10,
      gConfig.tempOffsetX10, gConfig.humOffsetX10);
}

bool isPortableMode() { return gConfig.operationMode == OperationMode::Portable; }

void setOperationMode(OperationMode mode) {
  if (gConfig.operationMode == mode) {
    return;
  }

  gConfig.operationMode = mode;
  gStorage.saveConfig(gConfig);
  gStatusPublishPending = true;
  if (mode == OperationMode::Portable) {
    gPortableSyncState = PortableSyncState::Idle;
    gPortableSyncSent = 0;
    gPortableSyncTotal = 0;
    startBeepPattern(2, 70, 60);
    Serial.println("[MODE] Switched to PORTABLE");
  } else {
    gPortableSyncState = PortableSyncState::Idle;
    gPortableSyncSent = 0;
    gPortableSyncTotal = 0;
    startBeepPattern(1, 120, 0);
    Serial.println("[MODE] Switched to NORMAL");
  }
}

uint32_t currentEpoch() {
  if (gRtc.isValid()) {
    updateLastKnownEpoch(gRtc.getEpoch());
    return gLastKnownEpoch;
  }
  if (gLastKnownEpoch == 0) {
    updateLastKnownEpoch(Config::MIN_VALID_EPOCH);
  }
  return gLastKnownEpoch + ((millis() - gLastKnownEpochMs) / 1000UL);
}

const char* backendStateLabel(BackendState state) {
  switch (state) {
    case BackendState::Ok:
      return "OK";
    case BackendState::Degraded:
      return "DEG";
    case BackendState::Down:
      return "DOWN";
    case BackendState::Unknown:
    default:
      return "UNK";
  }
}

bool isValidEpoch(time_t epoch) {
  return epoch >= static_cast<time_t>(Config::MIN_VALID_EPOCH);
}

void beginNtpSync(const char* reason, uint32_t timeoutMs) {
  if (!gWifi.isConnected() || gNtpSyncInProgress) {
    return;
  }

  configTime(0, 0, Config::NTP_SERVER_PRIMARY, Config::NTP_SERVER_SECONDARY,
             Config::NTP_SERVER_TERTIARY);
  gLastNtpAttemptMs = millis();
  gNtpSyncStartedMs = gLastNtpAttemptMs;
  gNtpSyncTimeoutMs = timeoutMs;
  gNtpSyncReason = reason;
  gNtpSyncInProgress = true;
  Serial.printf("[NTP] Sync start reason=%s\n", reason);
}

bool pollNtpSync() {
  if (!gNtpSyncInProgress) {
    return false;
  }

  time_t ntpEpoch = time(nullptr);
  if (isValidEpoch(ntpEpoch)) {
    updateLastKnownEpoch(static_cast<uint32_t>(ntpEpoch));
    gLastNtpSyncMs = millis();
    gHasSyncedNtp = true;
    gNtpSyncInProgress = false;

    bool rtcUpdated = gRtc.setEpoch(gLastKnownEpoch);
    Serial.printf("[NTP] Sync success epoch=%lu rtc=%s\n",
                  static_cast<unsigned long>(gLastKnownEpoch),
                  rtcUpdated ? "updated" : "unavailable");
    return true;
  }

  if ((millis() - gNtpSyncStartedMs) >= gNtpSyncTimeoutMs) {
    Serial.printf("[NTP] Sync failed reason=%s timeout=%lums\n", gNtpSyncReason,
                  static_cast<unsigned long>(gNtpSyncTimeoutMs));
    gNtpSyncInProgress = false;
    return false;
  }
  return false;
}

void maintainTimeSync(uint32_t nowMs) {
  if (!gWifi.isConnected()) {
    gNtpSyncInProgress = false;
    return;
  }

  pollNtpSync();

  bool rtcInvalid = !gRtc.isValid();
  bool retryDue = (gLastNtpAttemptMs == 0) ||
                  ((nowMs - gLastNtpAttemptMs) >= Config::NTP_RETRY_INTERVAL_MS);
  bool periodicDue =
      !gHasSyncedNtp || ((nowMs - gLastNtpSyncMs) >= Config::NTP_RESYNC_INTERVAL_MS);

  if (rtcInvalid) {
    if (!gNtpSyncInProgress && retryDue) {
      beginNtpSync("rtc-recovery", Config::NTP_SYNC_TIMEOUT_MS);
    }
    return;
  }

  if (!gNtpSyncInProgress && periodicDue && retryDue) {
    beginNtpSync("periodic", Config::NTP_SYNC_TIMEOUT_MS);
  }
}

void rememberEvent(uint32_t eventTs, const char* code, int32_t value, int32_t limit,
                   uint32_t duration, bool recovery) {
  EventRecord event;
  event.ts = eventTs;
  strncpy(event.code, code, sizeof(event.code) - 1);
  event.code[sizeof(event.code) - 1] = '\0';
  event.value = value;
  event.limit = limit;
  event.duration = duration;
  event.recovery = recovery;
  gStorage.addEvent(event);
}

void publishExceptionEvent(const char* code, int32_t value, int32_t limit, uint32_t duration,
                           bool recovery = false) {
  EventSpoolRecord record;
  record.sequence = gStorage.nextSequence();
  record.timestamp = currentEpoch();
  strncpy(record.code, code, sizeof(record.code) - 1);
  record.code[sizeof(record.code) - 1] = '\0';
  record.value = value;
  record.limit = limit;
  record.duration = duration;
  record.recovery = recovery;
  record.rtcOk = gRtc.isValid();

  String payload = buildEventPayload(record, false);
  bool livePublished = gMqtt.isConnected() ? gMqtt.publishException(payload) : false;
  if (gConfig.spoolEnabled && gSpool.isReady()) {
    gSpool.appendEvent(record, livePublished);
  }
  rememberEvent(record.timestamp, record.code, record.value, record.limit, record.duration,
                record.recovery);
  Serial.printf("[ALARM] code=%s value=%ld limit=%ld dur=%lus recovery=%d\n", code,
                static_cast<long>(value), static_cast<long>(limit),
                static_cast<unsigned long>(duration), recovery ? 1 : 0);
}

String buildStatusPayload() {
  JsonDocument doc;
  doc["r"] = gStatus.rssi;
  doc["m"] = gStatus.mqttOk ? 1 : 0;
  doc["sn"] = gStatus.sensorOk ? 1 : 0;
  doc["rtc"] = gStatus.rtcOk ? 1 : 0;
  doc["o"] = gStatus.oledOk ? 1 : 0;
  doc["fw"] = Config::FW_VERSION;
  doc["rr"] = gResetReason;
  doc["cm"] = gConfig.commissioningMode ? 1 : 0;
  doc["zp"] = ZoneProfiles::label(gConfig.zoneProfile);
  doc["sq"] = gStorage.currentSequence();
  doc["sp"] = gConfig.spoolEnabled ? 1 : 0;
  doc["lu"] = gLastGoodSampleEpoch;
  doc["up"] = uptimeSeconds();
  doc["heap"] = ESP.getFreeHeap();
  doc["sc"] = gSensorFailCount;
  doc["wc"] = gWifiReconnectCount;
  doc["mc"] = gMqttReconnectCount;
  doc["om"] = isPortableMode() ? 1 : 0;
  doc["pp"] = static_cast<uint32_t>(gPortablePendingCount);
  doc["xs"] = gSpool.isExternalSpoolActive() ? 1 : 0;
  doc["xf"] = gSpool.hadExternalSpoolFallback() ? 1 : 0;
  const char* fallbackReason = gSpool.externalSpoolFallbackReason();
  if (fallbackReason != nullptr && fallbackReason[0] != '\0') {
    doc["xr"] = fallbackReason;
  }
  doc["be"] = static_cast<uint8_t>(gStatus.backendState);
  doc["ba"] = gStatus.backendApiOk ? 1 : 0;
  doc["bi"] = gStatus.backendIngestOk ? 1 : 0;
  doc["bl"] = gBackendAckStaleSeconds;
  doc["bok"] = gLastReading.batteryValid ? 1 : 0;
  if (gLastReading.batteryValid) {
    doc["bv"] = gLastReading.batteryMilliVolt;
    doc["bp"] = gLastReading.batteryPercent;
  }

  String payload;
  payload.reserve(Config::MQTT_PAYLOAD_LENGTH);
  serializeJson(doc, payload);
  return payload;
}

void publishStatus() {
  Serial.println("[MQTT] Publishing status");
  gMqtt.publishStatus(buildStatusPayload());
}

String buildCalibrationMetadataPayload() {
  JsonDocument doc;
  doc["ver"]  = gCalState.calVer;
  doc["st"]   = gCalState.calStatus;
  doc["ts"]   = gCalState.calTempSlopeX1000;
  doc["to"]   = gCalState.calTempOffsetX10;
  doc["hs"]   = gCalState.calHumSlopeX1000;
  doc["ho"]   = gCalState.calHumOffsetX10;
  doc["rev"]  = gCalState.calRevision;
  doc["upd"]  = gCalState.calUpdatedEpoch;

  String payload;
  payload.reserve(128);
  serializeJson(doc, payload);
  return payload;
}

void publishCalibrationMetadataEvent() {
  if (!gMqtt.isConnected()) {
    return;
  }
  String payload = buildCalibrationMetadataPayload();
  gMqtt.publishCalibrationMetadata(payload);
}

void updateAlarmState(AlarmState& alarm, bool breached, int32_t actual, int32_t limitX10) {
  uint32_t nowMs = millis();
  uint32_t nowEpoch = currentEpoch();

  if (breached) {
    if (!alarm.breachActive) {
      alarm.breachActive = true;
      alarm.confirmed = false;
      alarm.breachStartedMs = nowMs;
      alarm.breachStartedEpoch = nowEpoch;
    }

    if (!alarm.confirmed &&
        (nowMs - alarm.breachStartedMs) >= (static_cast<uint32_t>(gConfig.alarmDelayS) * 1000UL)) {
      alarm.confirmed = true;
      publishExceptionEvent(alarm.code, actual, limitX10, nowEpoch - alarm.breachStartedEpoch);
    }
    return;
  }

  if (alarm.breachActive) {
    uint32_t duration = nowEpoch - alarm.breachStartedEpoch;
    if (alarm.confirmed) {
      publishExceptionEvent(alarm.code, actual, limitX10, duration, true);
    }
    alarm.breachActive = false;
    alarm.confirmed = false;
    alarm.breachStartedMs = 0;
    alarm.breachStartedEpoch = 0;
  }
}

void updateFailureState(bool currentOk, bool& activeFlag, uint32_t& startedEpoch, const char* code) {
  uint32_t nowEpoch = currentEpoch();
  if (!currentOk && !activeFlag) {
    activeFlag = true;
    startedEpoch = nowEpoch;
    publishExceptionEvent(code, 0, 0, 0, false);
    return;
  }

  if (currentOk && activeFlag) {
    activeFlag = false;
    publishExceptionEvent(code, 1, 0, nowEpoch - startedEpoch, true);
    startedEpoch = 0;
  }
}

void updatePortableSyncState() {
  if (!isPortableMode()) {
    gPortableSyncState = PortableSyncState::Idle;
    gPortablePendingCount = 0;
    return;
  }

  gPortablePendingCount = gSpool.pendingPortableTelemetryCount();
  if (gPortableSyncState == PortableSyncState::Syncing) {
    return;
  }

  if (gPortablePendingCount > 0 && gWifi.isConnected()) {
    gPortableSyncState = PortableSyncState::Ready;
  } else {
    gPortableSyncState = PortableSyncState::Idle;
  }
}

const char* modeLineLabel() {
  if (!isPortableMode()) {
    bool hasBackendEndpoint = gConfig.backendHttpBaseUrl.length() > 0;
    bool backendDown = hasBackendEndpoint && gStatus.backendState == BackendState::Down;
    if (gStatus.wifiOk && gStatus.mqttOk && !backendDown) {
      return "NORMAL / ONLINE";
    }
    return "NORMAL / OFFLINE";
  }

  switch (gPortableSyncState) {
    case PortableSyncState::Ready:
      return "READY SYNC";
    case PortableSyncState::Syncing:
      return "SYNCING";
    case PortableSyncState::Idle:
    default:
      return "PORTABLE";
  }
}

void buildDisplayDetail(char* output, size_t outputLen) {
  if (!isPortableMode()) {
    snprintf(output, outputLen, "MQ:%s BE:%s", gStatus.mqttLabel, backendStateLabel(gStatus.backendState));
    return;
  }

  if (gPortableSyncState == PortableSyncState::Syncing) {
    snprintf(output, outputLen, "%u/%u", static_cast<unsigned>(gPortableSyncSent),
             static_cast<unsigned>(gPortableSyncTotal));
    return;
  }

  snprintf(output, outputLen, "PEND:%u", static_cast<unsigned>(gPortablePendingCount));
}

void refreshDisplay() {
  updatePortableSyncState();
  char detail[24];
  buildDisplayDetail(detail, sizeof(detail));
  gDisplay.showStatus(gConfig, gLastReading, gStatus, modeLineLabel(), detail, gTechnicalScreenVisible);
}

void showBootStep(const char* step, const char* detail) {
  gDisplay.showBootScreen("LJK MONITOR", step, detail);
}

bool applySetCommand(const String& payload) {
  RuntimeConfig beforeConfig = gConfig;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[CMD] Invalid set payload: %s\n", payload.c_str());
    return false;
  }

  if (doc["ri"].is<int>()) {
    int value = doc["ri"].as<int>();
    if (value >= 10 && value <= 3600) {
      gConfig.reportIntervalS = static_cast<uint16_t>(value);
    }
  }
  if (doc["ad"].is<int>()) {
    int value = doc["ad"].as<int>();
    if (value >= 5 && value <= 3600) {
      gConfig.alarmDelayS = static_cast<uint16_t>(value);
    }
  }
  if (doc["th"].is<int>()) {
    gConfig.tempHighX10 = static_cast<int16_t>(doc["th"].as<int>());
  }
  if (doc["tl"].is<int>()) {
    gConfig.tempLowX10 = static_cast<int16_t>(doc["tl"].as<int>());
  }
  if (doc["hh"].is<int>()) {
    gConfig.humHighX10 = static_cast<int16_t>(doc["hh"].as<int>());
  }
  if (doc["hl"].is<int>()) {
    gConfig.humLowX10 = static_cast<int16_t>(doc["hl"].as<int>());
  }
  if (doc["cm"].is<int>()) {
    gConfig.commissioningMode = doc["cm"].as<int>() != 0;
  }
  if (doc["cli"].is<int>()) {
    int value = doc["cli"].as<int>();
    if (value >= Config::MIN_COMMISSIONING_LOG_INTERVAL_S && value <= 3600) {
      gConfig.commissioningLogIntervalS = static_cast<uint16_t>(value);
    }
  }
  if (doc["bz"].is<int>()) {
    gConfig.buzzerEnabled = doc["bz"].as<int>() != 0;
  }
  if (doc["se"].is<int>()) {
    gConfig.spoolEnabled = doc["se"].as<int>() != 0;
  }
  if (doc["si"].is<int>()) {
    int value = doc["si"].as<int>();
    if (value >= Config::MIN_SPOOL_INTERVAL_S && value <= 3600) {
      gConfig.spoolIntervalS = static_cast<uint16_t>(value);
    }
  }
  if (doc["sb"].is<int>()) {
    int value = doc["sb"].as<int>();
    if (value >= Config::MIN_SYNC_BATCH_SIZE && value <= Config::MAX_SYNC_BATCH_SIZE) {
      gConfig.syncBatchSize = static_cast<uint8_t>(value);
    }
  }
  if (doc["sy"].is<int>()) {
    int value = doc["sy"].as<int>();
    if (value >= static_cast<int>(Config::MIN_SYNC_INTERVAL_MS) && value <= 60000) {
      gConfig.syncIntervalMs = static_cast<uint32_t>(value);
    }
  }
  if (doc["to"].is<int>()) {
    int value = doc["to"].as<int>();
    if (value >= Config::MIN_TEMP_OFFSET_X10 && value <= Config::MAX_TEMP_OFFSET_X10) {
      gConfig.tempOffsetX10 = static_cast<int16_t>(value);
    }
  }
  if (doc["ho"].is<int>()) {
    int value = doc["ho"].as<int>();
    if (value >= Config::MIN_HUM_OFFSET_X10 && value <= Config::MAX_HUM_OFFSET_X10) {
      gConfig.humOffsetX10 = static_cast<int16_t>(value);
    }
  }

  // ── Calibration v2 fields ───────────────────────────────────────────────────
  bool calChanged = false;
  if (doc["cal_ver"].is<int>()) {
    int value = doc["cal_ver"].as<int>();
    if (value >= Config::MIN_CAL_VER && value <= Config::MAX_CAL_VER) {
      gConfig.calVer = static_cast<uint8_t>(value);
      calChanged = true;
    }
  }
  if (doc["cal_status"].is<int>()) {
    int value = doc["cal_status"].as<int>();
    if (value >= Config::MIN_CAL_STATUS && value <= Config::MAX_CAL_STATUS) {
      gConfig.calStatus = static_cast<uint8_t>(value);
      calChanged = true;
    }
  }
  if (doc["cal_ts"].is<int>()) {
    int value = doc["cal_ts"].as<int>();
    if (value >= Config::MIN_CAL_TEMP_SLOPE && value <= Config::MAX_CAL_TEMP_SLOPE) {
      gConfig.calTempSlopeX1000 = static_cast<int16_t>(value);
      calChanged = true;
    }
  }
  if (doc["cal_to"].is<int>()) {
    int value = doc["cal_to"].as<int>();
    if (value >= Config::MIN_CAL_TEMP_OFFSET && value <= Config::MAX_CAL_TEMP_OFFSET) {
      gConfig.calTempOffsetX10 = static_cast<int16_t>(value);
      calChanged = true;
    }
  }
  if (doc["cal_hs"].is<int>()) {
    int value = doc["cal_hs"].as<int>();
    if (value >= Config::MIN_CAL_HUM_SLOPE && value <= Config::MAX_CAL_HUM_SLOPE) {
      gConfig.calHumSlopeX1000 = static_cast<int16_t>(value);
      calChanged = true;
    }
  }
  if (doc["cal_ho"].is<int>()) {
    int value = doc["cal_ho"].as<int>();
    if (value >= Config::MIN_CAL_HUM_OFFSET && value <= Config::MAX_CAL_HUM_OFFSET) {
      gConfig.calHumOffsetX10 = static_cast<int16_t>(value);
      calChanged = true;
    }
  }

  // Legacy mapping: 'to'/'ho' map to calTempOffsetX10/calHumOffsetX10
  // when new cal_to/cal_ho fields are not sent
  if (!doc["cal_to"].is<int>() && doc["to"].is<int>()) {
    gConfig.calTempOffsetX10 = gConfig.tempOffsetX10;
    calChanged = true;
  }
  if (!doc["cal_ho"].is<int>() && doc["ho"].is<int>()) {
    gConfig.calHumOffsetX10 = gConfig.humOffsetX10;
    calChanged = true;
  }

  if (doc["pm"].is<int>()) {
    gConfig.operationMode =
        doc["pm"].as<int>() == 1 ? OperationMode::Portable : OperationMode::Normal;
  }
  if (doc["pb"].is<int>()) {
    int value = doc["pb"].as<int>();
    if (value >= Config::MIN_PORTABLE_BATCH_SIZE &&
        value <= static_cast<int>(Config::MAX_PORTABLE_BATCH_SIZE)) {
      gConfig.portableBatchSize = static_cast<uint8_t>(value);
    }
  }
  if (doc["pe"].is<const char*>()) {
    gConfig.backendHttpBaseUrl = String(doc["pe"].as<const char*>());
  }
  if (doc["pt"].is<const char*>()) {
    gConfig.backendIngestBearerToken = String(doc["pt"].as<const char*>());
    gConfig.backendTokenExpiresEpoch = 0;
  }
  if (doc["ps"].is<const char*>()) {
    gConfig.backendDeviceSecret = String(doc["ps"].as<const char*>());
  }
  if (doc["zp"].is<const char*>()) {
    gConfig.zoneProfile = ZoneProfiles::parse(String(doc["zp"].as<const char*>()));
    bool applyProfile = true;
    if (doc["ap"].is<int>()) {
      applyProfile = doc["ap"].as<int>() != 0;
    }
    if (applyProfile) {
      ZoneProfiles::applyDefaults(gConfig.zoneProfile, gConfig);
    }
  }

  normalizeRuntimeConfig();
  if (gHasLastValidReading) {
    applyCalibration(gLastValidReading);
    gLastReading = gLastValidReading;
  }
  bool configChanged = !isSameConfig(beforeConfig, gConfig);
  if (configChanged) {
    gStorage.saveConfig(gConfig);
  }
  gStatusPublishPending = true;

  // Publish calibration metadata event if cal state changed
  if (calChanged) {
    gCalState.calVer = gConfig.calVer;
    gCalState.calStatus = gConfig.calStatus;
    gCalState.calTempSlopeX1000 = gConfig.calTempSlopeX1000;
    gCalState.calTempOffsetX10 = gConfig.calTempOffsetX10;
    gCalState.calHumSlopeX1000 = gConfig.calHumSlopeX1000;
    gCalState.calHumOffsetX10 = gConfig.calHumOffsetX10;
    gCalState.calRevision++;
    gCalState.calUpdatedEpoch = currentEpoch();
    Serial.printf("[CAL] Metadata updated rev=%lu ts=%lu\n",
                  static_cast<unsigned long>(gCalState.calRevision),
                  static_cast<unsigned long>(gCalState.calUpdatedEpoch));
    gMqtt.publishCalibrationMetadata(buildCalibrationMetadataPayload());
  }

  Serial.printf("[CMD] Set command applied changed=%s\n", configChanged ? "yes" : "no");
  logConfigSummary("[CMD]");
  return true;
}

CommandAction parseDoCommand(const String& payload) {
  if (payload == "r") {
    return CommandAction::Reboot;
  }
  if (payload == "d") {
    return CommandAction::DisplayOn;
  }
  if (payload == "s") {
    return CommandAction::PublishStatusNow;
  }
  if (payload == "f") {
    return CommandAction::ForceConfigPortal;
  }
  if (payload == "m") {
    return CommandAction::MuteAlarmBuzzer;
  }
  if (payload == "u") {
    return CommandAction::UnmuteAlarmBuzzer;
  }
  return CommandAction::None;
}

void onSetCommand(const String& payload) {
  Serial.printf("[CMD] Set command: %s\n", payload.c_str());
  applySetCommand(payload);
}

void onDoCommand(const String& payload) {
  Serial.printf("[CMD] Do command: %s\n", payload.c_str());
  switch (parseDoCommand(payload)) {
    case CommandAction::Reboot:
      ESP.restart();
      break;
    case CommandAction::DisplayOn:
      gDisplay.wakeFor(Config::DISPLAY_AUTO_OFF_MS);
      refreshDisplay();
      break;
    case CommandAction::PublishStatusNow:
      gStatusPublishPending = true;
      break;
    case CommandAction::ForceConfigPortal:
      gForcePortalRequested = true;
      break;
    case CommandAction::MuteAlarmBuzzer:
      gAlarmBuzzerMuted = true;
      syncAlarmOutputs(gStatus.anyAlarmActive);
      Serial.println("[ALARM] Buzzer muted");
      break;
    case CommandAction::UnmuteAlarmBuzzer:
      gAlarmBuzzerMuted = false;
      syncAlarmOutputs(gStatus.anyAlarmActive);
      Serial.println("[ALARM] Buzzer unmuted");
      break;
    case CommandAction::None:
    default:
      break;
  }
}

void enterConfigPortal() {
  Serial.println("[WIFI] Entering captive portal");
  publishExceptionEvent("CFG", 1, 0, 0, false);
  showBootStep("CONFIG PORTAL", "Starting...");
  RuntimeConfig beforeConfig = gConfig;
  bool connected = gWifi.startConfigPortal(gConfig);
  normalizeRuntimeConfig();
  if (!isSameConfig(beforeConfig, gConfig)) {
    gStorage.saveConfig(gConfig);
  }
  gMqtt.setConfig(gConfig);
  WiFi.setHostname(gWifi.getHostname(gConfig).c_str());
  if (connected) {
    gStatusPublishPending = true;
  }
}

void handleSampling(uint32_t nowMs) {
  if ((nowMs - gLastSampleMs) < (static_cast<uint32_t>(gConfig.sampleIntervalS) * 1000UL)) {
    return;
  }
  gLastSampleMs = nowMs;

  SensorReading reading;
  bool ok = gSensor.read(reading);
#if WM_BATTERY_ENABLED
  BatteryReading battery;
  bool batteryOk = gBattery.read(battery);
  if (batteryOk && battery.valid) {
    reading.batteryValid = true;
    reading.batteryMilliVolt = battery.millivolt;
    reading.batteryPercent = battery.percent;
  }
#endif
  gStatus.sensorOk = ok && reading.latestSampleOk;
  if (!ok) {
    gSensorFailCount++;
    gSensorFailStreak++;
    Serial.printf("[SENSOR] Sample fail count=%lu streak=%lu\n",
                  static_cast<unsigned long>(gSensorFailCount),
                  static_cast<unsigned long>(gSensorFailStreak));
    if ((gSensorFailStreak % Config::SENSOR_RECOVERY_I2C_THRESHOLD) == 0) {
      gSensor.forceRecovery(true);
    } else if ((gSensorFailStreak % Config::SENSOR_RECOVERY_REINIT_THRESHOLD) == 0) {
      gSensor.forceRecovery(false);
    }
    if (gHasLastValidReading) {
      gLastReading = gLastValidReading;
      gLastReading.fromLastValid = true;
      gLastReading.latestSampleOk = false;
      gLastReading.sensorPresent = gSensor.isPresent();
      gLastReading.sensorRuntimeError = gSensor.hasRuntimeError();
    } else {
      gLastReading = reading;
    }
    Serial.println("[SENSOR] Sampling cycle invalid, telemetry will be skipped");
  }
  if (ok) {
    gSensorFailStreak = 0;
    applyCalibration(reading);
    gLastReading = reading;
    gLastValidReading = reading;
    gHasLastValidReading = true;
    gLastGoodSampleEpoch = currentEpoch();
    updateAlarmState(gTempHigh, reading.temperatureX10 > static_cast<int32_t>(gConfig.tempHighX10),
                     reading.temperatureX10, gConfig.tempHighX10);
    updateAlarmState(gTempLow, reading.temperatureX10 < static_cast<int32_t>(gConfig.tempLowX10),
                     reading.temperatureX10, gConfig.tempLowX10);
    updateAlarmState(gHumHigh, reading.humidityX10 > static_cast<int32_t>(gConfig.humHighX10),
                     reading.humidityX10, gConfig.humHighX10);
    updateAlarmState(gHumLow, reading.humidityX10 < static_cast<int32_t>(gConfig.humLowX10),
                     reading.humidityX10, gConfig.humLowX10);
  }

  gStatus.anyAlarmActive =
      gTempHigh.confirmed || gTempLow.confirmed || gHumHigh.confirmed || gHumLow.confirmed;
  refreshDisplay();
}

void handleTelemetry(uint32_t nowMs) {
  if (!gLastReading.valid || !gLastReading.latestSampleOk) {
    return;
  }

  bool backendPathBlocked = isBackendPathBlocked();
  bool mqttConnected = gMqtt.isConnected();
  bool canPublishLive = mqttConnected && !backendPathBlocked;
  bool reportDue =
      (nowMs - gLastTelemetryMs) >= (static_cast<uint32_t>(gConfig.reportIntervalS) * 1000UL);
  bool spoolDue =
      (nowMs - gLastTelemetrySpoolMs) >= (static_cast<uint32_t>(gConfig.spoolIntervalS) * 1000UL);

  if (isPortableMode()) {
    if (!(gConfig.spoolEnabled && spoolDue && gSpool.isReady())) {
      return;
    }

    TelemetrySpoolRecord record;
    record.sequence = gStorage.nextSequence();
    record.timestamp = currentEpoch();
    record.temperatureX10 = gLastReading.temperatureX10;
    record.humidityX10 = gLastReading.humidityX10;
    record.sampleOk = true;
    record.rtcOk = gRtc.isValid();
    record.hasRawValues = gLastReading.valid;
    record.rawTemperatureX10 = gLastReading.rawTemperatureX10;
    record.rawHumidityX10 = gLastReading.rawHumidityX10;
    record.batteryValid = gLastReading.batteryValid;
    record.batteryMilliVolt = gLastReading.batteryMilliVolt;
    record.batteryPercent = gLastReading.batteryPercent;
    if (gSpool.appendPortableTelemetry(record)) {
      gLastTelemetrySpoolMs = nowMs;
      Serial.println("[SPOOL] Portable telemetry stored");
    }
    return;
  }

  if (canPublishLive && !reportDue) {
    return;
  }

  if (!canPublishLive && !(gConfig.spoolEnabled && spoolDue)) {
    return;
  }

  TelemetrySpoolRecord record;
  record.sequence = gStorage.nextSequence();
  record.timestamp = currentEpoch();
  record.temperatureX10 = gLastReading.temperatureX10;
  record.humidityX10 = gLastReading.humidityX10;
  record.sampleOk = true;
  record.rtcOk = gRtc.isValid();
  // v2: include raw values for calibration traceability
  record.hasRawValues = gLastReading.valid;
  record.rawTemperatureX10 = gLastReading.rawTemperatureX10;
  record.rawHumidityX10 = gLastReading.rawHumidityX10;
  record.batteryValid = gLastReading.batteryValid;
  record.batteryMilliVolt = gLastReading.batteryMilliVolt;
  record.batteryPercent = gLastReading.batteryPercent;

  if (canPublishLive) {
    gLastTelemetryMs = nowMs;
    if (gMqtt.publishTelemetry(buildTelemetryPayload(record, false, gConfig.calVer))) {
      Serial.println("[MQTT] Telemetry live sent");
      return;
    }

    if (gConfig.spoolEnabled && gSpool.isReady()) {
      gSpool.appendTelemetry(record, false);
      gLastTelemetrySpoolMs = nowMs;
      Serial.println("[SPOOL] Telemetry stored because publish failed");
    }
    return;
  }

  if (gConfig.spoolEnabled && gSpool.isReady() && spoolDue) {
    gSpool.appendTelemetry(record, false);
    gLastTelemetrySpoolMs = nowMs;
    if (!mqttConnected) {
      Serial.println("[SPOOL] Telemetry stored because mqtt offline");
    } else {
      Serial.println("[SPOOL] Telemetry stored because backend path not healthy");
    }
  }
}

String buildBackendBaseUrl(const String& bulkEndpoint) {
  if (bulkEndpoint.length() == 0) {
    return String();
  }

  int schemePos = bulkEndpoint.indexOf("://");
  if (schemePos < 0) {
    return String();
  }

  int hostStart = schemePos + 3;
  int pathPos = bulkEndpoint.indexOf('/', hostStart);
  if (pathPos < 0) {
    return bulkEndpoint;
  }

  return bulkEndpoint.substring(0, pathPos);
}

String buildBackendDeviceAuthTokenEndpoint(const String& bulkEndpoint) {
  String endpoint = buildBackendBaseUrl(bulkEndpoint);
  if (endpoint.length() == 0) {
    return endpoint;
  }
  endpoint += "/api/devices/auth/token";
  return endpoint;
}

String buildBackendHealthEndpoint(const String& bulkEndpoint) {
  String endpoint = buildBackendBaseUrl(bulkEndpoint);
  if (endpoint.length() == 0) {
    return endpoint;
  }
  endpoint += "/health";
  return endpoint;
}

String buildBackendIngestAckEndpoint(const String& bulkEndpoint) {
  String endpoint = buildBackendBaseUrl(bulkEndpoint);
  if (endpoint.length() == 0) {
    return endpoint;
  }
  endpoint += "/api/device-ingest/";
  endpoint += gConfig.siteId;
  endpoint += "/";
  endpoint += gConfig.deviceId;
  endpoint += "/ack";
  return endpoint;
}

bool parseBackendTokenResponse(const String& body, String& accessToken, uint32_t& expiresEpoch) {
  accessToken = "";
  expiresEpoch = 0;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    return false;
  }

  if (!doc["accessToken"].is<const char*>() || !doc["expiresAtEpoch"].is<uint32_t>()) {
    return false;
  }

  accessToken = String(doc["accessToken"].as<const char*>());
  expiresEpoch = doc["expiresAtEpoch"].as<uint32_t>();
  accessToken.trim();
  return accessToken.length() > 0 && expiresEpoch > 0;
}

bool isBackendTokenUsable(uint32_t nowEpoch) {
  if (gConfig.backendIngestBearerToken.length() == 0 || gConfig.backendTokenExpiresEpoch == 0) {
    return false;
  }
  if (nowEpoch == 0) {
    return true;
  }
  if (gConfig.backendTokenExpiresEpoch <= nowEpoch) {
    return false;
  }
  return (gConfig.backendTokenExpiresEpoch - nowEpoch) > Config::BACKEND_TOKEN_REFRESH_SKEW_S;
}

bool refreshBackendIngestBearerToken() {
  if (gConfig.backendDeviceSecret.length() == 0) {
    return false;
  }

  String authEndpoint = buildBackendDeviceAuthTokenEndpoint(gConfig.backendHttpBaseUrl);
  if (authEndpoint.length() == 0) {
    Serial.println("[BACKEND] Device auth endpoint invalid");
    return false;
  }

  JsonDocument req;
  req["siteId"] = gConfig.siteId;
  req["deviceId"] = gConfig.deviceId;
  req["deviceSecret"] = gConfig.backendDeviceSecret;
  String payload;
  serializeJson(req, payload);

  HTTPClient http;
  http.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  http.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  if (!http.begin(authEndpoint)) {
    Serial.println("[BACKEND] Device auth HTTP begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int statusCode = http.POST(payload);
  String responseBody = http.getString();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    Serial.printf("[BACKEND] Device auth token request failed code=%d\n", statusCode);
    return false;
  }

  String token;
  uint32_t expiresEpoch = 0;
  if (!parseBackendTokenResponse(responseBody, token, expiresEpoch)) {
    Serial.println("[BACKEND] Device auth token response invalid");
    return false;
  }

  bool tokenChanged =
      gConfig.backendIngestBearerToken != token || gConfig.backendTokenExpiresEpoch != expiresEpoch;
  gConfig.backendIngestBearerToken = token;
  gConfig.backendTokenExpiresEpoch = expiresEpoch;
  if (tokenChanged) {
    gStorage.saveConfig(gConfig);
  }
  Serial.printf("[BACKEND] Device token refreshed exp=%lu\n", static_cast<unsigned long>(expiresEpoch));
  return true;
}

bool checkBackendHealthProbe() {
  String endpoint = buildBackendHealthEndpoint(gConfig.backendHttpBaseUrl);
  if (endpoint.length() == 0) {
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  http.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  if (!http.begin(endpoint)) {
    return false;
  }

  int statusCode = http.GET();
  String responseBody = http.getString();
  http.end();
  if (statusCode < 200 || statusCode >= 300) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, responseBody)) {
    return false;
  }
  if (!doc["status"].is<const char*>()) {
    return false;
  }
  return String(doc["status"].as<const char*>()) == "ok";
}

bool checkBackendIngestAckProbe() {
  String endpoint = buildBackendIngestAckEndpoint(gConfig.backendHttpBaseUrl);
  if (endpoint.length() == 0) {
    return false;
  }

  uint32_t nowEpoch = currentEpoch();
  if (!isBackendTokenUsable(nowEpoch) && gConfig.backendDeviceSecret.length() > 0) {
    if (!refreshBackendIngestBearerToken()) {
      return false;
    }
  }
  if (gConfig.backendIngestBearerToken.length() == 0) {
    return false;
  }

  String authHeader = "Bearer ";
  authHeader += gConfig.backendIngestBearerToken;

  HTTPClient http;
  http.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  http.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  if (!http.begin(endpoint)) {
    return false;
  }
  http.addHeader("Authorization", authHeader);
  int statusCode = http.GET();
  String responseBody = http.getString();
  http.end();
  if (statusCode == 401 && gConfig.backendDeviceSecret.length() > 0) {
    if (!refreshBackendIngestBearerToken()) {
      return false;
    }

    authHeader = "Bearer ";
    authHeader += gConfig.backendIngestBearerToken;
    HTTPClient retryHttp;
    retryHttp.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
    retryHttp.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
    if (!retryHttp.begin(endpoint)) {
      return false;
    }
    retryHttp.addHeader("Authorization", authHeader);
    statusCode = retryHttp.GET();
    responseBody = retryHttp.getString();
    retryHttp.end();
  }
  if (statusCode < 200 || statusCode >= 300) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, responseBody)) {
    return false;
  }
  if (!doc["lastSequence"].is<long>()) {
    return false;
  }

  gBackendLastAckSequence = doc["lastSequence"].as<long>();
  gBackendLastAckEpoch = currentEpoch();
  return true;
}

void updateBackendStateMachine(uint32_t nowMs) {
  bool hasBackendEndpoint = gConfig.backendHttpBaseUrl.length() > 0;
  bool canCheckIngestAck =
      gConfig.backendDeviceSecret.length() > 0 || gConfig.backendIngestBearerToken.length() > 0;

  if (!gWifi.isConnected() || !hasBackendEndpoint) {
    gStatus.backendApiOk = false;
    gStatus.backendIngestOk = false;
    gStatus.backendState = BackendState::Unknown;
    return;
  }

  if ((nowMs - gLastBackendHealthCheckMs) >= Config::BACKEND_HEALTH_CHECK_INTERVAL_MS) {
    gLastBackendHealthCheckMs = nowMs;
    bool ok = checkBackendHealthProbe();
    gStatus.backendApiOk = ok;
    if (ok) {
      gBackendHealthFailCount = 0;
    } else if (gBackendHealthFailCount < 255) {
      gBackendHealthFailCount++;
    }
  }

  if (canCheckIngestAck && (nowMs - gLastBackendAckCheckMs) >= Config::BACKEND_INGEST_ACK_INTERVAL_MS) {
    gLastBackendAckCheckMs = nowMs;
    bool ok = checkBackendIngestAckProbe();
    gStatus.backendIngestOk = ok;
    if (ok) {
      gBackendAckFailCount = 0;
      gBackendAckStaleSeconds = 0;
    } else if (gBackendAckFailCount < 255) {
      gBackendAckFailCount++;
    }
  } else if (!canCheckIngestAck) {
    gStatus.backendIngestOk = false;
    gBackendAckFailCount = 0;
    gBackendAckStaleSeconds = 0;
  }

  if (gBackendLastAckEpoch > 0) {
    uint32_t nowEpoch = currentEpoch();
    gBackendAckStaleSeconds = nowEpoch > gBackendLastAckEpoch ? nowEpoch - gBackendLastAckEpoch : 0;
    if (gBackendAckStaleSeconds > Config::BACKEND_INGEST_STALE_SECONDS) {
      gStatus.backendIngestOk = false;
    }
  }

  bool apiDown = gBackendHealthFailCount >= Config::BACKEND_FAIL_THRESHOLD;
  bool ackDown = canCheckIngestAck && gBackendAckFailCount >= Config::BACKEND_FAIL_THRESHOLD;
  if (apiDown || ackDown) {
    gStatus.backendState = BackendState::Down;
  } else if (gStatus.backendApiOk && (!canCheckIngestAck || !gStatus.backendIngestOk)) {
    gStatus.backendState = BackendState::Degraded;
  } else if (gStatus.backendApiOk && gStatus.backendIngestOk) {
    gStatus.backendState = BackendState::Ok;
  } else {
    gStatus.backendState = BackendState::Unknown;
  }
}

void updateBackendPathBlockGate(uint32_t nowMs) {
  bool hasBackendEndpoint = gConfig.backendHttpBaseUrl.length() > 0;
  if (!hasBackendEndpoint) {
    if (gBackendPathBlockedLatched) {
      Serial.println("[BACKEND] Path gate unblocked (endpoint empty)");
    }
    gBackendPathBlockedLatched = false;
    gBackendHealthyStreak = 0;
    return;
  }

  if (gStatus.backendState == BackendState::Down) {
    if (!gBackendPathBlockedLatched) {
      gBackendPathBlockedLatched = true;
      gBackendBlockedSinceMs = nowMs;
      Serial.println("[BACKEND] Path gate blocked (state=Down)");
    }
    gBackendHealthyStreak = 0;
    return;
  }

  if (!gBackendPathBlockedLatched) {
    return;
  }

  if (gStatus.backendState == BackendState::Ok) {
    if (gBackendHealthyStreak < 255) {
      gBackendHealthyStreak++;
    }
  } else {
    gBackendHealthyStreak = 0;
  }

  bool debounceSatisfied = (nowMs - gBackendBlockedSinceMs) >= kBackendUnblockDebounceMs;
  if (debounceSatisfied && gBackendHealthyStreak >= kBackendUnblockMinHealthySamples) {
    gBackendPathBlockedLatched = false;
    gBackendHealthyStreak = 0;
    Serial.println("[BACKEND] Path gate unblocked (stable healthy samples)");
  }
}

bool isBackendPathBlocked() {
  bool hasBackendEndpoint = gConfig.backendHttpBaseUrl.length() > 0;
  return hasBackendEndpoint && gBackendPathBlockedLatched;
}

bool parsePortableBulkResponse(const String& body, int& accepted, int& duplicate, int& failed) {
  accepted = 0;
  duplicate = 0;
  failed = 0;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    return false;
  }

  if (!doc["accepted"].is<int>() || !doc["duplicate"].is<int>() || !doc["failed"].is<int>()) {
    return false;
  }

  accepted = doc["accepted"].as<int>();
  duplicate = doc["duplicate"].as<int>();
  failed = doc["failed"].as<int>();
  return accepted >= 0 && duplicate >= 0 && failed >= 0;
}

String buildPortableBulkPayload(const TelemetrySpoolRecord* records, size_t count) {
  String payload;
  payload.reserve(96 + (count * 30));
  payload = "{\"s\":\"";
  payload += gConfig.siteId;
  payload += "\",\"d\":\"";
  payload += gConfig.deviceId;
  payload += "\",\"m\":\"p\",\"r\":[";

  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      payload += ',';
    }
    const TelemetrySpoolRecord& record = records[i];
    payload += '[';
    payload += String(record.sequence);
    payload += ',';
    payload += String(record.timestamp);
    payload += ',';
    payload += String(record.temperatureX10);
    payload += ',';
    payload += String(record.humidityX10);
    payload += ',';
    payload += record.rtcOk ? '1' : '0';
    payload += ']';
  }
  payload += "]}";
  return payload;
}

void stopPortableSync() {
  gPortableSyncState = PortableSyncState::Ready;
  gPortableSyncSent = 0;
  gPortableSyncTotal = 0;
}

void logRuntimeDiagnostics(uint32_t nowMs) {
  if ((nowMs - gLastRuntimeDiagLogMs) < Config::DIAG_RUNTIME_LOG_INTERVAL_MS) {
    return;
  }
  gLastRuntimeDiagLogMs = nowMs;
  size_t pendingTelemetry = gSpool.pendingTelemetryBytes();
  size_t pendingEvents = gSpool.pendingEventBytes();
  size_t pendingPortable = gSpool.pendingPortableTelemetryCount();
  Serial.printf(
      "[DIAG] loopMax=%lums heapFree=%u heapMin=%u spoolExt=%d spoolT=%u spoolE=%u spoolP=%u\n",
      static_cast<unsigned long>(gLoopMaxDurationMs), static_cast<unsigned>(ESP.getFreeHeap()),
      static_cast<unsigned>(ESP.getMinFreeHeap()), gSpool.isExternalSpoolActive() ? 1 : 0,
      static_cast<unsigned>(pendingTelemetry), static_cast<unsigned>(pendingEvents),
      static_cast<unsigned>(pendingPortable));
  gLoopMaxDurationMs = 0;
}

void handlePortableSync(uint32_t nowMs) {
  if (!isPortableMode() || gPortableSyncState != PortableSyncState::Syncing) {
    return;
  }

  if (!gWifi.isConnected()) {
    stopPortableSync();
    return;
  }
  if (gConfig.backendHttpBaseUrl.length() == 0) {
    stopPortableSync();
    Serial.println("[PORTABLE] HTTP endpoint empty, sync stopped");
    return;
  }
  if (static_cast<int32_t>(nowMs - gLastPortableSyncMs) < 0 ||
      (nowMs - gLastPortableSyncMs) < Config::PORTABLE_SYNC_INTERVAL_MS) {
    return;
  }
  gLastPortableSyncMs = nowMs;

  TelemetrySpoolRecord batch[Config::MAX_PORTABLE_BATCH_SIZE];
  size_t requested = gConfig.portableBatchSize;
  if (requested > Config::MAX_PORTABLE_BATCH_SIZE) {
    requested = Config::MAX_PORTABLE_BATCH_SIZE;
  }
  size_t loaded = gSpool.loadPortableTelemetryBatch(batch, requested);
  if (loaded == 0) {
    gPortableSyncState = PortableSyncState::Idle;
    gPortableSyncSent = 0;
    gPortableSyncTotal = 0;
    return;
  }

  String payload = buildPortableBulkPayload(batch, loaded);
  uint32_t nowEpoch = currentEpoch();
  if (!isBackendTokenUsable(nowEpoch) && gConfig.backendDeviceSecret.length() > 0) {
    if (!refreshBackendIngestBearerToken()) {
      Serial.println("[PORTABLE] Token refresh failed, sync postponed");
      stopPortableSync();
      return;
    }
  }

  HTTPClient http;
  http.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  http.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
  if (!http.begin(gConfig.backendHttpBaseUrl)) {
    Serial.println("[PORTABLE] HTTP begin failed");
    stopPortableSync();
    return;
  }

  http.addHeader("Content-Type", "application/json");
  if (gConfig.backendIngestBearerToken.length() > 0) {
    String authHeader = "Bearer ";
    authHeader += gConfig.backendIngestBearerToken;
    http.addHeader("Authorization", authHeader);
  }

  int statusCode = http.POST(payload);
  if (statusCode == 401 && gConfig.backendDeviceSecret.length() > 0) {
    http.end();
    Serial.println("[PORTABLE] 401 received, refreshing token then retrying once");
    if (!refreshBackendIngestBearerToken()) {
      stopPortableSync();
      return;
    }

    HTTPClient retryHttp;
    retryHttp.setConnectTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
    retryHttp.setTimeout(Config::BACKEND_HTTP_TIMEOUT_MS);
    if (!retryHttp.begin(gConfig.backendHttpBaseUrl)) {
      Serial.println("[PORTABLE] Retry HTTP begin failed");
      stopPortableSync();
      return;
    }
    retryHttp.addHeader("Content-Type", "application/json");
    if (gConfig.backendIngestBearerToken.length() > 0) {
      String authHeader = "Bearer ";
      authHeader += gConfig.backendIngestBearerToken;
      retryHttp.addHeader("Authorization", authHeader);
    }
    statusCode = retryHttp.POST(payload);
    String retryBody = retryHttp.getString();
    retryHttp.end();

    if (statusCode < 200 || statusCode >= 300) {
      Serial.printf("[PORTABLE] Retry failed code=%d\n", statusCode);
      stopPortableSync();
      return;
    }

    int accepted = 0;
    int duplicate = 0;
    int failed = 0;
    if (!parsePortableBulkResponse(retryBody, accepted, duplicate, failed)) {
      Serial.println("[PORTABLE] Invalid bulk response");
      stopPortableSync();
      return;
    }
    if (failed > 0) {
      Serial.printf("[PORTABLE] Backend failed=%d, data kept for retry\n", failed);
      stopPortableSync();
      return;
    }

    size_t confirmed = static_cast<size_t>(accepted + duplicate);
    if (confirmed > loaded) {
      confirmed = loaded;
    }
    if (confirmed > 0) {
      gSpool.markPortableTelemetryBatchSynced(confirmed);
      gPortableSyncSent += confirmed;
    }
    gPortablePendingCount = gSpool.pendingPortableTelemetryCount();
    if (gPortablePendingCount == 0) {
      gPortableSyncState = PortableSyncState::Idle;
      gPortableSyncSent = 0;
      gPortableSyncTotal = 0;
      Serial.println("[PORTABLE] Sync complete");
    }
    return;
  }
  String responseBody = http.getString();
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    Serial.printf("[PORTABLE] HTTP POST failed code=%d\n", statusCode);
    stopPortableSync();
    return;
  }

  int accepted = 0;
  int duplicate = 0;
  int failed = 0;
  if (!parsePortableBulkResponse(responseBody, accepted, duplicate, failed)) {
    Serial.println("[PORTABLE] Invalid bulk response");
    stopPortableSync();
    return;
  }

  if (failed > 0) {
    Serial.printf("[PORTABLE] Backend failed=%d, data kept for retry\n", failed);
    stopPortableSync();
    return;
  }

  size_t confirmed = static_cast<size_t>(accepted + duplicate);
  if (confirmed > loaded) {
    confirmed = loaded;
  }
  if (confirmed > 0) {
    gSpool.markPortableTelemetryBatchSynced(confirmed);
    gPortableSyncSent += confirmed;
  }

  gPortablePendingCount = gSpool.pendingPortableTelemetryCount();
  if (gPortablePendingCount == 0) {
    gPortableSyncState = PortableSyncState::Idle;
    gPortableSyncSent = 0;
    gPortableSyncTotal = 0;
    Serial.println("[PORTABLE] Sync complete");
  }
}

void handleStatus(uint32_t nowMs) {
  bool due = (nowMs - gLastStatusMs) >= (static_cast<uint32_t>(gConfig.statusIntervalS) * 1000UL);
  if (!due && !gStatusPublishPending) {
    return;
  }
  gLastStatusMs = nowMs;
  gStatusPublishPending = false;
  publishStatus();
}

void startPortableSync(uint32_t nowMs) {
  if (!isPortableMode()) {
    return;
  }
  if (!gWifi.isConnected()) {
    Serial.println("[PORTABLE] Start sync skipped, WiFi disconnected");
    return;
  }
  if (!gSpool.hasPendingPortableTelemetry()) {
    Serial.println("[PORTABLE] Start sync skipped, no pending records");
    return;
  }
  if (gConfig.backendHttpBaseUrl.length() == 0) {
    Serial.println("[PORTABLE] Start sync skipped, endpoint empty");
    return;
  }

  gPortableSyncState = PortableSyncState::Syncing;
  gPortableSyncTotal = gSpool.pendingPortableTelemetryCount(10000);
  gPortableSyncSent = 0;
  gLastPortableSyncMs = nowMs;
  Serial.printf("[PORTABLE] Sync started total=%u endpoint=%s\n",
                static_cast<unsigned>(gPortableSyncTotal), gConfig.backendHttpBaseUrl.c_str());
}

void handleCommissioningLog(uint32_t nowMs) {
  if (!gConfig.commissioningMode) {
    return;
  }

  if ((nowMs - gLastCommissioningLogMs) <
      (static_cast<uint32_t>(gConfig.commissioningLogIntervalS) * 1000UL)) {
    return;
  }
  gLastCommissioningLogMs = nowMs;

  if (gLastReading.valid) {
    Serial.printf("[COMM] T=%.1fC H=%.1f%% ok=%d alarm=%d rssi=%ld heap=%lu up=%lu zone=%s\n",
                  gLastReading.temperatureX10 / 10.0f, gLastReading.humidityX10 / 10.0f,
                  gLastReading.latestSampleOk ? 1 : 0, gStatus.anyAlarmActive ? 1 : 0,
                  static_cast<long>(gStatus.rssi), static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(uptimeSeconds()),
                  ZoneProfiles::label(gConfig.zoneProfile));
  } else {
    Serial.printf("[COMM] T=--.- H=--.- ok=0 alarm=%d rssi=%ld heap=%lu up=%lu zone=%s\n",
                  gStatus.anyAlarmActive ? 1 : 0, static_cast<long>(gStatus.rssi),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(uptimeSeconds()),
                  ZoneProfiles::label(gConfig.zoneProfile));
  }
}

void trackConnectionHealth() {
  bool wifiOk = gStatus.wifiOk;
  bool mqttOk = gStatus.mqttOk;
  uint32_t nowEpoch = currentEpoch();

  if (wifiOk) {
    gLastWifiOkEpoch = nowEpoch;
    if (!gPrevWifiOk) {
      beepOnce(80);
    }
    if (gHasSeenWifiOk && !gPrevWifiOk) {
      gWifiReconnectCount++;
      Serial.printf("[WIFI] Reconnected count=%lu\n",
                    static_cast<unsigned long>(gWifiReconnectCount));
    }
    gHasSeenWifiOk = true;
  }
  if (!wifiOk && gPrevWifiOk) {
    startBeepPattern(2, 50, 50);
  }

  if (mqttOk) {
    gLastMqttOkEpoch = nowEpoch;
    if (gHasSeenMqttOk && !gPrevMqttOk) {
      gMqttReconnectCount++;
      Serial.printf("[MQTT] Reconnected count=%lu\n",
                    static_cast<unsigned long>(gMqttReconnectCount));
    }
    gHasSeenMqttOk = true;
  }

  gPrevWifiOk = wifiOk;
  gPrevMqttOk = mqttOk;
}

void updateSubsystemStatus() {
  gStatus.wifiOk = gWifi.isConnected();
  gStatus.mqttOk = gMqtt.isConnected();
  gStatus.rtcOk = gRtc.isValid();
  gStatus.oledOk = gDisplay.isAvailable();
  gStatus.rssi = gWifi.getRSSI();
  gStatus.mqttLabel = gMqtt.connectionLabel();
}

void syncCalStateFromConfig() {
  // Sync runtime CalibrationState from RuntimeConfig after load
  gCalState.calVer = gConfig.calVer;
  gCalState.calStatus = gConfig.calStatus;
  gCalState.calTempSlopeX1000 = gConfig.calTempSlopeX1000;
  gCalState.calTempOffsetX10 = gConfig.calTempOffsetX10;
  gCalState.calHumSlopeX1000 = gConfig.calHumSlopeX1000;
  gCalState.calHumOffsetX10 = gConfig.calHumOffsetX10;
  gCalState.calRevision = 1;   // revision 1 = boot-loaded state
  gCalState.calUpdatedEpoch = currentEpoch();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(Pins::BUZZER, OUTPUT);
  digitalWrite(Pins::BUZZER, LOW);
  pinMode(Pins::RELAY_ALARM, OUTPUT);
  externalAlarmOff();

  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  Wire.setClock(Config::I2C_CLOCK_HZ);
  gButton.begin();

  gStorage.begin();
  gStorage.loadConfig(gConfig);
  normalizeRuntimeConfig();
  syncCalStateFromConfig();
  gBootCount = gStorage.incrementBootCount();
  gSpool.begin();

  bool forcePortalAtBoot = gButton.isHeldAtBoot(Config::BUTTON_BOOT_HOLD_MS);

  Serial.printf("[BOOT] LJK Warehouse Monitor FW %s\n", Config::FW_VERSION);
  gResetReason = resetReasonLabel(esp_reset_reason());
  Serial.printf("[BOOT] Reset reason=%s\n", gResetReason);
  Serial.printf("[BOOT] Boot count=%lu forcePortal=%d\n", static_cast<unsigned long>(gBootCount),
                forcePortalAtBoot ? 1 : 0);

#if defined(WM_POLICY_ALLOW_EXPERIMENTAL)
// Board name string
#if defined(WM_BOARD_ESP32)
#define WM_POLICY_BOARD_STR "ESP32"
#elif defined(WM_BOARD_ESP32S2)
#define WM_POLICY_BOARD_STR "ESP32-S2"
#elif defined(WM_BOARD_ESP32C3)
#define WM_POLICY_BOARD_STR "ESP32-C3"
#else
#define WM_POLICY_BOARD_STR "unknown"
#endif

// Sensor name string
#if defined(WM_SENSOR_TYPE_SHT20)
#define WM_POLICY_SENSOR_STR "SHT20"
#elif defined(WM_SENSOR_TYPE_AHT10)
#define WM_POLICY_SENSOR_STR "AHT10"
#elif defined(WM_SENSOR_TYPE_HTU21)
#define WM_POLICY_SENSOR_STR "HTU21"
#else
#define WM_POLICY_SENSOR_STR "unknown"
#endif

  Serial.printf("[POLICY] EXPERIMENTAL mode active - not for production\n");
  Serial.printf("[POLICY] board=%s sensor=%s\n", WM_POLICY_BOARD_STR, WM_POLICY_SENSOR_STR);
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  Serial.printf("[BOOT] C3 pins: SDA=%d SCL=%d BTN=%d BZ=%d REL=%d\n",
                Pins::I2C_SDA, Pins::I2C_SCL, Pins::BUTTON, Pins::BUZZER, Pins::RELAY_ALARM);
#endif

  scanI2CBus();

  gDisplay.begin();
  showBootStep("BOOTING", "Init RTC/Sensor...");
  bool rtcDetected = gRtc.begin();
  bool sensorReady = gSensor.begin();
#if WM_BATTERY_ENABLED
  bool batteryReady = gBattery.begin();
#endif

  if (!rtcDetected) {
    Serial.println("[RTC] DS3231 not detected");
  } else if (gRtc.wasLostPower()) {
    Serial.println("[RTC] DS3231 present but lost power, setting from build time");
    bool rtcAdjusted = gRtc.setToBuildTime();
    Serial.printf("[RTC] Adjust from build time: %s\n", rtcAdjusted ? "success" : "failed");
    Serial.printf("[RTC] %s\n", gRtc.isValid() ? "DS3231 ready" : "DS3231 invalid/fail");
  } else if (!gRtc.isValid()) {
    Serial.println("[RTC] DS3231 present but time invalid, setting from build time");
    bool rtcAdjusted = gRtc.setToBuildTime();
    Serial.printf("[RTC] Adjust from build time: %s\n", rtcAdjusted ? "success" : "failed");
    Serial.printf("[RTC] %s\n", gRtc.isValid() ? "DS3231 ready" : "DS3231 invalid/fail");
  } else {
    Serial.println("[RTC] DS3231 ready");
  }

  if (gRtc.isValid()) {
    updateLastKnownEpoch(gRtc.getEpoch());
  }

  Serial.printf("[SENSOR] %s %s\n", gSensor.sensorName(), sensorReady ? "ready" : "init fail");
#if WM_BATTERY_ENABLED
  Serial.printf("[BATTERY] %s pin=%u\n", batteryReady ? "ready" : "init fail",
#if defined(WM_BATTERY_ADC_PIN)
                static_cast<unsigned>(WM_BATTERY_ADC_PIN)
#else
                static_cast<unsigned>(Pins::BATTERY_ADC)
#endif
  );
#else
  Serial.println("[BATTERY] disabled by WM_BATTERY_ENABLED=0");
#endif

  WiFi.setHostname(gWifi.getHostname(gConfig).c_str());
  gMqtt.begin(gConfig, onSetCommand, onDoCommand);
  gSync.begin(gSpool, gMqtt);
  logConfigSummary("[BOOT]");

  if (forcePortalAtBoot) {
    rememberEvent(currentEpoch(), "CFG", 1, 0, 0, false);
  }
  RuntimeConfig configBeforeWifi = gConfig;
  showBootStep("WIFI", forcePortalAtBoot ? "Portal requested" : "Auto connect...");
  gWifi.autoConnect(gConfig, forcePortalAtBoot);
  normalizeRuntimeConfig();
  if (!isSameConfig(configBeforeWifi, gConfig)) {
    gStorage.saveConfig(gConfig);
  }
  gMqtt.setConfig(gConfig);

  if (gWifi.isConnected()) {
    showBootStep("TIME SYNC", "NTP boot sync...");
    beginNtpSync("boot", Config::NTP_BOOT_SYNC_TIMEOUT_MS);
  } else if (!gRtc.isValid() && gRtc.isAvailable()) {
    Serial.println("[RTC] WiFi unavailable, keeping build-time fallback until NTP is reachable");
  }

  gStatus.rtcOk = gRtc.isValid();
  gStatus.oledOk = gDisplay.isAvailable();
  gStatus.sensorOk = gSensor.isAvailable();
  gStatus.wifiOk = gWifi.isConnected();
  gStatus.mqttOk = false;
  gStatus.backendApiOk = false;
  gStatus.backendIngestOk = false;
  gStatus.backendState = BackendState::Unknown;
  gStatus.rssi = gWifi.getRSSI();
  gPrevWifiOk = gStatus.wifiOk;
  gPrevMqttOk = gStatus.mqttOk;
  gPrevDisplayWifiOk = gStatus.wifiOk;
  gPrevDisplayMqttOk = gStatus.mqttOk;
  gPrevDisplayRtcOk = gStatus.rtcOk;
  gPrevDisplaySensorOk = gStatus.sensorOk;
  gPrevDisplayBackendState = gStatus.backendState;
  gPrevDisplayRssi = gStatus.rssi;
  if (gStatus.wifiOk) {
    gHasSeenWifiOk = true;
    gLastWifiOkEpoch = currentEpoch();
  }
  setExternalAlarm(false);
  syncAlarmOutputs(false);
  gLastCommissioningLogMs = millis();
  gLastTelemetrySpoolMs = millis();

  showBootStep("BOOT DONE", "Preparing runtime...");
  refreshDisplay();
  rememberEvent(currentEpoch(), "BR", static_cast<int32_t>(gBootCount), 0, 0, false);
  if (gConfig.buzzerEnabled) {
    beepOnce(120);
  }
  Serial.println("[BOOT] System init complete");
}

void loop() {
  uint32_t loopStartMs = millis();
  uint32_t nowMs = millis();

  ButtonEvent buttonEvent = gButton.update(nowMs);
  if (buttonEvent == ButtonEvent::ShortPress) {
    if (gShortPressCount == 0 ||
        (nowMs - gShortPressWindowStartMs) > Config::BUTTON_MULTI_CLICK_WINDOW_MS) {
      gShortPressCount = 1;
      gShortPressWindowStartMs = nowMs;
    } else {
      gShortPressCount++;
    }
    gLastShortPressMs = nowMs;
    gShortPressActionPending = true;

    if (gShortPressCount >= 4 &&
        (nowMs - gShortPressWindowStartMs) <= Config::BUTTON_MULTI_CLICK_WINDOW_MS) {
      setOperationMode(isPortableMode() ? OperationMode::Normal : OperationMode::Portable);
      gShortPressCount = 0;
      gShortPressActionPending = false;
      gTechnicalScreenVisible = false;
      gDisplay.wakeFor(Config::DISPLAY_AUTO_OFF_MS);
      refreshDisplay();
    }
  } else if (buttonEvent == ButtonEvent::LongPress) {
    if (isPortableMode() && gPortableSyncState == PortableSyncState::Ready) {
      startPortableSync(nowMs);
      gDisplay.wakeFor(Config::DISPLAY_AUTO_OFF_MS);
      refreshDisplay();
    } else {
      gForcePortalRequested = true;
    }
  }

  if (gShortPressActionPending &&
      (nowMs - gLastShortPressMs) > Config::BUTTON_MULTI_CLICK_WINDOW_MS) {
    gShortPressActionPending = false;
    gShortPressCount = 0;
    gTechnicalScreenVisible = !gTechnicalScreenVisible;
    beepOnce(gTechnicalScreenVisible ? 60 : 30);
    gDisplay.wakeFor(Config::DISPLAY_AUTO_OFF_MS);
    refreshDisplay();
  }

  if (gForcePortalRequested) {
    gForcePortalRequested = false;
    enterConfigPortal();
  }

  gWifi.ensureConnected(nowMs);
  maintainTimeSync(nowMs);
  gMqtt.loop(nowMs, gWifi.isConnected());
  updateSubsystemStatus();
  updateBackendStateMachine(nowMs);
  updateBackendPathBlockGate(nowMs);
  gSpool.maintainExternalSpool(nowMs);
  trackConnectionHealth();
  bool displayStatusChanged = (gStatus.wifiOk != gPrevDisplayWifiOk) ||
                              (gStatus.mqttOk != gPrevDisplayMqttOk) ||
                              (gStatus.rtcOk != gPrevDisplayRtcOk) ||
                              (gStatus.sensorOk != gPrevDisplaySensorOk) ||
                              (gStatus.backendState != gPrevDisplayBackendState) ||
                              (gStatus.rssi != gPrevDisplayRssi);
  if (displayStatusChanged) {
    refreshDisplay();
    gPrevDisplayWifiOk = gStatus.wifiOk;
    gPrevDisplayMqttOk = gStatus.mqttOk;
    gPrevDisplayRtcOk = gStatus.rtcOk;
    gPrevDisplaySensorOk = gStatus.sensorOk;
    gPrevDisplayBackendState = gStatus.backendState;
    gPrevDisplayRssi = gStatus.rssi;
  }

  if (gBootPublishPending && gMqtt.isConnected()) {
    publishExceptionEvent("BR", static_cast<int32_t>(gBootCount), 0, 0, false);
    gBootPublishPending = false;
    gStatusPublishPending = true;
  }

  // Publish calibration metadata after boot (event-driven, best-effort)
  if (gCalMetadataPublishPending && gMqtt.isConnected()) {
    publishCalibrationMetadataEvent();
    gCalMetadataPublishPending = false;
  }

  handleSampling(nowMs);
  updateFailureState(gStatus.sensorOk, gSensorFailActive, gSensorFailStartEpoch, "SF");
  updateFailureState(gStatus.rtcOk, gRtcFailActive, gRtcFailStartEpoch, "RF");
  updateFailureState(gStatus.wifiOk, gWifiFailActive, gWifiFailStartEpoch, "WF");
  updateFailureState(gStatus.mqttOk, gMqttFailActive, gMqttFailStartEpoch, "MF");
  syncAlarmOutputs(gStatus.anyAlarmActive);

  handleTelemetry(nowMs);
  handlePortableSync(nowMs);
  handleStatus(nowMs);
  handleCommissioningLog(nowMs);
  if (!isPortableMode()) {
    gSync.loop(nowMs, gConfig, gMqtt.isConnected() && !isBackendPathBlocked());
  }

  gDisplay.loop(nowMs);
  updateBuzzer(nowMs);
  logRuntimeDiagnostics(nowMs);
  uint32_t loopDurationMs = millis() - loopStartMs;
  if (loopDurationMs > gLoopMaxDurationMs) {
    gLoopMaxDurationMs = loopDurationMs;
  }
  delay(Config::MAIN_LOOP_IDLE_MS);
}
