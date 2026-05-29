#include "button_manager.h"

#include "config.h"

ButtonManager::ButtonManager(uint8_t pin) : pin_(pin) {}

void ButtonManager::begin() {
  pinMode(pin_, INPUT_PULLUP);
  stableState_ = digitalRead(pin_);
  lastReading_ = stableState_;
  lastDebounceMs_ = millis();
}

bool ButtonManager::isHeldAtBoot(uint32_t holdMs) const {
  if (digitalRead(pin_) != LOW) {
    return false;
  }

  uint32_t start = millis();
  while (millis() - start < holdMs) {
    if (digitalRead(pin_) != LOW) {
      return false;
    }
    delay(10);
  }
  return true;
}

ButtonEvent ButtonManager::update(uint32_t nowMs) {
  bool reading = digitalRead(pin_);
  if (reading != lastReading_) {
    lastDebounceMs_ = nowMs;
    lastReading_ = reading;
  }

  if ((nowMs - lastDebounceMs_) < Config::BUTTON_DEBOUNCE_MS) {
    return ButtonEvent::None;
  }

  if (reading != stableState_) {
    stableState_ = reading;
    if (stableState_ == LOW) {
      pressedAtMs_ = nowMs;
      longPressSent_ = false;
    } else {
      if (!longPressSent_ && pressedAtMs_ != 0 &&
          (nowMs - pressedAtMs_) >= Config::BUTTON_DEBOUNCE_MS) {
        pressedAtMs_ = 0;
        return ButtonEvent::ShortPress;
      }
      pressedAtMs_ = 0;
    }
  }

  if (stableState_ == LOW && !longPressSent_ && pressedAtMs_ != 0 &&
      (nowMs - pressedAtMs_) >= Config::BUTTON_LONG_PRESS_MS) {
    longPressSent_ = true;
    return ButtonEvent::LongPress;
  }

  return ButtonEvent::None;
}
