# Engineering execution record — 2026-11-03

## Audit and plan

Clean main at 6c7cf0b; existing origin https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone.git. History f323835 → e186721 → 6c7cf0b already contained experimental PlatformIO firmware and tests. No unrelated user work was present. No AGENTS.md was found in the project/parent audit. Retain structure and pinned platform; extend modules, correct hardware mismatch, then execute tests, review, document, commit and push without force.

| Phase | Files / completed work | Executed checks / failures / dependencies | Next |
|---|---|---|---|
| 1 Repository audit | Read Git state/history, apps/common modules, tests, docs, CI | Read-only audit; no test claim. Found MPU6050/potentiometer/motor pin mismatch | Architecture |
| 2 Architecture | Existing PlatformIO/FreeRTOS/encrypted ESP-NOW retained | Checked pinned Espressif APIs and MPU6500/HC-SR04 references; exact ESC/board circuits remain unknown | Shared definitions |
| 3 Protocol/config | config.h, packet.h: v2 flex/gesture/orientation, GPIOs and gates | Packet tests executed in phase 10; no physical verification | Glove |
| 4 Glove | flex.h, gesture.h, glove main/calibration.h; measured NVS endpoints, filtering, mapping/interlocks | Native gesture/flex tests and firmware build in phase 10; real calibration pending | Drone sensors |
| 5 Sensors | imu.h MPU6500 identity/readback/level offsets; range.h and ultrasonic.h | Native estimator/range failure tests; real I2C/ADC/RMT and offsets pending | Wireless |
| 6 Wireless | transport.h variable lengths, priority emergency mailbox, atomics; private provision tool | Mock protocol/integration tests pass; real RF not executed | Control |
| 7 Control | Configurable cascaded angle/rate gains; existing estimator/PID retained | PID/estimator native tests and assumed-plant simulation pass; real tuning pending | Mixer/ESC |
| 8 Mixer/ESC | Configurable confirmed numbering, desaturation, shared PWM conversion; separate motor_test image | Boundary/sign/saturation tests; builds recorded in validation results; actual ESC compatibility pending | Safety |
| 9 Safety | safety.h / drone integration: emergency latch, timing/sensor/link/optional battery supervision | State, corruption, timeout, recovery and output-gating host tests pass; physical stop behavior pending | Test execution |
| 10 Software tests | Added sensor_gesture_tests, pipeline_tests, closed_loop, host runner and expanded CI | Actual results and toolchain bootstrap warnings in validation-results; no hardware claims | Hardware procedure |
| 11 Hardware procedure | testing-and-validation.md, wiring, calibration and ESC gate docs | All ten ordered hardware stages NOT EXECUTED; hardware/instruments unavailable to this session | Documentation/Git |
| 12 Documentation/Git | Updated README, current docs/diagrams/pin/motor maps, validation report and CI | Implementation 812d5b6 pushed to main; CI run 41620143407 passed all four jobs; local native/build/static checks passed; serial inventory found no ports | Physical validation |

Owner clarified HC-SR04, GPIO25/26/32/33, CCW top-view motor numbering from M1 front-right, M1/M3 CCW and M2/M4 CW. Roll/yaw mixer signs and independent expected-sign tests were revised accordingly. Actual HC-SR04 pins/level shifting and ESC/power details remain unverified; no fabricated calibration values or flight-ready gains were supplied.
