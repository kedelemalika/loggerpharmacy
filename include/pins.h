#pragma once

#include <Arduino.h>

namespace Pins {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3 provisional mapping (safe defaults, avoid boot strap heavy pins).
// TODO(hw): Verify against actual ESP32-C3 Super Mini schematic before production flashing.
// Some board variants may repurpose GPIO2/GPIO3/GPIO10 for onboard LED or boot-related wiring.
constexpr uint8_t I2C_SDA = 4;
constexpr uint8_t I2C_SCL = 5;
constexpr uint8_t BUTTON = 2;
constexpr uint8_t BUZZER = 3;
constexpr uint8_t RELAY_ALARM = 10;
constexpr uint8_t EXT_FLASH_SCK = 6;
constexpr uint8_t EXT_FLASH_MOSI = 7;
constexpr uint8_t EXT_FLASH_MISO = 1;
constexpr uint8_t EXT_FLASH_CS = 0;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
// ESP32-S2 / LOLIN S2 Mini provisional mapping.
// TODO(hw): Verify this mapping on physical LOLIN S2 Mini / Wemos S2 Mini S2FN4R2 board.
// Avoid USB native, boot strap, and internal flash/PSRAM-conflict pins if board wiring differs.
constexpr uint8_t I2C_SDA = 33;
constexpr uint8_t I2C_SCL = 35;
constexpr uint8_t BUTTON = 21;
constexpr uint8_t BUZZER = 12;
constexpr uint8_t RELAY_ALARM = 13;
constexpr uint8_t EXT_FLASH_SCK = 7;
constexpr uint8_t EXT_FLASH_MOSI = 9;
constexpr uint8_t EXT_FLASH_MISO = 11;
constexpr uint8_t EXT_FLASH_CS = 10;
#elif defined(CONFIG_IDF_TARGET_ESP32)
// ESP32 WROOM-32D / DevKit mapping.
constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t BUTTON = 27;
constexpr uint8_t BUZZER = 25;
constexpr uint8_t RELAY_ALARM = 26;
constexpr uint8_t EXT_FLASH_SCK = 18;
constexpr uint8_t EXT_FLASH_MOSI = 23;
constexpr uint8_t EXT_FLASH_MISO = 19;
constexpr uint8_t EXT_FLASH_CS = 5;
#else
// Fallback to ESP32 classic mapping.
constexpr uint8_t I2C_SDA = 21;
constexpr uint8_t I2C_SCL = 22;
constexpr uint8_t BUTTON = 27;
constexpr uint8_t BUZZER = 25;
constexpr uint8_t RELAY_ALARM = 26;
constexpr uint8_t EXT_FLASH_SCK = 18;
constexpr uint8_t EXT_FLASH_MOSI = 23;
constexpr uint8_t EXT_FLASH_MISO = 19;
constexpr uint8_t EXT_FLASH_CS = 5;
#endif

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint8_t OLED_RESET = 255;
constexpr uint8_t BATTERY_ADC = 4;
}  // namespace Pins
