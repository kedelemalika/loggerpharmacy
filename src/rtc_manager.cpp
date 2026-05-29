#include "rtc_manager.h"

bool RTCManager::begin() {
  available_ = rtc_.begin();
  if (!available_) {
    valid_ = false;
    lostPower_ = false;
    return false;
  }

  lostPower_ = rtc_.lostPower();

  DateTime current = rtc_.now();
  valid_ = !lostPower_ && isDateTimeValid(current);

  return true;
}

bool RTCManager::isValid() const { return available_ && valid_; }

bool RTCManager::isAvailable() const { return available_; }

DateTime RTCManager::now() {
  if (!available_) {
    return DateTime(2000, 1, 1, 0, 0, 0);
  }
  return rtc_.now();
}

uint32_t RTCManager::getEpoch() {
  return static_cast<uint32_t>(now().unixtime());
}

bool RTCManager::getDateString(char* out, size_t len) {
  if (!out || len < 11) return false;

  DateTime current = now();
  int n = snprintf(out, len, "%04d-%02d-%02d",
                   current.year(), current.month(), current.day());

  return n > 0 && (size_t)n < len;
}

bool RTCManager::getTimeString(char* out, size_t len) {
  if (!out || len < 9) return false;

  DateTime current = now();
  int n = snprintf(out, len, "%02d:%02d:%02d",
                   current.hour(), current.minute(), current.second());

  return n > 0 && (size_t)n < len;
}

bool RTCManager::wasLostPower() const { return available_ && lostPower_; }

bool RTCManager::setToBuildTime() {
  if (!available_) {
    return false;
  }

  rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime current = rtc_.now();
  lostPower_ = false;
  valid_ = isDateTimeValid(current);

  return valid_;
}

bool RTCManager::setEpoch(uint32_t epoch) {
  if (!available_) {
    return false;
  }

  rtc_.adjust(DateTime(epoch));

  DateTime current = rtc_.now();
  lostPower_ = false;
  valid_ = isDateTimeValid(current);

  return valid_;
}

bool RTCManager::isDateTimeValid(const DateTime& dt) const {
  return dt.year() >= 2024 && dt.year() < 2100;
}
