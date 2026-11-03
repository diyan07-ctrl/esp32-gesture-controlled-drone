# Attitude, PID and Quad-X control

Body axes are FRD: X forward, Y right, Z down. Positive roll lowers the right side, positive pitch raises the nose, positive yaw is clockwise viewed from above. Mount the sensor raw X forward, Y left, Z up. The driver maps acceleration to a gravity proxy (-rawX,+rawY,+rawZ), and angular rate to FRD (+rawGX,-rawGY,-rawGZ). Verify each sign physically.

MPU6500 identity is 0x70 at WHO_AM_I 0x75; I2C address 0x68 is a separate concept. The driver selects ±8 g, ±500°/s, gyro and accel DLPF setting 3, 1 kHz internal filtered sample output (divider 0), sampled by the target 250 Hz application. Register readback, I2C errors, data-ready and numeric/range checks reject failures. At boot, after 100 ms warmup, 1000 consecutive level/stationary samples estimate gyro bias and one-position accelerometer offsets against (0,0,1). Movement resets accumulation; no successful calibration within 15 s faults. This cannot establish six-face scale, misalignment, temperature compensation or true physical level.

The complementary estimator integrates body rates through Euler kinematics and blends accelerometer roll/pitch with a .5 s time constant only when acceleration magnitude is 0.85–1.15 g. Dynamic acceleration can still corrupt tilt. Pitch beyond ±60° and invalid dt fail estimation. Yaw has no absolute reference and drifts; no magnetometer is specified. Yaw commands use rate, not integrated heading.

Outer roll/pitch error × configurable angleKp (initial 4) produces bounded rate demand (±90°/s). Inner roll/pitch rate and yaw-rate PID use measured angular velocity. Derivative acts on measurement with a 20 ms low-pass; bounded conditional integration prevents local windup and freezes on prior-cycle mixer saturation. Default kp=.001, ki=.0002, kd=.00001, output limit=.2 are **bench starting values, not validated flight gains**. All axes instantiate independently and reset while disarmed/near-zero throttle. Use real measured timing, vibration and plant response to tune. validDt currently supports 2–6 ms; changing cfg::periodUs outside the nominal 4 ms contract requires updating timing validation and tests.

Owner numbering, viewed from above:

~~~text
                  FRONT
     M2 FL / CW             M1 FR / CCW
       GPIO26                 GPIO25
               ESP32 + IMU
     M3 RL / CCW            M4 RR / CW
       GPIO32                 GPIO33
~~~

| Motor | Unsaturated contribution |
|---|---|
| M1 front-right CCW | T - R + P + Y |
| M2 front-left CW | T + R + P - Y |
| M3 rear-left CCW | T + R - P + Y |
| M4 rear-right CW | T - R - P - Y |

Motor reaction yaw opposes propeller spin. Positive roll increases left motors; positive pitch increases front motors; positive yaw increases CCW motors. Configured sign arrays and GPIO array are the source of truth. Confirm thrust direction as well as spin. Desaturation uniformly scales differential demands if their span exceeds one, then shifts collective into the feasible interval, preserving differential ratios. Output clamps to [0,1]. Saturation changes commanded collective; it is not a model of available thrust. ESC PWM clamps to configured endpoints and invalid/disarmed output is zero duty.

## Tuning sequence

1. Props off: verify IMU axis signs, the complete GPIO/position/spin mapping and correction direction against the equations.
2. Measure loop timing/jitter and PWM edges under radio/log/range load; validate every failsafe before propulsion work.
3. Identify ESC/motor response and thrust/current on an appropriate guarded fixture; establish safe idle/endpoints and actuator latency.
4. Tune inner rates first using measured response and bounded inputs; begin with low gains, characterize oscillation and damping, then add limited integral and derivative only as supported by logs.
5. Tune outer angles more slowly than rate loops; test saturation, integrator recovery, yaw coupling and disturbances. Record gains, mass/CG, prop/pack, timestamps and plots.
6. Flight acceptance requires independently reviewed hardware, power, control signs, failsafe behavior and supervised controlled tests. Assumed-plant convergence cannot substitute for these.

HC-SR04 provides filtered distance only: reject zero/timeout and outside 0.02–4 m, low-pass .25 per accepted sample, mark stale after 300 ms. Its effective range, tilt/ground effects, rotor noise and update rate require measurements. No altitude-hold controller is implemented.

Sources: [TDK MPU6500 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf), [Espressif pinned LEDC API](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-ledc.h), [HC-SR04 module datasheet](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf).
