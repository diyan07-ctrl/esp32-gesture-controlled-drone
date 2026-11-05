# SIL validation changes

| Cycle | Evidence | Change | Regression / residual |
|---|---|---|---|
| Initial baseline | Revision `673eaf6318e6270986582f455044876d98ce4f4d`: 7,049 C++ unit assertions, 501 mocked integration assertions, 4 Python tests, 553 repository checks and five PlatformIO builds passed locally | None | Historical baseline rerun before edits; no hardware evidence |
| Native extraction | Shared Pilot and Stabilizer were isolated from ESP32 adapters | Thin firmware adapters and native bridge call the same classes; gains, gates and motor-test source unchanged | 10,007 orchestration assertions check equation parity and pre-arm safety |
| Before SIL | `build/sil/before/results.json`: 18 scenarios, 24 filter trials, six sensitivity runs, one unexpected estimator-envelope failure | Investigated actual C++ estimator state | 59.9° plus valid gyro rate crossed 60° and returned valid for one cycle |
| Firmware fix | Estimator boundary test failed before change | Reject candidate attitude before committing it | Unit/SIL boundary and recovery tests pass; no unexpected failures in final run |
| Report audit | Duplicate arrivals made `delivered/sent` exceed one | Report unique delivery `(sent-dropped)/sent` separately from arriving copies | Seeded network regression checks both; protocol and simulated RF unchanged |

The report retains the original four limitations plus two full-chain tilt recovery limitations found by the stronger directional audit. The simulation has not justified a PID gain change or established airborne safety.
