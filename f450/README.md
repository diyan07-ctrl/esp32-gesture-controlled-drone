# F450 laptop-controlled ESP32 controller

This directory is the current laptop-control implementation. Build it separately from the legacy gesture firmware in the parent repository. It has not been flown or bench verified. Exact reference gains are preserved; they are not evidence that these gains stabilize an F450 with BNO055 and 50 Hz ESCs. See [VALIDATION.md](VALIDATION.md) for executed checks and results.

Stage 3 safety policy and propeller-free procedures are in [docs/testing-and-validation.md](docs/testing-and-validation.md). `Safety.h` supplies the sequence/token barrier and latched state checks shared with native tests. The motor rotations below are intended wiring/propulsion directions, **not physically verified directions**.

## Files and build

- `F450_ESP32_FlightController/F450_ESP32_FlightController.ino`: complete Arduino sketch.
- `F450_ESP32_FlightController/Control.h`: independently testable PID, quaternion and mixer mathematics.
- `FlightSupport.h`: startup reference, filtered height estimation, altitude controller, freshness and sequence helpers.
- `Protocol.h`: shared, software-tested JSON pilot/gain validation used by the firmware.
- `f450_ground_station.py`: Tkinter laptop GUI, Python standard library only.
- `test_control.cpp`: native mathematical regression tests.

Use ESP32 Dev Module / DevKit V1, Arduino ESP32 **2.0.17**, 4 MB flash, 115200 serial. Core 3.x has different LEDC/watchdog APIs and is not supported by this sketch. Install Adafruit BNO055 1.6.4, Adafruit Unified Sensor, Adafruit BusIO and ArduinoJson 6.21.5 through Library Manager. Open the sketch inside its matching folder. Alternatively, the pinned PlatformIO environment selects the correct core:

API verification sources: [Espressif core 2.0.17 LEDC declarations](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-ledc.h) and [Bosch BNO055 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bno055-ds000.pdf). Physical mounting, individual ESC specifications and actual PWM are still bench checks.

```powershell
python -m pip install platformio==6.1.18
python -m platformio run -d f450
python -m platformio run -d f450 -t upload
```

Run those commands from the repository root. Upload is an intentional separate operation; this implementation has not been uploaded during development.

## Wiring and coordinate convention

| Device | Connection |
|---|---|
| M1 front-right CW | GPIO25 |
| M2 front-left CCW | GPIO26 |
| M3 rear-left CW | GPIO32 |
| M4 rear-right CCW | GPIO33 |
| BNO055 SDA / SCL | GPIO21 / GPIO22 |
| BNO055 address / supply | 0x29 / 3.3 V |
| HC-SR04 trigger / echo | GPIO18 / GPIO19 |
| HC-SR04 supply | regulated 5 V |
| Echo divider | echo → 1 kΩ → GPIO19 → 2 kΩ → common ground |

Common ground is required between ESP32, sensors and ESC signal grounds. Propulsion is four A2212 1000KV motors, four 30A ESCs and a 3S 2200mAh LiPo. There is no battery voltage input in the supplied hardware specification, so the reference battery monitor is omitted.

Mount BNO055 logical **X forward, Y right, Z down**, using its chip axes rather than assuming breakout silkscreen alignment. This is a right-handed body frame: positive roll lowers the right side, positive pitch raises the nose, positive yaw turns the nose right viewed from above. Automatic reference capture removes initial orientation but cannot correct an arbitrary sensor-to-frame axis permutation. Verify all three signs physically before motor operation. A different mounting requires a consistent fixed rotation of both quaternion and gyro, not just angle sign changes.

ESC outputs use real LEDC hardware, 50 Hz, 16-bit duty resolution: `duty = round(pulse_us * 65536 / 20000)`. Disarmed pulses are 1000 µs. Output resolution is approximately 0.305 µs. A generic “30A ESC” does not identify a protocol specification: confirm the actual model accepts 50 Hz, 1000–2000 µs, and measure all four channels using an oscilloscope before connecting propellers. ESC endpoint calibration is not performed automatically.

## Reference audit and numerical comparison

Primary source: Pallab Das, [Flight_Controler_Code.ino](https://github.com/pallabdasc1/ESP32_Code_for_Quadcopter/blob/main/Flight_Controler_Code.ino). The downloaded source is retained as `reference_controller.txt`. The other root sketch, `freertos_testing.ino`, contains two printing tasks and no flight-control equations; a copy is retained. The repository's PPM receiver is replaced by UDP. Reference source permits reuse in its README; this implementation attributes its control equations here.

| Parameter / algorithm | Reference source | Implemented |
|---|---|---|
| Roll / pitch angle Kp, Ki, Kd | 1.5, 0, 0.6 each | exactly the same |
| Roll / pitch rate Kp, Ki, Kd | 0.7, 0, 0.01 each | exactly the same |
| Yaw rate Kp, Ki, Kd | 2, 12, 0 | exactly the same |
| Altitude PID | absent | 0, 0, 0; user configures, acknowledges review while disarmed, then requests ALT |
| PID integral / output clamp | ±400 / ±400 | exactly the same for every PID |
| PID integration | trapezoidal, dt=0.004 | same equation, measured dt, nominal 0.004 |
| Derivative | error difference / 0.004, no filter | same equation with measured dt, no added filter |
| Loop | busy wait 4000 µs, floating timer, serial in loop | scheduled 4000 µs, integer timer, no control-loop printing |
| Angle demand | 0.10 × (PPM−1500) | 50 × normalized command, equivalent for PPM 1000–2000 |
| Yaw demand | 0.15 × (PPM−1500) | 75 × normalized command, same range |
| Gyro calibration | 2000 samples with delay(1), average | 200 stationary 100 Hz samples over ≥2 s, average |
| MPU address / ranges | 0x68; gyro /65.5, accel /4096 | BNO055 0x29; gyro /16 deg/s, accel /100 m/s² |
| MPU accel offsets | X−0.02 g, Z−0.08 g | omitted; sensor-specific values have no transferable meaning |
| MPU low-pass register | 0x1A=0x05 | BNO055 IMUPLUS internal fusion; no equivalent coefficient |
| MPU gyro range / accel range registers | 0x1B=0x08 / 0x1C=0x10 | BNO055 default fusion settings |
| Accel angle | atan(Ay/√(Ax²+Az²)); −atan(Ax/√(Ay²+Az²)); radians divided by 3.142/180 | relative quaternion converted by standard atan2/asin, degrees via π |
| Kalman initial angle / uncertainty | 0 / 2² each | initial relative angle 0; reference scalar estimator replaced by BNO fusion |
| Kalman prediction | x←x+0.004ω; P←P+0.004²×4² | internal BNO fusion, no exposed equivalent |
| Kalman measurement update | K=P/(P+3²); x←x+K(z−x); P←(1−K)P | internal BNO fusion; no invented equivalent noise parameters |
| Sensor sampling | MPU read each nominal 4 ms | checked BNO reads every 10 ms; PID every 4 ms holds latest feedback |
| Throttle ceiling | 1800 | 1800 before mixer |
| Mixer scale | 1.024 converts µs to 250 Hz/12-bit counts | removed from microsecond path; PWM hardware layer converts once |
| Mixer output cap | if >2000 then 1989 | retained exactly, including its discontinuity |
| Running idle / cutoff | 1180 / 1000 | 1180 / 1000 µs |
| Low throttle cutoff | PPM throttle <1050 resets motors and PID | explicit DISARM/STOP resets PID, writes 1000 µs |
| Arming | no explicit armed state | explicit ARM with valid reference, fresh sensors/commands and neutral input |
| Receiver loss | 10,000,000 µs without >10 channel change | 300 ms without a valid newer CMD; repeated neutral packets remain alive |
| PWM | GPIO5/18/19/15, 250 Hz, 12-bit duty | GPIO25/26/32/33, 50 Hz, 16-bit pulse conversion |
| Battery ADC | GPIO25, 3.3 V /4095, divider 77600/29400 Ω, warning <9 V | omitted: GPIO25 is now M1 and no battery sensor specified |
| Watchdog | commented-out 5 s initialization/reset | active 1 s task watchdog, reset to disarmed boot |

PID equation: `I=clamp(Iprev+Ki*(e+eprev)*dt/2,−400,400)`; `u=clamp(Kp*e+I+Kd*(e−eprev)/dt,−400,400)`. Roll/pitch angle outputs are desired body rates; rate outputs feed the mixer. Yaw is **rate control**, with zero command requesting zero yaw rate. The reference has no yaw heading controller; relative heading is displayed and wraps at ±180°. Quaternion calculations are continuous across that crossing (quaternion sign equivalence is respected). Euler angles have their usual pitch singularity; armed operation beyond 65° roll/pitch faults.

The reference scalar Kalman estimator fuses raw acceleration with gyro. Applying it unchanged on top of an already fused BNO attitude would double-fuse correlated measurements with unrelated noise constants. It is replaced at the sensor-estimation boundary. The cascaded angle/rate control equations and numerical gain defaults remain intact. BNO fusion latency and 50 Hz actuator updates change the dynamics; matching closed-loop stability is not claimed. The reference's 1.024 is a hardware conversion (4096 counts / 4000 µs). Applying it before the new 50 Hz conversion was a bug, corrected in stage 2. All mixed motor values now remain in microseconds. Only `pulseToDuty()` converts to counts: `round(us * 65536 / 20000)`. These are discrete PWM counts, so exact requested widths have unavoidable rounding error up to 0.153 µs; see the measured-by-calculation table in VALIDATION.md. Clock accuracy and physical pulse shape still require scope verification.

## Mixer signs

Reference outputs are `1.024*(T−R−P−Y, T−R+P+Y, T+R+P−Y, T+R−P+Y)` in its motor ordering. For this specified arrangement, implemented outputs before clamps are:

| Motor | Equation | +roll | +pitch | +yaw |
|---|---|---|---|---|
| M1 front-right CW | T−R+P−Y | decrease | increase | decrease |
| M2 front-left CCW | T+R+P+Y | increase | increase | increase |
| M3 rear-left CW | T+R−P−Y | increase | decrease | decrease |
| M4 rear-right CCW | T−R−P+Y | decrease | decrease | increase |

Negative corrections reverse these effects. Left lift produces right-down roll, front lift produces nose-up pitch; CCW rotor acceleration produces clockwise body reaction. Mathematical checks do not verify actual motor direction, propeller orientation or sensor mounting.

## Startup reference and faults

Power on stationary on a **level surface**, with propellers removed for bench work. IMUPLUS avoids magnetometer dependence and does not require manual calibration or stored calibration offsets. It supplies relative yaw with drift, not a magnetic compass heading. No system can infer that an arbitrary stationary support surface is level from these sensors alone.

Initialization uses Adafruit BNO055. Subsequent register reads check transaction lengths, fusion status/error and quaternion norm (0.95–1.05). The default unit settings use 1/16 deg/s gyro units. Capture requires 200 consecutive 10 ms samples: acceleration magnitude within 0.5 m/s² of gravity, each gyro below 3 deg/s, and orientation within 1° of the start of the window. Motion or invalid reads reset that window. The last valid normalized quaternion becomes q0 and the mean gyro becomes bias. Feedback is `inverse(q0)*q`, converted to roll/pitch/yaw; body rates subtract bias. Once captured, reference is never recaptured until reboot, even after sensor failure.

Each new quaternion is aligned to the previous quaternion's hemisphere (q and −q encode the same orientation). A relative change exceeding 30° in a 10 ms sensor sample is rejected after reference capture as implausible. This is a sensor fault guard, not a reference controller gain. Recovery after a large discontinuity may require reboot while disarmed; reference is never silently replaced.

This removes initial attitude offset and estimates stationary gyro bias. It cannot remove scale errors, temperature drift, vibration, sensor misalignment, or acceleration-induced fusion errors. Constant readings cannot conclusively prove sensor freshness; acknowledged I2C/status checks catch disconnects and invalid responses but not every internal frozen-data fault. Initialization failure keeps ARM unavailable until reboot; the library's initialization may wait for the device while the independently running controller continues disarmed.

ARM requires reference ready, IMU age <50 ms, valid command age <300 ms, AP client present, valid PWM initialization, all pilot axes neutral, manual collective at 1180 µs, and roll/pitch within 5°. It starts at running idle, 1180 µs; stopped output is 1000 µs. Duplicate ARM while armed is rejected without resetting controllers. Telemetry reports `arming_block_reason`. Sensor failure, sensor age >50 ms, command loss >300 ms, no AP clients, excessive tilt, invalid/stale range while holding, invalid motor calculations, or a zero/>8 ms control interval disarms. STOP/DISARM writes cutoff and clears PID history. There is no automatic landing on link failure. A 1 s task watchdog reboots if control stops progressing; boot writes cutoff before sensors initialize.

Ultrasonic triggering is every 60 ms; interrupt edges are accepted only within a pending trigger, with a 15 ms timeout and no pulseIn. Slant range must be finite and 2–200 cm. With the sensor pointing along body down and a level startup reference, `height = D*cos(roll_deg*π/180)*cos(pitch_deg*π/180)+4.03 cm`; at zero tilt this is exactly D+4.03. Measurements beyond 30° roll/pitch or with invalid/stale IMU data are rejected. A three-sample median rejects isolated outliers, followed by a first-order low-pass filter with τ=0.15 s and alpha=sample_dt/(τ+sample_dt). Three valid samples and age ≤250 ms are required. Invalid echoes mark health false immediately and leave the last height unchanged. During hold, unhealthy range causes failsafe cutoff without further integration. Range rejection count, age and health are explicit telemetry. Beam spread, rotor airflow, soft/reflective surfaces, slopes, obstructions and temperature can defeat this short-range estimate; filtering adds delay and cannot establish ground identity.

Altitude PID is a separate addition in µs/cm, µs/(cm·s), and µs·s/cm; no altitude gain is reference-derived. Configure gains in the GUI, bench-review them, then press the explicit confirmation button while disarmed (`ALT_VALIDATE`). This records an operator acknowledgement, not automatic proof of stability. The flag clears on PID updates, reset and reboot, and is not persisted with gains. Zero defaults remain deliberately unconfigured. No unproven nonzero gains are supplied.

ALT captures current collective as feedforward and starts its internal target at measured height, then ramps toward requested height at 30 cm/s. R/F adjust requested target at ±30 cm/s in hold only, bounded 6.03–204.03 cm. The altitude PID runs only on a new accepted measurement, using measurement delta time; its integral and correction clamp at ±400, with integration rollback when collective saturation would be driven further. Collective clamps to 1180–1800 µs and slews at 100 µs/s in both modes. Invalid/stale measurements never accumulate integral. The target ramp and collective slew prevent an instantaneous collective step on mode changes; attitude corrections can still change individual motor commands.

Manual mode uses continuous `CMD.throttle` in microseconds, controlled by the GUI slider or Up/Down keys (2 µs per 50 Hz command). ALTITUDE_HOLD ignores this field so manual and altitude controllers cannot fight. MANUAL captures the present collective and clears altitude history; the GUI adopts the reported collective for its slider. SPACE neutralizes roll/pitch/yaw/climb and holds collective; X stops everything. Old clients omitting throttle retain the last manual demand. Legacy BASE remains disarmed-only; a value above 1180 must return to idle before ARM. Choosing an ALT target never changes manual throttle.

State transitions are explicit: BOOT → SENSOR_INITIALIZATION → STARTUP_REFERENCE → DISARMED. ARM checks the existing state before any transition; a valid request passes through ARMING_CHECKS to ARMED. Link/range failure latches FAILSAFE; IMU, PWM, motor, timing and watchdog initialization/feed errors latch FAULT. Both clear armed state and write minimum pulses. STOP/DISARM enters DISARMED from ordinary operation but **cannot clear FAILSAFE or FAULT**, and cannot bypass startup reference capture. Sensor recovery alone cannot rearm. FAULT requires reboot after correcting the cause. FAILSAFE requires fresh neutral commands, idle collective, healthy/fresh IMU, level attitude, AP connection and valid PWM/watchdog; a range-loss latch additionally requires three fresh consecutive range samples. Press “Clear recoverable failsafe” (CLEAR_FAULT with current token), then obtain the new token and press ARM separately. Clearance itself never arms. Battery reports NOT_INSTALLED. Invalid PID updates are rejected atomically without changing gains or renewing pilot freshness.

## Ground station

Install Python 3.10+ with Tcl/Tk (included in standard Windows installer); no pip GUI dependency is needed. On Linux install your distribution's `python3-tk`. Join **F450_DRONE**, password **F450Drone2026**, then:

```powershell
python f450/f450_ground_station.py
```

Allow UDP inbound 4211 on the laptop firewall. Use ARM button only after startup telemetry is valid. W/S pitch forward/back, A/D roll left/right, Q/E yaw left/right, R/F altitude up/down, SPACE clears pilot input, X sends STOP. Keyboard demands are full normalized steps, equivalent to ±50° angle and ±75 deg/s yaw; this preserves reference command range and requires particular care during restrained validation. Typing in entry fields suppresses pilot keys except X. Focus loss clears keys; GUI sends neutral commands at 50 Hz. X also works while editing a field. Closing sends STOP three times and RELEASE; communication timeout is the fallback.

Current gains are displayed separately from editable fields. ± buttons change by 0.01; numeric entries allow other precision. Send all PID gains applies atomically on ESP32 and resets PID history to prevent stored integral/derivative history from carrying across configurations. Runtime tuning can produce abrupt output changes. ACK reports acceptance/rejection; missing ACK is displayed after 1 s. Reset restores the five reference gain triples and zero altitude gains, disables altitude hold, and leaves NVS unchanged. Save gains to NVS is allowed only disarmed; validated stored gains load on reboot. Use Reset then Save to persist original defaults. Base throttle and target are not persisted. Base throttle is editable only disarmed, range 1180–1800 µs.

## UDP protocol, version 1

UTF-8 JSON datagrams, commands to 192.168.4.1:4210, replies/telemetry to sender IP:4211. Maximum command length is 1199 bytes. Stage 4 replaces implicit first-packet ownership with GET discovery followed by explicit HELLO acquisition. GET without a session is read-only discovery from any IP; it never claims ownership or changes sequence state. HELLO must include the discovered uint32 `session` generation and requires disarmed output. When unowned, HELLO establishes its own fresh sequence window. When the current owner resumes, HELLO must advance the existing sequence window; foreign acquisition while occupied is rejected. A successful HELLO invalidates the ARM token and requires a fresh valid CMD before ARM. Only valid newer owner/session CMD packets refresh command-loss detection; queries, HELLO, PID edits and other traffic do not. AP-password access remains the only authentication; session IDs are replay barriers, not cryptographic credentials.

Each command has `type` string and `seq` unsigned 32-bit integer. Sequences advance with unsigned modular difference in (0, 2³¹); duplicate, backward and exactly half-range jumps are rejected. Malformed JSON, missing fields and negative/fractional/overflow/bool/string sequences are ignored. Valid envelopes with invalid payloads consume sequence but do not refresh pilot commands. STOP/DISARM always cut off even when stale or foreign, advance the high-water mark only for a newer owner sequence, and invalidate the arming token. They never clear a fault latch. Foreign STOP cannot poison the owner's sequence high-water mark.

ARM requires `arm_token`, an unsigned 32-bit value returned in ACK/TEL. STOP/DISARM, fault events, successful ARM, clearance, HELLO and RELEASE invalidate it. A delayed ARM with an old token is rejected even if its sequence is newer. Token initialization is randomized at boot and cannot wrap/reuse within a boot. Fault-latch and fresh-input checks are unchanged. The GUI supplies tokens on explicit button presses, never automatic ARM retries.

All owner commands now include `session` in addition to `type` and `seq`. RELEASE is accepted only from the current owner IP, in the current generation, while disarmed, with a strictly newer sequence. Every rejected RELEASE—including armed, duplicate, delayed, foreign, malformed or retired-session requests—leaves ownership, order, ARM token, control freshness and safety state unchanged. A successful RELEASE retires the generation, clears ownership/order/fresh-CMD state, invalidates the ARM token and leaves the stopped/fault state intact. Repeated RELEASE while unowned returns NO_SESSION. Old commands/HELLO/RELEASE cannot act on a later generation, even from the same IP. Generations are randomized at boot and increment on release without reuse; exhaustion requires reboot. A new laptop obtains the new generation with GET and HELLO using any valid initial sequence. No command other than HELLO can claim an empty slot.

ACK retains type/seq/ok/reason/arm_token and adds session/owned/owner_active/last_seq/seq_set. `owned` means the responding IP matches the active owner; the GUI only confirms local ownership after a matching successful HELLO ACK. TEL retains its existing data and adds session/owned/owner_active. The old implicit-acquisition client must be upgraded alongside this firmware: accepting commands without a generation would defeat cross-session replay protection.

| Type | Additional fields | Meaning |
|---|---|---|
| CMD | roll,pitch,yaw,climb: finite numbers −1..1; optional throttle: 1180..1800 µs | pilot input, 50 Hz; throttle ignored during altitude hold |
| ARM | arm_token: uint32 from latest ACK/TEL | explicit arm after all checks; token consumed on success |
| DISARM / STOP | none | immediate minimum-pulse output; invalidate token, preserve FAULT/FAILSAFE |
| CLEAR_FAULT | arm_token: current uint32 | explicit recoverable FAILSAFE clearance only; never arms; FAULT requires reboot |
| GET | session optional | no session: read-only discovery; with session: ordered owner query; ACK includes gains/base and ownership metadata |
| HELLO | session: uint32 from discovery | disarmed acquisition; current owner resume requires newer sequence |
| PID | gains: six arrays of three finite numbers, each 0..100 | order roll angle, pitch angle, roll rate, pitch rate, yaw rate, altitude; Kp,Ki,Kd |
| RESET | none | reference defaults plus zero altitude gains |
| SAVE | none | persist current gains only, disarmed |
| BASE | value: 1180..1800 | manual collective base µs, disarmed |
| ALT | value: 6.03..204.03 | enable hold and set target frame height cm |
| ALT_VALIDATE | none | disarmed operator acknowledgement of configured altitude gains; requires healthy range |
| MANUAL | none | disable hold and clear altitude PID history |
| RELEASE | session plus normal seq | newer current-owner request only, disarmed; retire session and token, never clear a fault |

Example after GET/HELLO: `{"type":"CMD","session":42,"seq":123,"roll":0,"pitch":0,"yaw":0,"climb":0}`. Replace 42 with the acknowledged session generation.

TEL packets at 10 Hz retain the existing angles, gyro/bias, gains, distance, height, target, outputs and motors. Added fields are mode, state, collective_us, manual_throttle_us, alt_error, alt_configured, alt_validated, range_age_ms, range_rejected, reference_samples, pwm_ok, arming_available, arming_block_reason and battery_status. `target` is the ramped controller target; range distance is latest accepted raw slant range, and height is filtered vertical height. Outputs are roll/pitch/yaw/altitude corrections in µs, motors are M1..M4 µs. Absolute yaw is not geographic heading. The GUI accepts only 192.168.4.1 and reports telemetry loss after 500 ms.

## Validation and troubleshooting

| Area | Implemented | Compile checked | Bench verified | Required physical validation |
|---|---|---|---|---|
| Reference PID / limits / mixer / quaternion | yes | see validation log | no | correction signs and stable gains |
| Four-channel LEDC | yes | see validation log | no | scope frequency, widths, cutoff, ESC model compatibility |
| BNO startup / bias / status | yes | see validation log | no | mounted axes, flat stationary capture, unplug faults |
| HC-SR04 / offset / hold | yes | see validation log | no | known-distance ruler test, echoes, altitude tuning |
| UDP / runtime gains / telemetry / NVS | yes | see validation log | no | actual AP traffic, loss, invalid packets, reboot persistence |
| Watchdog / scheduling | yes | see validation log | no | measured jitter and watchdog reset under load |
| Python console | yes | see validation log | no | interactive keyboard and real UDP session |

With propellers removed: scope all pins at boot, ARM, STOP and watchdog reset; confirm M1..M4 ordering; tilt each axis and verify corrective outputs oppose the motion; cross heading ±180°; move during startup to ensure capture delays; unplug IMU and interrupt UDP to confirm cutoff. Check range at several known distances and confirm +4.03 cm offset. Send duplicate sequences, NaN, Infinity, malformed arrays and out-of-range values; confirm rejection without changing gains or renewing command freshness. Test gain query/update/reset, NVS save/reboot, and loss of valid echoes during hold. Measure control timing under sustained UDP traffic before powered fixture testing. No unrestrained flight should be inferred from mathematical tests or compilation.

No reference-ready indication: check address, wiring and the stationary window. OLD_SEQUENCE: while disarmed use Connect / resume; GET synchronizes the current owner's sequence before HELLO. OWNER_BUSY: the other owner must disarm and validly RELEASE; a foreign laptop cannot force release. A lost/rejected HELLO never confirms ownership. Successful Release ownership stops pilot sending until another explicit Connect / resume. Rejected RELEASE retains ownership only when ACK confirms the same active generation/IP; timeout marks ownership unconfirmed. Closing sends best-effort STOP and RELEASE without waiting for delivery, so a later restart may need same-IP resume. No telemetry: check Wi-Fi/firewall. ARM rejected: inspect token, reference, neutral axes and input age. Altitude rejected: inspect range validity and configured/reviewed gains. Never enlarge timing thresholds to conceal missed deadlines.
