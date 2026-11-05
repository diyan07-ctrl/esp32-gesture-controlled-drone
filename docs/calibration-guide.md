# Calibration guide

All initial work is propeller-off, with propulsion disconnected until the procedure explicitly calls for a controlled motor test. Never calibrate ESC endpoints automatically at boot. Preserve actual sensor readings and calibration/test records.

## MPU6050 on both boards

Mount raw X forward, Y left, Z up and verify the breakout circuit. On reset place the board/glove flat and stationary in its known level reference. After 100 ms warmup, the driver accumulates 1000 consecutive samples (about 4 s at nominal application rate), rejecting movement, non-level gravity and excessive rates. It estimates gyro XYZ biases and level accelerometer XYZ offsets against 0,0,1 g. Timeout at 15 s latches failure. Repeat after temperature stabilization, test each axis against physical motion and record noise/offsets. Boot level offsets do not calibrate scale/misalignment; perform six-face accelerometer and temperature characterization before flight. Yaw drifts without an absolute reference.

## Two flex sensors

Use a 115200 serial terminal on the **glove**, with drone propulsion disconnected. While glove is disarmed:

1. Hold both fingers fully open; send lowercase o. Keep still for 250 acquisition cycles (~1 s); record printed open endpoints.
2. Bend both sensors to the chosen comfortable full-bend posture; send b. Hold for another 250 cycles and record bent endpoints.
3. Confirm valid=1, adequate measured span/noise margin and non-rail ADC values. Inverted wiring is supported; changing wiring requires recalibration.
4. Also capture the pot LOW/HIGH with l/h as described below, then send s to persist
   all analog endpoints in glove-sens-v1 NVS. Reboot; confirm sensors-cal=1 and repeat
   observations. Old flex-only records are intentionally not accepted.
5. Send x while disarmed to erase and repeat if needed. The implementation rejects calibration commands while the glove arm request is active. Starting a capture inhibits commands until a successful save; failed storage keeps the edit inhibited and marks a partially saved record invalid. Disarm before any capture/save/erase; NVS is never written in the flight controller loop.

Default endpoint values are invalid; no commands are permitted until actual endpoints are captured. Internal normalized gesture defaults are provisional, not measured ADC calibration. Tune bend hysteresis, tilt/rate thresholds, smoothing and debounce using repeated real gestures and false-positive tests. Two ADCs can fail plausibly; rails are detected, every stuck input is not.

## Potentiometer and capture stability

Use independent glove_sensors firmware first, with no radio/ESC authority. At
chosen non-rail LOW position send l; at HIGH send h, holding each for 250 samples.
Capture spread must stay <=32 counts, endpoints strictly inside 32..4063 and span
at least 500 counts. Both directions of pot wiring are supported. Configure and
electrically verify end resistance/conditioning or travel stops if physical end
positions rail the ADC; do not save rail endpoints. s explicitly saves only valid
complete analog calibration after capture ends; x erases this revision's record.
Losing calibration permission cancels a capture. Real flash persistence, power-
interruption behavior and open-wiper faults still require hardware tests.
[Detailed sensor-only procedure and log fields](sensor-migration.md).

## Neutral and arm

After pairing and healthy calibrated sensors, open fingers at low pot and hold
level to capture neutral. Both bent while level for 1 s sends arm, after which the
drone keeps its own interlocks. High pot prevents arming; transmitted throttle is
zero until ARMED acknowledgement, then follows only the pot. Flex sensors never
increment/decrement throttle in the glove image. Validate without motor signals;
emergency/drone faults remain latched. Both IMU physical acceptance tests must pass
before further flight-control integration; unchanged PID code is not flight approval.

## ESC, range and optional battery

Identify the exact ESC and manufacturer-supported signal rate, endpoints, startup hold and no-signal behavior. Configure cfg PWM parameters; validate electrically before changing output gates. Props removed: use the separate test image and a secure fixture for low-output motor numbering/direction tests. Do not infer universal endpoint calibration behavior.

HC-SR04 Trigger16/Echo17 are proposed pins. Verify Echo divider/level shifter before enabling ultrasonicVerified, then compare U telemetry with measured targets at multiple distances, angles, surfaces and motor-noise conditions. Range is supplementary, not altitude hold.

Only enable batteryMonitorInstalled after divider/protection is fitted and measured. Compare ADC voltage to a calibrated DMM over the full pack range, set batteryScale from actual measurements, characterize loaded sag/noise and choose thresholds accordingly. The reported 50C marking is not a substitute for measured propulsion/power limits.
