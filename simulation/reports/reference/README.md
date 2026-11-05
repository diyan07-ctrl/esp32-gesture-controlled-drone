# Executed SIL reference evidence

This folder contains laptop-generated **simulation evidence**, not hardware or flight measurements. [REPORT.md](REPORT.md) has the numerical tables and findings. [results.json](results.json) retains full metrics, the exact model configuration, source fingerprints, named checks and before/after comparison. [before-results.json](before-results.json) is the pre-fix report with one unexpected estimator-envelope failure. [automated-tests.json](automated-tests.json) records the 21 Python SIL unit tests. `plots/` has all 27 generated filter, ADC and scenario graphs. Full trace JSON files remain in ignored `build/sil/report/` because they are bulky; regenerate them using the command below.

Both [active dashboard](dashboard.png) and [wireless-fault dashboard](fault-dashboard.png) are screenshots of the running Qt application on Windows, captured with `QWidget.grab` and checked for nonempty plots and a valid OpenGL context. The corresponding [3D viewport](dashboard-3d.png) and [fault viewport](fault-dashboard-3d.png) were taken with `grabFramebuffer`. They are *simulated* states, not synthetic image mockups or hardware telemetry.

```powershell
.\.venv\Scripts\python.exe -m simulation.validate --output build/sil/report --baseline simulation/reports/reference/before-results.json --firmware
```

The checked-in reference was generated while implementation files were uncommitted, so `working_tree_dirty=true` and `revision` identifies the parent commit. The native/harness SHA-256 fingerprints in `results.json` identify the exact tested sources. CI reruns validation at the published commit. The initial baseline was run at the parent revision before edits. Software results are qualified by the six visible engineering limitations; none establishes physical F450 flight readiness.
