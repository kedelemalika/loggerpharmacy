#pragma once

#include <Arduino.h>

namespace Topics {
inline String devicePrefix(const String& siteId, const String& deviceId) {
  String topic;
  topic.reserve(6 + siteId.length() + deviceId.length());
  topic = "ljk/d/";
  topic += siteId;
  topic += '/';
  topic += deviceId;
  return topic;
}

inline String withSuffix(const String& siteId, const String& deviceId, const char* suffix) {
  String topic = devicePrefix(siteId, deviceId);
  topic += suffix;
  return topic;
}

inline String telemetry(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/t");
}

inline String status(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/s");
}

inline String availability(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/a");
}

inline String exceptionTopic(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/x");
}

inline String commandWildcard(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/c/#");
}

inline String commandSet(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/c/set");
}

inline String commandDo(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/c/do");
}

inline String calibrationMetadata(const String& siteId, const String& deviceId) {
  return withSuffix(siteId, deviceId, "/m/cal");
}
}  // namespace Topics
