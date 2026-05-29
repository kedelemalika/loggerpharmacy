#pragma once

#include <Arduino.h>
#include <WiFiManager.h>

#include "types.h"

class WiFiManagerExt {
 public:
  WiFiManagerExt();

  bool autoConnect(RuntimeConfig& config, bool forcePortal);
  bool ensureConnected(uint32_t nowMs);
  bool startConfigPortal(RuntimeConfig& config);
  bool isConnected() const;
  int32_t getRSSI() const;
  String getHostname(const RuntimeConfig& config) const;
  bool isTlsEnabled(const RuntimeConfig& config) const;
  bool isTlsInsecure(const RuntimeConfig& config) const;
  const char* tlsModeLabel(const RuntimeConfig& config) const;

 private:
  void preparePortal(const RuntimeConfig& config);
  void applyPortalValues(RuntimeConfig& config);
  void normalizeConfig(RuntimeConfig& config) const;
  bool parseBoolParam(const char* value, bool defaultValue) const;
  static void saveConfigCallback();

  WiFiManager wifiManager_;
  bool shouldSaveConfig_ = false;
  bool paramsAdded_ = false;
  uint32_t lastEnsureMs_ = 0;

  WiFiManagerParameter siteIdParam_;
  WiFiManagerParameter deviceIdParam_;
  WiFiManagerParameter mqttHostParam_;
  WiFiManagerParameter mqttPortParam_;
  WiFiManagerParameter mqttUserParam_;
  WiFiManagerParameter mqttPassParam_;
  WiFiManagerParameter mqttTlsParam_;
  WiFiManagerParameter mqttTlsInsecureParam_;
  WiFiManagerParameter buzzerEnabledParam_;
  WiFiManagerParameter backendHttpBaseUrlParam_;
  WiFiManagerParameter backendIngestBearerTokenParam_;
  WiFiManagerParameter backendDeviceSecretParam_;
  WiFiManagerParameter portableBatchSizeParam_;

  static WiFiManagerExt* instance_;
};
