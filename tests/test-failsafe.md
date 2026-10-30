# Failsafe acceptance cases

Use safety.h directly in core tests. Walk BOOT → SENSOR_INITIALIZATION → CALIBRATION → DISARMED → ARMING_CHECKS → ARMED. Hold every prerequisite for 1 s after a fresh arm release and at zero throttle. Individually remove sensor, battery, calibration, enable, hardware gate, low-throttle and kill prerequisites; each must prevent arming. Test timeout latch, kill latch, initialization FAULT, non-recovery and timer wrap.

Additional physical fault injection is mandatory: while actuator power is disconnected scope outputs during radio silence, invalid stream, sensor unplug, ADC open/short, permit wire break, overrun and watchdog stall. Observe outputs until reset and during reboot. No automatically generated throttle or reconnect may re-enable pulses. Current policy is bench inhibition, not safe airborne landing.
