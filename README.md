# Project Overview

Firmware ESP32 untuk monitoring suhu/kelembaban gudang farmasi dengan MQTT, local spool (LittleFS), opsi external flash spool (W25Q64 raw SPI queue), mode portable HTTP sync, DS3231 RTC, OLED, buzzer, relay alarm, dan captive portal WiFiManager.

Versi firmware di source: `1.0.4` (`include/config.h`).

# Features

- Sampling sensor berkala (`sampleIntervalS`, default 10s).
- Telemetry MQTT (`/t`) dan status MQTT (`/s`).
- Exception/alarm MQTT (`/x`) + availability (`/a`) retained.
- MQTT command channel (`/c/set`, `/c/do`).
- Calibration v2 (slope + offset) + metadata channel (`/m/cal`).
- Spool internal via LittleFS (`/data/*.log`) + replay backlog.
- External flash spool manager (raw sector queue, fallback otomatis ke LittleFS).
- Mode portable: batch telemetry ke backend HTTP.
- RTC DS3231 + fallback build-time + NTP sync saat WiFi tersedia.
- Captive portal config (WiFi + parameter runtime utama).
- Battery monitoring compile-time optional (`WM_BATTERY_ENABLED`).

# Hardware Requirements

- Board ESP32 family (env tersedia untuk ESP32, ESP32-S2, ESP32-C3).
- Sensor (pilih tepat satu per build): `SHT20` atau `AHT10` atau `HTU21`.
- RTC DS3231 (I2C, alamat 0x68).
- OLED SH1106/SH110X I2C alamat 0x3C.
- Relay alarm output.
- Buzzer output.
- Tombol input (short/long press + hold at boot).
- Optional external SPI flash spool (kode log menyebut W25Q64).
- Optional battery divider ke ADC.

# Wiring Diagram

## Core I2C + IO (konseptual)

- I2C bus:
  - `SDA -> Pins::I2C_SDA`
  - `SCL -> Pins::I2C_SCL`
  - perangkat I2C: OLED (0x3C), RTC (0x68), sensor (0x38/0x40 tergantung tipe)
- Relay alarm:
  - `RELAY -> Pins::RELAY_ALARM` (aktif LOW)
- Buzzer:
  - `BUZZER -> Pins::BUZZER`
- Button:
  - `BUTTON -> Pins::BUTTON`
- External flash SPI:
  - `SCK/MOSI/MISO/CS -> Pins::EXT_FLASH_*`

`PERLU KONFIRMASI`: Skematik fisik per PCB/board variant tidak ada di repository.

# GPIO Mapping

Sumber: `include/pins.h`.

## ESP32 (CONFIG_IDF_TARGET_ESP32)

- I2C SDA: GPIO21
- I2C SCL: GPIO22
- Button: GPIO27
- Buzzer: GPIO25
- Relay: GPIO26
- ExtFlash SCK/MOSI/MISO/CS: GPIO18/23/19/5
- Battery ADC default pin: GPIO4

## ESP32-S2 (CONFIG_IDF_TARGET_ESP32S2)

- I2C SDA: GPIO33
- I2C SCL: GPIO35
- Button: GPIO21
- Buzzer: GPIO12
- Relay: GPIO13
- ExtFlash SCK/MOSI/MISO/CS: GPIO7/9/11/10
- Battery ADC default pin: GPIO4

## ESP32-C3 (CONFIG_IDF_TARGET_ESP32C3)

- I2C SDA: GPIO4
- I2C SCL: GPIO5
- Button: GPIO2
- Buzzer: GPIO3
- Relay: GPIO10
- ExtFlash SCK/MOSI/MISO/CS: GPIO6/7/1/0
- Battery ADC default pin: GPIO4

Catatan source code: mapping C3 dan S2 diberi komentar `provisional`/perlu verifikasi hardware.

# PlatformIO Environments

Sumber: `platformio.ini`.

Tidak ada `default_envs`, jadi tidak ada env yang aktif-by-default.

## Classification Legend

- `Tested`: sudah diuji pada perangkat nyata.
- `Recommended`: direkomendasikan untuk deployment saat ini.
- `Experimental`: untuk eksperimen/validasi compile path.
- `Diagnostic`: untuk uji diagnosis (mis. disable MQTT/extflash).

## Environment Classification

| Environment | Tested | Recommended | Experimental | Diagnostic | Catatan |
|---|---|---|---|---|---|
| `esp32-s2-saola-1-htu21-extflash-nocompact` | Yes | Yes | No | No | **Paling stabil saat ini**, sudah dipakai pengujian nyata, production candidate |
| `esp32dev` | No | No | No | No | Stable build path ESP32 + SHT20 |
| `esp32-s2-saola-1` | No | No | No | No | Stable build path ESP32-S2 + AHT10 |
| `esp32-s2-saola-1-htu21` | No | No | No | No | Stable build path ESP32-S2 + HTU21 |
| `esp32-c3-devkitm-1` | No | No | No | No | Provisional pin mapping |
| `esp32-s2-saola-1-htu21-noextflash` | No | No | No | No | Runtime tanpa external flash |
| `test_strict_dual_sensor` | No | No | Yes | No | Negative test (expected fail) |
| `test_experimental_dual_sensor` | No | No | Yes | No | Negative test (expected fail) |
| `test_experimental_invalid_combo` | No | No | Yes | No | ALLOW_EXPERIMENTAL combo check bypass |
| `test_experimental_htu21_on_esp32` | No | No | Yes | No | Compile-only HTU21 path |
| `esp32-s2-saola-1-htu21-diag-nomqtt` | No | No | No | Yes | Diagnostic mode tanpa MQTT |
| `esp32-s2-saola-1-htu21-diag-nomqtt-noextflash` | No | No | No | Yes | Diagnostic tanpa MQTT + extflash |
| `esp32-s2-saola-1-htu21-diag-nomqtt-extflash-nocompact` | No | No | No | Yes | Diagnostic tanpa MQTT + no compact |

## Recommended

- `esp32-s2-saola-1-htu21-extflash-nocompact` (STRICT, ESP32-S2 + HTU21 + external flash, tested real device)

## Active (runtime/operasional, non-test)

- `esp32dev`
- `esp32-c3-devkitm-1` (provisional mapping)
- `esp32-s2-saola-1`
- `esp32-s2-saola-1-htu21`
- `esp32-s2-saola-1-htu21-noextflash`
- `esp32-s2-saola-1-htu21-extflash-nocompact`

## Recommended Production Environment

- Environment name: `esp32-s2-saola-1-htu21-extflash-nocompact`
- Status: `Tested`
- Hardware:
  - ESP32-S2 Saola-1
  - HTU21
  - External Flash
- Current deployment status:
  - Sudah digunakan untuk pengujian nyata pada perangkat fisik.
  - Menjadi rekomendasi utama deployment saat ini.
  - Label operasional saat ini: `TESTED`, `RECOMMENDED`, `PRODUCTION CANDIDATE`.
- Catatan battery:
  - `WM_BATTERY_ENABLED=0` (default)
  - Fitur battery tersedia di firmware, tetapi belum aktif secara default.

## Legacy / Diagnostic / Lab-only

- `test_strict_dual_sensor`
- `test_experimental_dual_sensor`
- `test_experimental_invalid_combo`
- `test_experimental_htu21_on_esp32`
- `esp32-s2-saola-1-htu21-diag-nomqtt`
- `esp32-s2-saola-1-htu21-diag-nomqtt-noextflash`
- `esp32-s2-saola-1-htu21-diag-nomqtt-extflash-nocompact`

## Deprecated

- Tidak ada label deprecated eksplisit di source.
- `PERLU KONFIRMASI`: jika tim ingin menandai env tertentu deprecated secara resmi.

# Build Configuration

Contoh flag inti:

- Policy:
  - `WM_POLICY_STRICT=1` (default recommended)
  - `WM_POLICY_ALLOW_EXPERIMENTAL=1` (lab only)
- Board selector:
  - `WM_BOARD_ESP32=1` / `WM_BOARD_ESP32S2=1` / `WM_BOARD_ESP32C3=1`
- Sensor selector (wajib tepat satu):
  - `WM_SENSOR_TYPE_SHT20=1` atau `WM_SENSOR_TYPE_AHT10=1` atau `WM_SENSOR_TYPE_HTU21=1`
- Diagnostic:
  - `WM_DIAG_DISABLE_MQTT=1`
  - `WM_DIAG_DISABLE_EXTFLASH=1`
  - `WM_DIAG_DISABLE_EXTFLASH_COMPACT=1`
- Battery:
  - `WM_BATTERY_ENABLED`
  - `WM_BATTERY_ADC_PIN`
  - `WM_BATTERY_DIVIDER_NUM`
  - `WM_BATTERY_DIVIDER_DEN`

Guard compile-time sensor ada di `include/sensor_interface.h`.

# MQTT Topics

Format prefix: `ljk/d/{site_id}/{device_id}`

- Telemetry: `/t`
- Status: `/s`
- Availability: `/a`
- Exception: `/x`
- Command subscribe wildcard: `/c/#`
- Command set: `/c/set`
- Command do: `/c/do`
- Calibration metadata: `/m/cal`

# MQTT Payload Reference

## Telemetry Payload (aktual)

Dibangun di `src/payload_builder.cpp`.

Field:

- Wajib: `n`, `t`, `h`, `ts`, `bf`, `rt`
- Opsional saat ada raw value: `raw_t`, `raw_h`, `cal_ver`
- Opsional saat battery valid: `bv`, `bp`

## Status Payload (aktual)

Dibangun di `buildStatusPayload()` (`src/main.cpp`).

Field saat ini:

- Konektivitas/health: `r`, `m`, `sn`, `rtc`, `o`, `fw`, `rr`, `lu`, `up`, `heap`
- Runtime config ringkas: `cm`, `zp`, `sq`, `sp`, `om`, `pp`
- Fault counter: `sc`, `wc`, `mc`
- External spool: `xs`, `xf`, opsional `xr`
- Backend state: `be`, `ba`, `bi`, `bl`
- Battery: `bok`, opsional `bv`, `bp`

## Telemetry Example

Contoh minimal:

```json
{"n":1024,"t":274,"h":681,"ts":1775268900,"bf":0,"rt":1}
```

Contoh dengan raw + battery:

```json
{"n":1024,"t":274,"h":681,"ts":1775268900,"bf":0,"rt":1,"raw_t":280,"raw_h":670,"cal_ver":2,"bv":4012,"bp":81}
```

## Status Example

```json
{"r":-61,"m":1,"sn":1,"rtc":1,"o":1,"fw":"1.0.4","rr":"POWERON","cm":0,"zp":"ambient","sq":2048,"sp":1,"lu":1775268900,"up":3600,"heap":182344,"sc":2,"wc":1,"mc":3,"om":0,"pp":0,"xs":1,"xf":0,"be":1,"ba":1,"bi":1,"bl":5,"bok":0}
```

# Storage Architecture

## LittleFS spool

- Directory: `/data`
- Files:
  - `/data/telemetry.log`
  - `/data/events.log`
  - `/data/portable_telemetry.log`
  - `/data/state.csv`
- State offset key:
  - `T`, `E`, `P` di `state.csv`

## External flash spool

- Dikelola `ExternalFlashManager` (raw SPI sector queue, bukan filesystem).
- Queue dipisah untuk telemetry/event/portable dengan slot magic:
  - `TEL1`, `EVT1`, `PRT1`
- Metadata sequence disimpan di NVS key `ex_*`.
- Saat extflash gagal runtime, spool fallback ke LittleFS otomatis.

## Spool queue format

### Telemetry line (LittleFS)

Format CSV:

- Legacy 7 field: `T,seq,ts,temp,hum,sampleOk,rtcOk`
- Baru 10 field (battery): `T,seq,ts,temp,hum,sampleOk,rtcOk,batteryValid,bv,bp`

### Event line (LittleFS)

`E,seq,ts,code,value,limit,duration,recovery,rtcOk`

## Backward compatibility

- Parser telemetry menerima 7 atau 10 field (`parseTelemetryLine`).
- Artinya record lama tanpa battery tetap bisa dibaca.
- `PERLU KONFIRMASI`: backlog telemetry yang berasal dari LittleFS tidak menyimpan `raw_t/raw_h/cal_ver` karena format spool line belum memuat field tersebut.

# Battery Monitoring

Implementasi ada di `src/battery_manager.cpp` dan dipakai di `src/main.cpp`.

- `WM_BATTERY_ENABLED` default `0` bila tidak didefinisikan.
- ADC pin:
  - pakai `WM_BATTERY_ADC_PIN` jika didefinisikan
  - fallback `Pins::BATTERY_ADC` (GPIO4)
- Divider:
  - `WM_BATTERY_DIVIDER_NUM` (default 2)
  - `WM_BATTERY_DIVIDER_DEN` (default 1)
- Kalkulasi:
  - ADC average 4 sampel
  - referensi 3300mV, 12-bit (0..4095)
  - `battery_mV = pin_mV * NUM / DEN`
- Persen baterai dipetakan linear 3200..4200mV ke 0..100.

## Battery Disabled Mode

Saat `WM_BATTERY_ENABLED=0`:

- `BatteryManager::begin()` return false.
- Sampling tidak membaca ADC battery.
- Status tetap kirim `bok=0`.
- `bv`/`bp` tidak muncul di telemetry/status.
- Boot log: `[BATTERY] disabled by WM_BATTERY_ENABLED=0`.

## Battery Enabled Mode

Saat `WM_BATTERY_ENABLED=1`:

- ADC diinisialisasi (`ADC_11db`).
- Sampling mengisi `reading.batteryValid`, `batteryMilliVolt`, `batteryPercent` bila baca valid.
- Payload telemetry/status menambahkan `bv` dan `bp` jika `batteryValid` true.
- Status `bok=1` jika valid.

## Wiring Battery Divider

BAT+ -> R1 100k -> ADC Node  
ADC Node -> R2 100k -> GND  
ADC Node -> 100nF -> GND  
ADC Node -> GPIO4

# Build Instructions

Contoh build:

```bash
pio run -e esp32dev
pio run -e esp32-s2-saola-1
pio run -e esp32-s2-saola-1-htu21
```

Env lab/test:

```bash
pio run -e test_strict_dual_sensor
pio run -e test_experimental_invalid_combo
```

# Flash Instructions

```bash
pio run -e esp32-s2-saola-1-htu21 -t upload
pio device monitor -b 115200
```

Tahan tombol saat boot (`BUTTON_BOOT_HOLD_MS`, default 2500ms) untuk force captive portal.

# OTA Instructions

Tidak ditemukan implementasi OTA firmware (`ArduinoOTA` / HTTP OTA update) di source saat ini.

- Status: belum ada mekanisme OTA firmware di codebase.
- `PERLU KONFIRMASI`: jika OTA dilakukan oleh tool/bootloader eksternal di luar repository ini.

# Troubleshooting

- Sensor tidak terbaca:
  - Cek I2C wiring sesuai board mapping.
  - Lihat serial log hasil scan I2C (`[I2C] ...`).
- MQTT tidak connect:
  - Verifikasi host/port/credential di captive portal.
  - Jika TLS verify aktif, pastikan CA cert valid di `include/certs.h`.
- External flash fallback:
  - Cek status payload `xs`, `xf`, `xr`.
  - Saat fallback, spool pindah ke LittleFS otomatis.
- Tidak ada data battery:
  - Cek `WM_BATTERY_ENABLED` dan wiring divider.

# Known Limitations

- Mapping ESP32-S2/ESP32-C3 ditandai provisional di source.
- Tidak ada OTA firmware built-in.
- Telemetry backlog dari spool LittleFS tidak menyertakan raw calibration fields (`raw_t`, `raw_h`, `cal_ver`).
- Mode `ALLOW_EXPERIMENTAL` hanya untuk lab, bukan produksi.

# Changelog Summary

- `v1.0.4`:
  - Calibration v2 (slope + offset) dengan metadata event `/m/cal`.
  - Status payload menambah metrik spool/external/backend/battery.
  - External flash spool manager + fallback runtime ke LittleFS.
  - Portable mode HTTP batch sync.
  - Battery feature compile-time gating (`WM_BATTERY_ENABLED`).
