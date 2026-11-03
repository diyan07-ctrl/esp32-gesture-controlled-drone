# Validation results — 2026-11-03

All results below were actually executed locally against this implementation. Hardware results are deliberately separate. The earlier 2026-10-30 tests described a different MPU6050/potentiometer baseline.

## Firmware builds

Executed: python -m platformio run -e glove -e drone -e motor_test. All three returned success; the final log contained no compiler warning: or error: diagnostics.

| Target | Result | Static RAM bytes | Flash bytes |
|---|---|---:|---:|
| glove | PASS | 44,320 / 327,680 | 768,797 / 1,310,720 |
| drone | PASS | 43,872 / 327,680 | 760,269 / 1,310,720 |
| motor_test | PASS | 21,560 / 327,680 | 271,793 / 1,310,720 |

Environment: Windows PowerShell, Python 3.11, PlatformIO Core 6.1.18, Espressif32 6.9.0, Arduino framework 3.20017.241212+sha.dcc1105b (Arduino ESP32 2.0.17), esptool 4.5.1, Xtensa 8.4.0+2021r2-patch5. Default private pairing absent, output gates false, optional ultrasonic/battery/switch paths disabled. Numbers therefore describe default images, not a provisioned output-enabled aircraft. Wi-Fi station mode is initialized for boot MAC diagnostics. Runtime heap/stack, real deadlines and edges remain unmeasured.

## Native tests and simulation

Executed: python tools/run_host_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe. Exit 0. Project C++ uses C++17, Wall/Wextra/Werror/pedantic. Zig version 0.13.0. Initial bundled runtime bootstrap emitted libunwind dllexport warnings from Zig's own sources; subsequent project builds did not emit C++ diagnostics. The retained legacy Wno-misleading-indentation option is from the original test setup; source is now consistently formatted with clang-format 18.1.8.

| Executable/check | Actual result |
|---|---|
| core_tests | PASS, 1,553 assertions: estimator, PID anti-windup, mixer, protocol CRC/ranges/session/sequence/wrap, safety and legacy interlocks |
| sensor_gesture_tests | PASS, 49 assertions: measured/inverted/invalid flex normalization, filtering, every default gesture, debounce, emergency latch, range timeout/health, PWM boundaries, v2 sensor fields |
| imu_driver_tests | PASS, 3,013 assertions against mocked I2C: wrong identity, readback mismatch, I2C/data-ready failure, raw conversion, saturation, interrupted level calibration and bias correction |
| calibration_storage_tests | PASS, 17 mocked serial/NVS assertions: capture, arm lockout, save/load, partial-write invalidation, open failure and erase |
| pipeline_tests | PASS, 501 mocked command-to-safety-to-PWM integration assertions: valid handshake/arm, corruption, duplicates, loss boundary, emergency, disarm, sensor/timing faults and non-rearm |
| Python utilities | PASS, 4 unittest cases: battery/divider/parser/invalid inputs |
| roll_model | PASS, finite bounded toy rate model; final rate 3.61019°/s, angle 20.2055° under assumptions |
| closed_loop | PASS, 62,727 assumed-plant steps/saturation cases; three-axis signs, both directions, convergence and bounds |
| Repository checker | PASS: local links/fences, default gates, motor GPIO/sign/CSV/SVG consistency, task output/receive contracts, pinned targets/CI, BOM IDs, secret patterns and generated-file checks |
| git diff --check | PASS: no whitespace errors |

Total native C++ unit assertions: 4,632; mocked integration assertions: 501. Counts are assertions/steps, not independent hardware test cases. Simulation omits real inertia/thrust/ESC latency/aerodynamics/vibration/sag/coupling and cannot establish F450 stability. Host tests do not execute real RF, GPIO, I2C, ADC, RMT, LEDC, NVS flash or FreeRTOS scheduling. Hardware adapters compile and IMU/NVS behaviors are exercised through mocks; hardware behavior remains unverified.

## Failures encountered and resolved

- Restricted toolchain/GitHub access: reran required commands with approved access. No credentials were printed or committed.
- Git ownership mismatch during authenticated fetch: used a process-scoped safe.directory exception for this exact workspace, without changing global trust. Fetch confirmed local/remote main were aligned before committing.
- No native unit/integration/simulation assertion or firmware build failure remained in the final local run. PowerShell wraps unittest stderr as NativeCommandError text when piping; the test runner exited 0 with all four tests OK.

## Not executed / hardware dependencies

All ten ordered hardware stages, actual wireless transport, sensor calibration on devices, PWM/trigger/echo scope traces, loaded power/battery tests, individual motors under new firmware, timing/jitter, guarded thrust, PID tuning and flight: **NOT EXECUTED**, because this session did not access the physical boards/instruments. The owner reports previous COM9 detection, 4 MB flash and individual ESC/motor tests; these are historical, not this firmware's validation evidence. HC-SR04 is confirmed, but proposed GPIO16/17 and Echo protection remain unverified. Motor numbering/GPIO/spins are owner-confirmed, not independently measured.

Known issues: immediate-cut airborne crash risk; no independent actuator watchdog or controlled landing; plausible stuck-sensor faults; yaw drift; no altitude hold; provisional gains/endpoints/thresholds; optional battery monitor disabled. Full details are in [known limitations](known-limitations.md). Before flight follow [hardware procedure](testing-and-validation.md), [calibration](calibration-guide.md), [tuning](flight-control-algorithm.md) and [output gates](output-enablement.md).

## GitHub publication

Target: [diyan07-ctrl/esp32-gesture-controlled-drone](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone), existing main branch. Authenticated Git access is available. Final commit hashes, push and actual CI outcome are recorded after publication; until then this paragraph is not a claim of push/CI success.
