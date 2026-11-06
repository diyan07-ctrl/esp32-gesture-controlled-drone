# Stage 2 engineering change report

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
