# Gesture-Controlled F450 Drone

Two ESP32 boards provide glove input and an experimental onboard quadcopter
controller. A separate USB laptop instrument cluster is the next planned subsystem.
**Engineering bench project; not flight-ready. Output verification gates remain false.**

[Repository](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone) ·
[CI](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/workflows/firmware-ci.yml) ·
[Design and phase plan](docs/master-plan.md) · [Hardware audit](docs/hardware-audit-2026-11-05.md)

## Status and hardware revision

The owner confirmed **MPU6050 on both boards and potentiometer throttle**.
Current source uses the MPU6050-specific profile and dedicated pot ADC36.
The driver deliberately rejects MPU6500. [Migration and sensor acceptance](docs/sensor-migration.md)
and [actual milestone results](docs/sensor-migration-results.md) distinguish
software checks from outstanding physical sensor tests.
The MPU6050/potentiometer migration is implemented for bench validation. The
desktop application remains planned. Current wiring and calibration instructions
now target MPU6050; historical validation records retain their original hardware.

| Subsystem | Implemented baseline | New target / acceptance still needed |
|---|---|---|
| Glove | MPU6050, stable flex/pot captures, NVS, filtering/hysteresis, ESP-NOW | Actual sensor/ADC/calibration and gesture bench acceptance |
| Drone | MPU6050, configurable complementary estimate; existing guarded controller retained | Independent physical IMU tests before further integration |
| Sensor diagnostics | Independent glove_sensors / drone_sensors images; no radio/ESC initialization | Real identity, signs, calibration, noise and timing records |
| Radio | Encoded v2 commands/status, CRC, sessions, encrypted peer | Actual two-board pairing, packet-loss and reconnect evidence |
| ESC | Separate motor_test, LEDC 50 Hz / 1000–2000 us candidate | Exact ESC compatibility, scope traces and motor checks |
| Desktop | Diagnostic parser utility | Standalone instrument cluster, 3D model, logging/replay |
| Safety | Guarded default outputs, bench fault latch | Validated airborne failsafe and all flight gates |

## Architecture

~~~text
Flex + potentiometer + glove MPU6050
             |
       Glove ESP32 ---- ESP-NOW validated commands ---- Drone ESP32
                                                          |
                                                MPU6050 -> estimator
                                                angle -> rate PID -> mixer
                                                          |
                                                 four ESCs / motors
                                                          |
                                               USB telemetry snapshots
                                                          |
                                             Laptop instruments / 3D / logs
~~~

All flight control remains onboard. Laptop disconnection cannot remove an arming
interlock or supply motor commands. FRD body axes: X forward, Y right, Z down;
positive roll right side down, pitch nose up, yaw clockwise viewed from above.
Yaw is relative gyro integration and drifts; there is no absolute heading,
altitude hold, position hold or autonomous landing.

Retain cascaded angle-P/rate-PID with separate axes, bounded integral/output,
filtered derivative on measurement and mixer desaturation. 250 Hz is a configured
control target, not measured sustained performance; PWM remains a separate 50 Hz
baseline. Gains are bench placeholders. [Upstream comparison, sources and license
review](docs/open-source-comparison.md) covers PX4, ArduPilot, Betaflight and x-io
Fusion. [Control equations and tuning](docs/flight-control-algorithm.md).

## Bill of materials

| Component | Specification | Qty | Purpose / compatibility |
|---|---|---:|---|
| Frame / distribution | F450 X frame / rated PDB | 1 | Verify orientation, terminals and current rating |
| Motors | A2212 1000KV | 4 | Exact maker, prop/load/current curves still needed |
| ESCs | 30 A | 4 | Identify model, BEC, logic level, PWM endpoints and shutdown |
| Drone MCU | ESP32 DevKit V1 | 1 | 3.3 V GPIO; regulated supply only |
| Glove MCU | ESP32 DevKit V1 | 1 | Independent power, ADC1 analog inputs |
| IMUs | MPU6050 breakouts | 2 | Driver checks identity 0x68; verify supply and I2C pullups |
| Flex sensors | Thumb and index resistive sensors | 2 | Measure flat/bent resistance and endpoints |
| Potentiometer | Linear analog throttle control | 1 | Actual resistance/travel and fault behavior to verify |
| Battery | 3S 11.1 V 2200 mAh LiPo | 1 | 12.6 V full; current capability/condition to validate |
| Propellers | Matched CW/CCW to actual motor/pack | 4 | Historical owner report 1045; load compatibility unverified |
| Electronics regulator | Verified board-compatible supply | 1 drone | Current/transient/USB backfeed validation required |
| Glove supply | USB or verified regulated source | 1 | No raw LiPo into MCU |
| Divider / conditioning | Two flex circuits and pot interface | 1 set | Select measured resistor values; safe ADC range |
| Harness / mounting | Rated connectors, wire, insulation, IMU mount | 1 set | Strain relief and separated power/sensor routing |
| Optional monitor | Protected calibrated battery divider | 1 | Disabled until electrically verified |
| Test equipment | DMM, controlled supply, scope/logic analyzer | Access | Required for electrical/timing acceptance |
| Loaded test equipment | Guarded thrust/current/thermal fixture | Access | Required before flight validation |

The detailed [BOM](hardware/bom.csv) now reflects MPU6050 and potentiometer.
HC-SR04 remains an optional historical accessory, disabled by default. No physical
harness or electrical compatibility is established by the inventory.

## Wiring

GPIO numbers are chip GPIOs, not connector positions. Remove propellers and
isolate propulsion power during wiring, firmware upload and sensor tests.

| Board | Signal | Connection | Status |
|---|---|---|---|
| Both | MPU6050 SDA / SCL | GPIO21 / GPIO22 | Retained candidate, pullups to 3.3 V |
| Both | MPU6050 VCC / GND | Verified compatible 3.3 V / local GND | Inspect actual breakout |
| Both | MPU6050 AD0 | GND for address 0x68 | Confirm identity separately |
| Glove | Thumb / index nodes | GPIO34 / GPIO35 | Separate 3.3 V flex dividers |
| Glove | Pot wiper | GPIO36 / VP | Implemented dedicated ADC1 input; confirm physical header |
| Glove | Pot endpoints | 3.3 V / GND | Confirm orientation and electrical travel |
| Drone | ESC1 signal | GPIO25, M1 front-right | Owner-reported, verify physically |
| Drone | ESC2 signal | GPIO26, M2 front-left | Owner-reported, verify physically |
| Drone | ESC3 signal | GPIO32, M3 rear-left | Owner-reported, verify physically |
| Drone | ESC4 signal | GPIO33, M4 rear-right | Owner-reported, verify physically |
| Drone | ESC signal grounds | Common electronics ground | Verify actual connector pinouts |
| Drone | Optional battery ADC | GPIO34 via verified protection/divider | Disabled |

~~~text
3V3 -- flex --+-- selected fixed resistor -- GND   (one per finger)
             +-- conditioning -- ADC34 or ADC35
3V3 -- pot end A; GND -- pot end B; wiper -- conditioning -- ADC36

3S battery -- rated power connection -- PDB -- ESC power -- motor phases
                                      |
                               verified regulator -- ESP32 supported input
ESP32 GPIO25/26/32/33 ------------------------------ ESC signals
ESP32 GND ----------------------------------------- ESC signal grounds
ESP32 3V3/GND -- MPU6050 supply; GPIO21/22 -- SDA/SCL
~~~

[Current sensor wiring contract](docs/sensor-migration.md) is checked against source
and automated assertions. GPIO assignments are software-verified; actual continuity,
voltage levels and harness placement remain unverified. The
[design-stage audit](docs/hardware-audit-2026-11-05.md)
provides divider selection and fault considerations. Fixed flex resistors depend
on measured sensor resistance; no exact value is established. GPIO34/35/36 have
no internal pull resistors. Keep analog inputs in their characterized safe range,
never above 3.3 V. Pot usable travel must stay inside ADC counts 32..4063;
design/verify end resistance or travel stops before calibration. Rail and calibrated-
range faults clear throttle, but a plausible floating/stuck wiper needs hardware
fault protection and remains a physical-test risk.

Never apply 3S directly to ESP32/IMU. Do not parallel BEC positives or assume every
ESC includes a BEC. Identify and isolate unused positive leads, use one verified
supply scheme and check simultaneous USB/external power. Common signal reference
is required; propulsion current must not flow in sensor ground wiring.

[Existing wiring diagrams](diagrams/README.md), [motor layout](diagrams/motor-layout.svg)
and [schematic status](hardware/schematics/README.md) are reference material.
A component-specific electrical schematic remains pending exact ESC, regulator,
breakout and analog-component identification; these drawings are not a verified
manufacturing schematic. GPIO order alone does not prove physical motor numbering.

## Firmware installation and baseline build

Current reproducible baseline: Python 3.11, PlatformIO Core 6.1.18,
Espressif32 6.9.0, Arduino ESP32 2.0.17, board esp32dev. Wire, WiFi and Preferences
are framework libraries; no external PID or IMU dependency is required. Current
ESC adapter uses LEDC; the historical ESP32Servo motor test is owner evidence,
not proof of this adapter's electrical behavior.

~~~powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.18
.\.venv\Scripts\python.exe -m platformio run -e glove
.\.venv\Scripts\python.exe -m platformio run -e drone
.\.venv\Scripts\python.exe -m platformio run -e motor_test
.\.venv\Scripts\python.exe -m platformio run -e glove_sensors -e drone_sensors
.\.venv\Scripts\python.exe tools/run_host_tests.py
node tools/check_repository.mjs
~~~

The native test runner needs a C++ compiler; Windows may use Zig 0.13.0 and
`--zig .venv/Lib/site-packages/ziglang/zig.exe`. Open the repository root in
VS Code with PlatformIO, preserving the common platformio.ini. Do not create
three unrelated Arduino sketches. [Detailed build/upload guide](docs/build-and-upload.md).

Before uploading, identify each board by USB enumeration and its own station MAC.
Use a verified current port; COM9 was historical, not guaranteed. First use the
sensor-only image on each respective board; COMx below is a placeholder:

~~~powershell
.\.venv\Scripts\python.exe -m serial.tools.list_ports
.\.venv\Scripts\python.exe -m platformio run -e glove_sensors -t upload --upload-port COMx
.\.venv\Scripts\python.exe -m platformio device monitor -p COMx -b 115200
~~~

Choose `-e drone_sensors` for the independent drone IMU test. Keep props removed,
propulsion disconnected and output gates false. The diagnostics initialize no ESCs
or radio. Record separate physical passes on both boards before further flight-
control integration. Normal images remain `-e glove` and `-e drone`.

## ESP-NOW setup

1. Obtain each board's printed station MAC from its correct firmware/serial port.
2. Follow [private provisioning](docs/communication-protocol.md) using the pairing
   tool; configure explicit peer MACs, compatible channel and private keys on both.
   Do not commit keys or local_secrets.h.
3. Compile and flash each board separately; both must use matching command v2.
4. Verify received sequence, acknowledgments, accepted/rejected/lost counts and
   receiver-local command age with props off. Default unprovisioned images cannot pair.
5. Test corruption, loss, timeout and reconnect: reconnection must never re-arm.
   Keep the laptop outside the command path.

[Protocol details](docs/communication-protocol.md) explain encoding and freshness.
The proposed USB COBS/CRC protocol is separate and described in the
[architecture](docs/master-plan.md); it is not yet the current 5 Hz serial format.

## Laptop application plan and installation status

Use Python 3.11, PySide6, pyqtgraph, PyOpenGL, NumPy and pyserial in a dedicated
virtual environment. There is **no ground_station/main.py or requirements file
in the current baseline**, so no dashboard launch is claimed. Dependency pinning,
installation commands and runnable package will accompany that subsystem commit.
[Desktop design and acceptance](docs/ground-station-design.md).

The planned application has a dark instrument cluster, quaternion-driven F450
model, four motor bars, throttle, attitude/rate cards, PID plots, raw/scaled IMU,
link/USB health, loop timing, logging, CSV export and labeled replay. Port/baud,
connect/disconnect, fullscreen, graph pause/reset, camera orbit/reset and
telemetry display controls remain desktop features. Serial acquisition uses a
worker thread and bounded queues. Offline/stale display freezes and labels the
last attitude; no simulated live movement. PWM is commanded output, not measured
thrust/RPM. Battery is unavailable until a verified divider is installed.

Identify the actual CP210x/CH340 or other USB bridge before installing its vendor
driver. A port cannot be shared with Serial Monitor. Hardware OpenGL rendering,
50 Hz throughput and latency need measurement; no zero-latency claim is made.
PID editing will require separate guarded configuration mode and explicit save;
initial desktop monitoring must be read-only.

## Calibration and operating procedure

The current [calibration guide](docs/calibration-guide.md) documents the migrated
MPU6050 and flex/pot pipeline: stationary startup gyro/level offsets, `o`/`b` flex
and `l`/`h` pot endpoint captures, then explicit `s` to save. No default endpoints
permit commands. Level offset is not six-face accelerometer calibration.
Do not save guessed endpoints or claim calibrated heading from a six-axis IMU.

1. Inspect frame, connectors, polarity, LiPo condition, regulator, IMU mounting and
   documented motor map. Remove all props for initial work.
2. Power electronics with propulsion isolated; inspect rails and sensor identity.
3. Start glove at measured low throttle and neutral pose; calibrate flex, pot,
   gyro bias and accelerometer as documented for the active hardware profile.
4. Start drone stationary/level; verify calibration, IMU signs and measured loop timing.
5. Verify RF sequences/ages, explicit stop, timeout and no automatic re-arm.
6. Connect dashboard only once implemented; inspect valid actual telemetry and
   correct display direction, output inhibition and warnings.
7. Arming requires low throttle, released-then-held deliberate arm input, healthy
   calibration/link/timing and all physical output gates. Default builds cannot arm
   motor outputs. Do not clear gates merely to make the system run.
8. Follow ordered propeller-off tests, then guarded characterization and reviewed
   control-sign/tuning procedures. No aggressive loaded tuning or free flight here.
9. Disarm deliberately, verify actuator shutdown and isolate battery power before
   handling propulsion. Disconnect glove/USB as appropriate for verified power wiring.
10. Inspect temperatures, damage, pack condition and logs; record failures and
    corrective actions before another run.

[Ordered acceptance tests](docs/testing-and-validation.md) ·
[Safety supervisor](docs/safety-and-failsafe.md) · [Output gates](docs/output-enablement.md).
Current link loss after 200 ms latches no-pulse shutdown. **This is a bench failsafe
that can crash an airborne aircraft.** No validated landing/airborne loss policy
is implemented; that remains a flight blocker.

## Troubleshooting

| Symptom | First checks |
|---|---|
| Upload fails | Correct board/port, data-capable USB cable, closed monitor, actual bridge driver, boot mode |
| IMU not detected | MPU6050 identity 0x68 vs I2C address, rails/pullups, SDA/SCL, AD0; MPU6500 deliberately rejected |
| Flex noisy/reversed | Common ground, measured divider/endpoints, orientation, wiring, ADC range and mechanical strain |
| Pot noisy/unsafe | Dedicated GPIO36 wiring, continuity, non-rail travel, stable l/h captures, calibrated range and wiper fault bias |
| ESP-NOW offline | Both MACs/channel/keys/version, provisioning, source filtering, age/ACK counters |
| Motor direction wrong | Props off; physical map, ESC phase order, spin and correction signs; never fix by guessing GPIO order |
| ESC will not arm | Actual manual/endpoints/logic, common ground and output gates; no automatic full-throttle calibration |
| IMU drift | Stationary calibration, warmup, mount/vibration, acceleration rejection; yaw drift has no absolute correction |
| PID oscillation | Stop test; review signs, saturation, filter delay, measured timing, actuator bandwidth and gains |
| Serial disconnect | Exclusive port ownership, cable/power, matching baud and protocol, boot/session reset |
| 3D blank/incorrect | App not implemented yet; later verify OpenGL driver/context and tested frame/quaternion transform |

[Detailed baseline troubleshooting](docs/troubleshooting.md).

## Structure, tests and evidence

Existing layout is retained: firmware/{common,glove,drone,motor_test}, tests/{unit,
integration}, simulation/tests, tools, docs, hardware and diagrams. The separate
planned ground_station/{app,dashboard,visualization,telemetry} will reuse the
versioned wire contract, not flight-loop code. [Final directory plan](docs/master-plan.md).

[Historical executed validation](docs/validation-results.md) belongs to its exact
MPU6500 implementation SHA and CI run, not the new target. Every phase must record
files/commit, build commands/results, procedures, actual measurements, failures
and remaining work. Distinguish compilation, host tests, hardware bench tests,
integrated tests and flight. Simulation is an assumed plant, not F450 evidence.

[This design-stage execution report](docs/design-review-results.md) records current
checks and publication. [Current sensor milestone evidence](docs/sensor-migration-results.md)
covers the migration. Separate physical IMU tests on both boards gate further
flight-control integration; telemetry, desktop and flight readiness remain later work.
Use a development branch and subsystem commits; do not overwrite main. No flight
release is implied by a successful build or push.

## Safety and license

Keep props removed around firmware development and bench electronics. Use a
controlled area, guards and appropriate supervision for later powered tests.
Balance-charge LiPo packs with suitable equipment, inspect damaged/swollen packs,
prevent shorts and follow the pack maker's handling/storage guidance. Verify
loaded voltage/current/thermal margins; labels do not prove power feasibility.

Hardware inspection, calibration, motor/prop direction, PWM/failsafe behavior,
loop latency, controller signs, measured tuning and supervised validation all
remain required before flight. Laptop availability is never a flight safety dependency.

[Known limitations](docs/known-limitations.md) · [Engineering report](docs/engineering-report.md) ·
[MIT license](LICENSE). Third-party adoption must preserve its own license and
attribution; the research comparison is not a claim of upstream certification.
