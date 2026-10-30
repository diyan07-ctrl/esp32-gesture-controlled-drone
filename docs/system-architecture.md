# System architecture

Glove IMU → calibration/filter → neutral-relative roll/pitch mapping + physical throttle/yaw inputs → 50 Hz pilot packet → encrypted ESP-NOW unicast. Drone callback copies 24 bytes into a latest-packet mailbox. The 250 Hz onboard task consumes a packet if present, validates it, and independently samples the drone IMU, estimates attitude, supervises safety, controls body rates, mixes outputs and updates hardware PWM. A new packet is not needed each control iteration.

Roll/pitch use angle targets (±20°) with a proportional outer loop (4/s, unvalidated) and inner angular-rate PID, rate limited to ±90°/s. Yaw is a ±60°/s rate command; it does not hold compass heading. Throttle is normalized collective, not altitude or motor RPM. The glove never supplies individual motor commands.

Coordinates are right-handed body FRD: +X forward, +Y right, +Z down; positive angular motion follows the right-hand rule. Positive roll lowers the right wing, positive pitch raises the nose, positive yaw turns clockwise viewed from above. Mount the MPU raw +X forward, +Y left, +Z up. Driver gyro transform is (rawX,-rawY,-rawZ); gravity proxy is (-rawAx,rawAy,rawAz), the negative specific-force vector in FRD. Verify each sign by physical rotation before outputs.

The code uses a core-1 FreeRTOS periodic task, priority 4, 8192-byte requested stack; Wi-Fi callbacks only enqueue. Serial formatting runs in the slower Arduino task, which receives a latest-snapshot mailbox. Queues allocate at initialization. Stack margin, heap, callback duration and scheduler jitter require measurement.

A provisional 4 ms loop follows MPU filtered sampling at 250 Hz. Conventional 50 Hz ESC PWM is an implementation candidate only. It may limit achievable closed-loop bandwidth; do not infer suitability before ESC and plant testing. An I2C timeout is bounded at 2 ms per transaction, not for the entire loop. Instrumentation detects overruns; no hard real-time guarantee has been established. No flash writes or runtime parameter persistence occur while armed.
