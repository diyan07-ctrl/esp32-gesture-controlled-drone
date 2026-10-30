# Flight-control algorithm

The in-tree MPU driver v0.1.0 verifies WHO_AM_I=0x68 and reads back wake/clock, DLPF, divider, gyro and accelerometer settings. It reads DATA_RDY before a 14-byte burst. It rejects I2C errors, missing fresh data and near-range saturation. Repeated identical numbers alone cannot prove a stuck sensor; a future redundancy/fault-injection review is required. No external IMU library is used.

At startup, 1000 consecutive stationary samples (about 4 s at target rate) estimate gyro bias. Movement resets accumulation; 15 s without completion faults. Acceptance requires norm near 1 g and each rate <3°/s. This is not six-face accelerometer calibration or robust temperature compensation.

The estimator uses gyro integration with Euler body-rate conversion and complementary accelerometer roll/pitch correction. Gravity proxy is normalized implicitly by atan2 (uniform scale cancels); its norm gates correction to 0.85–1.15 g. alpha=τ/(τ+dt), τ=0.5 s. This does not reject all translational acceleration, particularly horizontal accelerations that preserve norm. Rates are degrees/s, angles degrees, dt seconds. Euler validity is limited to modest tilt; |pitch|>60° faults. Yaw is integrated and drifts. No absolute heading or altitude estimate exists.

Outer angle error×4/s becomes bounded roll/pitch rate targets. Inner PIDs implement proportional error, bounded integral contribution, derivative on measurement (avoids setpoint kick), first-order 20 ms derivative filtering, conditional integration and output limit ±0.2. Prior-cycle mixer clipping freezes integration. Gains in control.h are **unvalidated bench starting values**, never tuned flight gains. Integral state resets on disarm and low throttle. No automatic idle speed is assumed.

Quad-X mixing for FRD coordinates:
```text
M1 front-left  CW  = T + R + P - Y
M2 front-right CCW = T - R + P + Y
M3 rear-right  CW  = T - R - P - Y
M4 rear-left   CCW = T + R - P + Y
```
Positive roll increases left thrust, positive pitch increases front thrust, positive yaw increases CCW prop thrust (CW reaction on airframe). Rotation means viewed from above. Clamp each command to [0,1] and reject non-finite inputs. This simple allocation gives no optimal control priority: clipping distorts collective and torques. Anti-windup mitigates accumulation but does not recover lost authority. A validated desaturation allocator is future work.

Target control rate 250 Hz matches filtered sensor configuration; pilot commands 50 Hz, status 10 Hz, serial snapshots ≤5 Hz. Maximum permitted interval is 6 ms; minimum 2 ms. Target wake jitter is ≤0.5 ms for bench evaluation, not a measured guarantee. Record wake jitter externally and internally. Fault on timing violation or ≥4 ms execution. Hardware LEDC keeps pulse timing independent of loop scheduling; the provisional 50 Hz frame still creates command-to-actuator delay requiring stability analysis.

Unit tests exercise the actual shared header implementation; hardware driver, RTOS scheduling, ESC edge timing and plant stability require physical tests. See [output gate](output-enablement.md).
