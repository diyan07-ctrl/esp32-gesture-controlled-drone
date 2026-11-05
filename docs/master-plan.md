# F450 ecosystem architecture and implementation contract

Date: 2026-11-05. Status: initial design for review, not an implemented release.
Owner confirmed MPU6050 on both boards and potentiometer throttle in this session.
This supersedes the November 3 MPU6500 / gesture-throttle target, but does not
retroactively change the hardware or validation claims of the existing source.

## Repository inspection

Inspected clean main at `14879d3`, following implementation `812d5b6`.
Remote: https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone.
Fetched origin before creating `development/f450-ground-station`; main is preserved.
No AGENTS.md was found in the repository file inventory.

The project already has PlatformIO glove, drone and motor_test environments;
shared explicitly encoded command/status packets, CRC/session/sequence checks,
encrypted peer transport, flex calibration, estimator, rate PID, mixer, safety
supervisor, unit/integration/simulation tests and CI. Preserve and extend these.
The separate motor_test image is the retained motor baseline; the owner-reported
ESP32Servo sketch is not identifiable as a separate tracked source file. Existing
LEDC output is 50 Hz, 1000–2000 us; physical compatibility remains unverified.

| Area | Observed source | Required change |
|---|---|---|
| IMU | Strict MPU6500 identity 0x70, including ACCEL_CONFIG2 writes | Explicit MPU6050 profile, identity/config readback and tests; do not accept 0x68 while retaining MPU6500-only register writes |
| Throttle | Flex gesture increments/holds command | Calibrated ADC1 potentiometer on proposed glove GPIO36; remove flex throttle accumulation in target profile |
| Config | Legacy throttle constant 34 overlaps thumb flex 34 | Separate pin names and compile-time collision checks |
| Control | 250 Hz target, measured dt, cascaded angle/rate, bench gains | Independent per-axis gain/filter settings and full diagnostics; physical timing/tuning |
| Serial | Bounded best-effort 5 Hz CSV-like diagnostics | Versioned framed CRC telemetry, 50 Hz initial target, instrument worker |
| Desktop | Parser utility only; no ground_station application | Separate packaged Python desktop app and tests |
| Failsafe | 200 ms loss causes latched no-pulse shutdown | Preserve bench-only behavior; airborne policy remains a flight blocker |

## Final software boundaries

System A owns sensor acquisition, calibration and pilot intent. At a nominal
250 Hz it reads both flex inputs, the potentiometer and IMU, estimates hand tilt,
applies neutral/deadband/slew limits and resolves mutually exclusive gestures.
Transmit bounded normalized commands at a proposed 50 Hz independently of
sampling. Thumb/index produce deliberate arm/disarm/stop states; the pot alone
sets collective. Arming requires calibrated endpoints, low pot throttle, released
then held arm gesture, healthy sensors and acknowledged receiver readiness.
Final gesture combinations must have tests for debounce, ambiguity and emergency
priority before replacing the existing mapping.

System B owns every flight-critical decision. The high-priority task reads and
validates IMU, snapshots the latest validated radio command, runs the supervisor,
estimator, outer angle and inner rate controllers, mixes and writes PWM. Keep
250 Hz as an unmeasured starting target, 50 Hz PWM as a separate actuator rate.
Use measured monotonic dt and deadline counters, not assumed sample periods.
No formatting, serial writes, NVS writes or per-cycle allocation in that task.
Snapshot publication uses fixed-size storage and bounded nonblocking handoff.
ESP-NOW callbacks only validate source/length enough to enqueue a bounded copy;
control-task validation checks all fields before use. RF remains asynchronous.

System C consumes USB telemetry. It cannot arm, drive individual motors or supply
flight-loop feedback. Serial parsing and logging run outside the GUI thread;
bounded buffers and latest-state replacement prevent lag accumulation. Parameter
monitoring is read-only by default. Later configuration mode requires onboard
disarmed state, zero throttle, explicit local configuration permit, bounded values,
ACK/readback, and a separate explicit save transaction. Never write NVS in flight.

## Frames, signals and control contract

Use body FRD: X forward, Y right, Z down. Angles/rates are degrees and degrees/s
at external interfaces; quaternion wire ordering is w,x,y,z. Define attitude as
active body-to-local-NED rotation; convert FRD/NED to the renderer's Z-up scene
once at its boundary. Yaw is relative integrated gyro yaw, not absolute heading.
Identity and positive single-axis rotations need analytic tests and physical
tilt verification. Use shortest-path normalized quaternion SLERP between measured
samples; never extrapolate beyond the latest received attitude.

Keep the documented owner-reported motor layout FR/FL/RL/RR and GPIO25/26/32/33;
independently verify the physical mapping before enabling outputs. The mixer is
T-R+P+Y, T+R+P-Y, T+R-P+Y, T-R-P-Y for M1–M4. Differential scaling and collective
shift avoid simple independent clipping; report requested and allocated outputs.
PWM command and output percentage are not measured thrust or RPM.

Adopt bounded roll/pitch angle P to desired body rates and independent roll,
pitch, yaw rate PID. Retain derivative on measurement, derivative low-pass,
integral limits, saturation feedback and reset while disarmed. Improve the
current global saturation freeze toward directional axis feedback only with
tests. Gains stay disabled for flight acceptance until measured tuning. See
[source comparison and recommendation](open-source-comparison.md).

Keep complementary estimation as a small, testable bench baseline. Calibration
must progress from stationary gyro bias/level offset to six-face scale/offset,
neutral capture and persisted records with version/checksum. Gate accelerometer
correction under non-gravity acceleration; log rejection and saturation. Compare
a pinned quaternion AHRS against recorded data before adopting it. Additional
filters require measured noise spectra and phase-delay justification. Existing
20 ms derivative filtering is about 8 Hz, not a universally suitable setting.

## Wireless and telemetry contracts

Retain shared command v2 initially: sessions, sequence, CRC, scaled demands and
explicit MAC/channel/key provisioning. Extend only through a versioned migration
with codec tests on both boards. CRC detects corruption; encrypted unicast provides
the configured RF security layer. Record accepted/lost/rejected packets separately.
Receiver-local arrival age drives timeout; transmitter clocks are not synchronized.
Timestamp, finger state, arm request and mode must have explicit units and ranges.

New USB telemetry v1 design: COBS-encoded binary records terminated by zero, a
magic identifier, version, message type, payload length, sequence, boot/session ID,
uint32 monotonic microseconds and CRC32 over header/payload. Explicit little-endian
encoding, bounded maximum decoded record (initial budget 384 bytes), no native
struct serialization. Publish the exact field offsets in a schema and cross-test
Python/C++ golden vectors before integration. Firmware boot text must be isolated
or the parser must discard it until a valid delimited record arrives.

Payload includes quaternion, Euler display angles, rates, raw ADC/IMU counts,
scaled/calibrated IMU, desired angle/rate, all three PID error/P/I/D/output values,
requested throttle, allocated collective, motor outputs, actual commanded PWM,
sensor/calibration/link validity bits, receiver counters/last sequence/age, state,
faults, loop period/execution/max/misses and telemetry drop count. Battery carries
an installed/valid flag; absent sensors produce unavailable readings, not zero volts.
Distinguish requested motor demands from actually suppressed actuator outputs.

Start at 50 Hz / 460800 baud after bridge verification. For a 384-byte decoded
upper bound, worst COBS overhead plus delimiter is at most 387 bytes: 19,350 B/s
at 50 Hz versus 46,080 B/s theoretical 8N1 capacity at 460800. At 115200 only
11,520 B/s is available: do not send that record at 50 Hz. Actual payload, driver
and rendering costs require measurements before 100 Hz. Latest-only control
snapshot, full-record capacity check and drop counter prevent backpressure.

Host tracks USB sequence gaps, RF sequence gaps, malformed frames, boot changes,
receipt timestamps and stale age separately. Clock offset is unknown: measure
receipt-to-paint latency first; true sensor-to-screen latency requires shared
instrumentation or a characterized clock synchronization procedure. Do not present
uncorrected cross-clock subtraction as end-to-end latency.

## Directory decision

Keep existing names rather than duplicating shared code or moving working files.
Only the ground_station subtree below is a planned addition, not current software.

~~~text
firmware/
  common/{configuration,protocol}/  shared control and wire contracts
  glove/src/                       transmitter and calibration
  drone/src/                       onboard flight controller
  motor_test/src/                  preserved standalone bench image
ground_station/                    planned separate application
  app/                            lifecycle, config, worker coordination
  dashboard/                      instruments and bounded plots
  visualization/                  F450 mesh, camera, quaternion transforms
  telemetry/                      codec, serial worker, recording, replay
  requirements.txt
  main.py
docs/                             architecture, wiring, operating/tuning evidence
hardware/{schematics}/             BOM and connection/motor CSVs
diagrams/                         existing wiring, layout and power diagrams
tests/{unit,integration}/          existing tests plus telemetry golden vectors
tests/ground_station/              planned codec, worker and attitude tests
simulation/tests/                  explicitly assumed plants
tools/                            host tests, provisioning and diagnostics
logs/                             planned ignored session data, no private keys
~~~

## Implementation order and acceptance evidence

1. Review this architecture, [hardware audit](hardware-audit-2026-11-05.md),
   [research](open-source-comparison.md), [desktop design](ground-station-design.md)
   and updated README. This is the immediate first-task review checkpoint requested
   in the master prompt; downstream implementation follows that review.
2. Add MPU6050 and pot profile in an isolated subsystem commit. Compile glove and
   drone independently. Mock identity/register/bias/ADC fault tests, then physical
   stable sensor/neutral/endpoints evidence. Retain motor_test source.
3. Add PID diagnostics and framed telemetry with malformed/partial/oversize/CRC,
   sequence wrap and C++/Python compatibility tests; compile drone independently.
4. Implement standalone ground station, codec/worker/replay/rotation tests, then
   real OpenGL launch and offline/stale checks. Replay is explicitly labeled.
5. Integrate actual boards, verify pairing/timeout/reconnect and signs props off;
   measure loop timing/PWM/serial rates under load. No board is presently assumed
   attached; absence of hardware blocks physical acceptance, not software work.
6. Document every result, commit each tested subsystem, push development branch,
   review exact-SHA CI. Release remains an engineering bench build until power,
   propulsion, sensor calibration, timing, airborne failsafe and flight gates close.

For every phase record source SHA, files, commands, exit codes, device IDs,
measurement method, actual results, failures and remaining work. Historical
[validation](validation-results.md) is not evidence for the proposed MPU6050 build.
