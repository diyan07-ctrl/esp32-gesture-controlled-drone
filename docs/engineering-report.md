# Engineering delivery report

Prepared 2026-10-30 in the Gesture_Controlled_Drone workspace.

## Repository and publication

Repository: [diyan07-ctrl/esp32-gesture-controlled-drone](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone), public. GitHub CLI now authenticates as diyan07-ctrl with repository/workflow access. The repository was created successfully after confirming the name was absent. Earlier sandbox restrictions are historical: an exact-path Git trust override now permits staging without changing global Git settings. See the publication record for push and CI verification.

The [publication record](publication.md) tracks authenticated ownership, remote visibility, uploaded files and validation. No flight-ready release is created.

## Hardware verdict and modifications

**Cannot verify the full aircraft with available information.** ESP32 remains the sole onboard controller. The 1000 KV rating is owner-reported, not a motor manufacturer identification. ESCs, battery C, PCB variants, breakout supplies, exact props, mass and thrust/load curves remain unknown. Add or verify rated distribution, dedicated electronics regulation, connectors/conductors, ADC input protection, independent glove throttle/yaw, explicit enable/arm/run-permit switches, glove power, passives and mounting. Final selection must close the [audit](hardware-feasibility-audit.md).

Nominal battery energy is 24.42 Wh; continuous capability is 2.2×C A with C unknown. Flight time is not measured. [Power budget](power-budget.md) gives conditional scenarios, conductor losses and regulator allowances; [battery analysis](battery-analysis.md) gives the full-charge divider design. Candidate supplier costs/stock are in the [Robu.in/FlyRobo snapshot](supplier-availability.md), separate from approval.

## Delivered engineering content

- [Glove/drone wiring, ESC power/signals/BEC treatment and power paths](connection-tables.md), [GPIO CSV](../hardware/pin-mapping.csv), [motor mapping CSV](../hardware/motor-mapping.csv), [electrical design](electrical-design.md), [BOM](bill-of-materials.md).
- [Six editable diagrams and motor SVG](../diagrams/README.md): system, glove, drone, motor layout, firmware and power.
- Independent glove/drone firmware with MPU register driver, bias calibration, complementary attitude estimation, relative tilt mapping, physical throttle/yaw, encrypted unicast transport, explicit protocol codec, PID/mixer, inhibited candidate LEDC outputs, state supervisor and telemetry.
- Shared C++ unit cases, Python utility tests, Node static checks, real-build CI definition, calculation/parser tools and a toy roll-plant numeric harness.
- [Build/upload](build-and-upload.md), [assembly](assembly-guide.md), [calibration](calibration-guide.md), [staged testing](testing-and-validation.md), [troubleshooting](troubleshooting.md), [safety](safety-and-failsafe.md) and [eight milestones](roadmap.md).

## Actual verification and limits

Both firmware applications compiled successfully in [GitHub Actions](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/runs/41005573875). The same run passed 1,537 shared C++ assertions, all 4 Python tests, repository static checks and the toy roll-model numeric harness. The first run failed framework-library discovery; project include/library detection was corrected and rebuilt. Earlier local toolchain restrictions are historical, not the current software-validation status. Sensor, RF, ESC, thermal, bench, HIL and flight tests remain unrun. Mermaid renderer/visual inspection remains pending. See [validation results](validation-results.md) for build versions, warnings and limits.

Four default output gates are false. Current failsafe is a latched bench motor-signal inhibit; it can cause uncontrolled descent airborne. Gains are unvalidated, yaw drifts, there is no altitude/position reference and simple clipping loses control authority. Firmware must not be treated as flight-ready.

## Remaining work

Close exact component identities and label data, procure verified mandatory parts, finalize protection/wiring/CAD, render/review diagrams, calibrate both sensors and controls, measure electrical/RF/timing performance, validate propulsion and controller response, implement a validated airborne loss policy, and complete supervised readiness/legal/site review. Keep software builds/tests passing as changes land. Publication is complete. The [issue backlog](open-issues.md) links the actual GitHub issues and defines closure evidence for remaining blockers.
