#include "battery_manager.h"

#include "pins.h"

#ifndef WM_BATTERY_ENABLED
#define WM_BATTERY_ENABLED 0
#endif

namespace {
#if defined(WM_BATTERY_ADC_PIN)
constexpr uint8_t kBatteryAdcPin = static_cast<uint8_t>(WM_BATTERY_ADC_PIN);
#else
constexpr uint8_t kBatteryAdcPin = Pins::BATTERY_ADC;
#endif

#if defined(WM_BATTERY_DIVIDER_NUM)
constexpr uint32_t kDividerNum = static_cast<uint32_t>(WM_BATTERY_DIVIDER_NUM);
#else
constexpr uint32_t kDividerNum = 2;
#endif

#if defined(WM_BATTERY_DIVIDER_DEN)
constexpr uint32_t kDividerDen = static_cast<uint32_t>(WM_BATTERY_DIVIDER_DEN);
#else
constexpr uint32_t kDividerDen = 1;
#endif

constexpr uint16_t kAdcMax = 4095;
constexpr uint16_t kAdcMilliVoltRef = 3300;
constexpr uint16_t kBatteryMinMilliVolt = 3200;
constexpr uint16_t kBatteryMaxMilliVolt = 4200;
}  // namespace

bool BatteryManager::begin() {
#if WM_BATTERY_ENABLED
  pinMode(kBatteryAdcPin, INPUT);
  analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
  available_ = true;
  return true;
#else
  available_ = false;
  return false;
#endif
}

bool BatteryManager::read(BatteryReading& outReading) {
  outReading = BatteryReading();
#if WM_BATTERY_ENABLED
  if (!available_) {
    return false;
  }

  uint32_t raw = analogRead(kBatteryAdcPin);
  if (raw > kAdcMax) {
    raw = kAdcMax;
  }

  adcSamples_[sampleIndex_] = static_cast<uint16_t>(raw);
  sampleIndex_ = (sampleIndex_ + 1) % 4;
  if (sampleCount_ < 4) {
    sampleCount_++;
  }

  uint32_t sum = 0;
  for (uint8_t i = 0; i < sampleCount_; ++i) {
    sum += adcSamples_[i];
  }
  uint32_t avgRaw = sampleCount_ > 0 ? (sum / sampleCount_) : raw;
  uint32_t pinMilliVolt = (avgRaw * kAdcMilliVoltRef) / kAdcMax;
  uint32_t batteryMilliVolt = (pinMilliVolt * kDividerNum) / kDividerDen;

  outReading.valid = true;
  outReading.rawAdc = static_cast<uint16_t>(avgRaw);
  outReading.millivolt = static_cast<uint16_t>(batteryMilliVolt);
  outReading.percent = clampPercentFromMilliVolt(outReading.millivolt);
  return true;
#else
  return false;
#endif
}

uint8_t BatteryManager::clampPercentFromMilliVolt(uint16_t millivolt) const {
  if (millivolt <= kBatteryMinMilliVolt) {
    return 0;
  }
  if (millivolt >= kBatteryMaxMilliVolt) {
    return 100;
  }
  uint32_t span = static_cast<uint32_t>(kBatteryMaxMilliVolt - kBatteryMinMilliVolt);
  uint32_t value = static_cast<uint32_t>(millivolt - kBatteryMinMilliVolt);
  return static_cast<uint8_t>((value * 100UL) / span);
}
