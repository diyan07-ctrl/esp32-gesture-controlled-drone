# Drone BNO055 communication bench

This is the current **drone-only** image. The glove MPU6050 firmware, historical
MPU6050 `drone` image, motor-test image and PID configuration are preserved.
`drone_bench` has no ESC interface, arming path, motor mixer or flight mode.
GPIO25/26/32/33 are set to `INPUT`; leave every ESC and motor disconnected.

## Hardware and coordinate limits

| Connection | ESP32 DevKit V1 | Evidence |
|---|---|---|
| BNO055 SDA | GPIO21 | Owner reported; I2C scan found `0x29` |
| BNO055 SCL | GPIO22 | Owner reported; scan and identity read succeeded at 100 kHz |
| BNO055 GND | ESP32 GND | Owner reported connection; continuity not measured |
| BNO055 VCC | Breakout-compatible regulated rail | Exact breakout, rail voltage and pullups not measured |
| BNO055 ADR | Unknown | Address observed `0x29`; actual pin level not measured |
| BNO055 INT | Not used | Polling at a 50 Hz target |
| ESC1–4 signal | GPIO25/26/32/33 reserved | Firmware sets inputs; no ESCs connected |

The driver checks BNO055 `CHIP_ID=0xA0`, searches `0x28` and `0x29`, and selects
Bosch **IMU mode** (accelerometer and gyro fusion, no magnetometer correction).
Its quaternion, roll and pitch are **sensor-frame diagnostics** until mounting and
axis signs are physically verified. Yaw is not an absolute heading. Gyro and
accelerometer calibration bits must both reach 3 before the estimate is marked
valid. Neither these bits nor a stationary reading prove flight suitability.
The BNO055 internal fusion is a black box; no custom complementary filter runs
in this image. A sustained acceleration and vibration acceptance test remains.

```text
ESP32 DevKit V1                 BNO055 breakout
GPIO21 (SDA) ------------------ SDA
GPIO22 (SCL) ------------------ SCL
GND --------------------------- GND
verified compatible rail ------ VCC

GPIO25/26/32/33 = INPUT; no ESC or motor connection in this test.
```

Inspect the actual breakout's input regulator and I2C pullup rails before
relying on the reported VCC connection. Do not put a raw 3S pack on either
board. The USB test uses the ESP32's USB supply.

## Build, provision and monitor on Windows PowerShell

Python 3.11+ and PlatformIO 6.1.18 were used locally. From repository root:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.18 pyserial==3.5
.\.venv\Scripts\python.exe -m platformio run -e drone_bench
```

Create `firmware/common/local_wifi.h` from
[`local_wifi.example.h`](../firmware/common/local_wifi.example.h), setting the
2.4 GHz network SSID/password, the laptop IPv4 destination and UDP port. The
real header is gitignored. Never paste credentials into committed source or
test output. `configuration/config.h` sets the expected ESP-NOW radio channel
(currently 1). The actual AP channel must match it. A different channel
prevents command acceptance; it is not silently changed. Provision encrypted
ESP-NOW pairing separately via the [existing protocol guide](communication-protocol.md)
when a second board becomes available. Unprovisioned builds initialize the
ESP-NOW subsystem but cannot exchange commands.

The default PlatformIO environment is now `drone_bench` to avoid selecting the
historical MPU6050 controller by accident. Explicit targets remain available.
Check the port before upload:

```powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports
.\.venv\Scripts\python.exe -m platformio run -e drone_bench -t upload --upload-port COM9
.\.venv\Scripts\python.exe tools/capture_drone_serial.py --port COM9 --reset --seconds 15 --output build/hardware/drone-serial.txt
```

Substitute the actual COM port. `--reset` toggles the USB serial control line
to capture startup messages; it is not needed for ordinary streaming. Check
`I2C_FOUND`, `BNO055_INIT`, `STATE`, `WIFI`, `ESPNOW` and `DIAG` lines. A
stationary calibration should reach `calibrated=1`. Do not infer a valid
orientation from a single sample; inspect stability and movement.

Start the read-only laptop listener on the same network:

```powershell
.\.venv\Scripts\python.exe -m ground_station.udp_listener --port 14550 --duration 15 --require-packets --jsonl build/hardware/drone-udp.jsonl
.\.venv\Scripts\python.exe -m unittest ground_station.test_udp -v
```

The UDP listener has no serial or motor command path. It reports no packet,
valid packet, invalid packet and stale telemetry or network loss. Inter-arrival
rate is a local receive statistic, **not** one-way network latency. JSONL
contains decoded received values and remains ignored under `build/`.

## State and wireless behavior

States are BOOT, IMU_INITIALIZATION, IMU_CALIBRATION,
WIRELESS_INITIALIZATION, WAITING_FOR_GLOVE, READY_SENSOR_LINK_ONLY,
COMMUNICATION_LOST, SENSOR_FAULT and SYSTEM_FAULT. `READY_SENSOR_LINK_ONLY`
cannot arm an aircraft. Every state transition is printed. Calibration or
sensor failure prevents the ready state. Invalid commands cannot change the
receiver's stored command; a command timeout is 200 ms. The shared ESP-NOW
receiver validates length, MAC, CRC, version, ranges, session and sequence.
Without a provisioned second ESP32, the expected state is WAITING_FOR_GLOVE.
ESP-NOW initialized is not evidence of two-board packet delivery.

Wi-Fi station and ESP-NOW use the same ESP32 radio/channel. The bench image
waits for station association before ESP-NOW initialization when Wi-Fi is
configured, checks the connected channel against the configured peer channel
and reports them separately. If association never succeeds, UDP cannot be
observed and ESP-NOW initialization remains pending. Wi-Fi reconnects are
counted. A radio/channel change rejects command processing. UDP sends
approximately every 20 ms when connected; failures are counted. USB `DIAG`
prints approximately every 200 ms.

## Actual hardware observations on 2026-11-05

| Check | Observed result | Limit |
|---|---|---|
| Board/port | CP210x USB serial on COM9, ESP32-D0WD-V3 rev 3.1 | Port may change |
| Flash | `drone_bench` compiled and flashed; esptool verified image hash | No motor hardware attached |
| I2C and identity | One responding device at `0x29`, BNO055 CHIP_ID `0xA0` | Breakout power/pullups not measured |
| Fusion sample | After settling, calibration byte `0x3C`, accel near 1 g, small gyro rates, sensor-frame roll/pitch available | Board mounting/axis signs not verified |
| Wi-Fi | Station connected on channel 1; board and laptop were on the same network | No RF stress or reconnection test |
| UDP run 1 | 698 valid v2 packets over 14 s, 0 invalid, 0 missing | Local arrival about 50 Hz; no synchronized latency |
| First movement window | **Invalid capture:** no serial lines and no UDP packets for 16 s although COM9 remained listed | Capture tool left RTS asserted, holding the ESP32 in reset; motion response unmeasured |
| Recovery | Controlled USB reset restored boot, sensor and Wi-Fi; 584 valid UDP packets over 12 s, 0 invalid/missing | Reset is not proof of automatic link recovery |
| Passive capture after tool fix | 20 diagnostic lines in 4 s; uptime progressed past 27 s with no boot line | Confirms serial open no longer reboots the ESP32 |
| Repeated physical tilt | 81 USB diagnostics and 797 valid UDP packets over 16 s; 0 invalid/missing; sensor-frame roll −33.55° to +16.09°, pitch −33.50° to +20.52° | Confirms live attitude changes, not sensor-to-airframe sign mapping |
| Tilt-window timing | Sample intervals 20,000–21,006 µs, execution 5,214–12,312 µs, max-since-boot 12,591 µs, 0 missed deadlines | 50 Hz bench target; no loaded-control timing claim |
| Controlled board reboot | UDP uptime reset from 246,834 to 2,781 ms; 3.422 s arrival gap, then valid packets resumed; 333 valid, 0 invalid, 3 missing in 10 s | Tests board reboot/reassociation, not an RF-only disconnect |
| ESP-NOW | Initialized; no configured peer and no packet exchange | Second ESP32 unavailable |
| Outputs | `outputs=DISABLED`; pins configured INPUT | No electrical scope measurement |

The serial capture utility was corrected to set DTR/RTS low **before** opening.
The initial movement capture is invalid because the tool reset the board. The
repeated tilt capture confirmed live changes, but the exact hand movement phases
were not timestamped, so sensor-frame sign and mounting alignment remain open.
The transient startup path briefly entered
SENSOR_FAULT before settling; the
sensor error counter reached 3 and later samples were valid. This warrants a
repeatability and I2C integrity check, especially given the unexplained
movement-window dropout. No physical tilt direction or angle range is claimed.

## Next hardware acceptance

1. Inspect and measure BNO055 breakout supply, SDA/SCL pullups and ground;
   add strain relief to USB and sensor wiring.
2. Repeat a labeled, timed movement test to map physical tilt direction to
   sensor-frame sign; characterize rate response, dropout and reset behavior.
3. Hold the board stationary for a timed drift/noise run; record all calibration
   bits, errors, timing and any invalid samples.
4. With a second ESP32, provision an encrypted peer and test real command
   reception, duplicates, corruption, timeout and reconnect on channel 1.
5. Verify Wi-Fi-only interruption/reassociation and channel mismatch. Measure packet
   intervals and CPU budget under both radios active.
6. Only after the IMU and communication benchmarks pass, define a measured
   sensor-to-body transform and revisit control integration separately.

Bosch register/scaling source: [BNO055 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bno055-ds000.pdf).
Radio channel source: [Espressif ESP-NOW documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/network/esp_now.html).
