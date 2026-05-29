#pragma once

#include <Arduino.h>

#include "types.h"

class ButtonManager {
 public:
  explicit ButtonManager(uint8_t pin);

  void begin();
  bool isHeldAtBoot(uint32_t holdMs) const;
  ButtonEvent update(uint32_t nowMs);

 private:
  uint8_t pin_;
  bool stableState_ = true;
  bool lastReading_ = true;
  uint32_t lastDebounceMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  bool longPressSent_ = false;
};
