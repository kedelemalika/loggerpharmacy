#pragma once

#include <Arduino.h>

#include "types.h"

String buildTelemetryPayload(const TelemetrySpoolRecord& record, bool backfill,
                             uint8_t calVer = 1);
String buildEventPayload(const EventSpoolRecord& record, bool backfill);
