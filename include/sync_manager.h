#pragma once

#include <Arduino.h>

#include "mqtt_manager.h"
#include "spool_manager.h"
#include "types.h"

class SyncManager {
 public:
  void begin(SpoolManager& spoolManager, MqttManager& mqttManager);
  void loop(uint32_t nowMs, const RuntimeConfig& config, bool mqttConnected);

 private:
  bool syncEvents(uint8_t batchLimit);
  bool syncTelemetry(uint8_t batchLimit);

  SpoolManager* spoolManager_ = nullptr;
  MqttManager* mqttManager_ = nullptr;
  uint32_t lastSyncMs_ = 0;
};
