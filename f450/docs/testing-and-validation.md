# Stage 4 protocol, safety and physical verification

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

STOP/DISARM execute immediately without ordinary ordering restrictions. A newer active-owner STOP advances the high-water mark; stale/foreign STOP never moves it backward or replaces it with another sender's sequence. Every STOP changes the arm token and clears armed output. Thus an old ARM with sequence before STOP fails ordering, and an unseen ARM with sequence after STOP but generated earlier fails its token check. Duplicate STOP may invalidate the token again; retrieve the new value before another explicit ARM. Boot tokens are randomized and never wrap/reuse within a boot. These barriers are not network authentication.

Stage 4 ownership lifecycle is read-only GET discovery → matching-generation HELLO → ordered owner commands → valid RELEASE. Discovery returns session, owner_active, owned, last_seq, seq_set and arm_token alongside normal fields; it cannot change the sequence window. No ordinary packet claims an empty slot. HELLO while unowned accepts a fresh uint32 starting sequence; current-owner HELLO while disarmed must advance its existing sequence. Foreign HELLO while occupied and all HELLO while armed are rejected. Success rotates the arm token and requires a fresh valid CMD. Acquisition/resume never clears FAULT/FAILSAFE or arms.

Every owner command carries `session` (uint32 generation), `type` and `seq`. RELEASE checks ownership, generation, disarmed status and newer sequence before changing anything. Rejected RELEASE leaves owner, sequence high-water/set flags, generation, ARM token, pilot freshness and safety state unchanged. A valid RELEASE clears the owner/window/fresh-CMD flag, retires the generation, rotates the token and leaves motors/safety untouched. Repeated/delayed release while empty is NO_SESSION; after reacquisition it is rejected by retired generation or foreign IP. Old owner CMD/ARM/HELLO cannot act on the empty or newly acquired session, including same-IP reacquisition. Generations increment on release without reuse; generation/token exhaustion requires reboot. New laptops discover the current generation and explicitly HELLO. A restarted current owner can use discovery's last_seq to resume with last_seq+1, including wrap. There is no foreign-IP takeover or implicit timeout release.

The laptop no longer sends RELEASE at startup. It confirms ownership only on its matching successful HELLO ACK, sends pilot packets only when confirmed, and marks successful release unowned. A rejected release retains ownership only if the response confirms the same generation and owner IP. A release/HELLO timeout leaves ownership unconfirmed; no automatic ARM follows. Correlated ACKs synchronize lifecycle; unrelated old ACKs cannot restore/drop confirmed ownership. The Connect / resume button restarts discovery while disarmed. Close still sends best-effort STOP and RELEASE without guaranteeing delivery. Use one pilot process per owner IP; identifiers are not cryptographic user identities. Upgrade laptop and firmware together.

## Complete dispatcher authorization audit

Every row requires a valid uint32-sequence envelope. Missing/malformed fields required by that row are rejected before actions. Owner commands require current owner IP and generation. All normally ordered duplicates/delays are rejected before action; invalid action payloads consume their accepted sequence, except rejected RELEASE which never mutates order.

| Command | Authorization / ordering | Safety / motor effects | Duplicate or delayed behavior |
|---|---|---|---|
| GET without session | any IP, valid envelope; read-only, no sequence-window mutation | query only | harmless repeat; no ownership or heartbeat |
| GET with session | current owner/generation, newer seq | query only | rejected by normal ordering |
| HELLO | unowned matching generation or current owner resume; disarmed; resume requires newer seq | never arms/clears latch; resets CMD freshness and rotates ARM token | duplicate active-session HELLO rejected; retired generation cannot reclaim slot |
| STOP / DISARM | any IP, valid envelope; ordering exception | immediate minimum pulses; preserve latches; rotate ARM token | always safe-stop; foreign/stale sequence cannot poison owner's window |
| RELEASE | current owner/generation, disarmed, newer seq | retire session; no safety/output change; never clears faults | all rejection paths side-effect free; no active owner means NO_SESSION |
| CMD | owner/generation/newer seq; valid pilot/throttle payload | pilot setpoints; collective only in manual mode; cannot arm | duplicates/invalid data cannot renew 300 ms deadline |
| ARM | owner/generation/newer seq plus fresh CMD, correct arm_token and all existing checks | explicit DISARMED→ARMED only | session/token/sequence barriers and latch checks prevent replay |
| CLEAR_FAULT | owner/generation/newer seq, current arm_token, healthy recoverable FAILSAFE | clears to DISARMED only, rotates token; FAULT requires reboot | cannot automatically arm; later ARM must be explicit |
| MANUAL | owner/generation/newer seq | preserves current collective during mode transition; no arm | replay blocked before mode callback |
| ALT | owner/generation/newer seq plus valid target, range and reviewed gains | altitude mode/target; no arm | replay blocked before mode callback |
| ALT_VALIDATE | owner/generation/newer seq, disarmed, nonzero Kp, healthy range | review acknowledgement only; no arm | replay blocked |
| PID / RESET | owner/generation/newer seq; PID numeric validation | update gains/history/mode as previously implemented; may affect later armed outputs | rejected before callbacks when replayed; PID atomic validation preserved |
| BASE | owner/generation/newer seq, disarmed, valid µs value | manual demand configuration only | replay blocked |
| SAVE | owner/generation/newer seq, disarmed | gains to NVS only | replay blocked |
| Unknown type | owner/generation/newer seq | INVALID; no action | valid envelope consumes seq, no heartbeat |

There is no separate heartbeat command: only successful CMD refreshes commandTime and session.commandSeen. HELLO/GET/RELEASE/PID/CLEAR do not keep an aircraft armed. Control-task timeout, no AP client, sensor faults and STOP behavior remain unchanged. Physical PID/altitude performance is not inferred from protocol tests.

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

Run `python f450/run_tests.py` with c++ on PATH, or add `--zig PATH_TO_ZIG`. It runs original control tests, Stage 2 regressions, Stage 3 safety/recovery/PWM, Stage 4 production-dispatcher JSON tests, Python tests and syntax checks. Install dependencies with `python -m platformio pkg install -d f450`; build with `python -m platformio run -d f450`. See [../VALIDATION.md](../VALIDATION.md) for actual output and final image sizes. Native Stage 4 tests use production ingress/authorization/RELEASE/ACK logic with native hardware/control-action adapters; they are not ESP32 network or electrical tests.

Then physically test startup motion, IMU unplug, invalid echoes, full three-sample recovery, fault-latch clearance, delayed ARM/STOP ordering over real UDP, stale CMD cutoff, NVS save/reboot and mode transitions. Measure PWM on all channels during STOP, reset and forced watchdog stall, and measure loop jitter under traffic. Manual throttle, explicit altitude mode, target/collective ramps, PID editing and NVS persistence are preserved. Altitude gains remain unconfigured until entered and reviewed; no bench-validated gains are supplied. Keep software verification, bench results and any later controlled flight results as separate records.
