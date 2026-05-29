#include "wifi_manager_ext.h"

#include <ctype.h>
#include <WiFi.h>

WiFiManagerExt* WiFiManagerExt::instance_ = nullptr;

WiFiManagerExt::WiFiManagerExt()
    : siteIdParam_("site", "site_id", "", 16),
      deviceIdParam_("dev", "device_id", "", 16),
      mqttHostParam_("mhost", "mqtt_host", "", 64),
      mqttPortParam_("mport", "mqtt_port", "", 6),
      mqttUserParam_("muser", "mqtt_username", "", 32),
      mqttPassParam_("mpass", "mqtt_password", "", 64),
      mqttTlsParam_("mtls", "mqtt_use_tls (1/0)", "", 2),
      mqttTlsInsecureParam_("mtlsi", "mqtt_tls_insecure (1/0)", "", 2),
      buzzerEnabledParam_("bzen", "buzzer_enabled (1/0)", "", 2),
      backendHttpBaseUrlParam_("phep", "backend_http_base_url", "", Config::PORTAL_BACKEND_URL_PARAM_LENGTH),
      backendIngestBearerTokenParam_("phtk", "backend_ingest_bearer_token", "", Config::PORTAL_BACKEND_BEARER_PARAM_LENGTH),
      backendDeviceSecretParam_("psec", "backend_device_secret", "", Config::PORTAL_BACKEND_DEVICE_SECRET_PARAM_LENGTH),
      portableBatchSizeParam_("phbs", "portable_batch_size", "", 4) {
  instance_ = this;
}

bool WiFiManagerExt::autoConnect(RuntimeConfig& config, bool forcePortal) {
  preparePortal(config);
  shouldSaveConfig_ = false;

  String hostname = getHostname(config);
  Serial.printf("[WIFI] Auto connect start, host=%s, portal=%s\n", hostname.c_str(),
                forcePortal ? "forced" : "auto");
  bool connected =
      forcePortal ? wifiManager_.startConfigPortal((hostname + "-cfg").c_str())
                  : wifiManager_.autoConnect(hostname.c_str());

  if (connected || shouldSaveConfig_) {
    applyPortalValues(config);
  }
  Serial.printf("[WIFI] Auto connect result=%s\n", connected ? "connected" : "not-connected");
  return connected;
}

bool WiFiManagerExt::ensureConnected(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  if ((nowMs - lastEnsureMs_) < Config::WIFI_RECONNECT_INTERVAL_MS) {
    return false;
  }

  lastEnsureMs_ = nowMs;
  Serial.println("[WIFI] Reconnect attempt");
  WiFi.reconnect();
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManagerExt::startConfigPortal(RuntimeConfig& config) {
  return autoConnect(config, true);
}

bool WiFiManagerExt::isConnected() const { return WiFi.status() == WL_CONNECTED; }

int32_t WiFiManagerExt::getRSSI() const {
  return isConnected() ? WiFi.RSSI() : -127;
}

String WiFiManagerExt::getHostname(const RuntimeConfig& config) const {
  String hostname;
  hostname.reserve(config.siteId.length() + config.deviceId.length() + 1);
  hostname = config.siteId;
  hostname += '-';
  hostname += config.deviceId;
  return hostname;
}

bool WiFiManagerExt::isTlsEnabled(const RuntimeConfig& config) const { return config.mqttUseTls; }

bool WiFiManagerExt::isTlsInsecure(const RuntimeConfig& config) const {
  return config.mqttTlsInsecure;
}

const char* WiFiManagerExt::tlsModeLabel(const RuntimeConfig& config) const {
  if (!isTlsEnabled(config)) {
    return "TCP";
  }
  return isTlsInsecure(config) ? "TLS-INSECURE" : "TLS-VERIFY";
}

void WiFiManagerExt::preparePortal(const RuntimeConfig& config) {
  wifiManager_.setSaveConfigCallback(saveConfigCallback);
  wifiManager_.setConfigPortalTimeout(config.portalTimeoutS);
  wifiManager_.setConnectTimeout(15);
  wifiManager_.setWiFiAutoReconnect(true);
  wifiManager_.setHostname(getHostname(config).c_str());

  siteIdParam_.setValue(config.siteId.c_str(), 16);
  deviceIdParam_.setValue(config.deviceId.c_str(), 16);
  mqttHostParam_.setValue(config.mqttHost.c_str(), 64);
  char portBuffer[6];
  snprintf(portBuffer, sizeof(portBuffer), "%u", config.mqttPort);
  mqttPortParam_.setValue(portBuffer, 6);
  mqttUserParam_.setValue(config.mqttUsername.c_str(), 32);
  mqttPassParam_.setValue(config.mqttPassword.c_str(), 64);
  mqttTlsParam_.setValue(config.mqttUseTls ? "1" : "0", 2);
  mqttTlsInsecureParam_.setValue(config.mqttTlsInsecure ? "1" : "0", 2);
  buzzerEnabledParam_.setValue(config.buzzerEnabled ? "1" : "0", 2);
  backendHttpBaseUrlParam_.setValue(config.backendHttpBaseUrl.c_str(), Config::PORTAL_BACKEND_URL_PARAM_LENGTH);
  backendIngestBearerTokenParam_.setValue(config.backendIngestBearerToken.c_str(),
                                         Config::PORTAL_BACKEND_BEARER_PARAM_LENGTH);
  backendDeviceSecretParam_.setValue(config.backendDeviceSecret.c_str(),
                                      Config::PORTAL_BACKEND_DEVICE_SECRET_PARAM_LENGTH);
  char portableBatchBuffer[4];
  snprintf(portableBatchBuffer, sizeof(portableBatchBuffer), "%u", config.portableBatchSize);
  portableBatchSizeParam_.setValue(portableBatchBuffer, 4);

  if (!paramsAdded_) {
    wifiManager_.addParameter(&siteIdParam_);
    wifiManager_.addParameter(&deviceIdParam_);
    wifiManager_.addParameter(&mqttHostParam_);
    wifiManager_.addParameter(&mqttPortParam_);
    wifiManager_.addParameter(&mqttUserParam_);
    wifiManager_.addParameter(&mqttPassParam_);
    wifiManager_.addParameter(&mqttTlsParam_);
    wifiManager_.addParameter(&mqttTlsInsecureParam_);
    wifiManager_.addParameter(&buzzerEnabledParam_);
    wifiManager_.addParameter(&backendHttpBaseUrlParam_);
    wifiManager_.addParameter(&backendIngestBearerTokenParam_);
    wifiManager_.addParameter(&backendDeviceSecretParam_);
    wifiManager_.addParameter(&portableBatchSizeParam_);
    paramsAdded_ = true;
  }
}

void WiFiManagerExt::applyPortalValues(RuntimeConfig& config) {
  config.siteId = String(siteIdParam_.getValue());
  config.deviceId = String(deviceIdParam_.getValue());
  config.mqttHost = String(mqttHostParam_.getValue());
  long rawPort = String(mqttPortParam_.getValue()).toInt();
  config.mqttPort = (rawPort >= 1 && rawPort <= 65535) ? static_cast<uint16_t>(rawPort) : 0;
  config.mqttUsername = String(mqttUserParam_.getValue());
  config.mqttPassword = String(mqttPassParam_.getValue());
  config.mqttUseTls = parseBoolParam(mqttTlsParam_.getValue(), config.mqttUseTls);
  config.mqttTlsInsecure =
      parseBoolParam(mqttTlsInsecureParam_.getValue(), config.mqttTlsInsecure);
  config.buzzerEnabled = parseBoolParam(buzzerEnabledParam_.getValue(), config.buzzerEnabled);
  config.backendHttpBaseUrl = String(backendHttpBaseUrlParam_.getValue());
  config.backendIngestBearerToken = String(backendIngestBearerTokenParam_.getValue());
  config.backendDeviceSecret = String(backendDeviceSecretParam_.getValue());
  int rawPortableBatch = String(portableBatchSizeParam_.getValue()).toInt();
  if (rawPortableBatch >= static_cast<int>(Config::MIN_PORTABLE_BATCH_SIZE) &&
      rawPortableBatch <= static_cast<int>(Config::MAX_PORTABLE_BATCH_SIZE)) {
    config.portableBatchSize = static_cast<uint8_t>(rawPortableBatch);
  }
  normalizeConfig(config);
  Serial.printf("[WIFI] Portal config saved, broker=%s:%u tls=%s\n", config.mqttHost.c_str(),
                config.mqttPort, tlsModeLabel(config));
}

void WiFiManagerExt::normalizeConfig(RuntimeConfig& config) const {
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
  if (config.portableBatchSize < Config::MIN_PORTABLE_BATCH_SIZE) {
    config.portableBatchSize = Config::MIN_PORTABLE_BATCH_SIZE;
  }
  if (config.portableBatchSize > Config::MAX_PORTABLE_BATCH_SIZE) {
    config.portableBatchSize = Config::MAX_PORTABLE_BATCH_SIZE;
  }
}

bool WiFiManagerExt::parseBoolParam(const char* value, bool defaultValue) const {
  if (value == nullptr) {
    return defaultValue;
  }

  while (*value != '\0' && isspace(static_cast<unsigned char>(*value))) {
    ++value;
  }

  size_t len = strlen(value);
  while (len > 0 && isspace(static_cast<unsigned char>(value[len - 1]))) {
    --len;
  }

  if (len == 1 && value[0] == '1') {
    return true;
  }
  if (len == 1 && value[0] == '0') {
    return false;
  }

  auto equalsIgnoreCase = [value, len](const char* expected) -> bool {
    size_t expectedLen = strlen(expected);
    if (len != expectedLen) {
      return false;
    }
    for (size_t i = 0; i < len; ++i) {
      if (tolower(static_cast<unsigned char>(value[i])) != expected[i]) {
        return false;
      }
    }
    return true;
  };

  if (equalsIgnoreCase("true") || equalsIgnoreCase("yes") || equalsIgnoreCase("on")) {
    return true;
  }
  if (equalsIgnoreCase("false") || equalsIgnoreCase("no") || equalsIgnoreCase("off")) {
    return false;
  }
  return defaultValue;
}

void WiFiManagerExt::saveConfigCallback() {
  if (instance_ != nullptr) {
    instance_->shouldSaveConfig_ = true;
  }
}
