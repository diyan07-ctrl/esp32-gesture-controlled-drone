# Stage 3 safety and physical verification

This is the current F450 procedure. Directions are intended configuration, not observed physical results. Legacy gesture firmware uses a separate configuration and must not be used to infer this mapping.

| Motor | Position | GPIO | Intended rotation viewed from above |
|---|---|---:|---|
| M1 | Front-right | 25 | CW |
| M2 | Front-left | 26 | CCW |
| M3 | Rear-left | 32 | CW |
| M4 | Rear-right | 33 | CCW |

BNO055: 0x29, SDA21, SCL22, 3.3 V; logical X forward/Y right/Z down. HC-SR04: TRIG18, ECHO19 through 1 kΩ series/2 kΩ to ground, regulated 5 V, common ground. Height correction remains +4.03 cm.

FRD +roll decreases M1/M4 and increases M2/M3; +pitch increases M1/M2 and decreases M3/M4. +yaw decreases CW M1/M3 and increases CCW M2/M4, producing clockwise body reaction viewed from above. Negative corrections reverse these signs. These are mathematical expectations verified in software, not physical motor/propeller verification. Mixer signs were not changed in Stage 3.

## Packet ordering and recovery

Commands are JSON to 192.168.4.1:4210, ACK/TEL to sender:4211; AP F450_DRONE / F450Drone2026. `seq` must be a JSON unsigned 32-bit integer, never a bool, string, fractional, negative or overflowing value. Only strictly forward differences less than 2³¹ are newer. Wrap 0xffffffff→0 is valid; exactly half-range is ambiguous and rejected. Invalid envelopes, concatenated JSON objects and trailing garbage/embedded NUL suffixes are ignored; ordinary trailing whitespace is accepted. Invalid payloads consume their accepted sequence without renewing CMD freshness.

STOP/DISARM execute immediately without ordinary ordering restrictions. A newer owner STOP advances the high-water mark; stale/foreign STOP never moves it backward or replaces it with another sender's sequence. Every STOP changes the arm token and clears armed output. Thus an old ARM with sequence before STOP fails ordering, and an unseen ARM with sequence after STOP but generated earlier fails its token check. Duplicate STOP may invalidate the token again; retrieve the new value before another explicit ARM. RELEASE can reset sequence only while disarmed and from the owner; it changes the token and cannot clear a latch. Boot tokens are randomized; tokens cannot wrap/reuse within a boot. This is not a network authentication scheme.

ARM requires `arm_token` from current ACK/TEL plus current state/input checks. Successful ARM consumes it. CLEAR_FAULT requires the current token, healthy inputs and a recoverable FAILSAFE; successful clearance consumes it and leaves DISARMED. A new token and separate explicit ARM are then required. GUI supports this automatically on button presses, never automatic retries. Out-of-order telemetry/ACK tokens can cause rejection; query again. No packet received before a stop/clear may use the resulting token.

| Event/state | Next state | Motor request | Recovery |
|---|---|---|---|
| Reset/BOOT | sensor initialization/reference, then DISARMED | 1000 µs after PWM initialization | fresh reference and explicit ARM; persisted gains do not persist arming |
| Ordinary STOP/DISARM | DISARMED | all 1000 µs immediately on handling | new token, fresh neutral CMD, separate ARM |
| STOP during initialization | remain in initialization | all 1000 µs | complete startup; no bypass |
| Link/range loss | latched FAILSAFE | all 1000 µs | healthy inputs; range recovery if range failed; explicit CLEAR_FAULT, then fresh ARM |
| IMU/PWM/motor/timing/watchdog error | latched FAULT | all 1000 µs when output hardware works | correct cause and reboot; CLEAR_FAULT/STOP/repeated ARM cannot clear |
| STOP or sensor recovery in a latched state | same latch | all 1000 µs | no automatic rearming |

Every normal arming precondition remains: valid startup reference, IMU ≤50 ms, CMD ≤300 ms, AP client, neutral axes, idle collective 1180 µs, roll/pitch <5°, successful PWM and watchdog setup. Range recovery additionally requires health from three consecutive valid measurements. A persistent condition blocks clearance or arming; no forced bypass command exists.

## Ultrasonic interruption and recovery

Echoes are associated with a pending 60 ms trigger cycle; a 15 ms timeout invalidates the sensor. Distance stays 2–200 cm. Tilt must be ≤30° and IMU fresh. Vertical estimate is D*cos(roll radians)*cos(pitch radians)+4.03 cm, assuming level startup and body-down alignment. Median-of-three and a τ=0.15 s low-pass are retained.

Any invalid echo, timeout or unusable IMU resets the consecutive count, ring index and filter samples. The old displayed height is not marked healthy. The next valid sample initializes the filter from the new height; two samples are still unhealthy; the third consecutive valid sample restores health. Another invalid reading restarts recovery. After >250 ms without an accepted reading, the next update starts a new three-sample window. `range_recovery_samples`, `range_ok`, age and rejection count explain status. No invalid/stale readings accumulate altitude integral. Recovery of range never clears a failsafe or arms automatically. Rotor wash, ground slope, soft/reflective surfaces, temperature and beam obstructions still require physical evaluation.

## ESC output semantics and stalls

The pinned Arduino-ESP32 2.0.17 LEDC channels 0–3 produce 50 Hz, 16-bit duties on 25/26/32/33. Conversion occurs once: round(pulse_us×65536/20000); no 1.024 scaling is applied. Counts for 1000/1180/1500/2000 µs are 3277/3867/4915/6554. Counts quantize pulse widths by at most 0.153 µs before oscillator error. Hardware scope checks remain required.

BOOT globals are disarmed with 1000 µs requests. PWM setup attaches channels then writes those requests before sensor initialization. ARM begins at 1180 µs idle only after checks; collective slew remains 100 µs/s afterward. STOP, DISARM, FAILSAFE and FAULT request 1000 µs and clear controller/mode state. This is a **minimum-throttle pulse**, not disabling the signal: LEDC continues issuing pulses. Actual ESC response, including whether the motor stops, depends on its model, endpoints and calibration; 1000 µs is not a verified shutdown guarantee. The firmware does not detach the signal as a substitute.

If the control task stalls, LEDC can continue its previous duty autonomously. The registered 1 s task watchdog is the software recovery mechanism; its initialization, registration and feed results are now checked and reported. Until panic/reset, previous PWM may persist. This is not an independent hardware motor cutoff or a proven worst-case stop latency. During MCU reset/bootloader execution, application code does not control GPIO waveforms or ESC signal-loss response. After setup resumes, firmware requests 1000 µs and remains disarmed. Verify reset behavior and watchdog behavior on a restrained, propeller-free bench; use appropriate external power isolation for that test.

## Propeller-free identification procedure

1. Disconnect propulsion power and physically remove all four propellers. Mark front and label M1..M4 using the table. Verify signal wiring continuity from each GPIO to the intended ESC; keep common ground.
2. With propulsion disconnected, scope each output at boot and STOP to identify its pin, 50 Hz period and nominal 1000 µs pulse. Confirm wiring identity rather than inferring it from rotation.
3. On a secured propeller-free setup, power ESCs using the manufacturer's prescribed procedure. Keep STOP and power isolation accessible. Complete startup, confirm neutral controls/idle collective, then explicitly ARM. Idle is a motor-running command; do not assume a specific shaft response.
4. Observe each shaft/bell separately from above, using a visible mark or suitable recording. Record actual CW/CCW for each labeled position and compare with intended mapping. Do not infer direction solely from wire colors or low-frame-rate video. If idle does not rotate reliably, follow the ESC manufacturer's test procedure; do not raise outputs arbitrarily.
5. STOP, verify physical motor response, then disconnect power before changing any wiring. Correct an unintended rotor direction through the supported ESC/motor wiring method. Do not change yaw mixer signs to conceal wiring errors. Propeller installation/orientation is a later independent check.

No steps above were performed during software development.

## Reproducible software and later hardware tests

Run `python f450/run_tests.py` with c++ on PATH, or add `--zig PATH_TO_ZIG`. It runs original control tests, Stage 2 tests, Stage 3 sequence/state/recovery/PWM tests, Python tests and syntax checks. Install dependencies with `python -m platformio pkg install -d f450`; build with `python -m platformio run -d f450`. See [../VALIDATION.md](../VALIDATION.md) for actual output and final image sizes.

Then physically test startup motion, IMU unplug, invalid echoes, full three-sample recovery, fault-latch clearance, delayed ARM/STOP ordering over real UDP, stale CMD cutoff, NVS save/reboot and mode transitions. Measure PWM on all channels during STOP, reset and forced watchdog stall, and measure loop jitter under traffic. Manual throttle, explicit altitude mode, target/collective ramps, PID editing and NVS persistence are preserved. Altitude gains remain unconfigured until entered and reviewed; no bench-validated gains are supplied. Keep software verification, bench results and any later controlled flight results as separate records.
