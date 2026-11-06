# F450 laptop-controlled ESP32 controller

This directory is the current laptop-control implementation. Build it separately from the legacy gesture firmware in the parent repository. It has not been flown or bench verified. Exact reference gains are preserved; they are not evidence that these gains stabilize an F450 with BNO055 and 50 Hz ESCs. See [VALIDATION.md](VALIDATION.md) for executed checks and results.

## Files and build

- `F450_ESP32_FlightController/F450_ESP32_FlightController.ino`: complete Arduino sketch.
- `F450_ESP32_FlightController/Control.h`: independently testable PID, quaternion and mixer mathematics.
- `f450_ground_station.py`: Tkinter laptop GUI, Python standard library only.
- `test_control.cpp`: native mathematical regression tests.

Use ESP32 Dev Module / DevKit V1, Arduino ESP32 **2.0.17**, 4 MB flash, 115200 serial. Core 3.x has different LEDC/watchdog APIs and is not supported by this sketch. Install Adafruit BNO055 1.6.4, Adafruit Unified Sensor, Adafruit BusIO and ArduinoJson 6.21.5 through Library Manager. Open the sketch inside its matching folder. Alternatively, the pinned PlatformIO environment selects the correct core:

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
| Altitude PID | absent | 0, 0, 0; disabled until explicit nonzero Kp and ALT request |
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
| Mixer scale | 1.024 | 1.024 retained in pulse calculation |
| Mixer output cap | if >2000 then 1989 | retained exactly, including its discontinuity |
| Running idle / cutoff | 1180 / 1000 | 1180 / 1000 µs |
| Low throttle cutoff | PPM throttle <1050 resets motors and PID | explicit DISARM/STOP resets PID, writes 1000 µs |
| Arming | no explicit armed state | explicit ARM with valid reference, fresh sensors/commands and neutral input |
| Receiver loss | 10,000,000 µs without >10 channel change | 300 ms without a valid newer CMD; repeated neutral packets remain alive |
| PWM | GPIO5/18/19/15, 250 Hz, 12-bit duty | GPIO25/26/32/33, 50 Hz, 16-bit pulse conversion |
| Battery ADC | GPIO25, 3.3 V /4095, divider 77600/29400 Ω, warning <9 V | omitted: GPIO25 is now M1 and no battery sensor specified |
| Watchdog | commented-out 5 s initialization/reset | active 1 s task watchdog, reset to disarmed boot |

PID equation: `I=clamp(Iprev+Ki*(e+eprev)*dt/2,−400,400)`; `u=clamp(Kp*e+I+Kd*(e−eprev)/dt,−400,400)`. Roll/pitch angle outputs are desired body rates; rate outputs feed the mixer. Yaw is **rate control**, with zero command requesting zero yaw rate. The reference has no yaw heading controller; relative heading is displayed and wraps at ±180°. Quaternion calculations are continuous across that crossing (quaternion sign equivalence is respected). Euler angles have their usual pitch singularity; armed operation beyond 65° roll/pitch faults.

The reference scalar Kalman estimator fuses raw acceleration with gyro. Applying it unchanged on top of an already fused BNO attitude would double-fuse correlated measurements with unrelated noise constants. It is therefore replaced at the sensor-estimation boundary. The cascaded angle/rate control equations and numerical gain defaults remain intact. BNO fusion latency and 50 Hz actuator update are material changes to dynamics; no matching closed-loop stability is claimed. The reference's 1.024 scale approximates converting microseconds into 12-bit counts at 250 Hz. Here the same numerical mixer scale is retained explicitly before the proper pulse-to-duty conversion, so it produces a 2.4% pulse increase before clipping rather than functioning as a hardware unit conversion. Validate the resulting pulses and collective base on the bench.

## Mixer signs

Reference outputs are `1.024*(T−R−P−Y, T−R+P+Y, T+R+P−Y, T+R−P+Y)` in its motor ordering. For this specified arrangement, implemented outputs before clamps are:

| Motor | Equation | +roll | +pitch | +yaw |
|---|---|---|---|---|
| M1 front-right CW | 1.024(T−R+P−Y) | decrease | increase | decrease |
| M2 front-left CCW | 1.024(T+R+P+Y) | increase | increase | increase |
| M3 rear-left CW | 1.024(T+R−P−Y) | increase | decrease | decrease |
| M4 rear-right CCW | 1.024(T−R−P+Y) | decrease | decrease | increase |

Negative corrections reverse these effects. Left lift produces right-down roll, front lift produces nose-up pitch; CCW rotor acceleration produces clockwise body reaction. Mathematical checks do not verify actual motor direction, propeller orientation or sensor mounting.

## Startup reference and faults

Power on stationary on a **level surface**, with propellers removed for bench work. IMUPLUS avoids magnetometer dependence and does not require manual calibration or stored calibration offsets. It supplies relative yaw with drift, not a magnetic compass heading. No system can infer that an arbitrary stationary support surface is level from these sensors alone.

Initialization uses Adafruit BNO055. Subsequent register reads check transaction lengths, fusion status/error and quaternion norm (0.95–1.05). The default unit settings use 1/16 deg/s gyro units. Capture requires 200 consecutive 10 ms samples: acceleration magnitude within 0.5 m/s² of gravity, each gyro below 3 deg/s, and orientation within 1° of the start of the window. Motion or invalid reads reset that window. The last valid normalized quaternion becomes q0 and the mean gyro becomes bias. Feedback is `inverse(q0)*q`, converted to roll/pitch/yaw; body rates subtract bias. Once captured, reference is never recaptured until reboot, even after sensor failure.

This removes initial attitude offset and estimates stationary gyro bias. It cannot remove scale errors, temperature drift, vibration, sensor misalignment, or acceleration-induced fusion errors. Constant readings cannot conclusively prove sensor freshness; acknowledged I2C/status checks catch disconnects and invalid responses but not every internal frozen-data fault. Initialization failure keeps ARM unavailable until reboot; the library's initialization may wait for the device while the independently running controller continues disarmed.

ARM requires reference ready, IMU age <50 ms, valid command age <300 ms, all commands neutral and roll/pitch within 5°. An ARM starts running at the configured base throttle. Sensor failure, sensor age >50 ms, command loss >300 ms, no AP clients, excessive tilt, altitude range loss while holding, or control interval >8 ms disarms. Faults latch disarmed until a fresh explicit ARM; STOP always works. There is no automatic landing on link failure. A 1 s task watchdog reboots if control stops progressing; boot always writes cutoff pulses before sensors initialize.

Ultrasonic triggering is every 60 ms; echo width is captured by interrupt without pulseIn. Accepted slant distance is 2–200 cm, using speed of sound 0.0343 cm/µs. `height_cm = distance_cm + 4.03` exactly as requested, without tilt compensation. Tilt, ground slope, soft surfaces, temperature and rotor wash affect it. ALT requires a recent valid reading and nonzero altitude Kp; invalid readings are discarded, and no valid measurement for 250 ms during hold disarms. The last value is displayed alongside range validity.

Altitude PID is an added controller in µs/cm, µs/(cm·s), and µs·s/cm. Its correction is added to base throttle. R/F change target at ±30 cm/s, bounded 6.03–204.03 cm. Zero gains are deliberate unconfigured defaults, not reference values. Set and validate altitude gains and measured hover base throttle on a suitable test fixture before attempting altitude hold. No arbitrary hover throttle or altitude PID is presented as flight-tested.

## Ground station

Install Python 3.10+ with Tcl/Tk (included in standard Windows installer); no pip GUI dependency is needed. On Linux install your distribution's `python3-tk`. Join **F450_DRONE**, password **F450Drone2026**, then:

```powershell
python f450/f450_ground_station.py
```

Allow UDP inbound 4211 on the laptop firewall. Use ARM button only after startup telemetry is valid. W/S pitch forward/back, A/D roll left/right, Q/E yaw left/right, R/F altitude up/down, SPACE clears pilot input, X sends STOP. Keyboard demands are full normalized steps, equivalent to ±50° angle and ±75 deg/s yaw; this preserves reference command range and requires particular care during restrained validation. Typing in entry fields suppresses pilot keys except X. Focus loss clears keys; GUI sends neutral commands at 50 Hz. X also works while editing a field. Closing sends STOP three times and RELEASE; communication timeout is the fallback.

Current gains are displayed separately from editable fields. ± buttons change by 0.01; numeric entries allow other precision. Send all PID gains applies atomically on ESP32 and resets PID history to prevent stored integral/derivative history from carrying across configurations. Runtime tuning can produce abrupt output changes. ACK reports acceptance/rejection; missing ACK is displayed after 1 s. Reset restores the five reference gain triples and zero altitude gains, disables altitude hold, and leaves NVS unchanged. Save gains to NVS is allowed only disarmed; validated stored gains load on reboot. Use Reset then Save to persist original defaults. Base throttle and target are not persisted. Base throttle is editable only disarmed, range 1180–1800 µs.

## UDP protocol, version 1

UTF-8 JSON datagrams, commands to 192.168.4.1:4210, replies/telemetry to sender IP:4211. This documented JSON protocol replaces the suggested CSV to allow explicit gain arrays and status fields. Max command length 1199 bytes. No authentication beyond AP password; use a dedicated network. First valid envelope claims ownership until disarmed RELEASE/reboot. Restarted clients with a new sequence should send RELEASE first if OLD_SEQUENCE occurs. Only valid newer CMD packets refresh the command-loss timer. Other traffic cannot keep the aircraft armed.

Each command has `type` string and `seq` unsigned 32-bit integer. Sequences must advance with modular difference in (0, 2³¹). Invalid command payloads consume their envelope sequence but do not refresh flight commands. STOP/DISARM are accepted from any sender regardless of sequence. A disarmed RELEASE from the current owner also bypasses sequence comparison to allow a restarted client to recover; the GUI sends RELEASE at startup. ACK: `{"type":"ACK","seq":123,"ok":true,"reason":"OK"}`; rejected commands return ok false and a reason. Malformed JSON/envelopes are ignored.

| Type | Additional fields | Meaning |
|---|---|---|
| CMD | roll,pitch,yaw,climb: finite numbers −1..1 | pilot input, transmitted continuously |
| ARM / DISARM / STOP | none | explicit arm / cutoff |
| GET | none | ACK includes current gains and base |
| PID | gains: six arrays of three finite numbers, each 0..100 | order roll angle, pitch angle, roll rate, pitch rate, yaw rate, altitude; Kp,Ki,Kd |
| RESET | none | reference defaults plus zero altitude gains |
| SAVE | none | persist current gains only, disarmed |
| BASE | value: 1180..1800 | manual collective base µs, disarmed |
| ALT | value: 6.03..204.03 | enable hold and set target frame height cm |
| MANUAL | none | disable hold and clear altitude PID history |
| RELEASE | none | release owner and sequence tracking, disarmed |

Example: `{"type":"CMD","seq":123,"roll":0,"pitch":0,"yaw":0,"climb":0}`.

TEL packets at 10 Hz contain relative and absolute quaternion-derived angles (degrees), bias-corrected gyro and bias (deg/s), distance/height/target (cm), all gains, base, outputs (roll,pitch,yaw,altitude), four pulse commands, AP client count, IMU/range validity and age, command age, reference/arming/altitude status, fault, watchdog state, overrun count, and control age. Absolute yaw is fusion yaw and is not geographic heading. All arrays follow the ordering documented above; motors are M1..M4. The GUI accepts packets only from 192.168.4.1. It displays telemetry loss after 500 ms.

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

No reference-ready indication: verify address 0x29, wiring, sensor status and stationary window; reboot after fixing initialization failures. OLD_SEQUENCE/OWNER_BUSY: disarm, RELEASE, then query again. No telemetry: check Wi-Fi subnet, UDP firewall and port conflicts. ARM rejected: neutralize keys and inspect IMU age/reference/tilt. Altitude rejected: inspect range validity and configure altitude Kp. Repeated timing faults: inspect task loading and I2C behavior; do not enlarge thresholds to conceal missed deadlines.
