#include "sensor_interface.h"

#include <Wire.h>

#include "config.h"

// =============================================================================
// Sensor driver source file.
//
// Each WM_SENSOR_TYPE_* macro activates exactly ONE driver below.
// The inactive drivers compile to empty translation units (header guards
// ensure their class definitions are skipped entirely).
//
// Adding a new sensor: add a new #elif block with its driver class and
// update the createSensorInstance() function at the bottom.
// =============================================================================

#if defined(WM_SENSOR_TYPE_SHT20)
// ---------------------------------------------------------------------------
// SHT20 Driver — raw I2C implementation for stability on ESP32.
// DFRobot_SHT20 library is available but not used because it can return
// plausible stale values when Wire reports errors.
// ---------------------------------------------------------------------------
namespace SHT20Private {
constexpr uint8_t kAddr = 0x40;
constexpr uint8_t kCmdTemp = 0xF3;
constexpr uint8_t kCmdHum = 0xF5;
constexpr uint8_t kCmdReset = 0xFE;
constexpr uint16_t kTempWaitMs = 90;
constexpr uint16_t kHumWaitMs = 35;
}  // namespace SHT20Private

class SHT20Driver : public ISensor {
 public:
  explicit SHT20Driver() : ISensor("SHT20", SHT20Private::kAddr) {}

  bool begin() override {
    Wire.beginTransmission(SHT20Private::kAddr);
    Wire.write(SHT20Private::kCmdReset);
    if (Wire.endTransmission() != 0) return false;
    delay(20);
    return isPresent();
  }

  bool read(float& temperature, float& humidity) override {
    if (!readMeasurement(SHT20Private::kCmdTemp, SHT20Private::kTempWaitMs,
                          temperature, false)) {
      return false;
    }
    delay(30);
    if (!readMeasurement(SHT20Private::kCmdHum, SHT20Private::kHumWaitMs,
                          humidity, true)) {
      return false;
    }
    return validate(temperature, humidity);
  }

  bool isPresent() const override {
    Wire.beginTransmission(SHT20Private::kAddr);
    return Wire.endTransmission() == 0;
  }

  const char* name() const override { return "SHT20"; }

 private:
  bool readMeasurement(uint8_t cmd, uint16_t waitMs, float& val, bool isHum) {
    Wire.beginTransmission(SHT20Private::kAddr);
    Wire.write(cmd);
    if (Wire.endTransmission() != 0) return false;
    delay(waitMs);
    uint8_t n = Wire.requestFrom(static_cast<int>(SHT20Private::kAddr), 3);
    if (n != 3) return false;
    uint8_t data[2];
    data[0] = Wire.read();
    data[1] = Wire.read();
    uint8_t crc = Wire.read();
    if (crc8(data, 2) != crc) return false;
    uint16_t raw = (static_cast<uint16_t>(data[0]) << 8 | data[1]) & 0xFFFC;
    val = isHum ? (-6.0f + 125.0f * static_cast<float>(raw) / 65536.0f)
                : (-46.85f + 175.72f * static_cast<float>(raw) / 65536.0f);
    return true;
  }

  bool validate(float t, float h) const {
    return isfinite(t) && isfinite(h) && t > -40.0f && t < 125.0f &&
           h >= 0.0f && h <= 100.0f;
  }

  uint8_t crc8(const uint8_t* d, uint8_t len) const {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; ++i) {
      crc ^= d[i];
      for (uint8_t b = 0; b < 8; ++b) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : (crc << 1);
      }
    }
    return crc;
  }
};

#elif defined(WM_SENSOR_TYPE_AHT10)
// ---------------------------------------------------------------------------
// AHT10 Driver — Adafruit_AHTX0 library implementation.
// ---------------------------------------------------------------------------
#include <Adafruit_AHTX0.h>
#include <Adafruit_Sensor.h>

namespace AHT10Private {
constexpr uint8_t kAddr = 0x38;
}  // namespace AHT10Private

class AHT10Driver : public ISensor {
 public:
  explicit AHT10Driver() : ISensor("AHT10", AHT10Private::kAddr), ok_(false) {}

  bool begin() override {
    ok_ = aht10_.begin();
    return ok_;
  }

  bool read(float& temperature, float& humidity) override {
    if (!ok_) return false;
    sensors_event_t humEvt, tempEvt;
    aht10_.getEvent(&humEvt, &tempEvt);
    temperature = tempEvt.temperature;
    humidity = humEvt.relative_humidity;
    return validate(temperature, humidity);
  }

  bool isPresent() const override {
    Wire.beginTransmission(AHT10Private::kAddr);
    return Wire.endTransmission() == 0;
  }

  const char* name() const override { return "AHT10"; }

 private:
  bool validate(float t, float h) const {
    return isfinite(t) && isfinite(h) && t > -40.0f && t < 125.0f &&
           h >= 0.0f && h <= 100.0f;
  }

  bool ok_;
  Adafruit_AHTX0 aht10_;
};

#elif defined(WM_SENSOR_TYPE_HTU21)
// ---------------------------------------------------------------------------
// HTU21 Driver — raw I2C implementation (HTU21D-compatible).
// ---------------------------------------------------------------------------
namespace HTU21Private {
constexpr uint8_t kAddr = 0x40;
constexpr uint8_t kCmdReset = 0xFE;
constexpr uint8_t kCmdTemp = 0xF3;
constexpr uint8_t kCmdHum = 0xF5;
constexpr uint16_t kTempWaitMs = 60;
constexpr uint16_t kHumWaitMs = 20;
}  // namespace HTU21Private

class HTU21Driver : public ISensor {
 public:
  explicit HTU21Driver() : ISensor("HTU21", HTU21Private::kAddr), ok_(false) {}

  bool begin() override {
    Wire.beginTransmission(HTU21Private::kAddr);
    Wire.write(HTU21Private::kCmdReset);
    if (Wire.endTransmission() != 0) return false;
    delay(15);
    ok_ = isPresent();
    return ok_;
  }

  bool read(float& temperature, float& humidity) override {
    if (!ok_) return false;
    if (!readMeasurement(HTU21Private::kCmdTemp, HTU21Private::kTempWaitMs,
                         temperature, false)) {
      return false;
    }
    delay(30);
    if (!readMeasurement(HTU21Private::kCmdHum, HTU21Private::kHumWaitMs,
                         humidity, true)) {
      return false;
    }
    return validate(temperature, humidity);
  }

  bool isPresent() const override {
    Wire.beginTransmission(HTU21Private::kAddr);
    return Wire.endTransmission() == 0;
  }

  const char* name() const override { return "HTU21"; }

 private:
  bool readMeasurement(uint8_t cmd, uint16_t waitMs, float& val, bool isHum) {
    Wire.beginTransmission(HTU21Private::kAddr);
    Wire.write(cmd);
    if (Wire.endTransmission() != 0) return false;

    delay(waitMs);
    uint8_t n = Wire.requestFrom(static_cast<int>(HTU21Private::kAddr), 3);
    if (n != 3) return false;

    uint8_t data[2];
    data[0] = Wire.read();
    data[1] = Wire.read();
    uint8_t crc = Wire.read();
    if (crc8(data, 2) != crc) return false;

    uint16_t raw = (static_cast<uint16_t>(data[0]) << 8 | data[1]) & 0xFFFC;
    if (isHum) {
      val = -6.0f + 125.0f * static_cast<float>(raw) / 65536.0f;
    } else {
      val = -46.85f + 175.72f * static_cast<float>(raw) / 65536.0f;
    }
    return true;
  }

  bool validate(float t, float h) const {
    return isfinite(t) && isfinite(h) && t > -40.0f && t < 125.0f &&
           h >= 0.0f && h <= 100.0f;
  }

  uint8_t crc8(const uint8_t* d, uint8_t len) const {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; ++i) {
      crc ^= d[i];
      for (uint8_t b = 0; b < 8; ++b) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                           : static_cast<uint8_t>(crc << 1);
      }
    }
    return crc;
  }

  bool ok_;
};

#else
// No sensor type defined — compile error is raised by sensor_interface.h
#endif

// ---------------------------------------------------------------------------
// Factory entry point — activated only when a sensor type is defined.
// ---------------------------------------------------------------------------
#if defined(WM_SENSOR_TYPE_SHT20)
ISensor* createSensorInstance() {
  static SHT20Driver inst;
  return &inst;
}
#elif defined(WM_SENSOR_TYPE_AHT10)
ISensor* createSensorInstance() {
  static AHT10Driver inst;
  return &inst;
}
#elif defined(WM_SENSOR_TYPE_HTU21)
ISensor* createSensorInstance() {
  static HTU21Driver inst;
  return &inst;
}
#endif
