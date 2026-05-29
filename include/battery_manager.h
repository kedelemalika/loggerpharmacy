#pragma once

#include <Arduino.h>

struct BatteryReading {
  bool valid = false;
  uint16_t millivolt = 0;
  uint8_t percent = 0;
  uint16_t rawAdc = 0;
};

class BatteryManager {
 public:
  bool begin();
  bool read(BatteryReading& outReading);
  bool isAvailable() const { return available_; }

 private:
  uint8_t clampPercentFromMilliVolt(uint16_t millivolt) const;

  bool available_ = false;
  uint8_t sampleCount_ = 0;
  uint16_t adcSamples_[4] = {0, 0, 0, 0};
  uint8_t sampleIndex_ = 0;
};
