# Hardware feasibility audit

Audit date: 2026-10-30. **Overall verdict: cannot be verified with available information.** Conditional architecture is feasible; propulsion, power delivery, assembly and flight stability remain unverified. No hardware was available for physical testing. The onboard ESP32-WROOM-32 remains the only flight-control computer.

## Evidence and compatibility matrix

| Subsystem | Selected hardware | Compatibility | Evidence | Required action |
|---|---|---|---|---|
| Frame | F450, Quad-X assumed | Unverified mechanically | User inventory only | Measure arm spacing, motor holes, clearance and mass; inspect PDB rating |
| Compute | Two ESP32-WROOM-32 DevKits | Conditional | Espressif supports I2C, timers, Wi-Fi and RTOS; exact board unknown | Identify both PCB variants, regulator and header labels |
| Sensors | Two MPU6050 modules | Conditional | TDK datasheet supports 6-axis sensing; breakout circuit unknown | Verify authenticity, supply path, 3.3 V pullups, axes and noise |
| Motors | Four A2212, **1000 KV reported by owner** | Not yet verified | KV is owner-reported; maker unknown | Obtain maker datasheet, cell rating, load/thermal/thrust curve, mounting dimensions |
| ESC | Four nominal 30 A ESCs | Unverified | Rating alone is insufficient | Identify model, continuous/peak rating, 3S support, protocol, logic threshold, connector order, BEC and calibration |
| Battery | 3S 2200 mAh | Unverified under load | Nominal energy 24.42 Wh; C-rating unknown | Read continuous C-rating and connector; measure sag, cell balance and condition |
| Propellers | None specified | Missing mandatory selection | KV alone cannot select a propeller | Match exact motor curve at 3S; select balanced CW/CCW pair sizes and hubs |
| Distribution | Unknown | Missing or unverified | F450 kits vary | Verify integrated PDB or add a rated distribution harness |
| Electronics supply | Separate 5 V buck proposed | Modification required | 3S is unsuitable for direct ESP32 input | Select documented 3S-input buck; provisional 5 V/2 A output; verify actual board input and loads |
| Link | ESP-NOW encrypted unicast planned | Suitable for evaluation | Espressif documents bidirectional peer messaging and callback constraints | Provision private keys/MACs; measure latency/loss under interference |

## Motor and propeller gate

An E-S Motor A2212 series manufacturer sheet lists several KV variants. Its 1000 KV row lists a 1047 propeller, 15.6 A, 885 g pull and 173 W, with 3S in the table. **This is an example from a different, unidentified product, not a specification for these motors.** Do not buy or install 1047 props on this basis. Maximum continuous current, maximum power, no-load speed, recommended ESC and supported cells for the owned motors remain unknown. Ideal KV × voltage is 11,100 rpm at nominal 3S and 12,600 rpm at full charge; neither is a measured no-load speed.

A 30 A label does not establish thermal margin or suitability. Confirm measured maximum motor/prop current stays within motor, ESC, wiring, connector and battery continuous limits at 12.6 V. Establish a design margin using actual thermal testing and manufacturer derating. Inspect bolt pattern, shaft/hub fit, bolt engagement and winding clearance on each F450 arm. Obtain thrust/current curves and weigh the complete aircraft; hover requires each motor to supply approximately total mass/4 in gram-force. Require documented control headroom above hover, provisionally total static thrust/weight ≥2, subject to engineering review.

## ESP32 feasibility

The classic ESP32 has sufficient documented peripheral functionality for I2C sensing, attitude estimation, PID, mixing, hardware PWM and ESP-NOW. This establishes capability, not deterministic timing for this application. Use a 250 Hz periodic task pinned to core 1, bounded I2C transactions, no heap allocation in the loop, queue-only Wi-Fi callbacks, and low-rate diagnostics in the idle Arduino task. Wi-Fi activity, scheduler latency, flash/cache stalls, I2C timeouts and serial output can cause jitter. Instrument wake interval and execution time; fault on a >6 ms interval or overrun of the 4 ms period. No worst-case execution or jitter measurement exists yet. Memory use must be recorded from actual builds and task stack high-water measurement before hardware approval.

## MPU6050 feasibility

The sensor offers selectable ±2/4/8/16 g and ±250/500/1000/2000 degrees/s ranges and I2C up to 400 kHz. Firmware proposes ±8 g, ±500 degrees/s, DLPF configuration 3 and sample divider 3 (1 kHz filtered gyro base /4 =250 Hz). Check TDK register descriptions and actual readback. A stationary calibration estimates gyro bias only; full six-face accelerometer scale/offset calibration and temperature characterization remain physical work. Keep short SDA/SCL wiring with pullups to 3.3 V, measure rise time, isolate from motor vibration, and place the IMU rigidly aligned near the center of mass. An excessively soft mount can resonate. Low-pass filtering and vibration measurements are required to prevent aliasing. Accelerometers measure specific force, not gravity alone. Fusion may use gravity only near 1 g; dynamic accelerations corrupt tilt estimates. Integrated yaw drifts: the MPU6050 has no magnetometer or absolute-heading reference. Attitude (body orientation) estimation does not provide reliable absolute heading.

## Stop gates

No motor outputs are enabled in the default build. Do not finalize assembly or enable outputs until exact boards, ESC signal timing/levels, power wiring and propulsion are verified. No commercial or dedicated flight controller is introduced. Software tests can proceed independently; they cannot certify flight.

## Sources

- [Espressif DevKitC V4 power and pinout](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/hw-reference/esp32/get-started-devkitc.html) — reference board only, not identification of owned boards.
- [TDK MPU6050 product specification](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf)
- [TDK register map revision 4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)
- [Espressif ESP-NOW API](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/network/esp_now.html)
- [E-S Motor manufacturer A2212 series sheet hosted by RobotShop](https://cdn.robotshop.com/rbm/a00a7635-653b-4220-aac9-b0c23c5c5e2c/5/520795f9-301f-4e66-a0a3-019811d1f78b/222c434c_a2212-brushless-motor.pdf) — illustrative only.
