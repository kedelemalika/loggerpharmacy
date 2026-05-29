#pragma once

#include <Arduino.h>

namespace Config {
constexpr char FW_VERSION[] = "1.0.4";
constexpr char NVS_NAMESPACE[] = "ljkmon";

constexpr char DEFAULT_SITE_ID[] = "dcjkt";
constexpr char DEFAULT_DEVICE_ID[] = "amb-01";
constexpr char DEFAULT_MQTT_HOST[] = "mqtt.ljkwarehouse.com";
constexpr uint16_t DEFAULT_MQTT_PORT = 8883;
constexpr uint16_t DEFAULT_MQTT_TLS_PORT = 8883;
constexpr char DEFAULT_MQTT_USERNAME[] = "dcjkt";
constexpr char DEFAULT_MQTT_PASSWORD[] = "";
constexpr bool DEFAULT_TLS_PRODUCTION_MODE = false;
constexpr bool DEFAULT_MQTT_USE_TLS = true;
constexpr bool DEFAULT_MQTT_TLS_INSECURE = !DEFAULT_TLS_PRODUCTION_MODE;

constexpr int16_t DEFAULT_TEMP_LOW_X10 = 150;
constexpr int16_t DEFAULT_TEMP_HIGH_X10 = 300;
constexpr int16_t DEFAULT_HUM_LOW_X10 = 300;
constexpr int16_t DEFAULT_HUM_HIGH_X10 = 750;
constexpr uint16_t DEFAULT_ALARM_DELAY_S = 120;
constexpr uint16_t DEFAULT_SAMPLE_INTERVAL_S = 10;
constexpr uint16_t DEFAULT_REPORT_INTERVAL_S = 30;
constexpr uint16_t DEFAULT_STATUS_INTERVAL_S = 600;
constexpr uint16_t DEFAULT_PORTAL_TIMEOUT_S = 180;
constexpr bool DEFAULT_COMMISSIONING_MODE = false;
constexpr uint16_t DEFAULT_COMMISSIONING_LOG_INTERVAL_S = 10;
constexpr bool DEFAULT_BUZZER_ENABLED = false;
constexpr uint8_t DEFAULT_OPERATION_MODE_RAW = 0;
constexpr char DEFAULT_BACKEND_HTTP_BASE_URL[] = "";
constexpr char DEFAULT_BACKEND_INGEST_BEARER_TOKEN[] = "";
constexpr char DEFAULT_BACKEND_DEVICE_SECRET[] = "";
constexpr uint32_t DEFAULT_BACKEND_TOKEN_EXPIRES_EPOCH = 0;
constexpr uint8_t DEFAULT_PORTABLE_BATCH_SIZE = 50;
constexpr bool DEFAULT_SPOOL_ENABLED = true;
constexpr uint16_t DEFAULT_SPOOL_INTERVAL_S = 30;
constexpr uint8_t DEFAULT_SYNC_BATCH_SIZE = 3;
constexpr uint32_t DEFAULT_SYNC_INTERVAL_MS = 2000;
constexpr int16_t DEFAULT_TEMP_OFFSET_X10 = 0;
constexpr int16_t DEFAULT_HUM_OFFSET_X10 = 0;
constexpr uint16_t MIN_SAMPLE_INTERVAL_S = 5;
constexpr uint16_t MIN_REPORT_INTERVAL_S = 10;
constexpr uint16_t MIN_STATUS_INTERVAL_S = 60;
constexpr uint16_t MIN_ALARM_DELAY_S = 5;
constexpr uint16_t MIN_COMMISSIONING_LOG_INTERVAL_S = 5;
constexpr uint16_t MIN_SPOOL_INTERVAL_S = 5;
constexpr uint8_t MIN_SYNC_BATCH_SIZE = 1;
constexpr uint8_t MAX_SYNC_BATCH_SIZE = 10;
constexpr uint8_t MIN_PORTABLE_BATCH_SIZE = 1;
constexpr uint8_t MAX_PORTABLE_BATCH_SIZE = 100;
constexpr uint32_t MIN_SYNC_INTERVAL_MS = 500;
constexpr int16_t MIN_TEMP_OFFSET_X10 = -100;
constexpr int16_t MAX_TEMP_OFFSET_X10 = 100;
constexpr int16_t MIN_HUM_OFFSET_X10 = -200;
constexpr int16_t MAX_HUM_OFFSET_X10 = 200;

// ── Calibration v2 constants ────────────────────────────────────────────────
constexpr uint8_t  DEFAULT_CAL_VER          = 1;   // 1=offset-only, 2=slope+offset
constexpr uint8_t  DEFAULT_CAL_STATUS       = 0;   // 0=uncalibrated
constexpr int16_t  DEFAULT_CAL_TEMP_SLOPE_X1000  = 1000;  // 1.000x
constexpr int16_t  DEFAULT_CAL_TEMP_OFFSET_X10   = 0;
constexpr int16_t  DEFAULT_CAL_HUM_SLOPE_X1000   = 1000;  // 1.000x
constexpr int16_t  DEFAULT_CAL_HUM_OFFSET_X10    = 0;

constexpr uint8_t  MIN_CAL_VER         = 1;
constexpr uint8_t  MAX_CAL_VER         = 2;
constexpr uint8_t  MIN_CAL_STATUS      = 0;
constexpr uint8_t  MAX_CAL_STATUS      = 2;
constexpr int16_t  MIN_CAL_TEMP_SLOPE  = 800;
constexpr int16_t  MAX_CAL_TEMP_SLOPE  = 1200;
constexpr int16_t  MIN_CAL_TEMP_OFFSET = -150;
constexpr int16_t  MAX_CAL_TEMP_OFFSET = 150;
constexpr int16_t  MIN_CAL_HUM_SLOPE    = 800;
constexpr int16_t  MAX_CAL_HUM_SLOPE    = 1200;
constexpr int16_t  MIN_CAL_HUM_OFFSET  = -250;
constexpr int16_t  MAX_CAL_HUM_OFFSET  = 250;

constexpr uint8_t SENSOR_RECOVERY_REINIT_THRESHOLD = 3;
constexpr uint8_t SENSOR_RECOVERY_I2C_THRESHOLD = 6;

constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 15000;
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint32_t DISPLAY_AUTO_OFF_MS = 15000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 3000;
constexpr uint32_t BUTTON_BOOT_HOLD_MS = 2500;
constexpr uint32_t BUTTON_MULTI_CLICK_WINDOW_MS = 700;
constexpr uint32_t MAIN_LOOP_IDLE_MS = 10;
constexpr uint32_t PORTABLE_SYNC_INTERVAL_MS = 1200;
constexpr uint32_t BACKEND_HTTP_TIMEOUT_MS = 8000;
constexpr uint32_t BACKEND_HEALTH_CHECK_INTERVAL_MS = 30000;
constexpr uint32_t BACKEND_INGEST_ACK_INTERVAL_MS = 30000;
constexpr uint8_t BACKEND_FAIL_THRESHOLD = 3;
constexpr uint32_t BACKEND_INGEST_STALE_SECONDS = 180;
constexpr char NTP_SERVER_PRIMARY[] = "pool.ntp.org";
constexpr char NTP_SERVER_SECONDARY[] = "time.google.com";
constexpr char NTP_SERVER_TERTIARY[] = "time.cloudflare.com";
constexpr uint32_t NTP_BOOT_SYNC_TIMEOUT_MS = 15000;
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 5000;
constexpr uint32_t NTP_POLL_INTERVAL_MS = 250;
constexpr uint32_t NTP_RETRY_INTERVAL_MS = 60000;
constexpr uint32_t NTP_RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t MIN_VALID_EPOCH = 1704067200UL;  // 2024-01-01T00:00:00Z
constexpr uint8_t SEQUENCE_RESERVE_BLOCK = 32;

constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t OLED_TEXT_SIZE = 1;
constexpr uint8_t EVENT_LOG_CAPACITY = 16;
constexpr uint8_t MQTT_QUEUE_CAPACITY = 8;
constexpr uint8_t MQTT_TOPIC_LENGTH = 96;
constexpr uint8_t MQTT_PAYLOAD_LENGTH = 160;
constexpr uint16_t PORTAL_BACKEND_URL_PARAM_LENGTH = 192;
constexpr uint16_t PORTAL_BACKEND_BEARER_PARAM_LENGTH = 384;
constexpr uint16_t PORTAL_BACKEND_DEVICE_SECRET_PARAM_LENGTH = 128;
constexpr uint32_t BACKEND_TOKEN_REFRESH_SKEW_S = 300;

constexpr char SPOOL_DIR[] = "/data";
constexpr char SPOOL_TELEMETRY_PATH[] = "/data/telemetry.log";
constexpr char SPOOL_EVENTS_PATH[] = "/data/events.log";
constexpr char SPOOL_PORTABLE_TELEMETRY_PATH[] = "/data/portable_telemetry.log";
constexpr char EXT_SPOOL_TELEMETRY_PATH[] = "/telemetry.log";
constexpr char EXT_SPOOL_EVENTS_PATH[] = "/events.log";
constexpr char EXT_SPOOL_PORTABLE_TELEMETRY_PATH[] = "/portable_telemetry.log";
constexpr char EXT_SPOOL_TEMP_PATH[] = "/.tmp";
constexpr char SPOOL_STATE_PATH[] = "/data/state.csv";
constexpr char SPOOL_TEMP_PATH[] = "/data/.tmp";
constexpr size_t MAX_TELEMETRY_SPOOL_BYTES = 64 * 1024;
constexpr size_t MAX_EVENTS_SPOOL_BYTES = 32 * 1024;
constexpr size_t MAX_PORTABLE_TELEMETRY_SPOOL_BYTES = 96 * 1024;
constexpr size_t MAX_EXT_TELEMETRY_SPOOL_BYTES = 2 * 1024 * 1024;
constexpr size_t MAX_EXT_EVENTS_SPOOL_BYTES = 512 * 1024;
constexpr size_t MAX_EXT_PORTABLE_TELEMETRY_SPOOL_BYTES = 4 * 1024 * 1024;
constexpr bool EXT_FLASH_ALLOW_AUTO_FORMAT = true;
constexpr uint32_t DIAG_RUNTIME_LOG_INTERVAL_MS = 60000;
constexpr uint32_t EXT_FLASH_SPI_HZ = 1000000;

constexpr float INVALID_SENSOR_VALUE = -9999.0f;
}  // namespace Config
