# Calibrated provisional gesture mapping

Both flex endpoints must be measured; invalid/missing/rail endpoints inhibit commands. The ADC signal is normalized by (raw-open)/(bent-open), supporting either wiring polarity, then first-order filtered with 40 ms time constant. A normalized span of at least 100 counts is required as an initial plausibility check, not proof of adequate noise margin.

GestureConfig in firmware/common/gesture.h is the mapping configuration. Defaults are provisional normalized thresholds: bend-on .75, bend-off .55, tilt-on 12°, tilt-off 8°, yaw-on 35°/s and yaw-off 20°/s. Tune these with actual flex and IMU data. Debounce is 150 ms; the dedicated arm gesture requires 1000 ms. Emergency bypasses ordinary gesture debounce after flex filtering. Commands are smoothed with a 100 ms time constant; raw ADC/IMU values never command motor outputs directly.

| Gesture | Provisional interpretation |
|---|---|
| Open fingers, level hand | Neutral directional demand; throttle follows calibrated pot |
| Open fingers, tilt forward/back | Pitch forward/back; configured maximum 20° |
| Open fingers, tilt left/right | Roll left/right; dominant tilt axis wins |
| Open fingers, wrist rotation | Signed gyro Z rate selects yaw-rate direction, maximum 60°/s; rate input avoids absolute-heading drift |
| Flex 1 bent, flex 2 open | No throttle increment; directional tilt/rate mapping remains available |
| Flex 2 bent, flex 1 open | No throttle decrement; pot remains sole throttle source |
| Both bent, level | Dedicated arm request after 1 s; zero throttle required |
| Both bent, backward tilt | Dedicated disarm after ordinary debounce |
| Both bent, forward tilt | Emergency stop, transmitter latch; no normal debounce |

After startup/reconnection, open both fingers at low pot and hold level to capture
neutral once. Both-bent arm requires released fingers observed at low throttle,
then a fresh 1 s hold; a high unfiltered pot target prevents arming. Drone arming
retains its own interlocks; transmitted throttle stays zero until ARMED status.
Thereafter only calibrated pot output sets throttle. Sensor/pot/link permission
loss resets arm/neutral; emergency remains latched across mapping resets. Received
FAULT/FAILSAFE also latches stop. No gesture clears emergency; inspect/reset both.
This mapping is implemented but still requires physical ergonomic/false-positive
tests. [Sensor and pot calibration](sensor-migration.md).

Optional legacy switches may be enabled if installed. Glove arm switch then acts as additional permission, and the hold-to-enable input must be low. They are not mandatory additions to the specified two-flex architecture. Glove sensor orientation and gesture ergonomics must be independently validated; false gesture classification remains a major physical-test risk. There is no altitude or heading hold. Test all combinations on the bench before connecting ESC signals.
