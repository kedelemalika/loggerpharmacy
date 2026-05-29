#include "sync_manager.h"

#include "payload_builder.h"
void SyncManager::begin(SpoolManager& spoolManager, MqttManager& mqttManager) {
  spoolManager_ = &spoolManager;
  mqttManager_ = &mqttManager;
  lastSyncMs_ = millis();
}

void SyncManager::loop(uint32_t nowMs, const RuntimeConfig& config, bool mqttConnected) {
  if (spoolManager_ == nullptr || mqttManager_ == nullptr || !config.spoolEnabled || !mqttConnected) {
    return;
  }
  if ((nowMs - lastSyncMs_) < config.syncIntervalMs) {
    return;
  }
  lastSyncMs_ = nowMs;

  uint8_t remaining = config.syncBatchSize;
  if (remaining == 0) {
    return;
  }

  if (spoolManager_->hasPendingEvents()) {
    syncEvents(remaining);
    remaining = 0;
  }

  if (remaining > 0 && spoolManager_->hasPendingTelemetry()) {
    syncTelemetry(remaining);
  }
}

bool SyncManager::syncEvents(uint8_t batchLimit) {
  uint8_t sent = 0;
  while (sent < batchLimit) {
    EventSpoolRecord record;
    if (!spoolManager_->peekNextEvent(record)) {
      return sent > 0;
    }

    if (!mqttManager_->publishException(buildEventPayload(record, true))) {
      return sent > 0;
    }

    spoolManager_->markEventSynced();
    sent++;
    Serial.printf("[SYNC] Event backfill sent seq=%lu\n", static_cast<unsigned long>(record.sequence));
  }
  return sent > 0;
}

bool SyncManager::syncTelemetry(uint8_t batchLimit) {
  uint8_t sent = 0;
  while (sent < batchLimit) {
    TelemetrySpoolRecord record;
    if (!spoolManager_->peekNextTelemetry(record)) {
      return sent > 0;
    }

    if (!mqttManager_->publishTelemetry(buildTelemetryPayload(record, true))) {
      return sent > 0;
    }

    spoolManager_->markTelemetrySynced();
    sent++;
    Serial.printf("[SYNC] Telemetry backfill sent seq=%lu\n",
                  static_cast<unsigned long>(record.sequence));
  }
  return sent > 0;
}
