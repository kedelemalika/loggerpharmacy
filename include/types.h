#pragma once

#include <Arduino.h>

#include "config.h"

enum class ZoneProfile {
  Ambient,
  Coldroom,
  Freezer,
  Custom
};

enum class OperationMode {
  Normal = 0,
  Portable = 1
};

enum class BackendState {
  Unknown = 0,
  Ok = 1,
  Degraded = 2,
  Down = 3
};

struct RuntimeConfig {
  String siteId = Config::DEFAULT_SITE_ID;
  String deviceId = Config::DEFAULT_DEVICE_ID;
  String mqttHost = Config::DEFAULT_MQTT_HOST;
  uint16_t mqttPort = Config::DEFAULT_MQTT_PORT;
  String mqttUsername = Config::DEFAULT_MQTT_USERNAME;
  String mqttPassword = Config::DEFAULT_MQTT_PASSWORD;
  bool mqttUseTls = Config::DEFAULT_MQTT_USE_TLS;
  bool mqttTlsInsecure = Config::DEFAULT_MQTT_TLS_INSECURE;
  int16_t tempLowX10 = Config::DEFAULT_TEMP_LOW_X10;
  int16_t tempHighX10 = Config::DEFAULT_TEMP_HIGH_X10;
  int16_t humLowX10 = Config::DEFAULT_HUM_LOW_X10;
  int16_t humHighX10 = Config::DEFAULT_HUM_HIGH_X10;
  uint16_t alarmDelayS = Config::DEFAULT_ALARM_DELAY_S;
  uint16_t sampleIntervalS = Config::DEFAULT_SAMPLE_INTERVAL_S;
  uint16_t reportIntervalS = Config::DEFAULT_REPORT_INTERVAL_S;
  uint16_t statusIntervalS = Config::DEFAULT_STATUS_INTERVAL_S;
  uint16_t portalTimeoutS = Config::DEFAULT_PORTAL_TIMEOUT_S;
  bool commissioningMode = Config::DEFAULT_COMMISSIONING_MODE;
  uint16_t commissioningLogIntervalS = Config::DEFAULT_COMMISSIONING_LOG_INTERVAL_S;
  bool buzzerEnabled = Config::DEFAULT_BUZZER_ENABLED;
  ZoneProfile zoneProfile = ZoneProfile::Ambient;
  bool spoolEnabled = Config::DEFAULT_SPOOL_ENABLED;
  uint16_t spoolIntervalS = Config::DEFAULT_SPOOL_INTERVAL_S;
  uint8_t syncBatchSize = Config::DEFAULT_SYNC_BATCH_SIZE;
  uint32_t syncIntervalMs = Config::DEFAULT_SYNC_INTERVAL_MS;

  // Legacy calibration offset (backward compat — used when calVer < 2 or for v1 payloads)
  int16_t tempOffsetX10 = Config::DEFAULT_TEMP_OFFSET_X10;
  int16_t humOffsetX10 = Config::DEFAULT_HUM_OFFSET_X10;

  // ── Calibration v2 state ───────────────────────────────────────────────────
  // calVer: calibration mode (1=offset-only, 2=slope+offset)
  uint8_t calVer = Config::DEFAULT_CAL_VER;
  // calStatus: calibration status (0=uncalibrated, 1=factory, 2=field)
  uint8_t calStatus = Config::DEFAULT_CAL_STATUS;
  // calTempSlopeX1000: temperature correction slope (1000 = 1.000x)
  int16_t calTempSlopeX1000 = Config::DEFAULT_CAL_TEMP_SLOPE_X1000;
  // calTempOffsetX10: temperature correction offset in X10 units
  int16_t calTempOffsetX10 = Config::DEFAULT_CAL_TEMP_OFFSET_X10;
  // calHumSlopeX1000: humidity correction slope (1000 = 1.000x)
  int16_t calHumSlopeX1000 = Config::DEFAULT_CAL_HUM_SLOPE_X1000;
  // calHumOffsetX10: humidity correction offset in X10 units
  int16_t calHumOffsetX10 = Config::DEFAULT_CAL_HUM_OFFSET_X10;

  OperationMode operationMode =
      static_cast<OperationMode>(Config::DEFAULT_OPERATION_MODE_RAW);
  String backendHttpBaseUrl = Config::DEFAULT_BACKEND_HTTP_BASE_URL;
  String backendIngestBearerToken = Config::DEFAULT_BACKEND_INGEST_BEARER_TOKEN;
  String backendDeviceSecret = Config::DEFAULT_BACKEND_DEVICE_SECRET;
  uint32_t backendTokenExpiresEpoch = Config::DEFAULT_BACKEND_TOKEN_EXPIRES_EPOCH;
  uint8_t portableBatchSize = Config::DEFAULT_PORTABLE_BATCH_SIZE;
};

struct SensorReading {
  bool valid = false;
  bool latestSampleOk = false;
  bool fromLastValid = false;
  bool sensorPresent = false;
  bool sensorRuntimeError = false;
  uint32_t lastGoodMs = 0;
  uint8_t retryCount = 0;
  float rawTemperatureC = Config::INVALID_SENSOR_VALUE;
  float rawHumidityPct = Config::INVALID_SENSOR_VALUE;
  int16_t rawTemperatureX10 = 0;
  int16_t rawHumidityX10 = 0;
  float temperatureC = Config::INVALID_SENSOR_VALUE;
  float humidityPct = Config::INVALID_SENSOR_VALUE;
  int16_t temperatureX10 = 0;
  int16_t humidityX10 = 0;
  uint32_t sampleCount = 0;
  bool batteryValid = false;
  uint16_t batteryMilliVolt = 0;
  uint8_t batteryPercent = 0;
};

struct DeviceStatus {
  bool wifiOk = false;
  bool mqttOk = false;
  bool sensorOk = false;
  bool rtcOk = false;
  bool oledOk = false;
  bool backendApiOk = false;
  bool backendIngestOk = false;
  BackendState backendState = BackendState::Unknown;
  int32_t rssi = -127;
  bool anyAlarmActive = false;
  const char* mqttLabel = "FAIL";
};

struct AlarmState {
  AlarmState() = default;
  explicit AlarmState(const char* alarmCode) : code(alarmCode) {}

  const char* code = "";
  bool breachActive = false;
  bool confirmed = false;
  uint32_t breachStartedMs = 0;
  uint32_t breachStartedEpoch = 0;
};

struct EventRecord {
  uint32_t ts = 0;
  char code[4] = {'\0', '\0', '\0', '\0'};
  int32_t value = 0;
  int32_t limit = 0;
  uint32_t duration = 0;
  bool recovery = false;
};

enum class ButtonEvent {
  None,
  ShortPress,
  LongPress
};

enum class CommandAction {
  None,
  Reboot,
  DisplayOn,
  PublishStatusNow,
  ForceConfigPortal,
  MuteAlarmBuzzer,
  UnmuteAlarmBuzzer
};

enum class QueueMessageType {
  Telemetry,
  Status,
  Exception
};

struct TelemetrySpoolRecord {
  uint32_t sequence = 0;
  uint32_t timestamp = 0;
  int16_t temperatureX10 = 0;
  int16_t humidityX10 = 0;
  bool sampleOk = false;
  bool rtcOk = false;
  // v2: raw sensor values (before calibration)
  int16_t rawTemperatureX10 = 0;
  int16_t rawHumidityX10 = 0;
  bool hasRawValues = false;
  bool batteryValid = false;
  uint16_t batteryMilliVolt = 0;
  uint8_t batteryPercent = 0;
};

struct EventSpoolRecord {
  uint32_t sequence = 0;
  uint32_t timestamp = 0;
  char code[4] = {'\0', '\0', '\0', '\0'};
  int32_t value = 0;
  int32_t limit = 0;
  uint32_t duration = 0;
  bool recovery = false;
  bool rtcOk = false;
};

enum class MqttConnectionState {
  Disconnected,
  TcpReady,
  TlsInsecure,
  TlsVerified,
  ConnectFailed,
  TlsConfigError,
  TlsHandshakeFailed
};

// Calibration state tracking (runtime, not persisted)
struct CalibrationState {
  uint8_t  calVer = Config::DEFAULT_CAL_VER;
  uint8_t  calStatus = Config::DEFAULT_CAL_STATUS;
  int16_t  calTempSlopeX1000 = Config::DEFAULT_CAL_TEMP_SLOPE_X1000;
  int16_t  calTempOffsetX10 = Config::DEFAULT_CAL_TEMP_OFFSET_X10;
  int16_t  calHumSlopeX1000 = Config::DEFAULT_CAL_HUM_SLOPE_X1000;
  int16_t  calHumOffsetX10 = Config::DEFAULT_CAL_HUM_OFFSET_X10;
  uint32_t calRevision = 0;
  uint32_t calUpdatedEpoch = 0;
};
