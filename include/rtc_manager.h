#pragma once

#include <Arduino.h>
#include <RTClib.h>

class RTCManager {
 public:
  bool begin();
  bool isValid() const;
  bool isAvailable() const;
  uint32_t getEpoch();
  bool getDateString(char* out, size_t len);
  bool getTimeString(char* out, size_t len);
  DateTime now();
  bool wasLostPower() const;
  bool setToBuildTime();
  bool setEpoch(uint32_t epoch);

 private:
  bool isDateTimeValid(const DateTime& dt) const;

  RTC_DS3231 rtc_;
  bool available_ = false;
  bool valid_ = false;
  bool lostPower_ = false;
};
