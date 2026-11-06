# Stage 2 validation record

Executed 2026-11-06. No firmware was uploaded. No hardware, interactive GUI or flight validation was performed.

| Feature | Implemented | Compile-verified | Software-test-verified | Bench-verified | Flight-validated |
|---|---|---|---|---|---|
| Reference PID defaults/equations/limits | yes | yes | yes | no | no |
| FRD mixer directions/combined/limits/invalid | yes | yes | yes | no | no |
| PWM conversion and invalid cutoff | yes | yes | math only | no | no |
| Four physical LEDC outputs | yes | yes | conversion only | no | no |
| Startup quaternion reference and bias | yes | yes | synthetic samples | no | no |
| BNO055 I2C acquisition/disconnect handling | yes | yes | inspection only | no | no |
| Filtered vertical height and freshness | yes | yes | yes | no | no |
| Interrupt echo acquisition/timeout | yes | yes | inspection only | no | no |
| Altitude PID/ramp/saturation/mode transition | yes | yes | yes | no | no |
| Manual collective/mode selection | yes | yes | controller/packet tests | no | no |
| UDP pilot/gain parsing/sequence/deadline | yes | yes | shared helpers | no | no |
| ESP32 socket ownership/AP/ACK delivery | yes | yes | laptop ACK handler only | no | no |
| States/arming/timing cutoff/watchdog | yes | yes | deadline helper only; transitions inspected | no | no |
| Ground station | yes | Python syntax | five unit tests | no | no |
| Battery monitor | no hardware installed | not applicable | not applicable | no | no |

## Executed results

`python f450/run_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe`: **PASS** for two native executables, five Python tests and Python syntax. Zig 0.13.0 compiles the actual shared firmware headers with pinned ArduinoJson headers, not a replacement Python model.

- Original native test: reference PID step/limits, mixer geometry, quaternion identity and heading crossing.
- Stage 2 native test: exact reference defaults; P/I/D calculations; integral limit; invalid dt; runtime gains; zero/positive/negative roll/pitch/yaw, combined/saturated/NaN mixer inputs; four PWM widths and invalid pulse fallback; nonzero roll/pitch/yaw references, bias, sign equivalence, invalid quaternion and heading wrap; altitude offset/range/freshness/outlier/tilt, unconfigured enable rejection, target increase/decrease, saturation, invalid-data integral freeze and bounded mode transition; actual shared JSON pilot/gain parsers, invalid payloads, atomic gain rejection, sequence wrap and communication deadline.
- Python: numeric rejection, JSON/sequence wrap, PID acknowledgement, throttle field, manual-mode collective adoption: **5 passed**.

Full PlatformIO compile/link/image generation: **PASS**, espressif32 6.9.0, Arduino-ESP32 2.0.17, ESP32 Dev Module, BNO055 library 1.6.4, ArduinoJson 6.21.5. Final image uses **46,532 bytes RAM**, **791,597 bytes flash**.

| Requested µs | Duty count | Calculated nominal µs |
|---:|---:|---:|
| 1000 | 3277 | 1000.061035 |
| 1180 | 3867 | 1180.114746 |
| 1500 | 4915 | 1499.938965 |
| 2000 | 6554 | 2000.122070 |

Period=20,000 µs; duty=round(us×65536/20000). Error ≤0.152588 µs from count quantization. These are calculations, not scope measurements. The redundant 1.024 factor is removed.

## Reproduction

```text
python -m platformio pkg install -d f450
python f450/run_tests.py
python -m platformio run -d f450
```

Runner uses `c++` on PATH or accepts `--zig PATH`. Windows GCC can fail with CreateProcess errors when long include paths exceed subprocess command limits. This session used a temporary `subst P:` mapping to the repository, PLATFORMIO_CORE_DIR=`P:\.cache\platformio`, then built `P:\f450`. The mapping was removed. A short checkout path also avoids this issue. No drive letter is embedded in source/configuration.

## Required physical checks

Identify exact ESC make/model; generic “30A” does not establish support for 50 Hz/1000–2000 µs. Scope all four signals at cutoff, idle, 1500, 2000 and after STOP/fault/watchdog reset. Verify motor ordering, rotation, props, BNO axes and corrective feedback signs. Test stationary reference capture, startup movement, nonzero initial orientation and IMU disconnect. Check known distances/tilts, echo timeout, surfaces and rotor airflow. Test actual AP/UDP ownership, duplicate/invalid/stale packets, acknowledgement delivery, PID save/reboot and state recovery. Measure loop jitter under traffic. Use controlled fixtures for collective response, mode transitions and altitude tuning before flight evaluation.

ALT_VALIDATE is an operator acknowledgement of reviewed gains, not an automated bench-test result. Zero altitude defaults remain explicitly unconfigured until entered and reviewed. Snapshot SHA256 remains `2E672F6156BA0EE8827CA9D77BFC78405ED8EBE2FB872EFE08EEBCB57C716496`; reference snapshots and legacy code were not modified.
