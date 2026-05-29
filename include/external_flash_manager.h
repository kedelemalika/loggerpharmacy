#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>

#include "config.h"
#include "types.h"

class ExternalFlashManager {
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

 private:
  struct QueueState {
    uint32_t writeSeq = 0;
    uint32_t syncedSeq = 0;
  };

  struct ReadCache {
    bool valid = false;
    uint32_t nextSeq = 0;
  };

  bool loadState();
  bool saveState();

  bool flashRead(uint32_t address, uint8_t* buffer, size_t len) const;
  bool flashWriteSector(uint32_t address, const uint8_t* data, size_t len);
  bool readTelemetrySeq(uint32_t seq, TelemetrySpoolRecord& record) const;
  bool readEventSeq(uint32_t seq, EventSpoolRecord& record) const;
  bool readPortableSeq(uint32_t seq, TelemetrySpoolRecord& record) const;

  bool appendTelemetryInternal(const TelemetrySpoolRecord& record, bool markSyncedIfContiguous);
  bool appendEventInternal(const EventSpoolRecord& record, bool markSyncedIfContiguous);
  bool appendPortableInternal(const TelemetrySpoolRecord& record);

  bool ready_ = false;
  bool nvsReady_ = false;
  Preferences prefs_;
  SPIClass spiBus_{FSPI};

  QueueState telemetryState_;
  QueueState eventState_;
  QueueState portableState_;

  ReadCache telemetryCache_;
  ReadCache eventCache_;
  uint32_t portableBatchSeq_[Config::MAX_PORTABLE_BATCH_SIZE] = {0};
  size_t portableBatchCount_ = 0;

  size_t telemetryCapacity_ = 0;
  size_t eventCapacity_ = 0;
  size_t portableCapacity_ = 0;
};
