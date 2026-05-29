#pragma once

#include <Arduino.h>

// =============================================================================
// Sensor Type Macros — exactly one must be defined per environment.
// Use -DWM_SENSOR_TYPE_XXX=1 in platformio.ini build_flags.
// =============================================================================
// #define WM_SENSOR_TYPE_SHT20   // Sensirion SHT20 via DFRobot_SHT20
// #define WM_SENSOR_TYPE_AHT10   // Adafruit AHT10/AHT20 via Adafruit_AHTX0
// #define WM_SENSOR_TYPE_HTU21   // TE Connectivity HTU21D via SparkFun HTU21

// =============================================================================
// Policy Macros — control what happens when an unvalidated board/sensor combo
// is compiled. Default is STRICT (fail-fast). ALLOW_EXPERIMENTAL is for lab
// testing only and must never be used in production.
// =============================================================================
// #define WM_POLICY_STRICT           // Default: #error on unvalidated combo
// #define WM_POLICY_ALLOW_EXPERIMENTAL // Allow non-validated combo to compile

#if !defined(WM_POLICY_STRICT) && !defined(WM_POLICY_ALLOW_EXPERIMENTAL)
#define WM_POLICY_STRICT 1
#endif

// =============================================================================
// Board Type Macros — set by platformio.ini per environment.
// =============================================================================
// #define WM_BOARD_ESP32     // ESP32 WROOM-32D / DevKit
// #define WM_BOARD_ESP32S2  // ESP32-S2 / LOLIN S2 Mini
// #define WM_BOARD_ESP32C3  // ESP32-C3 Super Mini

// =============================================================================
// Sensor type must be explicitly defined — no silent fallback.
// MUST have exactly one sensor type per build. Always enforced regardless
// of policy mode. ALLOW_EXPERIMENTAL only suppresses board-sensor combo
// checks, never bypasses the "exactly one sensor" rule.
// =============================================================================
#if !defined(WM_SENSOR_TYPE_SHT20) && !defined(WM_SENSOR_TYPE_AHT10) && \
    !defined(WM_SENSOR_TYPE_HTU21)
#error "Sensor type not defined. Add -DWM_SENSOR_TYPE_SHT20=1 (or _AHT10, _HTU21) to build_flags."
#endif

#if (defined(WM_SENSOR_TYPE_SHT20) && defined(WM_SENSOR_TYPE_AHT10)) || \
    (defined(WM_SENSOR_TYPE_SHT20) && defined(WM_SENSOR_TYPE_HTU21)) || \
    (defined(WM_SENSOR_TYPE_AHT10) && defined(WM_SENSOR_TYPE_HTU21))
#error "Multiple sensor types defined. Use only one: WM_SENSOR_TYPE_SHT20, WM_SENSOR_TYPE_AHT10, or WM_SENSOR_TYPE_HTU21."
#endif

// =============================================================================
// Validated Board <-> Sensor Combinations
// Format: board + sensor = supported (1=yes, 0=no)
// =============================================================================
#define WM_VAL_ESP32_SHT20    1
#define WM_VAL_ESP32_AHT10    0
#define WM_VAL_ESP32_HTU21    0
#define WM_VAL_ESP32S2_SHT20  0
#define WM_VAL_ESP32S2_AHT10  1
#define WM_VAL_ESP32S2_HTU21  1
#define WM_VAL_ESP32C3_SHT20  1
#define WM_VAL_ESP32C3_AHT10  0
#define WM_VAL_ESP32C3_HTU21  0

// =============================================================================
// Board-sensor compatibility matrix with policy-aware #error
// =============================================================================
#if defined(WM_BOARD_ESP32) && defined(WM_SENSOR_TYPE_SHT20)
#define WM_SENSOR_ADDRESS 0x40
#elif defined(WM_BOARD_ESP32) && defined(WM_SENSOR_TYPE_AHT10)
#define WM_SENSOR_ADDRESS 0x38
#elif defined(WM_BOARD_ESP32) && defined(WM_SENSOR_TYPE_HTU21)
#define WM_SENSOR_ADDRESS 0x40
#elif defined(WM_BOARD_ESP32S2) && defined(WM_SENSOR_TYPE_AHT10)
#define WM_SENSOR_ADDRESS 0x38
#elif defined(WM_BOARD_ESP32S2) && defined(WM_SENSOR_TYPE_SHT20)
#define WM_SENSOR_ADDRESS 0x40
#elif defined(WM_BOARD_ESP32S2) && defined(WM_SENSOR_TYPE_HTU21)
#define WM_SENSOR_ADDRESS 0x40
#elif defined(WM_BOARD_ESP32C3) && defined(WM_SENSOR_TYPE_SHT20)
#define WM_SENSOR_ADDRESS 0x40
#elif defined(WM_BOARD_ESP32C3) && defined(WM_SENSOR_TYPE_AHT10)
#define WM_SENSOR_ADDRESS 0x38
#elif defined(WM_BOARD_ESP32C3) && defined(WM_SENSOR_TYPE_HTU21)
#define WM_SENSOR_ADDRESS 0x40
#else
// Fallback for board without explicit WM_BOARD_XXX (e.g. custom board)
#define WM_SENSOR_ADDRESS 0x40
#endif

// Check validated matrix for STRICT policy
#if defined(WM_POLICY_STRICT)
#if defined(WM_BOARD_ESP32)
#if defined(WM_SENSOR_TYPE_SHT20) && !WM_VAL_ESP32_SHT20
#error "ESP32 + SHT20 is not validated."
#endif
#if defined(WM_SENSOR_TYPE_AHT10) && !WM_VAL_ESP32_AHT10
#error "ESP32 + AHT10 is not validated. Validated sensors for ESP32: SHT20."
#endif
#if defined(WM_SENSOR_TYPE_HTU21) && !WM_VAL_ESP32_HTU21
#error "ESP32 + HTU21 is not validated. Validated sensors for ESP32: SHT20."
#endif
#endif  // WM_BOARD_ESP32

#if defined(WM_BOARD_ESP32S2)
#if defined(WM_SENSOR_TYPE_AHT10) && !WM_VAL_ESP32S2_AHT10
#error "ESP32-S2 + AHT10 is not validated."
#endif
#if defined(WM_SENSOR_TYPE_SHT20) && !WM_VAL_ESP32S2_SHT20
#error "ESP32-S2 + SHT20 is not supported on this board. Use -DWM_SENSOR_TYPE_AHT10=1."
#endif
#if defined(WM_SENSOR_TYPE_HTU21) && !WM_VAL_ESP32S2_HTU21
#error "ESP32-S2 + HTU21 is not validated."
#endif
#endif  // WM_BOARD_ESP32S2

#if defined(WM_BOARD_ESP32C3)
#if defined(WM_SENSOR_TYPE_SHT20) && !WM_VAL_ESP32C3_SHT20
#error "ESP32-C3 + SHT20 is not validated."
#endif
#if defined(WM_SENSOR_TYPE_AHT10) && !WM_VAL_ESP32C3_AHT10
#error "ESP32-C3 + AHT10 is not validated. Validated sensors for ESP32-C3: SHT20."
#endif
#if defined(WM_SENSOR_TYPE_HTU21) && !WM_VAL_ESP32C3_HTU21
#error "ESP32-C3 + HTU21 is not validated. Validated sensors for ESP32-C3: SHT20."
#endif
#endif  // WM_BOARD_ESP32C3
#endif  // WM_POLICY_STRICT

// ALLOW_EXPERIMENTAL policy: warn but do not error on unvalidated combos.
#if defined(WM_POLICY_ALLOW_EXPERIMENTAL)
#pragma message "ALLOW_EXPERIMENTAL: Board-sensor guard checks are disabled. This build is for lab testing only."
#endif  // WM_POLICY_ALLOW_EXPERIMENTAL

// =============================================================================
// Sensor name strings — used by sensorName() at runtime.
// =============================================================================
namespace SensorNames {
#if defined(WM_SENSOR_TYPE_SHT20)
constexpr char kName[] = "SHT20";
#elif defined(WM_SENSOR_TYPE_AHT10)
constexpr char kName[] = "AHT10";
#elif defined(WM_SENSOR_TYPE_HTU21)
constexpr char kName[] = "HTU21";
#endif
}  // namespace SensorNames

// ISensor* createSensorInstance();

// =============================================================================
// ISensor Interface — pure abstract base for all sensor drivers.
// Each driver must implement all virtual methods.
// =============================================================================
class ISensor {
 public:
  virtual ~ISensor() = default;

  // Initialize the sensor hardware. Returns true if sensor is detected.
  virtual bool begin() = 0;

  // Read temperature (Celsius) and humidity (percent). Returns true if valid.
  virtual bool read(float& temperature, float& humidity) = 0;

  // Probe the I2C bus and return true if this sensor responds.
  virtual bool isPresent() const = 0;

  // Return a static name string for this sensor type.
  virtual const char* name() const = 0;

  // Return I2C address used by this sensor.
  constexpr uint8_t i2cAddress() const { return kAddress; }

 protected:
  constexpr ISensor(const char* name, uint8_t address) : name_(name), kAddress(address) {}
  const char* const name_;
  const uint8_t kAddress;
};
