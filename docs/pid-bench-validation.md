# BNO055 PID shadow validation and motor-test gate

This milestone uses the **actual BNO055** on the drone ESP32 and the existing
`flight::Stabilizer`, `flight::Pid` and `flight::mix` code. The separate
`drone_pid_bench` image runs on the ESP32, reads BNO055 samples in a 20 ms sensor
task and evaluates the controller at a measured 4 ms target. It neither includes
`esc.h` nor calls LEDC. GPIO25/26/32/33 are set to INPUT at startup. Motor mode
is deliberately `MOTOR_LOCKED`. All propellers remain removed.

## What the image measures

- BNO055 `CHIP_ID=0xA0` at the observed address `0x29`, acceleration in g,
  angular rates in degrees/s, WXYZ quaternion, derived sensor-frame roll,
  pitch and **relative** yaw, and calibration state.
- The BNO055 operates in Bosch IMU mode, using its own six-axis fusion. No
  second complementary filter is applied to its fused quaternion. No
  additional smoothing is inserted in this bench image; filtering delay
  therefore belongs to the BNO055 internal configuration and has not yet been
  characterized against an independent angle reference.
- The existing outer angle P controller generates bounded roll/pitch rate
  targets. Three existing rate PID instances use derivative on measurement,
  a 20 ms derivative filter, conditional integral update, saturation limits
  and the previous mix-saturation state. The gains remain unchanged.
- The X-frame mixer computes four **virtual normalized motor commands**. They
  are software values, not ESC PWM, thrust, RPM or evidence of motor motion.

The BNO055 values have not yet been transformed into verified F450 body axes.
This image feeds sensor-frame roll/pitch and gyro axes to the controller as a
**provisional sign hypothesis**. It can expose software timing and arithmetic
faults, but its corrections must not be used for propulsion control until a
timed physical axis mapping and motor map are independently verified. Holding
the frame by hand cannot measure closed-loop airframe stabilization.

## Build and run on Windows PowerShell

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.18 pyserial==3.5
$env:PLATFORMIO_CORE_DIR=Join-Path (Get-Location) '.cache/platformio'
.\.venv\Scripts\python.exe -m platformio run -e drone_pid_bench
.\.venv\Scripts\python.exe -m serial.tools.list_ports
.\.venv\Scripts\python.exe -m platformio run -e drone_pid_bench -t upload --upload-port COM9
.\.venv\Scripts\python.exe -m ground_station.shadow_monitor --port 14550 --duration 30 --jsonl build/hardware/pid-shadow.jsonl
```

The bench image uses the same ignored `firmware/common/local_wifi.h` as the
existing drone bench. Configure a compatible 2.4 GHz network and laptop IPv4
destination as documented in [BNO055 bench setup](drone-bno055-bench.md). Do
not commit the real Wi-Fi header. The new read-only
[`shadow_dashboard.py`](../ground_station/shadow_dashboard.py) is a local
Tkinter monitor with attitude and PID graphs, four calculated-motor bars,
calibration/fault/timing status and a disabled motor button:

```powershell
.\.venv\Scripts\python.exe -m ground_station.shadow_dashboard --port 14550
```

USB diagnostic commands are **not motor commands**. From another terminal:

```powershell
.\.venv\Scripts\python.exe tools/shadow_command.py shadow --port COM9
.\.venv\Scripts\python.exe tools/shadow_command.py roll_right --port COM9
.\.venv\Scripts\python.exe tools/shadow_command.py pitch_forward --port COM9
.\.venv\Scripts\python.exe tools/shadow_command.py neutral --port COM9
.\.venv\Scripts\python.exe tools/shadow_command.py motor_locked --port COM9
```

`sensor` and `stop` return to sensor-only calculation. `motor_locked` resets
PID state and calculated motors to zero. Reboot starts in sensor mode and never
arms. The dashboard sends no command of any kind. For reproducible source
tests, run `python tools/run_host_tests.py`, `node tools/check_repository.mjs`
and `python -m unittest simulation.test_sil -v` after rebuilding the SIL bridge.

## Read-only telemetry contract

The new `PS` v1 datagram is 194 bytes, little endian, with CRC16 CCITT-FALSE
over bytes 0–191. It carries mode, sequence, uptime, PID interval, IMU errors,
setpoint/measurement/error/P/I/D/output/rate target/measured rate for each axis,
four calculated motor values, accelerometer, gyroscope, quaternion, calibration
and sensor validity. Reserved physical-output bytes 190–191 must be zero;
the Python decoder rejects nonzero values. It reports `esc_pwm_us=None` and
`actual_motor_operation=None`. The existing `FT` v2 BNO055 bench telemetry and
ESP-NOW formats remain unchanged in their original images. The shadow image
does not consume glove commands.

## Mixer calculation table

These are **software-only** results from the existing configured X-frame mixer,
using virtual collective 0.35 and correction 0.05. Columns M1–M4 correspond to
the repository's *unverified physical* mapping FR/FL/RL/RR on GPIO25/26/32/33.

| Case | M1 | M2 | M3 | M4 |
|---|---:|---:|---:|---:|
| Zero correction | .35 | .35 | .35 | .35 |
| Roll +.05 | .30 | .40 | .40 | .30 |
| Roll −.05 | .40 | .30 | .30 | .40 |
| Pitch +.05 | .40 | .40 | .30 | .30 |
| Pitch −.05 | .30 | .30 | .40 | .40 |
| Yaw +.05 | .40 | .30 | .40 | .30 |
| Yaw −.05 | .30 | .40 | .30 | .40 |
| Roll/pitch/yaw +.05 | .40 | .40 | .40 | .20 |
| Collective 0 | 0 | 0 | 0 | 0 |
| Collective 1 | 1 | 1 | 1 | 1 |
| Saturation: T .95, R/P/Y .2 | 1 | 1 | 1 | .20 |

The [physical UDP capture report](pid-bench-capture-2026-11-05.md) gives actual
sensor angles, calculated PID outputs and mixer values. It also records a
telemetry defect found and fixed during the on-device run. The controller
return value and gains were unchanged.

## Executed validation on 2026-11-05

| Gate | Actual result | Scope |
|---|---|---|
| Seven PlatformIO targets | PASS: glove, drone, motor_test, glove_sensors, drone_sensors, drone_bench, drone_pid_bench | Compilation only; only PID shadow image flashed this milestone |
| Native host suite | PASS, including 44 new PID shadow/mixer/codec assertions and three Python shadow decoder tests | No physical ESC or RF load |
| Existing SIL regression | PASS: 21 deterministic tests, zero scenario failures, six documented limitations | Uses assumed plant, not this BNO055 hardware |
| Repository checks | PASS: 833 static assertions | Includes no-LEDC/no-ESC rule for PID shadow image |
| New image upload | PASS: COM9, ESP32-D0WD-V3, flash hash verified | ESC GPIOs configured INPUT |
| BNO055 identity | PASS: 0x29 / CHIP_ID 0xA0; calibration became valid | Breakout rail and axis mounting unmeasured |
| Sensor-only UDP | 396 valid packets in first 12 s, 0 invalid, 101 missing sequence numbers; later 168 sensor-mode packets used in the report | First run had a startup/reception gap; no synchronized latency |
| Fixed PID-output telemetry | 195 received shadow packets; maximum absolute `output − (P+I+D)` 1.70e-9 | Numerical parity; not stabilization proof |
| Setpoint sequence | 551 valid packets, 0 invalid/missing; output table in capture report | Board held approximately still; no motor command |
| Manual tilt | 794 valid packets, 0 invalid, 1 missing; sensor-frame roll −52.36° to −1.33°, pitch −37.55° to +26.99° | Physical movement phases not timestamped; body signs not confirmed |
| Motor locked | 100 valid packets, all calculated motors zero and ESC output state disabled | No oscilloscope verification of GPIO voltage |
| Individual motors | NOT RUN | ESC identity, startup/no-signal behavior and electrical gate incomplete |

The new Tkinter dashboard was smoke-tested by creating and updating a local Tk
window. Live screenshot/visual layout acceptance was not recorded. The on-device
control interval in the received tilt samples was 3987–4007 µs; this excludes
unreceived packets and does not establish worst-case timing under motor power.

## ESC protocol and physical motor-test gate

The repository's `motor_test` target contains a **candidate** 50 Hz,
1000–2000 µs LEDC adapter, but that prior software is not an ESC datasheet.
The owner reports the installed ESC signal range as 1000–2000 µs, 3S LiPo
ESC power and a checked shared signal ground. These are user-reported checks;
no oscilloscope trace or ESC manual was supplied. Manufacturer, exact model,
accepted startup/minimum pulse, no-signal response and BEC-positive wiring
remain unidentified. Battery polarity/voltage, regulated ESP32 supply,
BEC-positive isolation, frame/motor restraint and an accessible physical
battery disconnect have not been independently verified. The owner confirmed
propellers are physically removed.

**Result: ESC protocol verification FAILED / unavailable; individual motor
tests NOT RUN.** There are no actual ESC commands, PWM measurements, motor
responses, temperatures, rotation directions or stop-response results to
report. Do not flash `motor_test` on the unidentified powered ESCs. A physical
motor test can begin only after identifying the ESC model/manual and recording
the electrical, fixture and no-signal checks above. Test one motor at a time
with a verified conservative command and accessible battery disconnect. Never
infer flight readiness from motor spin.

## Remaining control work

1. Measure BNO055 breakout power, pullups, calibration repeatability,
   sensor-to-body signs and fusion delay against an independent reference.
2. Check the physical GPIO-to-ESC-to-motor map and actual rotation directions.
3. Identify each ESC and verify signal protocol, endpoints, startup and
   no-signal shutdown with a scope before any motor command.
4. Verify the full electrical and mechanical safety checklist, then perform
   one-motor, propeller-free tests with documented observations.
5. Repeat SIL with a characterized vehicle and measured actuator response
   before proposing gain changes. This shadow test does not identify stable
   flight gains or airborne failsafe behavior.
