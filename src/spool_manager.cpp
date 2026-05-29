#include "spool_manager.h"

#include <LittleFS.h>

#include "config.h"
#include "external_flash_manager.h"

namespace {
String trimLine(String value) {
  value.trim();
  return value;
}
constexpr uint8_t kExtFlashRetryCount = 2;
constexpr uint32_t kExtFlashReprobeIntervalMs = 5UL * 60UL * 1000UL;
}  // namespace

bool SpoolManager::begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[SPOOL] LittleFS init failed");
    ready_ = false;
    return false;
  }

  ready_ = ensureLayout() && loadState();
#if defined(WM_DIAG_DISABLE_EXTFLASH)
  externalSpoolReady_ = false;
  Serial.println("[DIAG] External flash spool disabled by WM_DIAG_DISABLE_EXTFLASH");
#else
  if (externalFlash_ == nullptr) {
    externalFlash_ = new ExternalFlashManager();
  }
  externalSpoolReady_ = externalFlash_ != nullptr && externalFlash_->begin();
  if (!externalSpoolReady_) {
    Serial.println(
        "[SPOOL] External W25Q64 unavailable, fallback all spool queues to internal LittleFS");
  } else {
    Serial.println("[SPOOL] External W25Q64 spool active for telemetry, events, portable");
  }
#endif
  Serial.printf("[SPOOL] Init %s\n", ready_ ? "ok" : "fail");
  return ready_;
}

bool SpoolManager::isReady() const { return ready_; }

void SpoolManager::disableExternalSpool(const char* reason) {
  if (!externalSpoolReady_) {
    return;
  }
  size_t extTelemetryPendingBytes = externalFlash_ != nullptr ? externalFlash_->pendingTelemetryBytes() : 0;
  size_t extEventsPending = externalFlash_ != nullptr ? externalFlash_->pendingEventBytes() : 0;
  size_t extPortablePending = externalFlash_ != nullptr ? externalFlash_->pendingPortableTelemetryCount(5000) : 0;
  externalSpoolFallbackSeen_ = true;
  externalSpoolFallbackReason_ = reason != nullptr ? reason : "unknown";
  externalSpoolReady_ = false;
  Serial.printf(
      "[SPOOL] External spool disabled at runtime (%s), switching to LittleFS "
      "[ofs t=%u e=%u p=%u extPending t=%u eBytes=%u p=%u]\n",
      reason != nullptr ? reason : "unknown",
      static_cast<unsigned>(telemetrySyncedOffset_),
      static_cast<unsigned>(eventSyncedOffset_),
      static_cast<unsigned>(portableTelemetrySyncedOffset_),
      static_cast<unsigned>(extTelemetryPendingBytes),
      static_cast<unsigned>(extEventsPending),
      static_cast<unsigned>(extPortablePending));
}

bool SpoolManager::isExternalSpoolActive() const { return externalSpoolReady_; }

bool SpoolManager::hadExternalSpoolFallback() const { return externalSpoolFallbackSeen_; }

const char* SpoolManager::externalSpoolFallbackReason() const {
  return externalSpoolFallbackReason_.length() > 0 ? externalSpoolFallbackReason_.c_str() : "";
}

void SpoolManager::maintainExternalSpool(uint32_t nowMs) {
  if (!ready_ || externalSpoolReady_) {
    return;
  }
  if ((nowMs - externalSpoolLastProbeMs_) < kExtFlashReprobeIntervalMs) {
    return;
  }
  externalSpoolLastProbeMs_ = nowMs;

  if (hasPendingTelemetry() || hasPendingEvents() || hasPendingPortableTelemetry()) {
    Serial.println("[SPOOL] External spool reprobe deferred: internal queue still has pending data");
    return;
  }

  if (externalFlash_ == nullptr) {
    externalFlash_ = new ExternalFlashManager();
    if (externalFlash_ == nullptr) {
      Serial.println("[SPOOL] External spool reprobe failed: allocator returned null");
      return;
    }
  }

  if (externalFlash_->begin()) {
    externalSpoolReady_ = true;
    Serial.println("[SPOOL] External spool re-enabled after runtime fallback");
  } else {
    Serial.println("[SPOOL] External spool reprobe failed, still using LittleFS");
  }
}

bool SpoolManager::appendTelemetry(const TelemetrySpoolRecord& record, bool markSyncedIfContiguous) {
  if (!ready_) {
    return false;
  }

  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->appendTelemetry(record, markSyncedIfContiguous)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("appendTelemetry");
  }

  String line = buildTelemetryLine(record);
  telemetryCache_.valid = false;
  return appendLine(Config::SPOOL_TELEMETRY_PATH, line, telemetrySyncedOffset_,
                    Config::MAX_TELEMETRY_SPOOL_BYTES, markSyncedIfContiguous);
}

bool SpoolManager::appendEvent(const EventSpoolRecord& record, bool markSyncedIfContiguous) {
  if (!ready_) {
    return false;
  }

  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->appendEvent(record, markSyncedIfContiguous)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("appendEvent");
  }

  String line = buildEventLine(record);
  eventCache_.valid = false;
  return appendLine(Config::SPOOL_EVENTS_PATH, line, eventSyncedOffset_,
                    Config::MAX_EVENTS_SPOOL_BYTES, markSyncedIfContiguous);
}

bool SpoolManager::appendPortableTelemetry(const TelemetrySpoolRecord& record) {
  if (!ready_) {
    return false;
  }

  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->appendPortableTelemetry(record)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("appendPortableTelemetry");
  }

  String line = buildTelemetryLine(record);
  portableBatchCount_ = 0;
  return appendLine(Config::SPOOL_PORTABLE_TELEMETRY_PATH, line, portableTelemetrySyncedOffset_,
                    Config::MAX_PORTABLE_TELEMETRY_SPOOL_BYTES, false);
}

bool SpoolManager::peekNextTelemetry(TelemetrySpoolRecord& record) {
  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->peekNextTelemetry(record)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("peekNextTelemetry");
  }

  if (!ready_) {
    return false;
  }

  while (true) {
    String line;
    size_t nextOffset = 0;
    if (!readNextLine(Config::SPOOL_TELEMETRY_PATH, telemetrySyncedOffset_, line, nextOffset)) {
      telemetryCache_.valid = false;
      return false;
    }
    if (line.length() == 0) {
      if (nextOffset <= telemetrySyncedOffset_) {
        telemetryCache_.valid = false;
        return false;
      }
      telemetrySyncedOffset_ = nextOffset;
      saveState();
      continue;
    }
    if (!parseTelemetryLine(line, record)) {
      telemetrySyncedOffset_ = nextOffset;
      saveState();
      telemetryCache_.valid = false;
      return false;
    }
    telemetryCache_.valid = true;
    telemetryCache_.nextOffset = nextOffset;
    return true;
  }
}

bool SpoolManager::peekNextEvent(EventSpoolRecord& record) {
  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->peekNextEvent(record)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("peekNextEvent");
  }

  if (!ready_) {
    return false;
  }

  while (true) {
    String line;
    size_t nextOffset = 0;
    if (!readNextLine(Config::SPOOL_EVENTS_PATH, eventSyncedOffset_, line, nextOffset)) {
      eventCache_.valid = false;
      return false;
    }
    if (line.length() == 0) {
      if (nextOffset <= eventSyncedOffset_) {
        eventCache_.valid = false;
        return false;
      }
      eventSyncedOffset_ = nextOffset;
      saveState();
      continue;
    }
    if (!parseEventLine(line, record)) {
      eventSyncedOffset_ = nextOffset;
      saveState();
      eventCache_.valid = false;
      return false;
    }
    eventCache_.valid = true;
    eventCache_.nextOffset = nextOffset;
    return true;
  }
}

bool SpoolManager::markTelemetrySynced() {
  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->markTelemetrySynced()) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("markTelemetrySynced");
  }

  if (!ready_ || !telemetryCache_.valid) {
    return false;
  }
  telemetrySyncedOffset_ = telemetryCache_.nextOffset;
  telemetryCache_.valid = false;
  return saveState();
}

bool SpoolManager::markEventSynced() {
  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->markEventSynced()) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("markEventSynced");
  }

  if (!ready_ || !eventCache_.valid) {
    return false;
  }
  eventSyncedOffset_ = eventCache_.nextOffset;
  eventCache_.valid = false;
  return saveState();
}

size_t SpoolManager::loadPortableTelemetryBatch(TelemetrySpoolRecord* records, size_t maxRecords) {
  if (externalSpoolReady_) {
    return externalFlash_->loadPortableTelemetryBatch(records, maxRecords);
  }

  if (!ready_ || records == nullptr || maxRecords == 0) {
    portableBatchCount_ = 0;
    return 0;
  }

  size_t limit = maxRecords;
  if (limit > Config::MAX_PORTABLE_BATCH_SIZE) {
    limit = Config::MAX_PORTABLE_BATCH_SIZE;
  }

  size_t cursor = portableTelemetrySyncedOffset_;
  size_t loaded = 0;
  while (loaded < limit) {
    String line;
    size_t nextOffset = 0;
    if (!readNextLine(Config::SPOOL_PORTABLE_TELEMETRY_PATH, cursor, line, nextOffset)) {
      break;
    }
    cursor = nextOffset;
    if (line.length() == 0) {
      continue;
    }

    TelemetrySpoolRecord parsed;
    if (!parseTelemetryLine(line, parsed)) {
      continue;
    }

    records[loaded] = parsed;
    portableBatchOffsets_[loaded] = nextOffset;
    loaded++;
  }

  portableBatchCount_ = loaded;
  return loaded;
}

bool SpoolManager::markPortableTelemetryBatchSynced(size_t count) {
  if (externalSpoolReady_) {
    for (uint8_t attempt = 0; attempt < kExtFlashRetryCount; ++attempt) {
      if (externalFlash_->markPortableTelemetryBatchSynced(count)) {
        return true;
      }
      yield();
      delay(1);
    }
    disableExternalSpool("markPortableTelemetryBatchSynced");
  }

  if (!ready_) {
    return false;
  }
  if (count == 0) {
    portableBatchCount_ = 0;
    return true;
  }
  if (count > portableBatchCount_) {
    return false;
  }

  portableTelemetrySyncedOffset_ = portableBatchOffsets_[count - 1];
  portableBatchCount_ = 0;
  return saveState();
}

bool SpoolManager::hasPendingTelemetry() const {
  if (externalSpoolReady_) {
    return externalFlash_->hasPendingTelemetry();
  }

  return ready_ && telemetrySyncedOffset_ < fileSize(Config::SPOOL_TELEMETRY_PATH);
}

bool SpoolManager::hasPendingEvents() const {
  if (externalSpoolReady_) {
    return externalFlash_->hasPendingEvents();
  }

  return ready_ && eventSyncedOffset_ < fileSize(Config::SPOOL_EVENTS_PATH);
}

bool SpoolManager::hasPendingPortableTelemetry() const {
  if (externalSpoolReady_) {
    return externalFlash_->hasPendingPortableTelemetry();
  }

  return ready_ && portableTelemetrySyncedOffset_ < fileSize(Config::SPOOL_PORTABLE_TELEMETRY_PATH);
}

size_t SpoolManager::pendingTelemetryBytes() const {
  if (externalSpoolReady_) {
    return externalFlash_->pendingTelemetryBytes();
  }

  size_t size = fileSize(Config::SPOOL_TELEMETRY_PATH);
  return size > telemetrySyncedOffset_ ? size - telemetrySyncedOffset_ : 0;
}

size_t SpoolManager::pendingEventBytes() const {
  if (externalSpoolReady_) {
    return externalFlash_->pendingEventBytes();
  }

  size_t size = fileSize(Config::SPOOL_EVENTS_PATH);
  return size > eventSyncedOffset_ ? size - eventSyncedOffset_ : 0;
}

size_t SpoolManager::pendingPortableTelemetryBytes() const {
  if (externalSpoolReady_) {
    return externalFlash_->pendingPortableTelemetryBytes();
  }

  size_t size = fileSize(Config::SPOOL_PORTABLE_TELEMETRY_PATH);
  return size > portableTelemetrySyncedOffset_ ? size - portableTelemetrySyncedOffset_ : 0;
}

size_t SpoolManager::pendingPortableTelemetryCount(size_t maxScanRecords) const {
  if (externalSpoolReady_) {
    return externalFlash_->pendingPortableTelemetryCount(maxScanRecords);
  }

  if (!ready_ || maxScanRecords == 0) {
    return 0;
  }

  File file = LittleFS.open(Config::SPOOL_PORTABLE_TELEMETRY_PATH, FILE_READ);
  if (!file) {
    return 0;
  }
  if (!file.seek(portableTelemetrySyncedOffset_)) {
    file.close();
    return 0;
  }

  size_t count = 0;
  while (file.available() && count < maxScanRecords) {
    String line = trimLine(file.readStringUntil('\n'));
    if (line.length() == 0) {
      continue;
    }
    TelemetrySpoolRecord parsed;
    if (parseTelemetryLine(line, parsed)) {
      count++;
    }
  }

  file.close();
  return count;
}

bool SpoolManager::ensureLayout() {
  if (!LittleFS.exists(Config::SPOOL_DIR) && !LittleFS.mkdir(Config::SPOOL_DIR)) {
    return false;
  }

  File telemetry = LittleFS.open(Config::SPOOL_TELEMETRY_PATH, FILE_APPEND);
  if (!telemetry) {
    return false;
  }
  telemetry.close();

  File events = LittleFS.open(Config::SPOOL_EVENTS_PATH, FILE_APPEND);
  if (!events) {
    return false;
  }
  events.close();

  File portableTelemetry = LittleFS.open(Config::SPOOL_PORTABLE_TELEMETRY_PATH, FILE_APPEND);
  if (!portableTelemetry) {
    return false;
  }
  portableTelemetry.close();

  return true;
}

bool SpoolManager::loadState() {
  telemetrySyncedOffset_ = 0;
  eventSyncedOffset_ = 0;
  portableTelemetrySyncedOffset_ = 0;

  if (!LittleFS.exists(Config::SPOOL_STATE_PATH)) {
    return saveState();
  }

  File file = LittleFS.open(Config::SPOOL_STATE_PATH, FILE_READ);
  if (!file) {
    return false;
  }

  while (file.available()) {
    String line = trimLine(file.readStringUntil('\n'));
    if (line.length() < 3) {
      continue;
    }
    int comma = line.indexOf(',');
    if (comma <= 0) {
      continue;
    }
    String key = line.substring(0, comma);
    size_t value = static_cast<size_t>(line.substring(comma + 1).toInt());
    if (key == "T") {
      telemetrySyncedOffset_ = value;
    } else if (key == "E") {
      eventSyncedOffset_ = value;
    } else if (key == "P") {
      portableTelemetrySyncedOffset_ = value;
    }
  }
  file.close();

  size_t telemetrySize = fileSize(Config::SPOOL_TELEMETRY_PATH);
  size_t eventSize = fileSize(Config::SPOOL_EVENTS_PATH);
  size_t portableTelemetrySize = fileSize(Config::SPOOL_PORTABLE_TELEMETRY_PATH);
  if (telemetrySyncedOffset_ > telemetrySize) {
    telemetrySyncedOffset_ = telemetrySize;
  }
  if (eventSyncedOffset_ > eventSize) {
    eventSyncedOffset_ = eventSize;
  }
  if (portableTelemetrySyncedOffset_ > portableTelemetrySize) {
    portableTelemetrySyncedOffset_ = portableTelemetrySize;
  }
  return saveState();
}

bool SpoolManager::saveState() {
  File file = LittleFS.open(Config::SPOOL_STATE_PATH, FILE_WRITE);
  if (!file) {
    return false;
  }
  file.printf("T,%u\n", static_cast<unsigned>(telemetrySyncedOffset_));
  file.printf("E,%u\n", static_cast<unsigned>(eventSyncedOffset_));
  file.printf("P,%u\n", static_cast<unsigned>(portableTelemetrySyncedOffset_));
  file.close();
  return true;
}

String SpoolManager::buildTelemetryLine(const TelemetrySpoolRecord& record) const {
  char buffer[80];
  int written = snprintf(buffer, sizeof(buffer), "T,%lu,%lu,%d,%d,%d,%d,%d,%u,%u",
                         static_cast<unsigned long>(record.sequence),
                         static_cast<unsigned long>(record.timestamp), record.temperatureX10,
                         record.humidityX10, record.sampleOk ? 1 : 0, record.rtcOk ? 1 : 0,
                         record.batteryValid ? 1 : 0, static_cast<unsigned>(record.batteryMilliVolt),
                         static_cast<unsigned>(record.batteryPercent));
  if (written <= 0) {
    return String();
  }

  String line;
  line.reserve(static_cast<unsigned>(written));
  line = buffer;
  return line;
}

String SpoolManager::buildEventLine(const EventSpoolRecord& record) const {
  char buffer[64];
  int written = snprintf(buffer, sizeof(buffer), "E,%lu,%lu,%s,%ld,%ld,%lu,%d,%d",
                         static_cast<unsigned long>(record.sequence),
                         static_cast<unsigned long>(record.timestamp), record.code,
                         static_cast<long>(record.value), static_cast<long>(record.limit),
                         static_cast<unsigned long>(record.duration), record.recovery ? 1 : 0,
                         record.rtcOk ? 1 : 0);
  if (written <= 0) {
    return String();
  }

  String line;
  line.reserve(static_cast<unsigned>(written));
  line = buffer;
  return line;
}

bool SpoolManager::appendLine(const char* path, const String& line, size_t& syncedOffset,
                              size_t maxBytes, bool markSyncedIfContiguous) {
  if (!compactIfNeeded(path, syncedOffset, maxBytes)) {
    return false;
  }

  size_t sizeBefore = fileSize(path);
  File file = LittleFS.open(path, FILE_APPEND);
  if (!file) {
    return false;
  }
  file.print(line);
  file.print('\n');
  size_t sizeAfter = file.size();
  file.close();

  if (markSyncedIfContiguous && syncedOffset >= sizeBefore) {
    syncedOffset = sizeAfter;
    if (!saveState()) {
      return false;
    }
  }

  return compactIfNeeded(path, syncedOffset, maxBytes);
}

bool SpoolManager::readNextLine(const char* path, size_t syncedOffset, String& line,
                                size_t& nextOffset) const {
  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  if (!file.seek(syncedOffset)) {
    file.close();
    return false;
  }

  line = trimLine(file.readStringUntil('\n'));
  nextOffset = file.position();
  file.close();
  return nextOffset > syncedOffset;
}

bool SpoolManager::parseTelemetryLine(const String& line, TelemetrySpoolRecord& record) const {
  char type = '\0';
  unsigned long seq = 0;
  unsigned long ts = 0;
  int temp = 0;
  int hum = 0;
  int sampleOk = 0;
  int rtcOk = 0;
  int batteryValid = 0;
  unsigned batteryMv = 0;
  unsigned batteryPct = 0;
  int count = sscanf(line.c_str(), "%c,%lu,%lu,%d,%d,%d,%d,%d,%u,%u", &type, &seq, &ts, &temp,
                     &hum, &sampleOk, &rtcOk, &batteryValid, &batteryMv, &batteryPct);
  if ((count != 7 && count != 10) || type != 'T') {
    return false;
  }

  record.sequence = static_cast<uint32_t>(seq);
  record.timestamp = static_cast<uint32_t>(ts);
  record.temperatureX10 = static_cast<int16_t>(temp);
  record.humidityX10 = static_cast<int16_t>(hum);
  record.sampleOk = sampleOk != 0;
  record.rtcOk = rtcOk != 0;
  if (count == 10) {
    record.batteryValid = batteryValid != 0;
    record.batteryMilliVolt = static_cast<uint16_t>(batteryMv);
    record.batteryPercent = static_cast<uint8_t>(batteryPct > 100 ? 100 : batteryPct);
  }
  return true;
}

bool SpoolManager::parseEventLine(const String& line, EventSpoolRecord& record) const {
  char type = '\0';
  unsigned long seq = 0;
  unsigned long ts = 0;
  char code[8] = {0};
  long value = 0;
  long limit = 0;
  unsigned long duration = 0;
  int recovery = 0;
  int rtcOk = 0;
  int count = sscanf(line.c_str(), "%c,%lu,%lu,%7[^,],%ld,%ld,%lu,%d,%d", &type, &seq, &ts, code,
                     &value, &limit, &duration, &recovery, &rtcOk);
  if (count != 9 || type != 'E') {
    return false;
  }

  record.sequence = static_cast<uint32_t>(seq);
  record.timestamp = static_cast<uint32_t>(ts);
  strncpy(record.code, code, sizeof(record.code) - 1);
  record.value = static_cast<int32_t>(value);
  record.limit = static_cast<int32_t>(limit);
  record.duration = static_cast<uint32_t>(duration);
  record.recovery = recovery != 0;
  record.rtcOk = rtcOk != 0;
  return true;
}

bool SpoolManager::compactIfNeeded(const char* path, size_t& syncedOffset, size_t maxBytes) {
#if defined(WM_DIAG_DISABLE_EXTFLASH_COMPACT)
  (void)path;
  (void)syncedOffset;
  (void)maxBytes;
  return true;
#else
  size_t currentSize = fileSize(path);
  if (currentSize <= maxBytes) {
    return true;
  }

  if (syncedOffset > 0) {
    if (!rewriteFromOffset(path, syncedOffset, syncedOffset)) {
      return false;
    }
    syncedOffset = 0;
    saveState();
    currentSize = fileSize(path);
    if (currentSize <= maxBytes) {
      return true;
    }
  }

  size_t keepFrom = currentSize > maxBytes ? currentSize - maxBytes : 0;
  if (keepFrom > 0) {
    File source = LittleFS.open(path, FILE_READ);
    if (!source) {
      return false;
    }
    if (!source.seek(keepFrom)) {
      source.close();
      return false;
    }
    if (keepFrom > 0) {
      source.readStringUntil('\n');
      keepFrom = source.position();
    }
    source.close();
  }

  if (!rewriteFromOffset(path, syncedOffset, keepFrom)) {
    return false;
  }
  syncedOffset = 0;
  return saveState();
#endif
}

bool SpoolManager::rewriteFromOffset(const char* path, size_t& syncedOffset, size_t copyFromOffset) {
  File source = LittleFS.open(path, FILE_READ);
  if (!source) {
    return false;
  }
  if (!source.seek(copyFromOffset)) {
    source.close();
    return false;
  }

  File dest = LittleFS.open(Config::SPOOL_TEMP_PATH, FILE_WRITE);
  if (!dest) {
    source.close();
    return false;
  }

  while (source.available()) {
    uint8_t buffer[128];
    size_t readBytes = source.read(buffer, sizeof(buffer));
    if (readBytes == 0) {
      break;
    }
    dest.write(buffer, readBytes);
  }

  source.close();
  dest.close();
  LittleFS.remove(path);
  if (!LittleFS.rename(Config::SPOOL_TEMP_PATH, path)) {
    syncedOffset = 0;
    return false;
  }
  return true;
}

size_t SpoolManager::fileSize(const char* path) const {
  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}
