# Changelog

## Unreleased — 2026-10-30

- Added evidence-based hardware audit with owner-reported 1000 KV and unresolved ESC/pack/PCB identities.
- Added provisional power design, complete connection tables, BOM and dated Robu.in/FlyRobo sourcing snapshot.
- Added six Mermaid sources and standalone Quad-X motor SVG.
- Added independent glove/drone applications, shared protocol, sensor estimator, PID/mixer, gated PWM, safety supervisor and diagnostics.
- Added host test suite, calculation/log tools, bounded toy model and actual-build CI workflow.
- Published all source files to the public diyan07-ctrl repository and verified anonymous access and remote source hashes.
- Fixed PlatformIO shared/framework library discovery; both firmware targets, 1,537 C++ assertions, 4 Python tests, static checks and toy simulation passed in GitHub Actions.
- Disambiguated the MPU I2C read overload and pinned workflow actions to verified current release commits. Created hardware/failsafe/physical-validation issues.
- Physical measurements and flight validation remain unexecuted. No public release or flight readiness is claimed.
