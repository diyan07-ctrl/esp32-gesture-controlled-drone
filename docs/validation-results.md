# Validation results — 2026-10-30

This record distinguishes performed checks from prepared tests. No hardware was physically tested.

| Check | Result | Evidence/limitation |
|---|---|---|
| Hardware feasibility review | Complete as provisional audit | Exact ESC, PCB variants, motor maker, prop data, pack C and mass absent |
| Earlier local toolchain preparation | Historically blocked | Initial sandbox denied Python download/local tool access; remote CI subsequently executed the real builds/tests |
| Glove firmware compile | PASS | CI run 41005573875 on e186721; static RAM 25,988 bytes, flash 312,137 bytes; these are build estimates, not runtime measurements |
| Drone firmware compile | PASS | Same CI run; static RAM 25,900 bytes, flash 314,905 bytes; runtime heap/stack/jitter still unmeasured |
| Shared C++ unit tests | PASS: 1,537 assertions | Actual compiled shared logic executed in CI |
| Python utility tests | PASS: 4 tests | Python 3.12.15 in CI; battery/divider/parser and invalid-input cases |
| Toy simulation | PASS: finite bounded numeric regression | Actual C++ roll model executed; final rate 3.61019 deg/s, angle 20.2055 deg under assumed plant. Not flight-stability evidence |
| Repository static checks | PASS: 416 assertions across 73 source files at publication | Actual Node execution; local Markdown links, motor mapping, GPIO/default gates, CI commands, BOM IDs, secret-pattern scan and basic Mermaid structure checked. Later runs report their current counts. Not exhaustive secret detection or visual rendering |
| Mermaid visual rendering | NOT RUN | Renderer unavailable; editable sources and motor SVG supplied |
| CI | PASS | Both firmware jobs and logic job succeeded in run 41005573875; see links below for current revision |
| Sensor/RF/ESC/bench/HIL/flight | NOT RUN | Physical hardware/instruments unavailable |
| GitHub publication | PASS | gh authenticated as diyan07-ctrl; repository created PUBLIC, source committed and pushed; initial 73 remote paths/blob hashes matched local |
| Anonymous public access | PASS | Repository and raw README both returned HTTP 200 without authorization headers |
| Engineering issue tracking | PASS | Issues #1 hardware/power, #2 airborne failsafe, #3 physical timing/sensor/controller evidence created |

Evidence: [successful run on e186721](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/runs/41005573875), [current workflow runs](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/workflows/firmware-ci.yml), [public source](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone).

Build environment: Ubuntu 24.04 hosted runner, PlatformIO Core 6.1.18, Espressif32 platform 6.9.0, framework-arduinoespressif32 3.20017.241212+sha.dcc1105b (Arduino 2.0.17), Xtensa toolchain 8.4.0+2021r2-patch5. Default pairing and output gates stayed disabled. This build therefore does not establish size or behavior of a provisioned, output-enabled aircraft.

The first CI run failed because framework Wire/WiFi libraries were not discovered through shared headers. A project include_dir and direct app-level framework includes fixed discovery; the subsequent run passed. That successful run reported an ambiguous Wire.requestFrom overload and legacy action-runtime deprecation warnings. A following change selects the exact uint8_t/size_t/bool overload and pins verified current action releases; inspect the corresponding current run for its observed results. No warnings were suppressed to conceal a compiler failure.

Initial connector/browser/filesystem restrictions were resolved for publication through the now-working authenticated CLI and process-scoped exact-path Git trust. Credentials were neither printed nor committed. No physical test or flight approval follows from this recovery.
