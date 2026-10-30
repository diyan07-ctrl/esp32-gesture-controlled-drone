# Glove command mapping

A complementary filter estimates tilt. Glove neutral is captured while arm and enable switches are released, throttle is physically below 2%, and sensor data are healthy. Neutral may update only in that condition. Hold hand in the intended neutral orientation until controls are enabled.

Relative roll/pitch divided by 30° is clamped to ±1 and passed through an 8% deadband, with linear rescaling outside it. Drone maps ±1 to ±20° angle targets. Positive glove roll means right tilt; positive pitch means nose-up. Confirm mounting and sign on the bench. This is direct calibrated tilt mapping, not gesture classification.

Yaw comes from a centered 0–2.5 V control, center 1.25 V, mapped through the same deadband to ±60°/s. Throttle uses a separate physical 0–2.5 V potentiometer mapped to 0–1000. Fixed endpoint assumptions must be calibrated against actual analog voltages. The MPU6050 cannot provide drift-free absolute glove yaw.

On boot packets are neutral, disabled, throttle zero. A valid current-session status acknowledgment is required, then arm release and physical zero throttle with enable released. Only a subsequent deliberate enable and arm sequence can request arming. Loss of fresh acknowledgments (>300 ms) clears command permission and zero-throttle interlocks. The onboard command timeout is 200 ms. A glove reboot creates a new random session, which the armed drone rejects; old commands expire and failsafe latches.

No throttle hold or resume on reconnection. Normal disarm requires arm switch release; enable release invokes link failsafe when armed. Switch debounce is not separately implemented; the 1 s uninterrupted onboard arming hold prevents bounce-induced arming, but bounce can cause nuisance disarm. Potentiometer plausibility/redundancy remains an important limitation.
