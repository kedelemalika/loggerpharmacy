#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "types.h"

class StorageManager {
 public:
  bool begin();
  void loadConfig(RuntimeConfig& config);
  void saveConfig(const RuntimeConfig& config);
  void saveThresholdsAndIntervals(const RuntimeConfig& config);
  void normalizeConfig(RuntimeConfig& config) const;
  uint32_t incrementBootCount();
  uint32_t nextSequence();
  uint32_t currentSequence() const;
  void addEvent(const EventRecord& event);
  size_t eventCount() const;
  const EventRecord* events() const;

 private:
  String sanitize(const String& value, const String& fallback) const;

  Preferences prefs_;
  bool ready_ = false;
  uint32_t cachedSequence_ = 0;
  uint32_t reservedSequenceLimit_ = 0;
  EventRecord eventLog_[Config::EVENT_LOG_CAPACITY];
  size_t eventHead_ = 0;
  size_t eventCount_ = 0;
};
