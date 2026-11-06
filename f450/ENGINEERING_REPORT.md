# Stage 4 protocol correction report

Stage 4 starts at 75402433eba659c6ef4d9ee0924cb55a4ac5e9ad and changes only protocol/session handling, client compatibility, tests and documentation. Hardware pins, mixer, PWM conversion/frequency, PID defaults, 4 ms loop, sensor/reference/altitude math and NVS persistence are preserved.

`Dispatcher.h` is now the production JSON command router called under the existing firmware mutex. It owns acquisition, authorization, sequence checks, STOP/DISARM exceptions and RELEASE. The .ino retains its existing control-action bodies behind an adapter. Native Stage 4 tests feed serialized JSON through this same router and inspect ACKs and state snapshots; there is no separate mock RELEASE policy.

Previously RELEASE bypassed sequence validation and cleared the window before replay checks. It now checks active owner IP, matching session generation, disarmed status and strictly forward sequence before any mutation. Rejection is side-effect free. Success retires the generation, clears owner/order/fresh-command state and rotates the arming token without changing safety state or outputs. Old traffic cannot reclaim the empty slot or affect a later session.

There was no HELLO procedure in Stage 3: implicit acquisition allowed stale packets to claim ownership. Stage 4 therefore adds GET discovery → HELLO acquisition/resume and a uint32 `session` field on owner commands. An empty slot admits a fresh initial sequence only through a matching-generation HELLO. Current-owner resume advances its existing window. Session IDs never reuse within a boot; exhaustion requires reboot. This necessary wire-protocol extension is implemented in the laptop at startup and through Connect / resume. Stage 3 clients must be upgraded.

The GUI sends pilot packets only after a correlated successful HELLO ACK, synchronizes sequence on same-IP resume, handles valid/rejected RELEASE without assuming success, ignores uncorrelated ownership ACKs, and preserves keyboard, throttle, altitude and tuning controls. Reconnecting an armed session is blocked. Only a fresh valid owner/session CMD renews the communication deadline; neither queries nor handshake traffic substitutes for pilot freshness. Fault latches and explicit fresh ARM remain required.

See VALIDATION.md for actual Stage 4 runs, image sizes and verification categories, and docs/testing-and-validation.md for the complete per-command authorization audit. Native adapters stand in for electrical I/O and control preconditions; actual UDP sockets, NVS hardware, timing and motors still need bench verification. No flight claim is made.

## Historical Stage 2 report

Historical Stage 2 record. Stage 3 sequence barriers, latched recovery policy and ultrasonic recovery supersede the earlier behavior; see `docs/testing-and-validation.md` and `VALIDATION.md` for the current revision.

The existing branch and controller architecture were corrected in place. Hardware pins, access-point settings, original PID defaults, source snapshots and legacy implementation are preserved.

## Files

Modified: `F450_ESP32_FlightController/F450_ESP32_FlightController.ino`, `Control.h`, `f450_ground_station.py`, `test_ground_station.py`, `README.md`, `VALIDATION.md`.

Added: `FlightSupport.h`, `Protocol.h` inside the existing sketch folder; `test_stage2.cpp`, `run_tests.py`, and this report. No second flight-controller sketch was introduced.

## Preserved mathematics

| Controller | Kp | Ki | Kd |
|---|---:|---:|---:|
| Roll angle | 1.5 | 0 | 0.6 |
| Pitch angle | 1.5 | 0 | 0.6 |
| Roll rate | 0.7 | 0 | 0.01 |
| Pitch rate | 0.7 | 0 | 0.01 |
| Yaw rate | 2 | 12 | 0 |

Trapezoidal integral, error-difference derivative, ±400 integral/output limits and nominal 4 ms scheduling remain intact. Invalid dt/input is now rejected. Yaw remains rate control. Altitude gains are a separate addition with no reference attribution or proposed proven nonzero defaults.

## Corrections

Removed the redundant 1.024 conversion. Motor calculations stay in µs until the single `pulseToDuty()` hardware boundary. All four pulses are validated before writing; invalid calculations trigger all-motor cutoff. An Arduino core-major guard prevents accidental compilation against incompatible 3.x APIs. Core 2.0.17 supports the selected LEDC calls, and LEDC initialization failure blocks arming.

Mixer signs retain the specified FRD geometry: +roll lowers right lift/increases left lift; +pitch increases front/decreases rear; +yaw decreases CW/increases CCW. Automated tests check zero, six directions, combined corrections, saturation and invalid inputs. Reference >2000→1989 saturation is retained. Physical rotor/propeller directions remain unverified.

The added height estimator associates echoes with triggers, handles timeout, validates range/tilt/IMU freshness, applies median-of-three and 0.15 s low-pass filtering, and reports health/age/rejections. With aligned body-down sensing and a level startup reference, height is D*cos(roll)*cos(pitch)+4.03 cm.

Altitude PID runs only on new valid measurements, using measurement delta time. It clamps integral/output and rolls back integration that drives collective saturation. Enabling captures current collective, starts the internal target at measured height, and ramps toward the requested target. Collective slews at 100 µs/s, target at 30 cm/s. Invalid/stale data never integrate and cause cutoff during hold. PID configuration plus explicit disarmed ALT_VALIDATE acknowledgement is required; that acknowledgement is cleared by updates/reset/reboot and is not proof of physical stability.

Manual mode receives optional continuous CMD.throttle in µs from slider/Up/Down. Altitude mode ignores it. MANUAL adopts current collective and clears altitude history; GUI follows the reported value. R/F remain altitude commands. SPACE neutralizes pilot axes while holding collective; STOP overrides ramps with cutoff. Old clients omitting throttle retain the last manual demand.

Startup still uses inverse(q_initial)*q_current, 200 stationary samples and gyro bias averaging. Shared code normalizes quaternions, aligns signs and rejects >30° inter-sample jumps after capture. Reference is not recaptured. Mount logical X forward/Y right/Z down; with that mounting the sensor-to-body transform is identity. Referencing cannot fix incorrect mounting axes, scale errors, drift or vibration.

Explicit BOOT/SENSOR_INITIALIZATION/STARTUP_REFERENCE/DISARMED/ARMING_CHECKS/ARMED/FAILSAFE/FAULT states and arming-block telemetry explain availability. Duplicate ARM no longer resets an armed controller. Invalid PID updates leave gains unchanged. Sensor, timing, link/range and invalid-output paths cut motors. Battery reports NOT_INSTALLED. Freshness timestamps are now evaluated after taking the mutex, avoiding unsigned underflow when a concurrent sensor update was newer than the control timestamp.

## Results and remaining work

Two native executables and five Python tests passed. Full firmware compilation/link/image generation passed: 46,532 bytes RAM, 791,597 bytes flash. VALIDATION.md classifies each feature separately and records reproduction commands, quantized PWM widths and the physical checklist. No bench or flight validation is claimed. Exact ESC specifications, real PWM, mounting/signs, sensor fault handling, loop jitter, Wi-Fi delivery, altitude tuning and controlled flight remain physical work.
