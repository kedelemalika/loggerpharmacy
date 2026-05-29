#pragma once

// sensor_interface.h provides:
//   - ISensor pure interface
//   - WM_SENSOR_TYPE_* compile-time guards
//   - Board-sensor compatibility matrix with STRICT/ALLOW_EXPERIMENTAL policy
#include "sensor_interface.h"
#include "types.h"

// Forward declaration — defined in sensor_driver.cpp
// when WM_SENSOR_TYPE_* is active. Only one definition exists per build.
ISensor* createSensorInstance();

// =============================================================================
// SensorManager — high-level sensor abstraction layer.
//
// Wraps an ISensor instance (created by createSensorInstance) and adds:
//   - Moving average over last 3 samples
//   - Runtime error tracking
//   - Recovery triggers (I2C bus reinit / sensor reinit)
//   - Calibration via offset (applied after moving average)
//
// Application code should interact with SensorManager, not directly with ISensor.
// =============================================================================
class SensorManager {
 public:
  // Initialize sensor. Returns true if sensor detected at I2C address.
  bool begin();

  // Read a single sample, apply moving average and calibration.
  // Returns true if a valid reading was produced.
  bool read(SensorReading& outReading);

  // True if the most recent read produced valid data.
  bool isAvailable() const { return available_; }

  // True if sensor responded to I2C probe at last read attempt.
  bool isPresent() const { return present_; }

  // True if any read has failed since last successful sample.
  bool hasRuntimeError() const { return runtimeError_; }

  // Return sensor type name string (e.g. "SHT20", "AHT10").
  const char* sensorName() const;

  // Force sensor reset: optionally reinitialize I2C bus first.
  void forceRecovery(bool reinitBus);

 private:
  int16_t scaleX10(float value) const;
  float movingAverage(const float* values, uint8_t count) const;

  ISensor* sensor_ = nullptr;
  bool available_ = false;
  bool present_ = false;
  bool runtimeError_ = false;
  bool lastSampleOk_ = false;
  bool hadLoggedFailure_ = false;
  uint32_t lastGoodMs_ = 0;
  float tempSamples_[3] = {0.0f, 0.0f, 0.0f};
  float humSamples_[3] = {0.0f, 0.0f, 0.0f};
  uint8_t sampleIndex_ = 0;
  uint8_t validSamples_ = 0;
  uint32_t sampleCount_ = 0;
};