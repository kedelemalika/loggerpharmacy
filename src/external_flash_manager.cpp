#include "external_flash_manager.h"

#include <cstring>

#include "pins.h"

namespace {
constexpr uint32_t kSpiHz = Config::EXT_FLASH_SPI_HZ;
constexpr uint32_t kSectorSize = 4096;
constexpr uint32_t kTelemetryMagic = 0x54454C31;  // TEL1
constexpr uint32_t kEventMagic = 0x45565431;      // EVT1
constexpr uint32_t kPortableMagic = 0x50525431;   // PRT1
constexpr uint32_t kFlashReadCmd = 0x03;
constexpr uint32_t kFlashWriteEnableCmd = 0x06;
constexpr uint32_t kFlashSectorEraseCmd = 0x20;
constexpr uint32_t kFlashPageProgramCmd = 0x02;
constexpr uint32_t kFlashReadStatusCmd = 0x05;
constexpr uint8_t kStatusBusyMask = 0x01;
constexpr uint32_t kPageSize = 256;
constexpr uint32_t kTimeoutMs = 3000;

constexpr uint32_t kTelemetryBase = 0;
constexpr uint32_t kEventBase = Config::MAX_EXT_TELEMETRY_SPOOL_BYTES;
constexpr uint32_t kPortableBase = Config::MAX_EXT_TELEMETRY_SPOOL_BYTES + Config::MAX_EXT_EVENTS_SPOOL_BYTES;

template <typename T>
struct Slot {
  uint32_t magic;
  uint32_t seq;
  T payload;
  uint32_t tail;
};

using TelemetrySlot = Slot<TelemetrySpoolRecord>;
using EventSlot = Slot<EventSpoolRecord>;

uint32_t addrFor(uint32_t base, size_t capacity, uint32_t seq) {
  if (capacity == 0 || seq == 0) {
    return base;
  }
  size_t index = static_cast<size_t>((seq - 1) % static_cast<uint32_t>(capacity));
  return base + static_cast<uint32_t>(index) * kSectorSize;
}

void writeEnable(SPIClass& spi) {
  spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(Pins::EXT_FLASH_CS, LOW);
  spi.transfer(kFlashWriteEnableCmd);
  digitalWrite(Pins::EXT_FLASH_CS, HIGH);
  spi.endTransaction();
}

uint8_t readStatus(SPIClass& spi) {
  spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(Pins::EXT_FLASH_CS, LOW);
  spi.transfer(kFlashReadStatusCmd);
  uint8_t status = spi.transfer(0x00);
  digitalWrite(Pins::EXT_FLASH_CS, HIGH);
  spi.endTransaction();
  return status;
}

bool waitReady(SPIClass& spi) {
  uint32_t start = millis();
  while ((millis() - start) < kTimeoutMs) {
    if ((readStatus(spi) & kStatusBusyMask) == 0) {
      return true;
    }
    yield();
    delay(1);
  }
  return false;
}

bool eraseSector(SPIClass& spi, uint32_t address) {
  writeEnable(spi);
  spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(Pins::EXT_FLASH_CS, LOW);
  spi.transfer(kFlashSectorEraseCmd);
  spi.transfer(static_cast<uint8_t>((address >> 16) & 0xFF));
  spi.transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
  spi.transfer(static_cast<uint8_t>(address & 0xFF));
  digitalWrite(Pins::EXT_FLASH_CS, HIGH);
  spi.endTransaction();
  return waitReady(spi);
}

bool pageProgram(SPIClass& spi, uint32_t address, const uint8_t* data, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    size_t chunk = len - offset;
    if (chunk > kPageSize) {
      chunk = kPageSize;
    }
    writeEnable(spi);
    spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(Pins::EXT_FLASH_CS, LOW);
    uint32_t pageAddr = address + static_cast<uint32_t>(offset);
    spi.transfer(kFlashPageProgramCmd);
    spi.transfer(static_cast<uint8_t>((pageAddr >> 16) & 0xFF));
    spi.transfer(static_cast<uint8_t>((pageAddr >> 8) & 0xFF));
    spi.transfer(static_cast<uint8_t>(pageAddr & 0xFF));
    for (size_t i = 0; i < chunk; ++i) {
      spi.transfer(data[offset + i]);
    }
    digitalWrite(Pins::EXT_FLASH_CS, HIGH);
    spi.endTransaction();
    if (!waitReady(spi)) {
      return false;
    }
    offset += chunk;
    yield();
  }
  return true;
}

bool readBytes(SPIClass& spi, uint32_t address, uint8_t* data, size_t len) {
  spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(Pins::EXT_FLASH_CS, LOW);
  spi.transfer(kFlashReadCmd);
  spi.transfer(static_cast<uint8_t>((address >> 16) & 0xFF));
  spi.transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
  spi.transfer(static_cast<uint8_t>(address & 0xFF));
  for (size_t i = 0; i < len; ++i) {
    data[i] = spi.transfer(0x00);
  }
  digitalWrite(Pins::EXT_FLASH_CS, HIGH);
  spi.endTransaction();
  return true;
}

template <typename T>
size_t pendingCount(const T& state) {
  if (state.writeSeq <= state.syncedSeq) {
    return 0;
  }
  return static_cast<size_t>(state.writeSeq - state.syncedSeq);
}
}  // namespace

bool ExternalFlashManager::begin() {
  telemetryCache_.valid = false;
  eventCache_.valid = false;
  portableBatchCount_ = 0;

  telemetryCapacity_ = Config::MAX_EXT_TELEMETRY_SPOOL_BYTES / kSectorSize;
  eventCapacity_ = Config::MAX_EXT_EVENTS_SPOOL_BYTES / kSectorSize;
  portableCapacity_ = Config::MAX_EXT_PORTABLE_TELEMETRY_SPOOL_BYTES / kSectorSize;

  if (telemetryCapacity_ == 0 || eventCapacity_ == 0 || portableCapacity_ == 0) {
    Serial.println("[EXTFLASH] invalid extflash capacity");
    ready_ = false;
    return false;
  }

  nvsReady_ = prefs_.begin(Config::NVS_NAMESPACE, false);
  pinMode(Pins::EXT_FLASH_CS, OUTPUT);
  digitalWrite(Pins::EXT_FLASH_CS, HIGH);
  spiBus_.begin(Pins::EXT_FLASH_SCK, Pins::EXT_FLASH_MISO, Pins::EXT_FLASH_MOSI, Pins::EXT_FLASH_CS);

  uint8_t probe[3] = {0};
  if (!readBytes(spiBus_, 0, probe, sizeof(probe))) {
    Serial.println("[EXTFLASH] raw probe read failed");
    ready_ = false;
    return false;
  }

  if (!loadState()) {
    ready_ = false;
    return false;
  }

  ready_ = true;
  Serial.printf("[EXTFLASH] Raw queue ready (cap T=%u E=%u P=%u)\n", static_cast<unsigned>(telemetryCapacity_),
                static_cast<unsigned>(eventCapacity_), static_cast<unsigned>(portableCapacity_));
  return true;
}

bool ExternalFlashManager::isReady() const { return ready_; }

bool ExternalFlashManager::loadState() {
  telemetryState_.writeSeq = nvsReady_ ? prefs_.getUInt("ex_t_w", 0) : 0;
  telemetryState_.syncedSeq = nvsReady_ ? prefs_.getUInt("ex_t_s", 0) : 0;
  eventState_.writeSeq = nvsReady_ ? prefs_.getUInt("ex_e_w", 0) : 0;
  eventState_.syncedSeq = nvsReady_ ? prefs_.getUInt("ex_e_s", 0) : 0;
  portableState_.writeSeq = nvsReady_ ? prefs_.getUInt("ex_p_w", 0) : 0;
  portableState_.syncedSeq = nvsReady_ ? prefs_.getUInt("ex_p_s", 0) : 0;

  if (telemetryState_.syncedSeq > telemetryState_.writeSeq) telemetryState_.syncedSeq = telemetryState_.writeSeq;
  if (eventState_.syncedSeq > eventState_.writeSeq) eventState_.syncedSeq = eventState_.writeSeq;
  if (portableState_.syncedSeq > portableState_.writeSeq) portableState_.syncedSeq = portableState_.writeSeq;

  if (pendingCount(telemetryState_) > telemetryCapacity_) {
    telemetryState_.syncedSeq = telemetryState_.writeSeq - static_cast<uint32_t>(telemetryCapacity_);
  }
  if (pendingCount(eventState_) > eventCapacity_) {
    eventState_.syncedSeq = eventState_.writeSeq - static_cast<uint32_t>(eventCapacity_);
  }
  if (pendingCount(portableState_) > portableCapacity_) {
    portableState_.syncedSeq = portableState_.writeSeq - static_cast<uint32_t>(portableCapacity_);
  }

  return saveState();
}

bool ExternalFlashManager::saveState() {
  if (!nvsReady_) {
    return true;
  }
  prefs_.putUInt("ex_t_w", telemetryState_.writeSeq);
  prefs_.putUInt("ex_t_s", telemetryState_.syncedSeq);
  prefs_.putUInt("ex_e_w", eventState_.writeSeq);
  prefs_.putUInt("ex_e_s", eventState_.syncedSeq);
  prefs_.putUInt("ex_p_w", portableState_.writeSeq);
  prefs_.putUInt("ex_p_s", portableState_.syncedSeq);
  return true;
}

bool ExternalFlashManager::flashRead(uint32_t address, uint8_t* buffer, size_t len) const {
  return readBytes(const_cast<SPIClass&>(spiBus_), address, buffer, len);
}

bool ExternalFlashManager::flashWriteSector(uint32_t address, const uint8_t* data, size_t len) {
  if (len > kSectorSize) {
    return false;
  }
  uint8_t page[kSectorSize];
  memset(page, 0xFF, sizeof(page));
  memcpy(page, data, len);
  if (!eraseSector(spiBus_, address)) {
    return false;
  }
  return pageProgram(spiBus_, address, page, sizeof(page));
}

bool ExternalFlashManager::appendTelemetryInternal(const TelemetrySpoolRecord& record,
                                                   bool markSyncedIfContiguous) {
  TelemetrySlot slot{};
  slot.magic = kTelemetryMagic;
  slot.seq = telemetryState_.writeSeq + 1;
  slot.payload = record;
  slot.tail = kTelemetryMagic ^ slot.seq;

  uint32_t address = addrFor(kTelemetryBase, telemetryCapacity_, slot.seq);
  if (!flashWriteSector(address, reinterpret_cast<const uint8_t*>(&slot), sizeof(slot))) {
    return false;
  }

  uint32_t prevWrite = telemetryState_.writeSeq;
  telemetryState_.writeSeq = slot.seq;
  if (markSyncedIfContiguous && telemetryState_.syncedSeq >= prevWrite) {
    telemetryState_.syncedSeq = telemetryState_.writeSeq;
  }
  if (pendingCount(telemetryState_) > telemetryCapacity_) {
    telemetryState_.syncedSeq = telemetryState_.writeSeq - static_cast<uint32_t>(telemetryCapacity_);
  }
  telemetryCache_.valid = false;
  return saveState();
}

bool ExternalFlashManager::appendEventInternal(const EventSpoolRecord& record,
                                               bool markSyncedIfContiguous) {
  EventSlot slot{};
  slot.magic = kEventMagic;
  slot.seq = eventState_.writeSeq + 1;
  slot.payload = record;
  slot.tail = kEventMagic ^ slot.seq;

  uint32_t address = addrFor(kEventBase, eventCapacity_, slot.seq);
  if (!flashWriteSector(address, reinterpret_cast<const uint8_t*>(&slot), sizeof(slot))) {
    return false;
  }

  uint32_t prevWrite = eventState_.writeSeq;
  eventState_.writeSeq = slot.seq;
  if (markSyncedIfContiguous && eventState_.syncedSeq >= prevWrite) {
    eventState_.syncedSeq = eventState_.writeSeq;
  }
  if (pendingCount(eventState_) > eventCapacity_) {
    eventState_.syncedSeq = eventState_.writeSeq - static_cast<uint32_t>(eventCapacity_);
  }
  eventCache_.valid = false;
  return saveState();
}

bool ExternalFlashManager::appendPortableInternal(const TelemetrySpoolRecord& record) {
  TelemetrySlot slot{};
  slot.magic = kPortableMagic;
  slot.seq = portableState_.writeSeq + 1;
  slot.payload = record;
  slot.tail = kPortableMagic ^ slot.seq;

  uint32_t address = addrFor(kPortableBase, portableCapacity_, slot.seq);
  if (!flashWriteSector(address, reinterpret_cast<const uint8_t*>(&slot), sizeof(slot))) {
    return false;
  }

  portableState_.writeSeq = slot.seq;
  if (pendingCount(portableState_) > portableCapacity_) {
    portableState_.syncedSeq = portableState_.writeSeq - static_cast<uint32_t>(portableCapacity_);
  }
  portableBatchCount_ = 0;
  return saveState();
}

bool ExternalFlashManager::appendTelemetry(const TelemetrySpoolRecord& record,
                                           bool markSyncedIfContiguous) {
  return ready_ && appendTelemetryInternal(record, markSyncedIfContiguous);
}

bool ExternalFlashManager::appendEvent(const EventSpoolRecord& record, bool markSyncedIfContiguous) {
  return ready_ && appendEventInternal(record, markSyncedIfContiguous);
}

bool ExternalFlashManager::appendPortableTelemetry(const TelemetrySpoolRecord& record) {
  return ready_ && appendPortableInternal(record);
}

bool ExternalFlashManager::readTelemetrySeq(uint32_t seq, TelemetrySpoolRecord& record) const {
  TelemetrySlot slot{};
  uint32_t address = addrFor(kTelemetryBase, telemetryCapacity_, seq);
  if (!flashRead(address, reinterpret_cast<uint8_t*>(&slot), sizeof(slot))) {
    Serial.printf("[EXTFLASH] readTelemetrySeq failed seq=%lu addr=0x%06lX\n",
                  static_cast<unsigned long>(seq), static_cast<unsigned long>(address));
    return false;
  }
  if (slot.magic != kTelemetryMagic || slot.seq != seq || slot.tail != (kTelemetryMagic ^ seq)) {
    Serial.printf(
        "[EXTFLASH] readTelemetrySeq invalid slot seq=%lu addr=0x%06lX "
        "magic=0x%08lX slotSeq=%lu tail=0x%08lX expectTail=0x%08lX\n",
        static_cast<unsigned long>(seq), static_cast<unsigned long>(address),
        static_cast<unsigned long>(slot.magic), static_cast<unsigned long>(slot.seq),
        static_cast<unsigned long>(slot.tail), static_cast<unsigned long>(kTelemetryMagic ^ seq));
    return false;
  }
  record = slot.payload;
  return true;
}

bool ExternalFlashManager::readEventSeq(uint32_t seq, EventSpoolRecord& record) const {
  EventSlot slot{};
  uint32_t address = addrFor(kEventBase, eventCapacity_, seq);
  if (!flashRead(address, reinterpret_cast<uint8_t*>(&slot), sizeof(slot))) {
    Serial.printf("[EXTFLASH] readEventSeq failed seq=%lu addr=0x%06lX\n",
                  static_cast<unsigned long>(seq), static_cast<unsigned long>(address));
    return false;
  }
  if (slot.magic != kEventMagic || slot.seq != seq || slot.tail != (kEventMagic ^ seq)) {
    Serial.printf(
        "[EXTFLASH] readEventSeq invalid slot seq=%lu addr=0x%06lX "
        "magic=0x%08lX slotSeq=%lu tail=0x%08lX expectTail=0x%08lX\n",
        static_cast<unsigned long>(seq), static_cast<unsigned long>(address),
        static_cast<unsigned long>(slot.magic), static_cast<unsigned long>(slot.seq),
        static_cast<unsigned long>(slot.tail), static_cast<unsigned long>(kEventMagic ^ seq));
    return false;
  }
  record = slot.payload;
  return true;
}

bool ExternalFlashManager::readPortableSeq(uint32_t seq, TelemetrySpoolRecord& record) const {
  TelemetrySlot slot{};
  uint32_t address = addrFor(kPortableBase, portableCapacity_, seq);
  if (!flashRead(address, reinterpret_cast<uint8_t*>(&slot), sizeof(slot))) {
    Serial.printf("[EXTFLASH] readPortableSeq failed seq=%lu addr=0x%06lX\n",
                  static_cast<unsigned long>(seq), static_cast<unsigned long>(address));
    return false;
  }
  if (slot.magic != kPortableMagic || slot.seq != seq || slot.tail != (kPortableMagic ^ seq)) {
    Serial.printf(
        "[EXTFLASH] readPortableSeq invalid slot seq=%lu addr=0x%06lX "
        "magic=0x%08lX slotSeq=%lu tail=0x%08lX expectTail=0x%08lX\n",
        static_cast<unsigned long>(seq), static_cast<unsigned long>(address),
        static_cast<unsigned long>(slot.magic), static_cast<unsigned long>(slot.seq),
        static_cast<unsigned long>(slot.tail), static_cast<unsigned long>(kPortableMagic ^ seq));
    return false;
  }
  record = slot.payload;
  return true;
}

bool ExternalFlashManager::peekNextTelemetry(TelemetrySpoolRecord& record) {
  if (!ready_) {
    return false;
  }
  while (telemetryState_.syncedSeq < telemetryState_.writeSeq) {
    uint32_t next = telemetryState_.syncedSeq + 1;
    if (readTelemetrySeq(next, record)) {
      telemetryCache_.valid = true;
      telemetryCache_.nextSeq = next;
      return true;
    }
    telemetryState_.syncedSeq = next;
    saveState();
    yield();
  }
  telemetryCache_.valid = false;
  return false;
}

bool ExternalFlashManager::peekNextEvent(EventSpoolRecord& record) {
  if (!ready_) {
    return false;
  }
  while (eventState_.syncedSeq < eventState_.writeSeq) {
    uint32_t next = eventState_.syncedSeq + 1;
    if (readEventSeq(next, record)) {
      eventCache_.valid = true;
      eventCache_.nextSeq = next;
      return true;
    }
    eventState_.syncedSeq = next;
    saveState();
    yield();
  }
  eventCache_.valid = false;
  return false;
}

size_t ExternalFlashManager::loadPortableTelemetryBatch(TelemetrySpoolRecord* records, size_t maxRecords) {
  if (!ready_ || records == nullptr || maxRecords == 0) {
    portableBatchCount_ = 0;
    return 0;
  }

  size_t limit = maxRecords;
  if (limit > Config::MAX_PORTABLE_BATCH_SIZE) {
    limit = Config::MAX_PORTABLE_BATCH_SIZE;
  }

  size_t loaded = 0;
  uint32_t seq = portableState_.syncedSeq;
  while (loaded < limit && seq < portableState_.writeSeq) {
    ++seq;
    if (readPortableSeq(seq, records[loaded])) {
      portableBatchSeq_[loaded] = seq;
      ++loaded;
    } else {
      portableState_.syncedSeq = seq;
      saveState();
    }
    yield();
  }

  portableBatchCount_ = loaded;
  return loaded;
}

bool ExternalFlashManager::markTelemetrySynced() {
  if (!ready_ || !telemetryCache_.valid) {
    return false;
  }
  telemetryState_.syncedSeq = telemetryCache_.nextSeq;
  telemetryCache_.valid = false;
  return saveState();
}

bool ExternalFlashManager::markEventSynced() {
  if (!ready_ || !eventCache_.valid) {
    return false;
  }
  eventState_.syncedSeq = eventCache_.nextSeq;
  eventCache_.valid = false;
  return saveState();
}

bool ExternalFlashManager::markPortableTelemetryBatchSynced(size_t count) {
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
  portableState_.syncedSeq = portableBatchSeq_[count - 1];
  portableBatchCount_ = 0;
  return saveState();
}

bool ExternalFlashManager::hasPendingTelemetry() const { return ready_ && telemetryState_.syncedSeq < telemetryState_.writeSeq; }

bool ExternalFlashManager::hasPendingEvents() const { return ready_ && eventState_.syncedSeq < eventState_.writeSeq; }

bool ExternalFlashManager::hasPendingPortableTelemetry() const {
  return ready_ && portableState_.syncedSeq < portableState_.writeSeq;
}

size_t ExternalFlashManager::pendingTelemetryBytes() const {
  return pendingCount(telemetryState_) * sizeof(TelemetrySpoolRecord);
}

size_t ExternalFlashManager::pendingEventBytes() const {
  return pendingCount(eventState_) * sizeof(EventSpoolRecord);
}

size_t ExternalFlashManager::pendingPortableTelemetryBytes() const {
  return pendingCount(portableState_) * sizeof(TelemetrySpoolRecord);
}

size_t ExternalFlashManager::pendingPortableTelemetryCount(size_t maxScanRecords) const {
  size_t count = pendingCount(portableState_);
  if (count > maxScanRecords) {
    count = maxScanRecords;
  }
  return count;
}
