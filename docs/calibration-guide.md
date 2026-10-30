# Calibration guide

Start with battery disconnected and props removed. Identify both board and breakout models, then verify supplies with a current-limited source. USB power only for initial sensors. Disconnect alternate supplies.

1. Verify I2C address 0x68 (AD0 low), WHO_AM_I and readback configuration. If no healthy samples appear, stop; do not bypass identity checks.
2. Mount raw X forward, raw Y left, raw Z up. Check telemetry at level: gravity proxy z about +1 g; x/y about zero. Rotate right-wing-down: roll and gx must increase; nose-up: pitch and gy increase; clockwise above: gz increases. Verify glove separately.
3. Leave stationary for 1000 consecutive samples. Bias calibration restarts on motion and faults after 15 s. Log rates after calibration and during warmup; measure residual bias and temperature drift.
4. Six-face accelerometer calibration is **not implemented**. Record readings on each ±axis and derive offset/scale; implement and test correction before relying on angle accuracy. Check alignment mechanically.
5. Calibrate battery voltage against a DMM at several voltages using the documented divider ratio/scale, with power-input protection reviewed.
6. Verify throttle 0/2.5 V endpoints and yaw 1.25 V center; hardcoded reference assumptions require a regulated reference and measurement. Check that disconnects inhibit commands where detectable.
7. With arm and enable released and throttle zero, hold glove neutral. Once healthy telemetry acknowledges the session, release arm, keep enable released at zero, then deliberately enable. Arming still stays blocked by default hardware flags.
8. Do not tune gains with freely rotating exposed propellers. Use simulation, safe guarded rigs and a documented staged controller procedure. Tune inner rate response before outer angle response; record firmware revision, gains and measured response.

Motor/ESC calibration is blocked pending the exact ESC manual. Never send full-throttle calibration sequences on startup.
