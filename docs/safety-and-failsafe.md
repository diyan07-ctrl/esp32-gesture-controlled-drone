# Safety supervisor

**Current policy is restricted to propeller-off development. It is not a validated airborne failsafe.** No altitude, rangefinder, GNSS or thrust model exists. Autonomous controlled landing cannot be guaranteed. Stopping motors airborne causes uncontrolled descent; continuing stale throttle can cause flyaway. Do not fly until a flight-loss policy is engineered and validated for the operating envelope.

| State | Entry and permitted exit | Output |
|---|---|---|
| BOOT | begin → SENSOR_INITIALIZATION | High impedance, external pulldowns |
| SENSOR_INITIALIZATION | IMU config succeeds → CALIBRATION; else FAULT | Inhibited |
| CALIBRATION | 1000 stationary samples → DISARMED; timeout/failure → FAULT | Inhibited |
| DISARMED | Fresh arm release observed; then all checks → ARMING_CHECKS | Inhibited |
| ARMING_CHECKS | All checks held 1 s → ARMED; any absent → DISARMED | Inhibited |
| ARMED | Explicit arm release → DISARMED; link/enable loss or kill → FAILSAFE; hard failure → FAULT | Permitted only with four compile-time verification gates |
| FAILSAFE | Latched; reset and inspection required | No PWM pulses |
| FAULT | Latched; reset and inspection required | No PWM pulses |

Arming checks require healthy calibrated IMU, valid dt/attitude, current-session command ≤200 ms old, enable, explicit arm, low throttle ≤2%, acceptable measured pack voltage, local run-permit contact closed and hardware/controller verification flags. A timeout during the arming hold clears the observed arm-release interlock; reconnecting with arm still held cannot complete arming. No ESC output is attached until an explicit successful arming transition. No pulses when disarmed; actual ESC stop behavior must be measured before output use. Hardware pulldowns reduce floating-input risk while ESP32 pins are high impedance.

Fault bits: sensor1, timing2, battery4, link8, emergency16, numerical32, setup64. Non-finite/range-invalid samples are rejected; invalid mixer values cannot reach PWM. Timing faults include 2–6 ms sample interval violations and execution overruns. I2C stalls are bounded per transaction and lead to sensor/timing fault. A 1 s task watchdog is configured; it is last-resort supervision and far slower than the command timeout. Its reset/boot sequence and PWM cessation require scope testing. An MCU hang can retain a previous peripheral pulse until reset. No independent hardware kill gate exists.

Bench timeout response latches immediate pulse inhibition. It intentionally does not execute an unvalidated throttle ramp, hover or landing. Normal ground disarm, link loss, brownout and catastrophic power-stage faults are distinct scenarios. An open local permit wire triggers emergency behavior; this GPIO cannot isolate battery power or recover a shorted ESC.

Battery detection is limited to total voltage plausibility and provisional thresholds; no cell monitor, current sensor or robust stuck-voltage detection. Logging failure must not block the controller. Power failure, detached propellers, shorted MOSFETs, sensor mounting failure or wrong motor order cannot be guaranteed recoverable in software.

Review [output enablement](output-enablement.md) and the [stage-gated validation plan](testing-and-validation.md) before energizing any motor.
