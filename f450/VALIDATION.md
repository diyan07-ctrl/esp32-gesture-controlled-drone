# Stage 4 validation record

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
| Production dispatcher ownership/session/RELEASE/ACK logic | yes | yes | serialized JSON through shared production dispatcher | no | no |
| Actual ESP32 sockets/AP/ACK delivery | yes | yes | hardware/network untested | no | no |
| STOP/ARM token and sequence barriers | yes | yes | deterministic shared helper tests | no | no |
| Latched FAULT/FAILSAFE and explicit clearance | yes | yes | deterministic shared state tests | no | no |
| Three-consecutive ultrasonic recovery | yes | yes | interruption, stale and filter-reset tests | no | no |
| Watchdog initialization/registration/feed reporting | yes | yes | hardware behavior untested | no | no |
| Ground station | yes | Python syntax | thirteen unit tests | no | no |
| Battery monitor | no hardware installed | not applicable | not applicable | no | no |

## Executed results

`python f450/run_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe`: results below are from the Stage 4 run. Zig 0.13.0 compiles actual shared firmware headers with pinned ArduinoJson headers. Four native executables, thirteen Python tests and Python syntax passed. No hardware tests were run.

- Original native test: reference PID step/limits, mixer geometry, quaternion identity and heading crossing.
- Stage 2 native test: exact reference defaults; P/I/D calculations; integral limit; invalid dt; runtime gains; zero/positive/negative roll/pitch/yaw, combined/saturated/NaN mixer inputs; four PWM widths and invalid pulse fallback; nonzero roll/pitch/yaw references, bias, sign equivalence, invalid quaternion and heading wrap; altitude offset/range/freshness/outlier/tilt, unconfigured enable rejection, target increase/decrease, saturation, invalid-data integral freeze and bounded mode transition; actual shared JSON pilot/gain parsers, invalid payloads, atomic gain rejection, sequence wrap and communication deadline.
- Stage 3 native: STOP token barrier, delayed/duplicate ARM, stale/foreign STOP, RELEASE, sequence wrap/half-range, token exhaustion, malformed/invalid envelopes, persistent FAULT/FAILSAFE, repeated ARM, sensor recovery, explicit clearance, three-consecutive height recovery, stale/filter reset and exact PWM counts.
- Stage 4 native: valid/current-owner RELEASE; foreign/malformed/duplicate/delayed/repeated RELEASE; armed rejection; RELEASE after STOP; discovery/acquisition/resume; fresh sequence windows and wrap; retired generation replay from old/same IP; old ARM/HELLO; FAULT/FAILSAFE/STOP preservation; all control routes gated by ownership/generation before callbacks. Rejected RELEASE snapshots prove owner/order/generation/token/freshness/safety/output state unchanged.
- Python: the previous seven tests plus confirmed HELLO acquisition, successful RELEASE/reacquisition, rejected RELEASE ownership reporting, same-IP sequence synchronization/foreign owner rejection, unrelated stale ACK rejection and armed reconnect guard: **13 passed**.

PlatformIO toolchain: espressif32 6.9.0, Arduino-ESP32 2.0.17, ESP32 Dev Module, BNO055 library 1.6.4, ArduinoJson 6.21.5. Full Stage 4 compile/link/image generation passed (12.45 seconds); final confirming build and image usage are below.

Final executed summary:

```text
test_control.cpp: PASS (original PID/mixer/quaternion regressions)
test_stage2.cpp: PASS (PID, mixer, orientation, altitude, JSON and PWM)
Stage3 UDP: stop barrier, delayed/duplicate ARM, stale/foreign STOP, release, wrap, half-range, token exhaustion PASS
Stage3 envelope: missing, negative, overflow, fractional, bool, string sequence and malformed JSON rejected PASS
Stage3 safety: latched FAULT/FAILSAFE, repeated ARM, sensor recovery, STOP bypass prevention, explicit clearance and fresh ARM PASS
Stage3 ultrasonic: three consecutive valid, invalid gap, two-valid rejection, implausible, stale restart, filter reset, tilt guard PASS
Stage3 PWM: 1000/1180/1500/2000 duty counts and zero-correction unscaled mixer PASS
Stage4 RELEASE: current owner, foreign, malformed, duplicate, delayed and repeated post-release PASS
Stage4 sessions: read-only GET, fresh HELLO window, old owner traffic/ARM/HELLO, armed rejection, release after STOP, same-IP generation replay PASS
Stage4 sequence: wraparound, duplicate HELLO, half-range rejection, unrelated new window PASS
Stage4 safety: RELEASE preserves FAULT/FAILSAFE, STOP barrier survives reacquisition, fresh CMD and explicit ARM still required PASS
Stage4 authorization: all control/configuration branches reject foreign IP before callbacks; foreign STOP/DISARM remain available PASS
Ran 13 tests in 0.005s
OK
RAM:   14.2% (used 46548 bytes from 327680 bytes)
Flash: 60.6% (used 794825 bytes from 1310720 bytes)
[SUCCESS] Took 3.48 seconds
```

The strict shared envelope parser rejects concatenated objects, trailing garbage and embedded NUL suffixes while accepting JSON whitespace. Stage 3 tests exercise shared sequence/state helpers. Stage 4 tests instead enter `dispatchDatagram()` with serialized JSON and examine production ACKs and session/order state across complete RELEASE paths. The .ino calls this exact function under its existing mutex. Native adapters supply hardware/control action callbacks; they do not duplicate RELEASE/HELLO/authorization policy or simulate electrical outputs. Control-action bodies, NVS and ESP32 socket/timing behavior remain hardware work. Python tests operate the actual client lifecycle and payload methods without opening an interactive GUI. No physical UDP, bench or flight validation is claimed.

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
