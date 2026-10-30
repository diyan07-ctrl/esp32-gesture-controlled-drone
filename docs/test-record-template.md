# Test record format

Create one Markdown record per actual test; do not prefill results.

| Field | Required content |
|---|---|
| Test ID and stage | Stage number plus unique test case |
| Date/operator | Actual session and responsible reviewer |
| Firmware | Git commit, target, local configuration hash excluding keys |
| Hardware | Board/ESC/motor/pack identifiers and wiring revision |
| Instruments | Model, range, calibration status and sample rate |
| Preconditions | Propellers removed or approved guarded rig, isolation, supply limits |
| Stimulus | Exact input, duration, injected fault and sequence |
| Expected | Numerical or state/timing criterion fixed before testing |
| Observed | Raw measurements, logs and traces |
| Verdict | Pass/fail/blocked; no unobserved passes |
| Follow-up | First failure, corrective change and retest scope |

Do not include MAC pairing secrets, precise site locations or personal data in public test logs.
