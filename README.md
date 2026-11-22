# F450 ESP32 engineering bench project

This project explores onboard attitude estimation and motor mixing for an F450 quadcopter. **AIRCRAFT NOT FLIGHT-READY.** The installed project uses one ESP32 and one MPU6050; no glove is in use. Laptop WASD/UDP remains experimental and Wi-Fi AP startup has an unresolved brownout blocker.

Start with [PROJECT_CLOSURE.md](PROJECT_CLOSURE.md) for status, source inventory and evidence, then the [technical handover](docs/handover.md) for wiring, equations, procedures and remaining work. Earlier gesture, BNO055 and ultrasonic designs are archival.

## Hardware and power

Owner reports an F450 frame, four A2212/13T 1000KV motors, four nominal 30A ESCs, 10-inch matched CW/CCW props, ESP32 DevKit V1, MPU6050, soldered PDB and 3S 2200mAh LiPo. Battery C-rating is unresolved: owner says 40C; earlier image reportedly shows 50C. No confirming label image or measurement is included in this closure.

```text
3S battery -> soldered PDB -> four ESCs -> three phases per motor
                         -> adjustable buck set to 5V -> ESP32 USB-C
ESP32 GND <-> ESC signal grounds / regulator ground
ESP32 SDA21 / SCL22 <-> MPU6050 (3.3V-safe logic and verified supply)
```

The power bank was removed for mass reduction from an initial reported 1.6kg; the new all-up mass is unknown. Buck current/transient/USB-C compatibility and power-path/backfeed checks remain open. Never apply battery voltage to ESP32 or MPU6050, parallel BEC positives, or connect laptop USB and buck power without verifying isolation. See the [current BOM](docs/bill-of-materials.md).

Top view, forward at the front: **FL GPIO26 CW; FR GPIO25 CCW; RL GPIO32 CCW; RR GPIO33 CW**. These are owner-reported directions, requiring independent physical verification. Remove all props, isolate propulsion during assembly/upload, inspect soldering/polarity/insulation, establish signal grounds, then verify sensor identity and axes on electronics power first.

## Reproducible software baseline

The default root target is **`drone_sensors`**: MPU6050 serial diagnostics only, ESC pins inputs, no radio or motor controller. It is the supported source baseline for the installed sensor, not a recreation of the missing owner bench sketch. Builds and host checks establish software behavior only.

| Source / target | Toolchain | Status |
|---|---|---|
| Root `drone_sensors` | Arduino ESP32 2.0.17 / espressif32 6.9.0 | Supported MPU6050 sensor-only baseline |
| Root `glove`, `drone`, `motor_test`, `glove_sensors` | Same pinned 2.0.17 | Archival glove/ESP-NOW or separate guarded motor fixture; four output gates false |
| Root `drone_bench`, `drone_pid_bench` | Same pinned 2.0.17 | Archival BNO055 diagnostics/shadow; no actuator authority |
| `f450` | Same pinned 2.0.17; BNO055 1.6.4 / ArduinoJson 6.21.5 | Archival BNO055/HC-SR04 UDP design; outputs inhibited |
| Arduino ESP32 3.x | Different pin-based LEDC API | No maintained or verified 3.x target in this checkout; do not claim a 3.x build |
| Reported MPU6050 motor bench, 1000–1300µs, nominal 1250µs | Source/core/gains not supplied | Owner observation only; not reproducible from repository |

Arduino 2.x uses `ledcSetup(channel,...)`, `ledcAttachPin(pin,channel)`, `ledcWrite(channel,...)`. Arduino 3.x removes the first two in favor of `ledcAttach(pin,...)` and pin-based writes; watchdog APIs also differ. See the [Espressif 2.x to 3.0 migration guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html). Porting requires a separately validated adapter. No 3.x claim is inferred from successful 2.x compilation.

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.18 ziglang==0.13.0
.\.venv\Scripts\python.exe -m platformio run
.\.venv\Scripts\python.exe tools/run_host_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe
.\.venv\Scripts\python.exe -m platformio pkg install -d f450
.\.venv\Scripts\python.exe f450/run_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe
node tools/check_repository.mjs
```

Linux CI uses g++ for root tests and c++ for F450 tests; omit `--zig`. Node 22 and Python 3.11/3.12 are used for checks. Optional assumed-plant simulation: install [simulation/requirements-core.txt](simulation/requirements-core.txt), then `python -m simulation.validate --skip-host --output build/sil/closure`. Logs and build artifacts belong under ignored `build/`.

## Results and limits

Owner reports MPU6050 angle/yaw-rate telemetry, dry-run pitch/roll/yaw mixing, audible speed changes with props removed, and shutdowns on invalid IMU data, attitude and timing limits. These observations are not RPM, thrust or flight stabilization measurements. The corresponding motor-bench source, trace data, active gains and 30-second timeout implementation are absent; acceptance remains open. Existing BNO055 captures and simulation reports retain their own revision labels.

Repeated Wi-Fi AP brownouts occurred even at reduced TX power; a basic no-Wi-Fi sketch works according to the owner. This remains an electrical blocker. No flight PID tuning, thrust margin, ESC scope timing or loaded power characterization has been established. Issues [#1](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/1), [#2](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/2), [#3](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/3) remain flight blockers.

[Executed closure checks](docs/closure-validation.md) distinguish local tests, exact-commit CI and physical tests. [Historical README](README.historical.md), historical records in `docs/`, [F450 archive](f450/README.md), and [simulation](simulation/README.md) remain available for attribution and regression work. License: [LICENSE](LICENSE); upstream reference attribution is preserved in the F450 archive. Verified source files contain no course/group roster to add here.
