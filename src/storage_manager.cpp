#include "storage_manager.h"

#include "zone_profiles.h"

bool StorageManager::begin() {
  ready_ = prefs_.begin(Config::NVS_NAMESPACE, false);
  if (ready_) {
    cachedSequence_ = prefs_.getUInt("seq", 0);
    reservedSequenceLimit_ = cachedSequence_;
  }
  return ready_;
}

void StorageManager::loadConfig(RuntimeConfig& config) {
  if (!ready_) {
    normalizeConfig(config);
    return;
  }

  config.siteId = sanitize(prefs_.getString("site_id", config.siteId), Config::DEFAULT_SITE_ID);
  config.deviceId =
      sanitize(prefs_.getString("device_id", config.deviceId), Config::DEFAULT_DEVICE_ID);
  config.mqttHost =
      sanitize(prefs_.getString("mqtt_host", config.mqttHost), Config::DEFAULT_MQTT_HOST);
  config.mqttPort = prefs_.getUShort("mqtt_port", config.mqttPort);
  config.mqttUsername =
      sanitize(prefs_.getString("mqtt_user", config.mqttUsername), Config::DEFAULT_MQTT_USERNAME);
  config.mqttPassword = prefs_.getString("mqtt_pass", config.mqttPassword);
  config.mqttUseTls = prefs_.getBool("mqtt_tls", config.mqttUseTls);
  config.mqttTlsInsecure = prefs_.getBool("tls_insec", config.mqttTlsInsecure);

  config.tempLowX10 = prefs_.getShort("temp_lo", config.tempLowX10);
  config.tempHighX10 = prefs_.getShort("temp_hi", config.tempHighX10);
  config.humLowX10 = prefs_.getShort("hum_lo", config.humLowX10);
  config.humHighX10 = prefs_.getShort("hum_hi", config.humHighX10);
  config.alarmDelayS = prefs_.getUShort("alarm_del", config.alarmDelayS);
  config.sampleIntervalS = prefs_.getUShort("sample_int", config.sampleIntervalS);
  config.reportIntervalS = prefs_.getUShort("report_int", config.reportIntervalS);
  config.statusIntervalS = prefs_.getUShort("status_int", config.statusIntervalS);
  config.portalTimeoutS = prefs_.getUShort("portal_to", config.portalTimeoutS);
  config.commissioningMode = prefs_.getBool("comm_mode", config.commissioningMode);
  config.commissioningLogIntervalS =
      prefs_.getUShort("comm_int", config.commissioningLogIntervalS);
  config.buzzerEnabled = prefs_.getBool("buzz_en", config.buzzerEnabled);
  config.zoneProfile = ZoneProfiles::parse(prefs_.getString("zone_prof", "ambient"));
  config.spoolEnabled = prefs_.getBool("spool_en", config.spoolEnabled);
  config.spoolIntervalS = prefs_.getUShort("spool_int", config.spoolIntervalS);
  config.syncBatchSize = prefs_.getUChar("sync_bat", config.syncBatchSize);
  config.syncIntervalMs = prefs_.getUInt("sync_intv", config.syncIntervalMs);

  // Load calibration v2 state (additive — new keys)
  config.calVer = prefs_.getUChar("cal_ver", config.calVer);
  config.calStatus = prefs_.getUChar("cal_status", config.calStatus);
  config.calTempSlopeX1000 = prefs_.getShort("cal_t_slp", config.calTempSlopeX1000);
  config.calTempOffsetX10 = prefs_.getShort("cal_t_off", config.calTempOffsetX10);
  config.calHumSlopeX1000 = prefs_.getShort("cal_h_slp", config.calHumSlopeX1000);
  config.calHumOffsetX10 = prefs_.getShort("cal_h_off", config.calHumOffsetX10);

  // Load legacy offsets for backward compat (existing keys)
  config.tempOffsetX10 = prefs_.getShort("temp_off", config.tempOffsetX10);
  config.humOffsetX10 = prefs_.getShort("hum_off", config.humOffsetX10);

  // Map legacy offsets to cal_* if v2 fields not yet stored
  if (prefs_.getUChar("cal_ver", 0) == 0) {
    // v2 cal fields not yet stored — seed from legacy offsets
    config.calTempOffsetX10 = config.tempOffsetX10;
    config.calHumOffsetX10 = config.humOffsetX10;
    // calVer stays at default (1 = offset-only)
  }

  config.operationMode =
      prefs_.getBool("op_mode_p", config.operationMode == OperationMode::Portable)
          ? OperationMode::Portable
          : OperationMode::Normal;
  config.backendHttpBaseUrl = prefs_.getString("p_http_ep", config.backendHttpBaseUrl);
  config.backendIngestBearerToken =
      prefs_.getString("p_http_tok", config.backendIngestBearerToken);
  config.backendDeviceSecret = prefs_.getString("p_dev_sec", config.backendDeviceSecret);
  config.backendTokenExpiresEpoch =
      prefs_.getUInt("p_tok_exp", config.backendTokenExpiresEpoch);
  config.portableBatchSize = prefs_.getUChar("p_http_bat", config.portableBatchSize);
  normalizeConfig(config);
}

void StorageManager::saveConfig(const RuntimeConfig& config) {
  if (!ready_) {
    return;
  }

  RuntimeConfig normalized = config;
  normalizeConfig(normalized);
  prefs_.putString("site_id", normalized.siteId);
  prefs_.putString("device_id", normalized.deviceId);
  prefs_.putString("mqtt_host", normalized.mqttHost);
  prefs_.putUShort("mqtt_port", normalized.mqttPort);
  prefs_.putString("mqtt_user", normalized.mqttUsername);
  prefs_.putString("mqtt_pass", normalized.mqttPassword);
  prefs_.putBool("mqtt_tls", normalized.mqttUseTls);
  prefs_.putBool("tls_insec", normalized.mqttTlsInsecure);
  prefs_.putBool("comm_mode", normalized.commissioningMode);
  prefs_.putUShort("comm_int", normalized.commissioningLogIntervalS);
  prefs_.putBool("buzz_en", normalized.buzzerEnabled);
  prefs_.putString("zone_prof", ZoneProfiles::label(normalized.zoneProfile));
  prefs_.putBool("spool_en", normalized.spoolEnabled);
  prefs_.putUShort("spool_int", normalized.spoolIntervalS);
  prefs_.putUChar("sync_bat", normalized.syncBatchSize);
  prefs_.putUInt("sync_intv", normalized.syncIntervalMs);

  // Persist calibration v2 state (additive)
  prefs_.putUChar("cal_ver", normalized.calVer);
  prefs_.putUChar("cal_status", normalized.calStatus);
  prefs_.putShort("cal_t_slp", normalized.calTempSlopeX1000);
  prefs_.putShort("cal_t_off", normalized.calTempOffsetX10);
  prefs_.putShort("cal_h_slp", normalized.calHumSlopeX1000);
  prefs_.putShort("cal_h_off", normalized.calHumOffsetX10);

  // Persist legacy offsets for backward compat
  prefs_.putShort("temp_off", normalized.tempOffsetX10);
  prefs_.putShort("hum_off", normalized.humOffsetX10);

  prefs_.putBool("op_mode_p", normalized.operationMode == OperationMode::Portable);
  prefs_.putString("p_http_ep", normalized.backendHttpBaseUrl);
  prefs_.putString("p_http_tok", normalized.backendIngestBearerToken);
  prefs_.putString("p_dev_sec", normalized.backendDeviceSecret);
  prefs_.putUInt("p_tok_exp", normalized.backendTokenExpiresEpoch);
  prefs_.putUChar("p_http_bat", normalized.portableBatchSize);
  saveThresholdsAndIntervals(normalized);
}

void StorageManager::saveThresholdsAndIntervals(const RuntimeConfig& config) {
  if (!ready_) {
    return;
  }

  RuntimeConfig normalized = config;
  normalizeConfig(normalized);
  prefs_.putShort("temp_lo", normalized.tempLowX10);
  prefs_.putShort("temp_hi", normalized.tempHighX10);
  prefs_.putShort("hum_lo", normalized.humLowX10);
  prefs_.putShort("hum_hi", normalized.humHighX10);
  prefs_.putUShort("alarm_del", normalized.alarmDelayS);
  prefs_.putUShort("sample_int", normalized.sampleIntervalS);
  prefs_.putUShort("report_int", normalized.reportIntervalS);
  prefs_.putUShort("status_int", normalized.statusIntervalS);
  prefs_.putUShort("portal_to", normalized.portalTimeoutS);
}

void StorageManager::normalizeConfig(RuntimeConfig& config) const {
  config.siteId.trim();
  config.deviceId.trim();
  config.mqttHost.trim();
  config.mqttUsername.trim();
  config.mqttPassword.trim();
  config.backendHttpBaseUrl.trim();
  config.backendIngestBearerToken.trim();
  config.backendDeviceSecret.trim();

  if (config.siteId.isEmpty()) {
    config.siteId = Config::DEFAULT_SITE_ID;
  }
  if (config.deviceId.isEmpty()) {
    config.deviceId = Config::DEFAULT_DEVICE_ID;
  }
  if (config.mqttHost.isEmpty()) {
    config.mqttHost = Config::DEFAULT_MQTT_HOST;
  }
  if (config.mqttPort == 0) {
    config.mqttPort =
        config.mqttUseTls ? Config::DEFAULT_MQTT_TLS_PORT : Config::DEFAULT_MQTT_PORT;
  }
  if (config.mqttUsername.isEmpty()) {
    config.mqttUsername = config.siteId;
  }
  if (config.sampleIntervalS < Config::MIN_SAMPLE_INTERVAL_S) {
    config.sampleIntervalS = Config::MIN_SAMPLE_INTERVAL_S;
  }
  if (config.reportIntervalS < Config::MIN_REPORT_INTERVAL_S) {
    config.reportIntervalS = Config::MIN_REPORT_INTERVAL_S;
  }
  if (config.statusIntervalS < Config::MIN_STATUS_INTERVAL_S) {
    config.statusIntervalS = Config::MIN_STATUS_INTERVAL_S;
  }
  if (config.alarmDelayS < Config::MIN_ALARM_DELAY_S) {
    config.alarmDelayS = Config::MIN_ALARM_DELAY_S;
  }
  if (config.commissioningLogIntervalS < Config::MIN_COMMISSIONING_LOG_INTERVAL_S) {
    config.commissioningLogIntervalS = Config::MIN_COMMISSIONING_LOG_INTERVAL_S;
  }
  if (config.spoolIntervalS < Config::MIN_SPOOL_INTERVAL_S) {
    config.spoolIntervalS = Config::MIN_SPOOL_INTERVAL_S;
  }
  if (config.syncBatchSize < Config::MIN_SYNC_BATCH_SIZE) {
    config.syncBatchSize = Config::MIN_SYNC_BATCH_SIZE;
  }
  if (config.syncBatchSize > Config::MAX_SYNC_BATCH_SIZE) {
    config.syncBatchSize = Config::MAX_SYNC_BATCH_SIZE;
  }
  if (config.syncIntervalMs < Config::MIN_SYNC_INTERVAL_MS) {
    config.syncIntervalMs = Config::MIN_SYNC_INTERVAL_MS;
  }
  if (config.portableBatchSize < Config::MIN_PORTABLE_BATCH_SIZE) {
    config.portableBatchSize = Config::MIN_PORTABLE_BATCH_SIZE;
  }
  if (config.portableBatchSize > Config::MAX_PORTABLE_BATCH_SIZE) {
    config.portableBatchSize = Config::MAX_PORTABLE_BATCH_SIZE;
  }
  if (config.backendIngestBearerToken.length() == 0) {
    config.backendTokenExpiresEpoch = 0;
  }
  if (config.tempOffsetX10 < Config::MIN_TEMP_OFFSET_X10) {
    config.tempOffsetX10 = Config::MIN_TEMP_OFFSET_X10;
  }
  if (config.tempOffsetX10 > Config::MAX_TEMP_OFFSET_X10) {
    config.tempOffsetX10 = Config::MAX_TEMP_OFFSET_X10;
  }
  if (config.humOffsetX10 < Config::MIN_HUM_OFFSET_X10) {
    config.humOffsetX10 = Config::MIN_HUM_OFFSET_X10;
  }
  if (config.humOffsetX10 > Config::MAX_HUM_OFFSET_X10) {
    config.humOffsetX10 = Config::MAX_HUM_OFFSET_X10;
  }

  // ── Calibration v2 normalization ───────────────────────────────────────────
  if (config.calVer < Config::MIN_CAL_VER) {
    config.calVer = Config::MIN_CAL_VER;
  }
  if (config.calVer > Config::MAX_CAL_VER) {
    config.calVer = Config::MAX_CAL_VER;
  }
  if (config.calStatus < Config::MIN_CAL_STATUS) {
    config.calStatus = Config::MIN_CAL_STATUS;
  }
  if (config.calStatus > Config::MAX_CAL_STATUS) {
    config.calStatus = Config::MAX_CAL_STATUS;
  }
  if (config.calTempSlopeX1000 < Config::MIN_CAL_TEMP_SLOPE) {
    config.calTempSlopeX1000 = Config::MIN_CAL_TEMP_SLOPE;
  }
  if (config.calTempSlopeX1000 > Config::MAX_CAL_TEMP_SLOPE) {
    config.calTempSlopeX1000 = Config::MAX_CAL_TEMP_SLOPE;
  }
  if (config.calTempOffsetX10 < Config::MIN_CAL_TEMP_OFFSET) {
    config.calTempOffsetX10 = Config::MIN_CAL_TEMP_OFFSET;
  }
  if (config.calTempOffsetX10 > Config::MAX_CAL_TEMP_OFFSET) {
    config.calTempOffsetX10 = Config::MAX_CAL_TEMP_OFFSET;
  }
  if (config.calHumSlopeX1000 < Config::MIN_CAL_HUM_SLOPE) {
    config.calHumSlopeX1000 = Config::MIN_CAL_HUM_SLOPE;
  }
  if (config.calHumSlopeX1000 > Config::MAX_CAL_HUM_SLOPE) {
    config.calHumSlopeX1000 = Config::MAX_CAL_HUM_SLOPE;
  }
  if (config.calHumOffsetX10 < Config::MIN_CAL_HUM_OFFSET) {
    config.calHumOffsetX10 = Config::MIN_CAL_HUM_OFFSET;
  }
  if (config.calHumOffsetX10 > Config::MAX_CAL_HUM_OFFSET) {
    config.calHumOffsetX10 = Config::MAX_CAL_HUM_OFFSET;
  }
}

uint32_t StorageManager::incrementBootCount() {
  if (!ready_) {
    return 0;
  }

  uint32_t boots = prefs_.getUInt("boot_cnt", 0) + 1;
  prefs_.putUInt("boot_cnt", boots);
  return boots;
}

uint32_t StorageManager::nextSequence() {
  if (!ready_) {
    cachedSequence_++;
    return cachedSequence_;
  }

  if (cachedSequence_ >= reservedSequenceLimit_) {
    reservedSequenceLimit_ = cachedSequence_ + Config::SEQUENCE_RESERVE_BLOCK;
    prefs_.putUInt("seq", reservedSequenceLimit_);
  }

  cachedSequence_++;
  return cachedSequence_;
}

uint32_t StorageManager::currentSequence() const { return cachedSequence_; }

void StorageManager::addEvent(const EventRecord& event) {
  eventLog_[eventHead_] = event;
  eventHead_ = (eventHead_ + 1) % Config::EVENT_LOG_CAPACITY;
  if (eventCount_ < Config::EVENT_LOG_CAPACITY) {
    eventCount_++;
  }
}

size_t StorageManager::eventCount() const { return eventCount_; }

const EventRecord* StorageManager::events() const { return eventLog_; }

String StorageManager::sanitize(const String& value, const String& fallback) const {
  return value.length() == 0 ? fallback : value;
}
