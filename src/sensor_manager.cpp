#include "sensor_manager.h"

#include <math.h>

#include "config.h"
#include <Wire.h>

namespace {
uint8_t gRetryCount = 0;
}  // namespace

bool SensorManager::begin() {
  sensor_ = createSensorInstance();
  if (!sensor_) {
    Serial.println("[SENSOR] Factory returned nullptr — no sensor type configured");
    runtimeError_ = true;
    present_ = false;
    available_ = false;
    return false;
  }

  present_ = sensor_->begin();
  available_ = present_;
  runtimeError_ = !present_;
  lastSampleOk_ = false;
  Serial.printf("[SENSOR] Init %s (%s @ 0x%02X)\n", present_ ? "ok" : "fail", sensor_->name(),
                sensor_->i2cAddress());
  return present_;
}

bool SensorManager::read(SensorReading& outReading) {
  float temperature = Config::INVALID_SENSOR_VALUE;
  float humidity = Config::INVALID_SENSOR_VALUE;

  if (!sensor_) {
    available_ = false;
    outReading = SensorReading();
    outReading.sensorPresent = false;
    outReading.sensorRuntimeError = true;
    return false;
  }

  // Probe before each read
  present_ = sensor_->isPresent();
  if (!present_) {
    runtimeError_ = true;
    Serial.printf("[SENSOR] Sensor %s not responding on 0x%02X\n", sensor_->name(),
                  sensor_->i2cAddress());
    delay(60);
  }

  bool valid = false;
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    if (attempt > 0) {
      Serial.printf("[SENSOR] Read retry %u/3\n", attempt + 1);
    }

    if (sensor_->read(temperature, humidity)) {
      if (attempt > 0) {
        Serial.printf("[SENSOR] Read recovered on attempt %u\n", attempt + 1);
      }
      valid = true;
      break;
    }

    runtimeError_ = true;
    lastSampleOk_ = false;
    Serial.println("[SENSOR] Read failed");
    if (attempt < 2) {
      Serial.println("[SENSOR] Reinit triggered");
      sensor_->begin();
    }
    delay(80);
  }

  outReading = SensorReading();
  outReading.sensorPresent = present_;
  outReading.sensorRuntimeError = runtimeError_;
  outReading.lastGoodMs = lastGoodMs_;

  if (!valid) {
    available_ = false;
    outReading.latestSampleOk = false;
    outReading.retryCount = 3;
    if (!hadLoggedFailure_) {
      Serial.println("[SENSOR] Latest sample invalid");
      hadLoggedFailure_ = true;
    }
    return false;
  }

  available_ = true;
  runtimeError_ = false;
  lastSampleOk_ = true;
  lastGoodMs_ = millis();
  tempSamples_[sampleIndex_] = temperature;
  humSamples_[sampleIndex_] = humidity;
  sampleIndex_ = (sampleIndex_ + 1) % 3;
  if (validSamples_ < 3) {
    validSamples_++;
  }
  sampleCount_++;

  float avgTemp = movingAverage(tempSamples_, validSamples_);
  float avgHum = movingAverage(humSamples_, validSamples_);

  outReading.valid = true;
  outReading.latestSampleOk = true;
  outReading.sensorPresent = present_;
  outReading.sensorRuntimeError = false;
  outReading.lastGoodMs = lastGoodMs_;
  outReading.rawTemperatureC = avgTemp;
  outReading.rawHumidityPct = avgHum;
  outReading.rawTemperatureX10 = scaleX10(avgTemp);
  outReading.rawHumidityX10 = scaleX10(avgHum);
  outReading.temperatureC = avgTemp;
  outReading.humidityPct = avgHum;
  outReading.temperatureX10 = scaleX10(avgTemp);
  outReading.humidityX10 = scaleX10(avgHum);
  outReading.sampleCount = sampleCount_;

  if (hadLoggedFailure_) {
    Serial.println("[SENSOR] Latest sample valid again");
    hadLoggedFailure_ = false;
  }
  return true;
}

const char* SensorManager::sensorName() const {
  return sensor_ ? sensor_->name() : "NONE";
}

void SensorManager::forceRecovery(bool reinitBus) {
  if (reinitBus) {
    Serial.println("[RECOVERY] I2C bus reinit triggered");
    Wire.end();
    delay(10);
    Wire.begin();
    Wire.setClock(Config::I2C_CLOCK_HZ);
    delay(20);
  }
  if (sensor_) {
    Serial.println("[RECOVERY] Sensor reinit triggered");
    sensor_->begin();
  }
}

int16_t SensorManager::scaleX10(float value) const {
  return static_cast<int16_t>(lroundf(value * 10.0f));
}

float SensorManager::movingAverage(const float* values, uint8_t count) const {
  if (count == 0) {
    return 0.0f;
  }

  float total = 0.0f;
  for (uint8_t i = 0; i < count; ++i) {
    total += values[i];
  }
  return total / static_cast<float>(count);
}