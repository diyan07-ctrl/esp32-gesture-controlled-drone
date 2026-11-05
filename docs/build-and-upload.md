# Reproducible build and upload

Python 3.11+ and PlatformIO Core 6.1.18 were used locally on Windows. CI uses Python 3.12 on Ubuntu. Root platformio.ini retains the existing espressif32@6.9.0 platform, esp32dev 4 MB board and Arduino ESP32 2.0.17 / IDF 4.4.7 APIs. Required Wire, WiFi and Preferences ship with the framework. No external IMU/PID dependency exists.

~~~sh
python -m venv .venv
# Activate the virtual environment using your shell's normal command.
python -m pip install platformio==6.1.18
python -m platformio run -e glove -e drone -e motor_test
python -m platformio run -e glove_sensors -e drone_sensors
python -m platformio pkg list -e drone
~~~

The generic esp32dev board describes classic ESP32, 240 MHz, 4 MB flash; verify exact DevKit V1 pinout/regulator/USB circuit. No architecture migration is needed. Do not upgrade to Arduino 3.x without adapting LEDC, ESP-NOW callbacks and watchdog APIs and retesting.

Start with independent glove_sensors/drone_sensors images; neither initializes
ESCs or radio. [Sensor acceptance](sensor-migration.md) and
[current build evidence](sensor-migration-results.md). Both physical IMU tests
must pass before further flight-control integration.

Default pairing and motor outputs are disabled. [Provision private peers](communication-protocol.md), then rebuild both endpoints. Never commit local_secrets.h or publish provisioned firmware binaries.

Props removed, propulsion disconnected, correct target/port identified:

~~~sh
pio run -e drone -t upload --upload-port COM9
pio device monitor -p COM9 -b 115200
pio run -e glove -t upload --upload-port YOUR_GLOVE_PORT
~~~

COM9/4 MB were owner-reported prior detection; no upload or current port/hardware detection occurred in this session. Identify the glove port separately. USB/external power isolation must match the actual board circuit.

Native GCC C++17 tests, Python and Node:

~~~sh
python tools/run_host_tests.py
node tools/check_repository.mjs
~~~

Windows alternative used here:

~~~sh
python -m pip install ziglang==0.13.0
python tools/run_host_tests.py --zig .venv/Lib/site-packages/ziglang/zig.exe
~~~

The runner compiles with warnings-as-errors for project C++, runs all unit/integration/simulation executables and Python utility tests. Initial Zig runtime bootstrap emitted libunwind dllexport warnings from its own bundled sources; these are separate from application compile diagnostics. Current actual build/test sizes and results appear in [validation-results](validation-results.md); runtime stack/heap, RF latency and jitter are unmeasured.
