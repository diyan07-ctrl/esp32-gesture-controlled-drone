# Known limitations and flight dependencies

- No hardware test, upload, scope capture or flight was executed in this implementation session. Prior owner COM9/4 MB and individual motor checks do not verify current firmware.
- Output gates remain false; pairing is unconfigured. Hardware configuration must be provisioned/verified rather than assuming default builds operate an aircraft.
- Owner numbering/spins/GPIOs are documented and configured; independent physical verification is pending. Exact ESC startup/endpoints/no-signal behavior and logic compatibility remain unknown.
- HC-SR04 model is confirmed, but proposed Trigger16/Echo17 wiring and Echo level protection are not verified. Acquisition remains disabled. No altitude hold exists.
- Flex endpoints must be measured on the actual glove. Provisional gesture thresholds and emergency ergonomics require false-positive/false-negative testing. Plausible stuck ADC/IMU faults cannot all be detected.
- Level boot calibration estimates offsets only, not accelerometer scale/misalignment or temperature drift. Mounting axis signs must be physically verified. Gyro yaw drifts; there is no absolute heading hold.
- PID gains/angle limits are configurable bench values. Assumed decoupled plant convergence does not establish real F450 stability, thrust or actuator bandwidth; 50 Hz ESC latency may substantially limit usable control bandwidth.
- Immediate-cut link/sensor/low-battery failsafe can crash an airborne drone. Controlled landing, redundancy and independent motor output watchdog are absent. LEDC may retain output during a stalled task until MCU reset.
- Optional battery divider is disabled until installed/verified. Configured raw low thresholds lack debounce/hysteresis and need loaded-pack characterization. The reported 50C/1045 ratings are not measured compatibility or thrust evidence.
- ESP-NOW pairing keys are private per setup; sequence/session/CRC checks are not durable replay defense. RF performance/interference/range are unmeasured. Latest mailbox loss counts include software overwrites.
- Native tests exercise shared algorithms/mocked integration; actual I2C, RMT, LEDC, ADC, RF, watchdog and FreeRTOS timing remain hardware-validation dependencies. Static memory numbers do not measure runtime heap or stack.

Follow [ordered hardware validation](testing-and-validation.md), [tuning](flight-control-algorithm.md), and [safety policy](safety-and-failsafe.md) before any supervised flight milestone.
