# Sensor migration execution report — 2026-11-05

Branch: development/f450-ground-station. This milestone implements the confirmed
MPU6050/pot target, stopping before further flight-control integration. No output
gate, PID gain, ESC adapter, motor_test source, drone control task or command-v2
transport/codec was changed. Main was not modified.

## Implemented and changed files

| Files | Change |
|---|---|
| firmware/common/imu.h | MPU6050 identity/reset/register readback, raw counts, clipping/errors, bias calibration/read-gap reset; Arduino macro collision fix |
| firmware/common/configuration/config.h | Dedicated pot36, sensor/filter/threshold config and pin/parameter assertions; flight gates remain false |
| firmware/common/control.h, flex.h | Configurable complementary/gravity gating and flex time constant; angular-wrap correction |
| firmware/common/potentiometer.h | Measured endpoints, inversion, faults/deadband, bounded-rise filter, stable endpoint capture |
| firmware/common/gesture.h | Opt-in pot mode with low-target/release arm guard; retained legacy algorithm for existing regression tests |
| firmware/glove/src/main.cpp, calibration.h | Pot acquisition, target/filtered separation, flex-only throttle disabled, neutral capture, guarded explicit versioned NVS save |
| firmware/sensor_diagnostics/src/main.cpp | Independent glove/drone sensor diagnostics, raw/calibrated health/attitude/ADC records, no ESC/radio initialization |
| platformio.ini, .github/workflows/firmware-ci.yml | Separate sensor-only images and five-image CI matrix |
| tests/unit/imu_driver_tests.cpp, calibration_storage_tests.cpp, sensor_migration_tests.cpp | Sensor register/scale/fault/bias, save failure, pot, hysteresis and estimator regressions |
| tests/mocks/Arduino.h, Wire.h | Actual Arduino word macro behavior and short-read fault injection |
| tools/run_host_tests.py, check_repository.mjs | Run new suite; assert profile/pins/default gates/diagnostic target coverage |
| README.md, docs/sensor-migration.md | Current wiring/calibration/diagnostics/acceptance contract |
| docs/{calibration-guide,connection-tables,gesture-control,flight-control-algorithm,system-architecture,build-and-upload,telemetry,testing-and-validation,bill-of-materials}.md | Reconciled current target and operator instructions |
| docs/master-plan.md, hardware-audit-2026-11-05.md | Link original inspection to subsequent implementation |
| hardware/bom.csv, pin-mapping.csv; diagrams/*.mmd and motor-layout.svg | MPU6050/pot inventory, GPIO and diagram labels; electrical verification still pending |

## Actual local compilation

Executed PlatformIO Core 6.1.18 using Python 3.11.9 on Windows, Espressif32 6.9.0,
Arduino ESP32 2.0.17, esp32dev. All five independent environments returned SUCCESS
and overall exit 0. Final corrected build log had no warning: or error: diagnostics.
Local build log: ignored build/sensor-migration-build.log. No generated binaries
or private provisioning are committed. Default pairing absent; all output gates false.

~~~powershell
$env:PLATFORMIO_CORE_DIR=Join-Path (Get-Location) '.cache/platformio'
./.venv/Scripts/python.exe -m platformio run -e glove -e drone -e motor_test -e glove_sensors -e drone_sensors
~~~

| Target | RAM bytes / 327,680 | Flash bytes / 1,310,720 | Actual result |
|---|---:|---:|---|
| glove | 44,424 | 771,461 | PASS |
| drone | 43,872 | 760,613 | PASS |
| motor_test | 21,560 | 271,793 | PASS |
| glove_sensors | 22,348 | 316,289 | PASS |
| drone_sensors | 21,924 | 296,641 | PASS |

## Actual automated logic results

Executed Python host runner with Zig 0.13.0 and project C++17 warning-as-error flags:
`./.venv/Scripts/python.exe tools/run_host_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe`.
Exit 0. Includes Arduino word macro in the mock to exercise the corrected decoder.

| Test | Actual result |
|---|---|
| Shared core | 1,553 assertions PASS |
| Existing sensor/gesture/PWM | 49 assertions PASS |
| MPU6050 mocked I2C/calibration | 3,042 assertions PASS |
| Mocked serial/NVS calibration | 49 assertions PASS, all nine save-write failure positions |
| Pot/flex/gesture/dual estimator | 2,356 assertions PASS |
| Mocked command/safety/PWM integration | 501 assertions PASS |
| Closed-loop assumed plant | 62,727 steps/saturation cases PASS |
| Toy roll model | Finite/bounded under assumptions; not aircraft evidence |
| Python utilities | 4 tests, OK |

Unit assertions total 7,049, plus 501 mocked integration assertions. These are
assertions, not independent hardware test cases. Tests cover wrong identities,
every configuration readback, signed FRD conversion, NACK/short/not-ready reads,
clipping, calibration interruption/bias/isolation, low/high/inverted/invalid pot,
noise capture rejection, hysteresis, high-pot arm prevention and emergency latch.
Both logical estimator instances pass synthetic tilt/rate checks; neither physical
IMU has passed its bench acceptance test yet.

Repository checker passed 553 assertions across 100 files. `git diff --check`
passed. Explicit Git diff checks confirmed no changes in motor_test, esc.h,
drone/src/main.cpp, protocol or transport; rate gains and all four false output
gates are preserved. C++ edits were formatted with clang-format 18.1.8.

## Issues encountered and resolved

The initial build completed with narrowing warnings revealing that Arduino's
function-like word macro intercepted the old byte-decoding helper. It was renamed
signedWord and the mock now includes that macro; the corrected builds/tests pass.
This was a real source defect that previous mocks did not expose; historical
compilation never established valid hardware readings.

The previous design-stage report described an apparently missing Python base.
This turn established that the runtime is present: sandbox denial produced the
misleading startup message. Approved execution reports Python 3.11.9 and permits
the full local builds/tests. No interpreter installation was needed.

## Remaining physical tests and integration gate

No firmware uploaded, serial port opened, sensor calibrated on a board, RF link
tested or motor actuated in this milestone. Physical read stability, identity,
sensor-to-body signs, known-angle accuracy, vibration/temperature/drift, timing,
ADC protection/endpoints/noise, real NVS power interruption, wiper faults and
full-travel safety remain **NOT EXECUTED**. GPIOs are software-consistent, not
continuity/voltage-verified. Follow [separate glove/drone acceptance](sensor-migration.md).

Both physical IMU subsystem records must pass before further flight-control
integration. Existing bench-only fault cutoff, provisional PID gains, unverified
ESC/power/propulsion and drifting yaw remain flight blockers. No flight-readiness
or measured latency claim is made.
