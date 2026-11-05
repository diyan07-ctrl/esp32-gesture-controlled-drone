# Open-source flight-control evaluation

Reviewed 2026-11-05 against the upstream source paths linked below. Links to main
and master are moving research references, not vendored dependencies. No upstream
code is copied in this documentation change. Pin a reviewed commit, preserve
file-level notices and add provenance before adopting code. The local MIT license
does not relicense incorporated third-party code.

| Project | Control and estimation | Scheduling/mixing/safety implications | ESP32 decision |
|---|---|---|---|
| PX4 | Cascaded attitude/rate design; rate error P/I with derivative from measured angular acceleration; allocator direction limits constrain integration | Work-queue/uORB and board drivers need substantial adaptation; estimator and allocation are separate modules | Best reference for small controller boundaries; full flight stack is outside this DevKit implementation |
| ArduPilot | Attitude demand shaping and rate control; AC_PID exposes separate target/error/D filtering, integral bounds and diagnostics | AP_HAL/AP_Param/filter/scheduler dependencies are extensive; motors and vehicle failsafes are separate concerns | Use design principles; do not transplant AC_PID as a standalone header |
| Betaflight | Rate-focused controller, quaternion IMU path, gyro and D filtering, I-term management and rate scheduling integration | Fast sensor/actuator assumptions, board drivers and feature macros do not match an I2C IMU plus 50 Hz generic ESC baseline | Useful signal-path reference; neither racing gains nor kilohertz targets transfer to F450 |
| x-io Fusion | Quaternion AHRS with accelerometer rejection/recovery and explicit sample period; supports no-magnetometer operation | Portable C estimator, not flight control, mixing or failsafe | Practical candidate to benchmark against complementary baseline; not yet adopted |

## Primary-source inspection and licenses

PX4 [RateControl source](https://github.com/PX4/PX4-Autopilot/blob/main/src/lib/rate_control/rate_control.cpp)
shows directional saturation feedback and bounded finite integral updates.
[Controller diagrams](https://docs.px4.io/main/en/flight_stack/controller_diagrams)
separate estimation, rate/attitude control and allocation, and describe gyro/D
filtering. [BSD-3-Clause license](https://github.com/PX4/PX4-Autopilot/blob/main/LICENSE):
retain copyright, conditions and disclaimer in source/binary distributions; obey
the non-endorsement condition. These are architectural references, not a claim
that the local controller is PX4 or has its validation coverage.

ArduPilot [AC_AttitudeControl](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AC_AttitudeControl/AC_AttitudeControl.cpp),
[AC_PID.cpp](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AC_PID/AC_PID.cpp),
[AC_PID.h](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AC_PID/AC_PID.h)
and [AP_Scheduler](https://github.com/ArduPilot/ardupilot/blob/master/libraries/AP_Scheduler/AP_Scheduler.cpp)
make parameter, math, HAL and timing dependencies explicit. The project
[license](https://github.com/ArduPilot/ardupilot/blob/master/COPYING.txt) is GPLv3;
check individual file notices and compatibility before redistribution of derived
code. This project will independently implement the general control equations
rather than copy GPL implementation into an MIT-only distribution.

Betaflight [pid.c](https://github.com/betaflight/betaflight/blob/master/src/main/flight/pid.c),
[gyro.c](https://github.com/betaflight/betaflight/blob/master/src/main/sensors/gyro.c)
and [imu.c](https://github.com/betaflight/betaflight/blob/master/src/main/flight/imu.c)
show how tightly controller/filter behavior is coupled to measured sample timing
and configuration. The [license](https://github.com/betaflight/betaflight/blob/master/LICENSE)
is GPLv3, with inspected source notices permitting v3 or later. Study the design;
do not copy code while claiming the combined work is MIT-only.

x-io [FusionAhrs.c](https://github.com/xioTechnologies/Fusion/blob/main/Fusion/FusionAhrs.c)
uses quaternion feedback with configurable acceleration rejection and recovery.
It is a maintained Madgwick-authored AHRS implementation, not an interchangeable
name for every original Madgwick gradient-descent implementation.
[MIT license](https://github.com/xioTechnologies/Fusion/blob/main/LICENSE.md):
retain its copyright and permission notice when incorporating it. A future
vendor import must include a commit hash, unchanged license and adaptation notes.

## Recommended compatible subset

Retain the existing independently implemented angle-P/rate-PID architecture for
bounded tilt operation, improve its observability, and validate it before changing
the estimator. Preserve measured dt, derivative-on-measurement, finite checks,
integral/output bounds, disarmed reset and mixer desaturation. Parameterize all
axes separately; report P/I/D/error/rate demand and allocation limits. Directional
anti-windup is preferable to freezing all axes whenever any motor saturates, but
requires mixer-to-controller feedback tests and measured validation.

Use the MPU6050's documented hardware DLPF with a matching explicit register
profile; do not reuse the MPU6500 accel DLPF register. Collect stationary and
vibration logs before selecting extra software gyro filters. Retain the gravity-
gated complementary filter for baseline comparison, and benchmark a pinned
quaternion estimator for noise, drift, recovery, execution cost and delay on both
boards. Neither six-axis approach gives absolute yaw. MPU6050 hardware identity,
calibration and sensor-to-body signs must be validated before any controller test.

For now retain 250 Hz as a configured bench target and 50 Hz ESC output as a
separate actuator constraint. Measure sampling jitter, execution maxima and
sample-to-PWM latency under RF/USB load before claiming deterministic performance.
An MPU6050 plus generic 50 Hz ESC has different bandwidth from modern autopilot
targets. Do not reuse their gains, notch frequencies or tuning maneuvers blindly.

## Failsafe recommendation

Separate command validation, arming permission, active control, explicit stop and
latched fault handling. Laptop loss must not change flight authority. Current
link timeout produces immediate no-pulse shutdown: retain only for propeller-off
bench work. Simply replacing it with a fixed throttle ramp is not a validated
landing either. The specified sensors cannot establish height/position; no safe
autonomous landing can be claimed. Airborne loss policy requires an operating
envelope, hazard analysis, appropriate sensing/backup pilot strategy and controlled
evidence before flight enablement. No full upstream failsafe has been ported.

Tune measured inner rates first, then outer angles, with recorded airframe mass,
CG, props, pack, control frequency, filters, actuator response and saturation.
Start with props off for signs/faults; use a suitable guarded fixture for loaded
characterization. Existing gains are software bench placeholders only. See the
[tuning procedure](flight-control-algorithm.md) and
[implementation plan](master-plan.md).
