#pragma once

#include <Arduino.h>

#include "types.h"

class ExternalFlashManager;

class SpoolManager {
 public:
  bool begin();
  bool isReady() const;

  bool appendTelemetry(const TelemetrySpoolRecord& record, bool markSyncedIfContiguous);
  bool appendEvent(const EventSpoolRecord& record, bool markSyncedIfContiguous);
  bool appendPortableTelemetry(const TelemetrySpoolRecord& record);

  bool peekNextTelemetry(TelemetrySpoolRecord& record);
  bool peekNextEvent(EventSpoolRecord& record);
  size_t loadPortableTelemetryBatch(TelemetrySpoolRecord* records, size_t maxRecords);

  bool markTelemetrySynced();
  bool markEventSynced();
  bool markPortableTelemetryBatchSynced(size_t count);

  bool hasPendingTelemetry() const;
  bool hasPendingEvents() const;
  bool hasPendingPortableTelemetry() const;
  size_t pendingTelemetryBytes() const;
  size_t pendingEventBytes() const;
  size_t pendingPortableTelemetryBytes() const;
  size_t pendingPortableTelemetryCount(size_t maxScanRecords = 1000) const;
  bool isExternalSpoolActive() const;
  bool hadExternalSpoolFallback() const;
  const char* externalSpoolFallbackReason() const;
  void maintainExternalSpool(uint32_t nowMs);

 private:
  struct ReadCache {
    bool valid = false;
    size_t nextOffset = 0;
  };

 bool ensureLayout();
  bool loadState();
  bool saveState();
  String buildTelemetryLine(const TelemetrySpoolRecord& record) const;
  String buildEventLine(const EventSpoolRecord& record) const;
  bool appendLine(const char* path, const String& line, size_t& syncedOffset, size_t maxBytes,
                  bool markSyncedIfContiguous);
  bool readNextLine(const char* path, size_t syncedOffset, String& line, size_t& nextOffset) const;
  bool parseTelemetryLine(const String& line, TelemetrySpoolRecord& record) const;
  bool parseEventLine(const String& line, EventSpoolRecord& record) const;
 bool compactIfNeeded(const char* path, size_t& syncedOffset, size_t maxBytes);
  bool rewriteFromOffset(const char* path, size_t& syncedOffset, size_t copyFromOffset);
  size_t fileSize(const char* path) const;
  void disableExternalSpool(const char* reason);

  bool ready_ = false;
  bool externalSpoolReady_ = false;
  bool externalSpoolFallbackSeen_ = false;
  uint32_t externalSpoolLastProbeMs_ = 0;
  String externalSpoolFallbackReason_ = "";
  ExternalFlashManager* externalFlash_ = nullptr;
  size_t telemetrySyncedOffset_ = 0;
  size_t eventSyncedOffset_ = 0;
  size_t portableTelemetrySyncedOffset_ = 0;
  ReadCache telemetryCache_;
  ReadCache eventCache_;
  size_t portableBatchOffsets_[Config::MAX_PORTABLE_BATCH_SIZE] = {0};
  size_t portableBatchCount_ = 0;
};
