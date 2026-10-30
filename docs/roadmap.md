# Engineering roadmap

| Milestone | Deliverables and exit evidence | Status |
|---|---|---|
| 1 Hardware verification | Exact motor/ESC/board/breakout IDs, battery C, mass, prop curve, PDB/regulator ratings | Blocked by physical data; 1000 KV owner report recorded |
| 2 Documentation | Architecture, provisional wiring, power budget, six diagrams, BOM; final revision after IDs | Provisional baseline |
| 3 Glove | MPU calibration, tilt mapping, independent controls, encrypted ESP-NOW and acknowledgment diagnostics | Implemented; compilation/hardware validation tracked separately |
| 4 Drone foundation | IMU, calibration, estimator, periodic task and timing counters | Implemented; sensor/timing measurements pending |
| 5 Control and outputs | PID, Quad-X mixer, peripheral PWM candidate; scope and plant evidence | Implemented; output gates closed |
| 6 Safety integration | Interlocks, session handling, timeout, battery, watchdog, fault injection | Logic implemented; airborne failsafe unresolved |
| 7 Validation | Host tests, both builds, RF, prop-off electrical/thermal tests and bounded simulation | See validation results; hardware not tested |
| 8 Flight-readiness review | Verified complete build, rotation/props, flight failsafe, legal/site review and supervised controlled plan | Not authorized by software completion |

Milestones are evidence gates, not dates or automatic approvals. Each failed test blocks dependent stages. No release is labelled flight-ready.
