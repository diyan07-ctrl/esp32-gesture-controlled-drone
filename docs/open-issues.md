# Engineering issue backlog

Open GitHub issues: [hardware and power verification #1](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/1), [airborne failsafe #2](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/2), and [physical timing/sensor/controller validation #3](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/issues/3). The table retains detailed acceptance criteria and completed software/publication items for traceability.

| Priority | Issue | Required evidence to close |
|---|---|---|
| P0 | Identify ESCs and all physical connector pins | Exact manual, signal level/rate/endpoints, BEC design, no-signal/boot scope traces |
| P0 | Close propulsion and battery feasibility | Motor maker/curve, selected balanced props, pack continuous C/condition, all-up mass, guarded thrust/current/thermal tests |
| P0 | Finalize power wiring and ADC input protection | Board schematics, regulator ratings/load tests, derated conductors/connectors, unpowered ADC back-feed protection, updated CAD schematic |
| P0 | Design and validate airborne link/battery failsafe | Explicit operating envelope, suitable sensors/model, hazard review and controlled evidence; bench cutoff is insufficient |
| Done | Obtain real compilation and execute logic suite | Both PlatformIO builds, 1,537 shared C++ assertions, 4 Python tests and toy numeric model passed in CI; maintain passing status |
| P1 | Validate timing and actuator bandwidth | IMU sampling/loop jitter traces with RF/log load, missed-deadline injection, ESC latency and stability margin |
| P1 | Sensor calibration and controller characterization | Six-face correction, temperature/vibration/noise records, rate-loop then angle-loop measured response with gain revision |
| P1 | Harden pilot analog controls and replay freshness | Pot fault detection, receiver boot challenge, fuzzing and failure/reconnect tests |
| Done | Complete authenticated public publication | diyan07-ctrl authenticated; public repository created; all source paths/hashes and anonymous repository/README access verified |

Use the linked issues to record hardware evidence and actual failed stages. Do not mark physical acceptance complete from CI results or create activity without a concrete engineering purpose.
