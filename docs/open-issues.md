# Engineering issue backlog

These are local issue specifications; no GitHub issues have been created.

| Priority | Issue | Required evidence to close |
|---|---|---|
| P0 | Identify ESCs and all physical connector pins | Exact manual, signal level/rate/endpoints, BEC design, no-signal/boot scope traces |
| P0 | Close propulsion and battery feasibility | Motor maker/curve, selected balanced props, pack continuous C/condition, all-up mass, guarded thrust/current/thermal tests |
| P0 | Finalize power wiring and ADC input protection | Board schematics, regulator ratings/load tests, derated conductors/connectors, unpowered ADC back-feed protection, updated CAD schematic |
| P0 | Design and validate airborne link/battery failsafe | Explicit operating envelope, suitable sensors/model, hazard review and controlled evidence; bench cutoff is insufficient |
| P0 | Obtain real compilation and execute logic suite | Both PlatformIO logs, host C++/Python results and CI checks; resolve every compile failure |
| P1 | Validate timing and actuator bandwidth | IMU sampling/loop jitter traces with RF/log load, missed-deadline injection, ESC latency and stability margin |
| P1 | Sensor calibration and controller characterization | Six-face correction, temperature/vibration/noise records, rate-loop then angle-loop measured response with gain revision |
| P1 | Harden pilot analog controls and replay freshness | Pot fault detection, receiver boot challenge, fuzzing and failure/reconnect tests |
| P1 | Complete authenticated public publication | diyan07-ctrl login verified, inspect name/content, authorized creation, push and public-file verification |

Create corresponding GitHub issues only after a working connection exists. Include references to the relevant documents and actual failed stage rather than artificial activity or invented results.
