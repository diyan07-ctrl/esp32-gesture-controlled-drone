# Design review execution record — 2026-11-05

## Scope and files

Immediate first task from the master prompt: inspect the repository and prepare
architecture, hardware audit, upstream comparison, algorithm/desktop recommendations,
directory plan and complete README draft for review before implementation.

Changed README.md; added master-plan.md, hardware-audit-2026-11-05.md,
open-source-comparison.md, ground-station-design.md and this report under docs.
No firmware, motor_test, private provisioning, output gate or existing test was
changed. Existing main and its prior evidence remain preserved in Git history.

Inspected local clean main `14879d3` and implementation `812d5b6`; fetched origin
successfully. Created `development/f450-ground-station` after sandbox approval for
Git ref writes. Owner explicitly confirmed both MPU6050s plus pot throttle.
No AGENTS.md was found by the repository inventory.

## Findings and limitations

Current driver rejects MPU6050; target migration is pending, not compiled as a
new profile. Existing cfg::throttle=34 would collide with thumb flex if reused.
Proposed pot GPIO36 needs physical confirmation and fault conditioning. Current
5 Hz diagnostic output is not the planned framed 50 Hz telemetry. The desktop
application does not exist. Exact ESC/regulator/breakout/analog components remain
unidentified. A verified electrical schematic and airborne failsafe cannot be
claimed. Source-level inspection does not establish physical pinouts or timing.

Primary upstream source/docs/licenses were inspected through web retrieval. No
upstream code was copied, vendored or built. Some vendor documentation URLs failed
to resolve; this review does not claim a completed MPU6050 register implementation.
Future vendor imports require pinned revisions and file-level attribution.

## Verification

Executed `node tools/check_repository.mjs`: PASS, 513 static assertions across
95 files, including local links and safe output defaults. Executed `git diff
--check`: PASS (Git reported only CRLF-to-LF conversion notices for README).

Attempted all three baseline PlatformIO targets and the native test runner through
`.venv/Scripts/python.exe`: both commands exited 1 before compilation. The virtual
environment references a missing Python311 installation at its recorded base
path. These are local environment failures, not passing builds or compiler failures.
No attempt was made to substitute historical results for this run. The existing
CI workflow can independently rebuild the branch after publication.

Checks exercise the unchanged MPU6500 source and new documentation only; even a
successful baseline build cannot validate MPU6050/pot hardware or desktop design.

## Publication and exact-commit CI

Pushed design commit `8cb5d4fce039a122db434bc5255ad15ddfcf2363` to
`development/f450-ground-station`, with upstream tracking and no main change.
[CI run 41982334382](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/runs/41982334382)
completed successfully on that SHA. All four jobs passed: glove, drone,
motor_test and logic. Inspected build logs contained no `warning:` or `error:`
diagnostics. The logic log reported:

| Check | Observed result |
|---|---|
| Repository | 513 static assertions PASS |
| Core | 1,553 assertions PASS |
| Sensor/gesture/PWM | 49 assertions PASS |
| MPU6500 driver/calibration, mocked I2C | 3,013 assertions PASS |
| Calibration storage, mocked NVS/serial | 17 assertions PASS |
| Command/safety/PWM mocked integration | 501 assertions PASS |
| Closed-loop assumed plant | 62,727 steps/saturation cases PASS |
| Python tools | 4 tests, OK |

| Firmware baseline | Static RAM bytes | Flash bytes | Result |
|---|---:|---:|---|
| glove | 44,320 / 327,680 | 768,845 / 1,310,720 | PASS |
| drone | 43,872 / 327,680 | 760,301 / 1,310,720 | PASS |
| motor_test | 21,560 / 327,680 | 271,793 / 1,310,720 | PASS |

This subsequent report-only update records the observed evidence. The exact SHA
above identifies the checked source; these results do not cover new MPU6050/pot
firmware, real hardware or a desktop app. GitHub CLI initially could not read its
configuration under the sandbox; the read was retried with approved access.

Hardware bench/integration, sensor readings, RF pairing, PWM measurements, sensor-
to-actuator latency, desktop launch/rendering, telemetry latency, tuning and flight:
**NOT EXECUTED**. No board was opened or firmware uploaded during this design stage.

## Next checkpoint

Review [architecture](master-plan.md), [hardware audit](hardware-audit-2026-11-05.md),
[research](open-source-comparison.md), [desktop design](ground-station-design.md)
and [README](../README.md). Then implement the explicitly confirmed MPU6050/pot
target, compile each image independently, and proceed to the separate desktop
subsystem. This is the review sequence explicitly requested by the master prompt,
not a claim that the complete ecosystem has been delivered.
