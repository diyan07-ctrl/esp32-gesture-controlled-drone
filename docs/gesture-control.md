# Calibrated provisional gesture mapping

Both flex endpoints must be measured; invalid/missing/rail endpoints inhibit commands. The ADC signal is normalized by (raw-open)/(bent-open), supporting either wiring polarity, then first-order filtered with 40 ms time constant. A normalized span of at least 100 counts is required as an initial plausibility check, not proof of adequate noise margin.

GestureConfig in firmware/common/gesture.h is the mapping configuration. Defaults are provisional normalized thresholds: bend-on .75, bend-off .55, tilt-on 12°, tilt-off 8°, yaw-on 35°/s and yaw-off 20°/s. Tune these with actual flex and IMU data. Debounce is 150 ms; the dedicated arm gesture requires 1000 ms. Emergency bypasses ordinary gesture debounce after flex filtering. Commands are smoothed with a 100 ms time constant; raw ADC/IMU values never command motor outputs directly.

| Gesture | Provisional interpretation |
|---|---|
| Open fingers, level hand | Neutral; hold existing commanded throttle, zero smoothed directional demand |
| Open fingers, tilt forward/back | Pitch forward/back; configured maximum 20° |
| Open fingers, tilt left/right | Roll left/right; dominant tilt axis wins |
| Open fingers, wrist rotation | Signed gyro Z rate selects yaw-rate direction, maximum 60°/s; rate input avoids absolute-heading drift |
| Flex 1 bent, flex 2 open | Increase throttle at configurable .15 normalized units/second |
| Flex 2 bent, flex 1 open | Decrease throttle at the same rate |
| Both bent, level | Dedicated arm request after 1 s; zero throttle required |
| Both bent, backward tilt | Dedicated disarm after ordinary debounce |
| Both bent, forward tilt | Emergency stop, transmitter latch; no normal debounce |

After startup/reconnection, open both fingers and hold level to establish neutral permission. Startup IMU calibration requires the level stationary mounting orientation; neutral is not continuously recentered. The mapping then retains arm request until disarm, emergency, invalid sensors or lost permission. Drone arming includes another explicit hold and interlocks; throttle accumulation is suppressed until drone ARMED status arrives. Keep level/open until arming is acknowledged. Link/sensor failures reset throttle and arm state; emergency remains latched across software mapping resets. A drone FAULT/FAILSAFE status also latches transmitter stop. No gesture clears a latched emergency; inspect and reset both systems.

Optional legacy switches may be enabled if installed. Glove arm switch then acts as additional permission, and the hold-to-enable input must be low. They are not mandatory additions to the specified two-flex architecture. Glove sensor orientation and gesture ergonomics must be independently validated; false gesture classification remains a major physical-test risk. There is no altitude or heading hold. Test all combinations on the bench before connecting ESC signals.
