#pragma once

#include "types.h"

namespace ZoneProfiles {
inline const char* label(ZoneProfile profile) {
  switch (profile) {
    case ZoneProfile::Coldroom:
      return "coldroom";
    case ZoneProfile::Freezer:
      return "freezer";
    case ZoneProfile::Custom:
      return "custom";
    case ZoneProfile::Ambient:
    default:
      return "ambient";
  }
}

inline ZoneProfile parse(const String& raw) {
  String value = raw;
  value.trim();
  value.toLowerCase();
  if (value == "coldroom") {
    return ZoneProfile::Coldroom;
  }
  if (value == "freezer") {
    return ZoneProfile::Freezer;
  }
  if (value == "custom") {
    return ZoneProfile::Custom;
  }
  return ZoneProfile::Ambient;
}

inline bool applyDefaults(ZoneProfile profile, RuntimeConfig& config) {
  switch (profile) {
    case ZoneProfile::Ambient:
      config.tempLowX10 = 150;
      config.tempHighX10 = 300;
      config.humLowX10 = 300;
      config.humHighX10 = 750;
      return true;
    case ZoneProfile::Coldroom:
      config.tempLowX10 = 20;
      config.tempHighX10 = 80;
      config.humLowX10 = 300;
      config.humHighX10 = 750;
      return true;
    case ZoneProfile::Freezer:
      config.tempLowX10 = -250;
      config.tempHighX10 = -150;
      config.humLowX10 = 0;
      config.humHighX10 = 1000;
      return true;
    case ZoneProfile::Custom:
    default:
      return false;
  }
}
}  // namespace ZoneProfiles
