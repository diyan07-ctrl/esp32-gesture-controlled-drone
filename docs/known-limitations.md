# Known limitations

- Hardware identity incomplete: motor maker/current/props, ESC model/BEC/protocol, battery C-rating, PCB variants and mass.
- All pin and power connections are provisional. No physical circuit, voltage, current, thrust, temperature, RF, timing or flight measurement exists.
- Default motor outputs and radio pairing are inhibited. No production-ready or flight-ready configuration is supplied.
- Firmware build and host C++ test execution depend on available tools; consult actual [results](validation-results.md). CI configuration alone is not a CI pass.
- Euler complementary estimator is restricted to modest tilt and cannot reject all dynamic acceleration. No absolute heading, position, altitude hold or landing.
- Gains are not tuned; 50 Hz actuator protocol candidate needs control-bandwidth validation. Saturation clips independently.
- Sensor/calibration faults latch conservatively. No redundant IMU or independent watchdog-controlled motor gate.
- Low-voltage thresholds are placeholders without load filtering or cell monitoring. Potentiometer and stuck ADC faults can remain plausible.
- ESP-NOW encrypted pairing is local-only; receiver reboot lacks a boot challenge/persistent anti-replay state. No hostile-environment security guarantee.
- Failsafe is bench pulse inhibition. It can cause catastrophic descent airborne and must be redesigned/validated before flight.
- Software disarm cannot isolate battery energy or stop an ESC with failed power transistors.
