# Reproducible build and upload

Install Python 3.12 and PlatformIO Core from its official package. The root project holds two independent targets; no duplicated nested INI files are needed:
```sh
python -m pip install platformio==6.1.18
pio run -e glove
pio run -e drone
```

Pinned platform: espressif32 6.9.0; board: esp32dev (generic classic ESP32 Dev Module, provisional until PCB identification); framework Arduino ESP32 2.0.17 / IDF 4.4.7 from that platform. No third-party IMU/fusion/PID packages. See [PlatformIO release](https://github.com/platformio/platform-espressif32/releases/tag/v6.9.0) and [Arduino release](https://github.com/espressif/arduino-esp32/releases/tag/2.0.17). Do not upgrade Arduino API versions without testing callback, watchdog and LEDC changes. Platform pinning constrains dependencies; preserve actual package list with build records.

Keep props off and ESC power disconnected. Confirm target board and USB port, then:
```sh
pio run -e glove -t upload --upload-port COM5
pio run -e drone -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
pio pkg list -e drone
```
COM5/COM6 are examples, not detected devices. On Linux use actual /dev paths. Do not upload to an unidentified connected device. Arduino serial auto-reset behavior and external supply isolation depend on board variant.

Default pairing is disabled. Provide the ignored local header as described in [protocol](communication-protocol.md) for encrypted link tests. All output verification flags remain false until [gate](output-enablement.md) is satisfied.

Host tests (GCC C++17, Node22+, Python3.12):
```sh
mkdir build
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -pedantic -I firmware/common tests/unit/core_tests.cpp -o build/core_tests
./build/core_tests
python -m unittest discover -s tests/unit -p test_tools.py -v
node tools/check_repository.mjs
g++ -std=c++17 -I firmware/common simulation/tests/roll_model.cpp -o build/roll_model
./build/roll_model
```
On Windows GCC creates .exe; use build/core_tests.exe. No compiler was found in this session. GitHub Actions actually runs both builds and tests if publication and Actions are available. A written workflow has no result until executed.
