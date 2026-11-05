# Actual drone BNO055 PID shadow capture

Generated from ignored local UDP JSONL logs. Values are sensor-frame measurements or software calculations; **no ESC signals were sent**.

Sensor-mode packets: 168. Setpoint-run packets: 551.

| Mode | Roll target ° | Pitch target ° | Samples | Mean roll PID | Mean pitch PID | Mean calculated M1 | M2 | M3 | M4 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| PID_SHADOW | +0.0 | +0.0 | 87 | -0.0039 | 0.0631 | 0.4170 | 0.4092 | 0.2830 | 0.2907 |
| PID_SHADOW | +5.0 | +0.0 | 117 | 0.0274 | 0.0593 | 0.3820 | 0.4367 | 0.3181 | 0.2632 |
| PID_SHADOW | -5.0 | +0.0 | 116 | -0.0124 | 0.0637 | 0.4261 | 0.4013 | 0.2739 | 0.2987 |
| PID_SHADOW | -5.0 | +5.0 | 115 | -0.0214 | 0.0927 | 0.4641 | 0.4212 | 0.2359 | 0.2788 |
| PID_SHADOW | -5.0 | -5.0 | 116 | -0.0305 | 0.0569 | 0.4374 | 0.3764 | 0.2626 | 0.3236 |

## Sensor-mode observation

- roll: observed -0.110° to -0.110°, standard deviation 0.0000° over this capture. Absolute orientation was not independently measured.
- pitch: observed -2.339° to -2.339°, standard deviation 0.0000° over this capture. Absolute orientation was not independently measured.
- Shadow control interval in received samples: 3993–4618 µs, mean 4001.01 µs. This excludes unreceived packets.
- Reported IMU errors: 0–0.

## Manual tilt capture

- 794 received packets; 794 sensor-valid; IMU error counter 0–0.
- Sensor-frame roll: -52.36° to -1.33°. Movement phases were not independently time-marked; physical body-axis signs remain unverified.
- Sensor-frame pitch: -37.55° to 26.99°. Movement phases were not independently time-marked; physical body-axis signs remain unverified.
- Calculated motor command range 0.000–0.800; control interval 3987–4007 µs in received samples. No ESC signal or actual motor response was measured.

## Locked motor mode

- 100 received packets; all calculated motors zero: True; all reported ESC outputs disabled: True.

## Simulation-driven diagnostic correction

The first on-device PID shadow capture reported nonzero P/I/D terms and changing mixer values while the telemetry `output` field remained zero. A local variable in `Pid::update` hid the new diagnostic member. The field was corrected without changing the returned control value or gains; a native regression assertion was added. After reflashing, 195 received shadow packets had a maximum absolute difference of 1.70e-9 between reported output and P+I+D. Before: output field stayed zero; after: it tracks the terms within float rounding.

## Interpretation and limits

The setpoint experiment held the physical board approximately still while USB commands changed the *virtual* target. Integral growth under a fixed error is expected; it is not evidence of airframe stabilization. Calculated motor values have no measured thrust, RPM or ESC PWM meaning. The BNO055 internal IMU fusion has no absolute yaw reference. Sensor-to-F450 axes and physical motor numbering remain unverified. No propeller-driven or flight test occurred.
