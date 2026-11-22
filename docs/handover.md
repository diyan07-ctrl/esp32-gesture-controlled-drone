# F450 technical handover — 2026-11-22

This is an engineering bench project. The current physical configuration comes from the owner's closure instructions; it has not been independently remeasured during this work. The goal is to understand sensor feedback, control calculations, actuator commands and safe bench shutdown. No flight stabilization has been demonstrated. See [closure status](../PROJECT_CLOSURE.md) and [test evidence](closure-validation.md).

## Hardware, power and mass

The [BOM](bill-of-materials.md) records installed components separately from historical accessories. Four A2212/13T 1000KV motors and nominal 30A ESCs are mounted on an F450. A three-phase BLDC motor requires electronic commutation: the ESC switches battery power into the phases. The ESP32 supplies a command signal, not motor current. KV approximates unloaded speed per volt; it does not determine loaded RPM, thrust or permissible propeller size. A generic 30A label does not prove input protocol, continuous thermal rating, BEC topology or compatibility.

Use two properly matched CW and two CCW 10-inch props, with pitch, hub mounting, blade orientation and intended thrust direction checked against the exact motor/prop data. Props remain removed during electronics and initial motor work. Audible speed change establishes that a motor responded, not speed, thrust margin or corrective flight torque.

```text
Battery (3S, 2200mAh) -> soldered PDB -> ESC power pairs -> motor phases
                                    -> buck input
Buck (owner-set 5V) -> verified USB-C power lead -> ESP32 USB-C
ESP32 GPIO -> ESC signal; ESP32/regulator GND -> each ESC signal ground
ESP32 SDA21 / SCL22 -> MPU6050 SDA / SCL
Verified breakout-compatible supply / GND -> MPU6050 VCC / GND
```

3S is 11.1V nominal and 12.6V fully charged. Nominal energy is 24.42Wh. Owner says 40C, while a prior image reportedly shows 50C; the repository previously recorded 50C. No label image was available to independently inspect here. The corresponding label arithmetic is 88A or 110A, respectively; neither is measured pack capability. Resolve continuous versus burst rating, exact pack identity, age, condition and sag before using either figure for sizing. The earlier BOM is preserved in [bom.historical.csv](../hardware/bom.historical.csv).

The separate power bank was removed to reduce an initially reported 1.6kg mass. New all-up mass, center of gravity and reduction are unknown. Weigh the assembled aircraft with flight battery and props before computing thrust-to-weight ratio. Do not infer a new mass or flight time from the removed component.

Inspect solder joints, polarity, insulation, short-circuit clearance, strain relief, wire/connector/PDB current ratings and battery restraint. Power ground must provide a low-impedance return without routing propulsion current through sensor wiring. Identify ESC positive/BEC leads and isolate unused positives; never parallel unknown BECs. Confirm MPU6050 breakout regulator and pullup topology; GPIO/I2C must be 3.3V safe. Raw battery voltage must never reach ESP32, USB or IMU pins.

The buck's stated setting is not a loaded measurement. Verify 5V at the board under load and during Wi-Fi transients, the buck's input range through 12.6V, current/thermal margin, ripple, start-up overshoot, reverse-current behavior and USB-C power presentation. Inspect the exact board power path before simultaneous laptop USB and buck connection. Do not join two powered USB sources or improvise backfeed isolation from a generic DevKit schematic.

## Motor mapping and control signs

Viewed from above, with front marked physically:

| Position | GPIO | Owner-reported rotor direction | +roll coefficient | +pitch coefficient | +yaw coefficient |
|---|---:|---|---:|---:|---:|
| FR / M1 | 25 | CCW | −1 | +1 | +1 |
| FL / M2 | 26 | CW | +1 | +1 | −1 |
| RL / M3 | 32 | CCW | +1 | −1 | +1 |
| RR / M4 | 33 | CW | −1 | −1 | −1 |

Positive body roll is right side down, pitch is nose up, yaw is nose right. Increased CCW rotor torque produces clockwise body reaction. Before clamps, commands in FR, FL, RL, RR order are `T−R+P+Y`, `T+R+P−Y`, `T+R−P+Y`, `T−R−P−Y`. This matches the root configuration. The archival F450 controller had the opposite rotor directions and yaw terms; closure explicitly corrects those terms and native tests. Its old validation reports describe the old source and cannot establish the corrected physical sign. Output inhibition stays in place.

## Source inventory and active gains

The only supported current-sensor entry point is root `drone_sensors`. It runs MPU6050 initialization, stationary calibration, complementary estimation and serial logging. It does not run PID, PWM, Wi-Fi, ESP-NOW or a motor-test timer. No P/I/D coefficient is active in this sensor-only image.

The owner's later motor-bench observations are retained below, but the exact sketch, source hash, trace, core version, active P/I/D and correction limits were not supplied or found in tracked files or local hardware captures. Therefore no zero/nonzero I/D, 30-second cutoff implementation or watchdog coverage can honestly be certified for that image. Recover that exact source and logs before reproducing it; do not transplant its 1000–1300µs envelope or 1250µs nominal into another protocol or controller.

| Archival source | Actual compiled defaults | Units / bounds |
|---|---|---|
| Root `Stabilizer`, not invoked by sensor-only target | Angle P=4; rate P=.001, I=.0002, D=.00001 on each axis | Rate demands ±90deg/s for roll/pitch; yaw command ±60deg/s; rate correction/integral ±.2 normalized |
| F450 angle roll/pitch | P=1.5, I=0, D=.6 | Angle error to desired rate; output ±400 |
| F450 rate roll/pitch | P=.7, I=0, D=.01 | Rate error to pulse correction; output ±400µs |
| F450 yaw rate | P=2, I=12, D=0 | Rate error to pulse correction; integral/output ±400µs |
| F450 altitude | P=0, I=0, D=0 | Unconfigured; no altitude hardware installed currently |

These are source defaults, not flight-tuned coefficients. F450 saved NVS gains can replace defaults at runtime; no current device's stored gains were read. Root BNO055 shadow uses the root controller, with virtual motors only. Sensor mode has no PID execution; shadow logs cannot be assigned to the current MPU6050 bench.

Root estimation forms accelerometer roll `atan2(ay,az)` and pitch `atan2(−ax,sqrt(ay²+az²))`, integrates body rates through Euler kinematics, and combines tilt estimates with `alpha=tau/(tau+dt)`, tau=.5s, only when acceleration magnitude is .85–1.15g. Pitch outside ±60° is rejected. Yaw integrates rates and drifts; it has no magnetic heading reference. Mounting expects raw sensor +X forward, +Y left, +Z up; the driver transforms acceleration/rates into its documented body convention. This assumption needs a measured three-axis sign test.

Root rate control uses `e=rate_target−rate_measured`, `I_candidate=clamp(I+Ki*e*dt,−.2,.2)`, derivative on negative measured-rate difference with .02s filtering, and `u=clamp(Kp*e+I+Kd*derivative,−.2,.2)`. Integration is rejected if it drives saturation further or the previous mixer saturated. Differential mixer span is scaled when necessary, then collective is shifted to fit [0,1]. Root candidate actuator conversion spans 1000–2000µs at 50Hz; disarm inhibits pulses, whose stopping behavior is ESC-specific and unverified.

F450 archival PID uses trapezoidal integration and unfiltered error derivative: `I=clamp(I+Ki*(e+e_prev)*dt/2,−400,400)` and `u=clamp(Kp*e+I+Kd*(e−e_prev)/dt,−400,400)`. Its integral clamp is bounded accumulation, not full mixer-aware anti-windup. The altitude controller separately rolls back saturation-driving integration. Running pulse clamp is 1180–2000µs, with the inherited >2000→1989 discontinuity; stop requests 1000µs. Closure does not transfer those values to the absent bench sketch. `round(us*65536/20000)` is a calculation, not a scope measurement. The closure output gate prevents LEDC attachment/writes in this archive.

## Firmware audit and limitations

| Concern | Audited behavior / remaining limit |
|---|---|
| MPU identity/configuration | WHO_AM_I must be 0x68; bus 0x68/0x69 separate; reset, register readback, ±8g/±500deg/s, DLPF=3, sample divider=0; sample-ready and clipping guards; mocked driver tests |
| Calibration | 1000 accepted stationary/near-level samples; movement/read failures restart incomplete accumulation; 15s diagnostic startup deadline requires reboot after inspection; not six-face/temperature calibration |
| Invalid values/timing | Estimator rejects nonfinite, clipped, invalid dt (2–6ms) and pitch-bound candidates before publishing; stale displayed angles must be read with estimate/read flags |
| Sensor-only boot/reboot | ESC pins inputs before sensor initialization; no actuator or radio authority; no dedicated control-task watchdog in diagnostics; hardware brownout/reset pins and ESC behavior unmeasured |
| Root historical drone | False hardware gates block ESC attachment; supervisor starts disarmed, deliberate release/arm hold, sensor/timing faults latch; control nominal 4ms; task watchdog initialized but feed return was unchecked (corrected in closure); reset cannot guarantee downstream ESC stop |
| F450 archival startup | BNO055 0x29, stationary quaternion/bias capture, stale/invalid IMU, attitude and interval guards; not usable with MPU6050; outputs now explicitly inhibited |
| F450 scheduling/watchdog | Nominal 4ms; zero/>8ms intervals fault when armed; timestamps after mutex for freshness; 1s control-task watchdog does not cover every task/peripheral deadlock or power transient; PWM may continue until reset; scope reset/no-signal response remains open |
| Latched stops and re-arm | Carried existing Stage 3/4 fixes missing from main; STOP cannot clear FAULT/FAILSAFE; recoverable latch needs explicit clear, fresh neutral input and separate ARM; fault needs reboot; no automatic airborne recovery |
| 30-second motor test | Owner-reported bench limit; source unavailable, NOT VERIFIED. Root motor_test is a distinct guarded 1s single-motor fixture; F450 archive has no 30s run limit and is inhibited |

Brownout detection remains enabled. No software timer, watchdog or minimum pulse guarantees safe behavior through a collapsing rail, bootloader interval, detached output or failed ESC. A stopped propeller-free motor is not a validated airborne failsafe: loss of lift can cause uncontrolled descent. Issue #2 remains open.

## Laptop/UDP audit

The archival F450 JSON router is shared directly with native tests. It rejects oversized/trailing/embedded-NUL/bad-envelope packets and nonfinite/out-of-range pilot/gain data. GET is read-only discovery; explicit HELLO establishes IP ownership and a generation. Only strictly newer owner/session CMD renews the 300ms pilot deadline. Duplicate, backward and half-range sequence jumps fail; wrap is modular. RELEASE must be newer, same owner/generation and disarmed; it retires the generation. Session/arm-token reuse is blocked within boot; random boot seeds are replay mitigation, not authentication.

STOP/DISARM bypass owner/order checks only after valid framing and always invalidate the arm token. Foreign STOP deliberately remains possible: this is a denial-of-service risk accepted for bench emergency stop, not an authenticated flight link. AP passwords are now local, ignored configuration, empty by default, so a clean checkout does not start an AP with published credentials. Prior committed passwords must be treated as public and retired.

The client requires explicit Connect/resume, correlated HELLO ACK and explicit pilot enable. Key release neutralizes that axis; SPACE clears axes; focus loss disables pilot input; X/STOP disables input and transmits STOP. There is no automatic re-arm. Sequenced TEL rejects duplicate/backward/wrong-generation/nonfinite/invalid-shape data. Uncorrelated or expired ACKs cannot restore tokens. Loss of TEL for 500ms disables input and ownership until explicit reconnect. Fresh ACKs alone cannot hide telemetry loss. Closing sends three best-effort STOPs then RELEASE and closes the socket. Delivery and release acceptance are not guaranteed by UDP; receiver timeout remains essential. This protocol/client change requires paired updates; old TEL without sequence is rejected.

Keyboard commands remain full-step normalized demands in the archive, not tuned flight inputs. Key-release neutral means zero attitude/rate request, not guaranteed motor stop or zero collective. Native/Python tests cover parsing, ordering, sessions, shutdown and mocked client behavior. They do not establish AP reliability, delivery latency, keyboard behavior on every OS or operator safety under flight load.

## Brownout investigation

Owner reports repeated brownouts at Wi-Fi AP startup, including reduced TX power; a basic no-Wi-Fi sketch works. The failure is unresolved. Socket compilation, private AP configuration, reduced transmit power or running diagnostics without Wi-Fi cannot prove it fixed.

With propulsion isolated: inspect board/USB cable/connectors, buck topology and soldered ground paths; record the exact sketch/hash, reset reason and serial boot log. Measure buck output at the ESP32 input and the board's 3.3V rail during AP startup with suitable probing, capturing minimum voltage, ripple, overshoot and current peaks. Compare a known adequate electronics supply and cable under the same load; vary one factor at a time. Check USB-C/power-path/backfeed behavior and regulator thermal/current limits. Repeat under representative guarded loads only after the electronics supply is stable. Record setup, instrument, trace, firmware and conditions. Do not disable brownout detection, enlarge safety timing limits or remove interlocks to hide failure.

## Evidence and safe test sequence

| Evidence category | What exists | What it establishes |
|---|---|---|
| Owner physical report (closure prompt) | MPU6050 angles/yaw rate; dry-run pitch/roll/yaw mixing; audible speed changes with props off; invalid-IMU/attitude/timing shutdowns; 1000–1300µs nominal 1250µs, 30s limit | Reported bench behavior only; missing exact source/logs/gains |
| Historical software logs | [BNO055 shadow capture](pid-bench-capture-2026-11-05.md); local ignored BNO055 JSONL | Sensor data and calculated commands for that revision; no measured RPM/thrust |
| Automated software checks | [Closure validation](closure-validation.md), native mocks and assumed-plant SIL | Software regression results under stated assumptions |
| Instrument measurements | No new DMM/scope/thrust/thermal traces | Electrical, actuator timing and loaded margins remain unverified |
| Flight | No verified flight stabilization data | No flight-readiness claim |

No current MPU6050 trace dataset is available for a defensible new chart. Historical simulation figures remain explicitly simulated; historical command/telemetry tables remain historical. Do not relabel commands as RPM.

1. Remove all props and isolate propulsion. Inspect the wiring/power items above and physically mark front, each output, sensor axes and polarity.
2. Build the default `drone_sensors` target from the documented checkout. Enumerate the actual serial port; do not reuse historical COM numbers. Upload only deliberately to the identified board with propulsion isolated.
3. Power electronics through a verified path. Capture boot, WHO_AM_I/config status, stationary calibration, valid read/estimate flags, dt/misses and deliberate roll/pitch/yaw motion. Do not treat retained angles after invalid reads as healthy feedback.
4. Recover the exact owner motor-bench source/logs and audit active P/I/D, limits, reset, invalid-data, attitude, overruns, explicit enable/arm/run/disarm, 30-second deadline and STOP priority in deterministic tests before reproducing it. Keep outputs inhibited meanwhile.
5. Identify exact ESC protocol and scope all four channels at boot, commanded minimum/nominal/ceiling, STOP, fault, timeout and reset. Confirm physical output order/direction with props removed, then corrective feedback signs. Do not automatically calibrate endpoints or send maximum throttle at boot.
6. Resolve AP brownouts with measured traces before testing actual UDP session/restart/loss/load behavior. The archival BNO055/ultrasonic sketch is not an MPU6050 replacement.
7. Only after separate engineering review, use guarded load fixtures for current, temperatures, thrust, power sag, vibration and authority. Reweigh all-up mass; validate airborne loss/low-battery behavior and control tuning with appropriate sensing and constraints before considering flight evaluation.
8. Stop, verify actuator state, disconnect propulsion/battery and inspect temperatures/damage before handling. Record failures without clearing interlocks to obtain a passing run.

## Solved problems, lessons and roadmap

Closure reconciles hardware claims, separates three historical architectures, provides a safe sensor-only default, corrects the archival yaw convention explicitly, preserves missing-main safety fixes, removes the published AP credential from runnable configuration, and tests client token/telemetry/shutdown behavior. Historical captures, reference snapshots, licenses and attribution remain intact. The pre-existing uncommitted dashboard JSONL change is excluded from the closure commits.

Lessons: a sensor estimate is not calibrated aircraft feedback; a calculated mix is not a measured actuator response; audible no-propeller feedback does not demonstrate flight stabilization; output inhibition does not solve airborne failure; and Wi-Fi brownouts require electrical evidence. A reproducible source hash and trace matter as much as a successful bench session.

Next: recover/version the MPU6050 bench sketch and trace/gains; resolve battery label and mass; close power/ESC/prop/load identities and measurements ([#1](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/1)); measure sensor/timing/radio/actuator behavior and tune only with revisioned evidence ([#3](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/3)); implement/validate an airborne loss/low-battery policy ([#2](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/2)). All remain open flight blockers, with no redundant issues created.

References: [MPU6050 register/driver references](sensor-migration.md), [historical control references](flight-control-algorithm.md), [upstream reference attribution and snapshots](../f450/README.md), [repository license](../LICENSE). No external images or invented citations are added. No verified course/group roster was found in tracked source files; historical metadata is retained unchanged.

Manufacturer references verified during closure: [MPU-6000/6050 specification revision 3.4](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf), [InvenSense register map revision 4.2, hosted copy](https://hw101.tbs1.de/mpu6050/doc/MPU-6000-6050-Register-Map.pdf), and [Espressif Arduino 2.x→3.0 API migration](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html). Broken legacy TDK download URLs were replaced; no downloaded images were added.
