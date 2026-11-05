# F450 software-in-the-loop laboratory

This laptop-only simulation executes the same C++ `Imu`, `Pilot`, `Estimator`, `Receiver`, `Safety`, `Stabilizer`, `Pid`, mixer and PWM calculation used by the firmware. A narrow C ABI supplies synthetic MPU6050 registers, ADC values, encoded ESP-NOW packets and deterministic time. No ESC, serial or radio output code is linked into the native library. Firmware output gates remain false. A separate **virtual-only** permit lets the supervisor exercise arming and numerical PWM without physical actuation.

Python supplies an **assumed**, configurable six-degree-of-freedom plant, motor lag, specific-force IMUs, seeded RF impairments, automated scenarios and reports. Qt displays actual simulation snapshots. These results reduce software risk and provide no hardware or flight validation.

## Install on Windows PowerShell

Use Python 3.11 or 3.12, Node.js 22 and Zig 0.13.0. Check `py -0p` or `python --version`; substitute your installed Python if `py -3.11` is unavailable. From the repository root:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r simulation/requirements.txt
.\.venv\Scripts\python.exe -m pip install ziglang==0.13.0 platformio==6.1.18
node --version
```

Qt needs a working OpenGL driver. For numerical tests without a GUI, install `simulation/requirements-core.txt` instead. PlatformIO is needed for the five ESP32 build targets.

## Run and reproduce

```powershell
.\.venv\Scripts\python.exe -m simulation.build_native
.\.venv\Scripts\python.exe -m unittest simulation.test_sil -v
.\.venv\Scripts\python.exe -m simulation.validate --output build/sil/report --firmware
.\.venv\Scripts\python.exe -m simulation.validate --scenario roll_step --output build/sil/roll-step
.\.venv\Scripts\python.exe -m simulation.dashboard --scenario hover
```

The validation command rebuilds the isolated native library, executes the existing host suite and repository checks, runs SIL regressions, 24 filter experiments, 18 scenario runs and six sensitivity runs, writes plots/traces/report, then compiles all five firmware and diagnostic targets. It returns nonzero on an unexpected acceptance failure. Named **LIMITATION** entries remain visible in JSON and Markdown. Use `--config path.json` for an edited copy of the model, `--scenario name` to select a scenario and `--baseline path/results.json` for before/after evidence. Output under `build/` is ignored by Git. `--skip-host` supports CI where the original host job runs separately.

`simulation/config/default.json` identifies every **simulation-only** physical, sensor and RF coefficient. Its analog endpoints are synthetic regression fixtures, not measured calibrations or firmware defaults. Replace the JSON values with measured curves, mass, inertia, lag, ADC calibration and RF timing only when measurements exist. Embedded PID gains remain unchanged.

## Dashboard

Start, pause/resume, reset and emergency simulation stop control a worker thread, not an aircraft. A one-frame GUI queue prevents rendering from slowing numerical steps. Choose a scenario and 1×, 5× or 20× speed. The model uses FRD body and NED world axes; cyan marks the front. Mouse orbit/zoom and Reset camera control the view. Position/altitude/orientation come from the physics state; the IMU panel separately displays truth, raw tilt and native complementary-filter estimates. Each motor shows normalized command, virtual 50 Hz ESC PWM, simulated RPM and estimated `kTω²` thrust. None is measured hardware thrust.

Check **Manual after scripted arm** for virtual hand-roll, hand-pitch, pot, thumb and index sliders. The deterministic 0–6.8 s calibration/low-throttle sequence precedes manual control. A support fixture holds the simulated vehicle until 9.5 s; this is not autonomous takeoff. No altitude/position hold exists, so open-loop hover can drift into the modeled ground.

Run selected/all tests launches an isolated report process with the fingerprint-verified native library. Export test report copies its output folder; Save configuration writes a JSON copy. Capture the **actual rendered** Qt/OpenGL dashboard:

```powershell
.\.venv\Scripts\python.exe -m simulation.dashboard --scenario roll_step --speed 5 --capture build/sil/dashboard.png
.\.venv\Scripts\python.exe -m simulation.dashboard --scenario wireless_loss --speed 5 --capture-time 13 --capture build/sil/fault-dashboard.png
```

## Model, scenarios and interpretation

- World north/east/down and body forward/right/down use SI units and quaternion propagation. Documented FR/FL/RL/RR arm/spin mapping is **not yet physically verified**. Motor thrust `T=kTω²`, reaction torque `Q=kQω²`, `Iω̇=τ−ω×Iω−Dω`, gravity/drag, finite motor lag and 50 Hz held PWM drive the plant.
- MPU simulation includes specific force from translation, angular rate, seeded noise/bias/drift, finite bandwidth approximation, ADC quantization and injected invalid samples. It feeds actual C++ MPU6050 scaling/calibration/filter code. The bandwidth approximation is not a measured MPU transfer function.
- Filter sweeps tau 0.25/0.5/1.0 s across stationary, gradual/sudden rotation, noise, gyro drift, translation, invalid data and timing jitter. Graphs compare truth, raw accelerometer tilt, integrated gyro and native filter. Yaw lacks absolute correction. Null timing metrics mean not observed or not applicable.
- Full-chain scenarios exercise glove, packet transport, safety, estimation, controller, mixer, virtual ESC and rigid body. The roll/pitch/combined/saturation *component* steps use encoded setpoints and gravity-only tilt sensing to isolate control dynamics. They do not establish translating-aircraft hover stability.
- The support fixture and assumed thrust coefficient make results conditional on the JSON. Six sensitivity runs vary mass, thrust coefficient and motor lag. Extreme command altitude excursions and motor saturation remain visible.

See the [versioned reference report](reports/reference/REPORT.md). Full traces and plots can be regenerated with `simulation.validate`; the reference omits bulky trace JSON files. Historical `simulation/tests/` host C++ model checks remain in the original host suite.

## Findings and gates

The baseline revealed a one-cycle pitch-envelope validity defect: the filter could return valid after a gyro update crossed 60°. Candidate state is now rejected before committing, with a C++ and SIL regression. A separate report fix distinguishes unique packet delivery from duplicate arrivals. See [change log](CHANGELOG.md). No PID gain was changed; the motor-test source and disabled hardware output gates remain intact.

Visible limits: translation can look like tilt to a six-axis accelerometer filter; full-chain tilt commands recover slowly under the assumed plant; command v2 has no sender-generation age, so a previously unseen delayed packet can look fresh; a plausible stuck pot cannot be distinguished from a held knob; no altitude controller exists. Current link/IMU fault inhibition would not be a validated airborne loss strategy. Host C++ timing is not ESP32 deadline or actuator latency. Physical IMU, ADC, ESC/thrust, battery, RF, deadlines and failsafe behavior still need propeller-free hardware validation before flight integration.

## Platform selection and provenance

| Option | Decision | License/dependency assessment |
|---|---|---|
| NumPy plus native C++ | Selected: deterministic fixed-step physics directly tests firmware classes | NumPy BSD-3-Clause; small laptop install |
| SciPy | Deferred: useful for later parameter identification, unnecessary here | BSD-3-Clause |
| PyBullet | Deferred: contacts/rigid body are useful but still require custom thrust/firmware adapters | zlib license |
| Webots | Deferred optional 3D world | Apache-2.0; separate desktop install |
| Gazebo | Deferred optional multirotor world | Apache-2.0; substantial plugin/setup work |
| PX4/ArduPilot SITL | Methodology and fault-injection references; no upstream flight stack code imported | Architecture reference only |

Sources: [PX4 SITL](https://docs.px4.io/main/en/simulation/), [ArduPilot SITL](https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html), [NumPy](https://github.com/numpy/numpy), [SciPy](https://github.com/scipy/scipy), [Bullet](https://github.com/bulletphysics/bullet3), [Webots](https://github.com/cyberbotics/webots), [Gazebo](https://github.com/gazebosim/gz-sim). Optional UI dependencies PySide6, pyqtgraph, PyOpenGL and Matplotlib remain installed separately, not vendored.
