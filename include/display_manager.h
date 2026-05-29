#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>

#include "types.h"

class DisplayManager {
 public:
  DisplayManager();

  bool begin();
  void wakeFor(uint32_t durationMs);
  void loop(uint32_t nowMs);
  void showBootScreen(const char* title, const char* step, const char* detail);
  void showStatus(const RuntimeConfig& config, const SensorReading& reading,
                  const DeviceStatus& status, const char* modeText,
                  const char* detailText, bool technicalView);
  bool isAvailable() const;

 private:
  static constexpr uint8_t kDisplayLines = 6;
  static constexpr uint8_t kLineBufferSize = 24;

  void renderLine(uint8_t line, const char* text);
  bool isFrameChanged(const char lines[kDisplayLines][kLineBufferSize]);
  void cacheFrame(const char lines[kDisplayLines][kLineBufferSize]);

  Adafruit_SH1106G display_;
  bool available_ = false;
  bool enabled_ = true;
  uint32_t displayUntilMs_ = 0;
  char lastFrame_[kDisplayLines][kLineBufferSize] = {};
  bool hasLastFrame_ = false;
};
