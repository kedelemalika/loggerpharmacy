# LJK Warehouse Ambient Monitor

Firmware ESP32 untuk monitoring suhu dan kelembaban gudang distributor farmasi dengan dukungan sensor per board (SHT20/AHT10), DS3231, OLED SSD1306, WiFiManager, MQTT, TLS, commissioning mode, profile zona, spool lokal, kalibrasi offset, dan health metrics.

## Ringkasan Fungsi

- Sampling suhu dan kelembaban berkala
- Waktu lokal dari RTC DS3231 dengan sinkronisasi NTP saat boot dan berkala
- OLED ringkas untuk status lapangan
- MQTT telemetry, status, availability, exception, command, dan calibration metadata
- Shared MQTT credential per site
- TLS insecure untuk testing awal
- TLS verified dengan CA certificate untuk produksi
- Offline queue RAM kecil untuk telemetry, status, dan exception
- Commissioning mode untuk validasi lapangan
- Zone profile untuk ambient, coldroom, freezer, atau custom
- Local spool dan replay backlog saat koneksi kembali
- **Calibration v2: slope + offset** dengan lightweight telemetry + event-driven metadata
- Health metrics ringkas untuk pemantauan device

## Struktur Topic MQTT

- `ljk/d/{site_id}/{device_id}/t`
- `ljk/d/{site_id}/{device_id}/s`
- `ljk/d/{site_id}/{device_id}/a`
- `ljk/d/{site_id}/{device_id}/x`
- `ljk/d/{site_id}/{device_id}/c/#`
- `ljk/d/{site_id}/{device_id}/m/cal` (calibration metadata — event-driven)

Contoh:

- `ljk/d/dcjkt/amb-01/t`
- `ljk/d/dcjkt/amb-01/s`
- `ljk/d/dcjkt/amb-01/a`
- `ljk/d/dcjkt/amb-01/x`
- `ljk/d/dcjkt/amb-01/c/set`
- `ljk/d/dcjkt/amb-01/c/do`
- `ljk/d/dcjkt/amb-01/m/cal`

## Calibration v2 — Slope + Offset

### Konsep

Firmware mendukung dua mode kalibrasi:

- **cal_ver=1 (offset-only):** `t_corr = raw_t + offset` — v1 legacy behavior
- **cal_ver=2 (slope+offset):** `t_corr = round(raw_t * slope/1000 + offset)`

Slope disimpan sebagai fixed-point X1000 (`1000` = 1.000x). Range: `[800, 1200]` = slope `[0.800x, 1.200x]`.

### Payload Strategy

| Payload | Frequency | Isi |
|---------|-----------|-----|
| **Telemetry rutin** (`/t`) | Setiap `report_interval_s` | `n,t,h,ts,bf,rt` + lightweight v2 (`raw_t,raw_h,cal_ver`) |
| **Calibration metadata event** (`/m/cal`) | Boot + setelah `c/set` kalibrasi | `ver,st,ts,to,hs,ho,rev,upd` (lengkap) |

Tidak semua metadata dikirim tiap telemetry — hanya `raw_t`, `raw_h`, `cal_ver` untuk mengurangi ukuran payload.

### Contoh Payload

**Telemetry rutin (v2 lightweight):**

```json
{"n":1842,"t":269,"h":692,"ts":1775268900,"bf":0,"rt":1,"raw_t":280,"raw_h":682,"cal_ver":2}
```

Field `t`/`h` tetap canonical corrected value. `raw_t`/`raw_h` untuk traceability.

**Calibration metadata event (event-driven, lengkap):**

```json
{"ver":2,"st":2,"ts":965,"to":8,"hs":980,"ho":12,"rev":3,"upd":1775269000}
```

Field:

- `ver`: calibration version (1=offset-only, 2=slope+offset)
- `st`: calibration status (0=uncalibrated, 1=factory, 2=field)
- `ts`: temperature slope X1000
- `to`: temperature offset X10
- `hs`: humidity slope X1000
- `ho`: humidity offset X10
- `rev`: calibration revision (increments tiap perubahan)
- `upd`: epoch update timestamp

## Contoh Payload

Telemetry:

```json
{"n":1842,"t":274,"h":682,"ts":1775268900,"bf":0,"rt":1,"bv":4012,"bp":81}
```

Status:

```json
{"r":-61,"m":1,"sn":1,"rtc":1,"o":1,"fw":"1.0.4","rr":"POWERON","cm":0,"zp":"ambient","lu":1775268900,"up":3600,"heap":182344,"sc":2,"wc":1,"mc":3,"bok":0}
```

Exception:

```json
{"n":1843,"cd":"TH","v":301,"lim":300,"dur":95,"ts":1775268995,"bf":0,"rt":1}
```

## Battery Sensing (ESP32-S2)

Default v4 saat ini adalah battery **disabled** untuk keamanan rollout:

- `WM_BATTERY_ENABLED=0`
- Firmware tidak menginisialisasi ADC battery.
- Firmware tidak membaca ADC battery.
- Status tetap publish `bok=0`.
- Telemetry tidak mengirim `bv`/`bp`.

Wiring baterai Li-ion 3.7V (full 4.2V):

- `BAT+ -> R1 100k -> titik ADC`
- `titik ADC -> R2 100k -> GND`
- `titik ADC -> GPIO ADC`
- `titik ADC -> C 100nF -> GND`

Dengan divider 100k:100k, tegangan di ADC = `Vbat / 2`.

Build flag yang dipakai di env `esp32-s2-saola-1-htu21-extflash-nocompact`:

- `WM_BATTERY_ENABLED=0` (default v4)
- `WM_BATTERY_ADC_PIN=4`
- `WM_BATTERY_DIVIDER_NUM=2`
- `WM_BATTERY_DIVIDER_DEN=1`

Untuk enable battery setelah hardware terpasang:

1. Ubah `WM_BATTERY_ENABLED=1` di env target.
2. Build dan upload ulang firmware.
3. Verifikasi payload status mengandung `bok=1` dan `bv`/`bp`.

Field payload battery:

- `bok`: battery reading valid (`0/1`)
- `bv`: battery voltage (mV)
- `bp`: battery percent (`0..100`)

### Validasi Dengan Multimeter

1. Ukur `BAT+` terhadap GND, catat `Vbat` (contoh: 3.92V).
2. Ukur titik ADC terhadap GND, pastikan ~`Vbat/2` (contoh: ~1.96V).
3. Cek data firmware:
   - `bv` harus mendekati nilai `Vbat` dalam mV (contoh ~3920).
   - Jika beda besar, cek toleransi resistor, noise, dan kalibrasi ADC.

## Board / Sensor Build Matrix

| Environment | Board | Sensor | Status |
|---|---|---|---|
| `esp32dev` | ESP32 WROOM-32D / DevKit | SHT20 | **Stable** |
| `esp32-s2-saola-1` | ESP32-S2 / LOLIN S2 Mini | AHT10 | **Stable** |
| `esp32-s2-saola-1-htu21` | ESP32-S2 / LOLIN S2 Mini | HTU21D | **Stable** |
| `esp32-c3-devkitm-1` | ESP32-C3 Super Mini | SHT20 | **Provisional** |

> **ESP32-C3 note:** Pin map (I2C SDA/GPIO4, SCL/GPIO5, button/GPIO2, buzzer/GPIO3, relay/GPIO10)
> is not fully verified against physical hardware. Some board variants may repurpose GPIO2/GPIO3
> for onboard LED or boot-strap wiring. Confirm before production flashing.

## ESP32-C3 Smoke Test

Pasang alat, flash firmware, buka serial monitor (115200). Cek baris berikut:

```
[BOOT] LJK Warehouse Monitor FW x.x.x
[BOOT] Reset reason=...
[BOOT] Boot count=...
[BOOT] C3 pins: SDA=4 SCL=5 BTN=2 BZ=3 REL=10
[SENSOR] SHT20 ready
```

**Checklist:**

- [ ] Baris `[BOOT] C3 pins:` muncul dengan nilai yang diharapkan
- [ ] Baris `[SENSOR] SHT20 ready` — bukan `init fail`
- [ ]OLED hidup, menampilkan identitas alat
- [ ] Tahan tombol GPIO2 >3 detik → captive portal aktif
- [ ] Buzzer GPIO3 berbunyi saat alarm trigger (test manual dengan set threshold rendah)

**Jika gagal:**

1. Cek SDA/SCL wiring (GPIO4/GPIO5)
2. Scan I2C: `pio run -e esp32-c3-devkitm-1 && pio device monitor` — cari `[SENSOR] init fail`
3.确认 board variant tidak pakai GPIO2/GPIO3/GPIO10 untuk onboard LED

Build per board:

```bash
pio run -e esp32dev
pio run -e esp32-s2-saola-1
pio run -e esp32-s2-saola-1-htu21
pio run -e esp32-c3-devkitm-1   # provisional
```

## Sensor Policy Mode

**Rule: Exactly one sensor per build.** Always enforced — applies in both STRICT and ALLOW_EXPERIMENTAL modes.

Policy mode only controls board+sensor compatibility checks:

| Mode | Flag | Behavior |
|---|---|---|
| **STRICT** (default) | `-DWM_POLICY_STRICT=1` | `#error` on unvalidated board+sensor combo |
| **ALLOW_EXPERIMENTAL** | `-DWM_POLICY_ALLOW_EXPERIMENTAL=1` | Warning, compile anyway (lab only) |

`ALLOW_EXPERIMENTAL` does NOT bypass the "exactly one sensor" rule.

### Validasi

```bash
# Dual sensor + STRICT   → FAIL (always enforced)
pio run -e test_strict_dual_sensor

# Dual sensor + ALLOW   → FAIL (always enforced)
pio run -e test_experimental_dual_sensor

# Invalid combo + ALLOW → PASS (single sensor, combo bypassed)
pio run -e test_experimental_invalid_combo

# HTU21 compile-only test (ALLOW, ESP32)
pio run -e test_experimental_htu21_on_esp32
```

### Experimental Runtime Warning

When firmware boots with `WM_POLICY_ALLOW_EXPERIMENTAL=1`, serial log shows at startup (once, not in loop):

```
[POLICY] EXPERIMENTAL mode active - not for production
[POLICY] board=ESP32 sensor=HTU21
```

Production builds use `WM_POLICY_STRICT` and never show this warning.

### HTU21 Compile-only Test Env

```
pio run -e test_experimental_htu21_on_esp32  # compile only, ESP32 + HTU21
```

This env keeps the HTU21 sensor path healthy in CI without requiring hardware validation.

### Error yang Diharapkan

Dual sensor (STRICT or ALLOW — always enforced):
```
#error "Multiple sensor types defined. Use only one: WM_SENSOR_TYPE_SHT20, WM_SENSOR_TYPE_AHT10, or WM_SENSOR_TYPE_HTU21."
```

Combo tidak valid (STRICT only — ALLOW suppresses):
```
#error "ESP32 + AHT10 is not validated. Validated sensors for ESP32: SHT20."
#error "ESP32-S2 + SHT20 is not supported on this board. Use -DWM_SENSOR_TYPE_AHT10=1 or -DWM_SENSOR_TYPE_HTU21=1."
#error "ESP32-C3 + AHT10 is not validated. Validated sensors for ESP32-C3: SHT20."
```

### Known Limitations

1. **ESP32-C3 hardware smoke test pending.** Pin map requires verification against actual ESP32-C3 Super Mini schematic.
2. **TLS insecure mode** (`mqtt_tls_insecure=1`) is available for commissioning initial testing only. Never use in production.
3. **Sensor recovery** (I2C bus reinit) may produce occasional data drop on edge cases. Runtime soak test recommended before final deployment.
4. **ESP32-S2 I2C bus** does not support SHT20. Use AHT10 (`-DWM_SENSOR_TYPE_AHT10=1`) or HTU21 (`-DWM_SENSOR_TYPE_HTU21=1`).
5. **ESP32-C3 I2C bus** does not support AHT10 (driver compatibility not verified). Use only SHT20 with `-DWM_SENSOR_TYPE_SHT20=1`.
6. Compile-time guard enforces "exactly one sensor" and board<->sensor pairing. Dual sensor builds always fail. Invalid combos fail in STRICT mode.

## Sinkronisasi Waktu

- RTC DS3231 tetap dipakai sebagai sumber waktu lokal utama
- Jika RTC lost power atau invalid, firmware akan fallback ke `build time` agar timestamp tidak mulai dari tahun 2000
- Setelah WiFi tersambung saat boot, firmware akan mencoba sinkronisasi NTP lalu menulis ulang waktu ke DS3231
- Saat runtime, firmware akan melakukan resync NTP berkala untuk mengurangi drift RTC
- Payload telemetry, status, dan exception tetap memakai epoch UTC sehingga tetap kompatibel dengan backend dan frontend

## Upload

```bash
pio run -t upload
```

Serial monitor:

```bash
pio device monitor
```

## Cara Masuk Captive Portal

- Otomatis jika WiFi belum tersimpan
- Tahan tombol sesuai board saat boot untuk force config portal
- Long press tombol sesuai board saat alat berjalan untuk masuk config portal

Mapping tombol per board (didefinisikan di `include/pins.h`):

- `esp32dev` (ESP32 WROOM-32D): GPIO27
- `esp32-s2-saola-1` (ESP32-S2): GPIO21
- `esp32-c3-devkitm-1` (ESP32-C3 Super Mini): GPIO2 *(provisional — confirm wiring)*

## Parameter Captive Portal

- `site_id`
- `device_id`
- `mqtt_host`
- `mqtt_port`
- `mqtt_username`
- `mqtt_password`
- `mqtt_use_tls (1/0)`
- `mqtt_tls_insecure (1/0)`

Validasi yang diterapkan:

- `site_id` tidak boleh kosong
- `device_id` tidak boleh kosong
- `mqtt_host` tidak boleh kosong
- `mqtt_port` tidak boleh nol
- `report_interval_s` minimal 10
- `sample_interval_s` minimal 5
- `status_interval_s` minimal 60
- `alarm_delay_s` minimal 5

## TLS Insecure

Untuk commissioning awal:

- `mqtt_use_tls = 1`
- `mqtt_tls_insecure = 1`
- `mqtt_port = 8883`

Mode ini memakai `setInsecure()` dan hanya cocok untuk testing.

## TLS Verified

Untuk produksi:

1. Buka `include/certs.h`
2. Ganti `MQTT_CA_CERT` dengan root CA asli broker
3. Pastikan:

- `mqtt_use_tls = 1`
- `mqtt_tls_insecure = 0`
- `mqtt_port = 8883`

## Cara Aktifkan Production TLS Dengan Cepat

Opsi runtime:

- di captive portal set `mqtt_use_tls=1`
- set `mqtt_tls_insecure=0`
- isi CA cert asli di `include/certs.h`

Opsi default firmware:

- edit `DEFAULT_TLS_PRODUCTION_MODE` di `include/config.h`

## Commissioning Mode

Commissioning mode dipakai saat instalasi awal, validasi alat, atau tuning threshold.

Default:

- `commissioning_mode = 0`
- `commissioning_log_interval_s = 10`

Jika aktif, firmware akan mencetak log periodik seperti:

```text
[COMM] T=27.0C H=69.2% ok=1 alarm=0 rssi=-61 heap=182344 up=3600 zone=ambient
```

Mode ini:

- tidak mematikan MQTT
- tidak mematikan alarm
- tidak mengubah payload telemetry utama
- hanya menambah log commissioning periodik

Offset yang aktif akan langsung mempengaruhi nilai commissioning, OLED, alarm, telemetry, dan spool.

## Zone Profile

Profile zona yang didukung:

- `ambient`
- `coldroom`
- `freezer`
- `custom`

Default profile:

- `ambient`

Default threshold per profile:

- `ambient`
  - `temp_low_x10 = 150`
  - `temp_high_x10 = 300`
  - `hum_low_x10 = 300`
  - `hum_high_x10 = 750`
- `coldroom`
  - `temp_low_x10 = 20`
  - `temp_high_x10 = 80`
  - `hum_low_x10 = 300`
  - `hum_high_x10 = 750`
- `freezer`
  - `temp_low_x10 = -250`
  - `temp_high_x10 = -150`
- `custom`
  - memakai threshold yang sudah ditentukan user

Catatan:

- profile tidak memaksa overwrite threshold setiap boot
- profile baru hanya akan menerapkan default saat diminta lewat command

## Contoh Command MQTT

Set parameter:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"ri\":60,\"ad\":120,\"th\":300,\"tl\":150,\"hh\":750,\"hl\":300}"
```

Aktifkan commissioning mode:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"cm\":1,\"cli\":10}"
```

Matikan commissioning mode:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"cm\":0}"
```

Set offset kalibrasi (legacy, v1 style):

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"to\":-4,\"ho\":10}"
```

Arti:
- `to=-4` suhu dikurangi `0.4C`
- `ho=10` RH ditambah `1.0%`

Range valid:
- `to=-100..100`
- `ho=-200..200`

Set calibration v2 (slope + offset):

```bash
# Set calibration version 2, field-calibrated (st=2), slope 0.965x, offset +0.8C
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"cal_ver\":2,\"cal_status\":2,\"cal_ts\":965,\"cal_to\":8,\"cal_hs\":980,\"cal_ho\":12}"
```

Range valid calibration v2:
- `cal_ver`: {1, 2}
- `cal_status`: {0, 1, 2} (0=uncalibrated, 1=factory, 2=field)
- `cal_ts`/`cal_hs`: [800, 1200]
- `cal_to`: [-150, 150]
- `cal_ho`: [-250, 250]

Legacy field precedence:
- `cal_to`/`cal_ho` baru mengambil prioritas jika dikirim
- Jika hanya `to`/`ho` dikirim, firmware map ke `cal_to`/`cal_ho`

Set zone profile ambient dan apply default threshold:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"zp\":\"ambient\",\"ap\":1}"
```

Set zone profile custom tanpa overwrite threshold aktif:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/set -m "{\"zp\":\"custom\",\"ap\":0}"
```

Publish status:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "s"
```

Display on:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "d"
```

Force config portal:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "f"
```

Reboot:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "r"
```

Mute buzzer alarm:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "m"
```

Unmute buzzer alarm:

```bash
mosquitto_pub -h mqtt.ljkwarehouse.com -p 8883 -u dcjkt -P "password" -t ljk/d/dcjkt/amb-01/c/do -m "u"
```

## Dokumentasi Serial Log

Prefix yang dipakai:

- `[BOOT]` boot count, firmware version, dan ringkasan config
- `[WIFI]` captive portal, hasil connect, reconnect
- `[MQTT]` connect broker, publish, queue
- `[TLS]` mode TLS, CA cert, dan masalah handshake
- `[SENSOR]` inisialisasi dan gagal baca sensor
- `[RTC]` inisialisasi RTC
- `[CMD]` command MQTT yang diterima
- `[ALARM]` alarm dan recovery
- `[RECOVERY]` sensor atau I2C recovery action
- `[CAL]` calibration state change dan metadata publish

Contoh log normal:

```text
[BOOT] LJK Warehouse Monitor FW 1.0.4
[BOOT] Reset reason=POWERON
[RTC] DS3231 ready
[SENSOR] AHT10 ready
[WIFI] Auto connect start, host=dcjkt-amb-01, portal=auto
[TLS] Insecure TLS mode enabled
[MQTT] Connected to mqtt.ljkwarehouse.com:8883 (TLS-INSECURE, queue=0)
[BOOT] System init complete
```

## Checklist Commissioning 1 Alat

1. Cek wiring sesuai board (`pins.h`) termasuk SDA/SCL dan tombol
2. Flash firmware dan buka serial monitor
3. Pastikan OLED hidup dan menampilkan identitas alat
4. Masuk captive portal bila perlu
5. Isi `site_id`, `device_id`, broker, username, password, dan mode TLS
6. Aktifkan commissioning mode jika perlu dengan `{"cm":1,"cli":10}`
7. Pastikan OLED menampilkan `W:OK` dan `M:TLS` atau `M:OK`
8. Pastikan topic availability retained menjadi `1`
9. Pastikan telemetry dan status diterima broker
10. Uji zone profile yang sesuai area alat
11. Jika perlu, set offset kalibrasi `to` dan `ho` tanpa reflash
12. Pastikan status payload menampilkan `lu`, `up`, `heap`, `sc`, `wc`, dan `mc`
13. Uji command `s`, `d`, `f`, `r`, `m`, dan `u`
14. Catat `device_id`, lokasi alat, zone profile, offset, dan hasil commissioning
