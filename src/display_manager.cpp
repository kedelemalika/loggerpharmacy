#include "display_manager.h"

#include <cstring>
#include <Wire.h>

#include "config.h"
#include "pins.h"

namespace {
const char* okFail(bool ok) { return ok ? "OK" : "ER"; }

const char* modeShort(const char* modeText) {
  if (modeText == nullptr) {
    return "UNK";
  }
  if (strstr(modeText, "SYNC") != nullptr) {
    return "SYN";
  }
  if (strstr(modeText, "PORTABLE") != nullptr) {
    return "PRT";
  }
  if (strstr(modeText, "NORMAL") != nullptr) {
    return "NOR";
  }
  return "UNK";
}
}

DisplayManager::DisplayManager()
    : display_(Config::OLED_WIDTH, Config::OLED_HEIGHT, &Wire, Pins::OLED_RESET) {}

bool DisplayManager::begin() {
  available_ = display_.begin(Config::OLED_ADDRESS, true);
  if (!available_) {
    return false;
  }

  display_.clearDisplay();
  display_.setTextSize(Config::OLED_TEXT_SIZE);
  display_.setTextColor(SH110X_WHITE);
  display_.display();
  wakeFor(Config::DISPLAY_AUTO_OFF_MS);
  return true;
}

void DisplayManager::wakeFor(uint32_t durationMs) {
  if (!available_) {
    return;
  }
  enabled_ = true;
  display_.oled_command(SH110X_DISPLAYON);
  displayUntilMs_ = millis() + durationMs;
}

void DisplayManager::loop(uint32_t nowMs) {
  if (!available_ || !enabled_) {
    return;
  }

  if (displayUntilMs_ != 0 && static_cast<int32_t>(nowMs - displayUntilMs_) >= 0) {
    display_.oled_command(SH110X_DISPLAYOFF);
    enabled_ = false;
  }
}

void DisplayManager::renderLine(uint8_t line, const char* text) {
  display_.setCursor(0, line * 10);
  display_.print(text);
}

void DisplayManager::showBootScreen(const char* title, const char* step, const char* detail) {
  if (!available_) {
    return;
  }

  enabled_ = true;
  display_.oled_command(SH110X_DISPLAYON);
  displayUntilMs_ = millis() + Config::DISPLAY_AUTO_OFF_MS;

  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print(title != nullptr ? title : "BOOT");
  display_.setCursor(0, 16);
  display_.print(step != nullptr ? step : "");
  display_.setCursor(0, 28);
  display_.print(detail != nullptr ? detail : "");
  display_.display();
  // Boot screen bypasses status-frame cache; force next status render to redraw.
  hasLastFrame_ = false;
}

bool DisplayManager::isFrameChanged(const char lines[kDisplayLines][kLineBufferSize]) {
  if (!hasLastFrame_) {
    return true;
  }

  for (uint8_t i = 0; i < kDisplayLines; ++i) {
    if (strncmp(lastFrame_[i], lines[i], kLineBufferSize) != 0) {
      return true;
    }
  }
  return false;
}

void DisplayManager::cacheFrame(const char lines[kDisplayLines][kLineBufferSize]) {
  for (uint8_t i = 0; i < kDisplayLines; ++i) {
    strncpy(lastFrame_[i], lines[i], kLineBufferSize - 1);
    lastFrame_[i][kLineBufferSize - 1] = '\0';
  }
  hasLastFrame_ = true;
}

void DisplayManager::showStatus(const RuntimeConfig& config, const SensorReading& reading,
                                const DeviceStatus& status, const char* modeText,
                                const char* detailText, bool technicalView) {
  if (!available_ || !enabled_) {
    return;
  }
  (void)config;

  if (!technicalView) {
    char tempLine[16] = {};
    char humLine[16] = {};
    if (reading.valid) {
      snprintf(tempLine, sizeof(tempLine), "%d.%dC", reading.temperatureX10 / 10,
               abs(reading.temperatureX10 % 10));
      snprintf(humLine, sizeof(humLine), "%d.%d%%", reading.humidityX10 / 10,
               abs(reading.humidityX10 % 10));
    } else {
      snprintf(tempLine, sizeof(tempLine), "--.-C");
      snprintf(humLine, sizeof(humLine), "--.-%%");
    }

    display_.clearDisplay();
    display_.setTextSize(3);
    display_.setCursor(0, 0);
    display_.print(tempLine);
    display_.setCursor(0, 34);
    display_.print(humLine);
    display_.display();
    hasLastFrame_ = false;
    return;
  }

  char lines[kDisplayLines][kLineBufferSize] = {};
  snprintf(lines[0], kLineBufferSize, "%s", modeShort(modeText));
  snprintf(lines[1], kLineBufferSize, "M:%c W:%c", status.mqttOk ? 'O' : 'E',
           status.wifiOk ? 'O' : 'E');

  if (reading.valid) {
    snprintf(lines[2], kLineBufferSize, "%d.%dC", reading.temperatureX10 / 10,
             abs(reading.temperatureX10 % 10));
    snprintf(lines[3], kLineBufferSize, "%d.%d%%", reading.humidityX10 / 10,
             abs(reading.humidityX10 % 10));
  } else {
    snprintf(lines[2], kLineBufferSize, "--.-C");
    snprintf(lines[3], kLineBufferSize, "--.-%%");
  }

  snprintf(lines[4], kLineBufferSize, "S:%c R:%c", status.sensorOk ? 'O' : 'E',
           status.rtcOk ? 'O' : 'E');
  if (detailText != nullptr &&
      (strstr(detailText, "PEND:") != nullptr || strchr(detailText, '/') != nullptr)) {
    snprintf(lines[5], kLineBufferSize, "%s", detailText);
  } else {
    snprintf(lines[5], kLineBufferSize, "%ddB", status.rssi);
  }

  if (!isFrameChanged(lines)) {
    return;
  }

  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print(lines[0]);   // top-left: mode
  display_.setCursor(80, 0);
  display_.print(lines[1]);   // top-right: MQTT/WiFi

  display_.setTextSize(2);
  display_.setCursor(0, 20);
  display_.print(lines[2]);   // center-left: temperature
  display_.setCursor(68, 20);
  display_.print(lines[3]);   // center-right: humidity

  display_.setTextSize(1);
  display_.setCursor(0, 56);
  display_.print(lines[4]);   // bottom-left: sensor/rtc
  int16_t rightX = 128 - static_cast<int16_t>(strlen(lines[5]) * 6);
  if (rightX < 64) {
    rightX = 64;
  }
  display_.setCursor(rightX, 56);
  display_.print(lines[5]);   // bottom-right: RSSI or pending/sync
  display_.display();
  cacheFrame(lines);
}

bool DisplayManager::isAvailable() const { return available_; }
