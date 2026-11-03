# Safety supervisor and deliberate failsafe policy

The software implements a conservative **immediate motor pulse shutdown**, suitable only as an unvalidated bench baseline. Loss of control in flight can cause an uncontrolled fall. No reliable altitude sensing, position sensing, controlled landing, redundancy or independent motor watchdog is available. Do not characterize the current failsafe as a validated airborne policy.

| State (wire value) | Entry/action | Exit |
|---|---|---|
| BOOT (0) | Output pins input/no pulses; no automatic arm | begin → SENSOR_INITIALIZATION |
| SENSOR_INITIALIZATION (1) | Identity/configuration checks | Pass → CALIBRATION; failure → FAULT |
| CALIBRATION (2) | Stationary level calibration; no motor pulses | Pass → DISARMED; timeout/failure → FAULT |
| DISARMED (3) | Zero output; observe fresh explicit arm release | All checks and new arm request → ARMING_CHECKS |
| ARMING_CHECKS (4) | Hold configured minimum ESC pulse, only after explicit request and verified output initialization | Checks continuously true for cfg::armHoldMs (1000 ms) → ARMED; invalid check → DISARMED |
| ARMED (5) | Bounded stabilized outputs; near-zero throttle resets PID | Explicit arm release → DISARMED; emergency/link loss → FAILSAFE; critical sensor/timing/numeric/battery fault → FAULT |
| FAILSAFE (6) | Zero duty/no pulses, latched | Inspection and system reset only |
| FAULT (7) | Zero duty/no pulses, latched | Inspection and system reset only |

This expands the requested SENSOR_CHECK into sensor initialization/calibration and keeps a separate FAULT diagnostic state. At every step emergency takes priority over normal arming/disarming and latches even while disarmed. Existing terminal faults remain latched. A validated stop uses a separate radio mailbox so later ordinary commands cannot overwrite it. Invalid or replayed packets do not refresh the connection.

Arming requires valid/calibrated IMU, valid timing, fresh enabled transmitter, zero throttle (≤20/1000), explicit arm request after observed release, initialized ESC output, radio success, all four verification gates, and adequate battery if monitoring is fitted. Optional physical run-permit must be closed if enabled. Unfitted battery/switch inputs default disabled; this does not establish their protection is unnecessary. Attach failure with output gates enabled is a setup fault. Flags do not constitute physical validation.

Command age >200 ms while armed, or loss of transmitter enable, latches Link failsafe; it is never processed as an ordinary zero-throttle command. Duplicate/corrupt/invalid packets cannot postpone timeout. Reconnect never clears FAILSAFE/FAULT. Link interruption during an arming hold cancels it and requires a new observed release. Explicit ordinary disarm removes pulses immediately and requires release again before a new arm attempt.

Fault bits: Sensor=1, Timing=2, Battery=4, Link=8, Emergency=16, Numeric=32, Setup=64. Interval outside 2–6 ms or measured execution reaching 4 ms faults the control loop. Task watchdog resets after approximately one second of missed service; LEDC may retain its last output during an MCU/task stall until reset, and reset/no-signal ESC behavior is unverified. A hardware output watchdog remains a preflight dependency.

With battery monitoring installed, invalid voltage or below configured low threshold (10.8 V provisional) causes motor shutdown; a 9.9 V critical check also exists. These raw loaded-pack limits have no hysteresis/debounce and must be characterized before flight; noisy ADC/sag can cause a cut. With no verified divider, monitoring is disabled and status reports 0 V/unavailable. HC-SR04 errors produce supplementary unhealthy telemetry and do not kill motors because altitude control does not depend on it. IMU faults are critical. Plausible stuck IMU/ADC values cannot all be diagnosed by range checks.
