# Project engineering closure — 2026-11-22

**SOFTWARE/DOCUMENTATION HANDOVER COMPLETE, AIRCRAFT NOT FLIGHT-READY.** This describes the documented source handover, not physical acceptance of the aircraft or recovery of the missing owner bench sketch. Execution/CI status is recorded in [closure validation](docs/closure-validation.md); pending checks prevent merge until resolved.

## Scope delivered

One [entry README](README.md), current [hardware BOM](hardware/bom.csv), and [technical handover](docs/handover.md) reconcile the installed MPU6050 aircraft, power/mass design, motor map, source status, control mathematics, test procedures, observations, brownout investigation and roadmap. Historical source/logs, reference snapshots, licensing and attribution remain available. Historical documents carry revision-context notices. No physical flight claim, new measurement or image is added.

Software changes retain two existing development commits absent from main, test their production JSON dispatcher, correct the archival F450 yaw signs explicitly, inhibit its propulsion outputs, remove its public AP password from runnable configuration, guard allocation failures, check the legacy drone watchdog feed, make sensor-only diagnostics the default, and harden laptop enable/telemetry/token/stop behavior. CI now builds/tests the previously omitted F450 archive. The unrelated pre-existing `ground_station/shadow_dashboard.py` modification remains outside closure commits.

## Source/version inventory

| Source | Version / role |
|---|---|
| Inspected main | [5db3032de3267e9f05559821ceba10cc5c91ebd1](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/commit/5db3032de3267e9f05559821ceba10cc5c91ebd1), merged PR #6; no open PRs at inspection |
| Existing safety/protocol commits | Original 7540243 and ef1e51e; carried as aed9709 and 9551f43 from main without rewriting history |
| Root `drone_sensors` | Supported current-sensor baseline; MPU6050, no Wi-Fi/ESC/PID authority |
| Root `firmware/` other targets | Historical MPU6050 glove/ESP-NOW controller, guarded fixture, BNO055 diagnostics/shadow; build separately |
| `f450/` | Archived BNO055/HC-SR04/UDP controller; corrected owner yaw convention, inhibited outputs; not installed hardware |
| `ground_station/` | Historical read-only BNO055 UDP/shadow codecs/monitors; not current motor-bench control |
| `simulation/` | Native firmware plus assumed physics/sensor/radio models; no aircraft validation |
| Reported MPU6050 motor bench | Exact source/hash, active gains, limits implementation and traces absent; recovery remains required |
| Build versions | PlatformIO 6.1.18, espressif32 6.9.0, Arduino ESP32 2.0.17; Zig 0.13.0 native Windows; Python 3.11.9 local, 3.12 CI; Node local 24.22.3, CI 22 |
| Arduino ESP32 3.x | No maintained target or executed build; incompatible LEDC/watchdog APIs require a reviewed port |

## Verified hardware and software status

Owner-reported current hardware: F450, four A2212/13T 1000KV motors, four 30A ESCs, 10-inch props, one ESP32 DevKit V1, one MPU6050, soldered PDB, 3S 2200mAh LiPo, adjustable buck set to 5V feeding ESP32 USB-C. No glove, BNO055 or ultrasonic sensor is currently in use. FR25 CCW, FL26 CW, RL32 CCW, RR33 CW is the owner-reported top-view map. It is reconciled in source; independent harness/rotation/feedback measurements remain open.

Prior power bank removed from initial reported 1.6kg assembly; new all-up mass unknown. C-rating unresolved: owner says 40C; prior image reportedly 50C; no label image was available for independent closure inspection. Buck compatibility, USB backfeed, power transients, current/thermal limits, prop matching and thrust margin are unverified.

Owner reports MPU6050 angles/yaw rate, dry-run axis mixing, audible motor-speed response with props off, and invalid-IMU/attitude/timing shutdowns in a 1000–1300µs, nominal 1250µs bench run with 30-second limit. These are reports, not newly executed tests, source-verifiable coefficients or instrument measurements. Existing BNO055 telemetry/shadow logs describe another sensor/revision. Flight stabilization, tuned gains, loaded power and ESC timing are not established.

## Test evidence

The [test table](docs/closure-validation.md) lists exact commands, results, failures/fixes, CI links and exclusions. Host mocks, loopback UDP and assumed-plant simulation are labeled separately. No board was flashed, powered or armed for this closure. ESP32 physical fault/reboot tests, oscilloscope PWM/rail measurements, loaded thrust/current/thermal tests and flight tests: **NOT EXECUTED**.

## Solved and remaining work

Solved in source/documentation: conflicting current-hardware claims, unsafe default architecture selection, omitted archival CI coverage, explicit archived motor-yaw convention, published AP credential, client token replay/telemetry freshness, session/fault recovery regressions and traceable handover instructions.

| Risk / issue | Disposition | Missing acceptance evidence |
|---|---|---|
| [#1 Hardware compatibility](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/1) | OPEN, flight blocker | Exact ESC/motor/battery/board/regulator identities; C label; new mass; power/backfeed/scope/current/thrust/thermal validation |
| [#2 Airborne failsafe](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/2) | OPEN, flight blocker | Operating envelope, sensing, validated in-flight link/battery failure/recovery policy; prop-off cutoff is insufficient |
| [#3 Sensor/control/radio/actuator measurement](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/3) | OPEN, flight blocker | Exact current bench source/gains/traces; IMU axes/noise; timing; actual radio loss/restart; ESC edge and controller-response measurements |
| Wi-Fi AP brownouts | Unresolved electrical blocker, tracked within #1/#3 | Owner reports repeated resets even with reduced TX; basic no-Wi-Fi sketch works; measured cause/fix absent |
| Missing current motor-bench artifact | Reproducibility blocker within #3 | Source, core, active P/I/D (including any zero terms), saturation, 30s deadline and STOP/reset traces |

No issue is closed and no redundant issue is created. Software interlocks cannot substitute for these criteria.

## Safe handover and roadmap

Begin with the root sensor-only build, props removed and propulsion isolated. Verify power paths, board identity, I2C rail/WHO_AM_I, calibration and all three body signs. Recover/version the exact bench sketch before repeating motor work. Resolve supply brownouts with measured input/3.3V transient and current traces. Identify/scope ESC protocol and fault/reset behavior before guarded propulsion characterization. Obtain mass/thrust/current/temperature evidence, then separately design and validate airborne failsafe and control tuning. Follow the full [handover procedure](docs/handover.md); do not enable output flags simply to make a test pass.

## Final source and review record

Branch: `closure/engineering-handover`. Exact final source commit and PR/merge/CI links are recorded below after checks and PR creation. A commit cannot contain its own SHA without changing that SHA; the immutable final head is therefore identified by the PR head/merge record, while subsequent documentation records can name the tested implementation commit explicitly.

Final implementation commit: pending final check/commit. PR: pending creation. Merge: pending exact-head CI and policy review. This section must be completed before delivery; it is not evidence that pending checks passed.
