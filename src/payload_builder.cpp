#include "payload_builder.h"

#include <ArduinoJson.h>

#include "config.h"

namespace {
String makePayloadString() {
  String payload;
  payload.reserve(Config::MQTT_PAYLOAD_LENGTH);
  return payload;
}
}  // namespace

String buildTelemetryPayload(const TelemetrySpoolRecord& record, bool backfill,
                             uint8_t calVer) {
  JsonDocument doc;
  doc["n"] = record.sequence;
  doc["t"] = record.temperatureX10;
  doc["h"] = record.humidityX10;
  doc["ts"] = record.timestamp;
  doc["bf"] = backfill ? 1 : 0;
  doc["rt"] = record.rtcOk ? 1 : 0;

  // v2 lightweight: include raw values + cal_ver when record has raw values
  if (record.hasRawValues) {
    doc["raw_t"] = record.rawTemperatureX10;
    doc["raw_h"] = record.rawHumidityX10;
    doc["cal_ver"] = calVer;
  }
  if (record.batteryValid) {
    doc["bv"] = record.batteryMilliVolt;
    doc["bp"] = record.batteryPercent;
  }

  String payload = makePayloadString();
  serializeJson(doc, payload);

  // Payload size safety guard: warn if approaching buffer limit
  if (payload.length() >= Config::MQTT_PAYLOAD_LENGTH) {
    Serial.printf("[PAYLOAD] WARNING: telemetry size=%u >= limit=%u\n",
                  static_cast<unsigned>(payload.length()),
                  static_cast<unsigned>(Config::MQTT_PAYLOAD_LENGTH));
  }

  return payload;
}

String buildEventPayload(const EventSpoolRecord& record, bool backfill) {
  JsonDocument doc;
  doc["n"] = record.sequence;
  doc["cd"] = record.code;
  doc["v"] = record.value;
  doc["lim"] = record.limit;
  doc["dur"] = record.duration;
  doc["ts"] = record.timestamp;
  doc["bf"] = backfill ? 1 : 0;
  doc["rt"] = record.rtcOk ? 1 : 0;
  if (record.recovery) {
    doc["ok"] = 1;
  }

  String payload = makePayloadString();
  serializeJson(doc, payload);
  return payload;
}
