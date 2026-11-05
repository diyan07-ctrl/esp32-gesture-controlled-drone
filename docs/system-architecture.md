# System architecture

The retained PlatformIO project now reads MPU6050, two flex inputs (34/35) and a
dedicated pot input (36) at a target 4 ms period. Flex thresholds have hysteresis;
the pot alone sets throttle, with calibrated endpoints, fault checks and bounded
rises. The glove applies permissions and sends unchanged v2 commands at 50 Hz.
NVS stores all analog endpoints only by explicit save while locally disarmed and
the last receiver state is not arming/armed. Flash writes are outside drone control.
Independent glove_sensors/drone_sensors images acquire/calibrate sensors without
initializing radio or ESCs. [Milestone contract](sensor-migration.md).

firmware/drone/src/main.cpp creates a priority-4 task pinned to core 1. At target 250 Hz it reads its own IMU, estimates orientation, consumes at most one packet, evaluates safety, runs cascaded control, mixes and updates LEDC. FreeRTOS absolute scheduling, interval bounds and execution-time checks detect timing failure. No serial formatting, echo wait or dynamic allocation occurs in that task's steady-state control path. Initialization allocates bounded queues/tasks. A one-second task watchdog is an additional reset mechanism, not an independent motor watchdog.

Radio callbacks run in the Wi-Fi task: bounded copies to separate normal/emergency queues transfer ownership. Atomic send flags/counters avoid callback/application races. A latest-snapshot queue transfers drone diagnostics to Arduino loop. HC-SR04 runs only in the lower-priority loop when verified: RMT generates the 12 µs trigger; an edge ISR records echo under a spinlock; poll processes widths, 30 ms timeout and 65 ms trigger interval. Range has no control authority.

Shared headers separate configuration, packet encoding, IMU I2C adapter, flex normalization, gesture logic, estimator/PID/mixer, PWM/ESC adapter, range filtering and safety state. Host tests compile the hardware-independent logic. They do not execute ESP-NOW RF, FreeRTOS timing, LEDC pins or actual I2C/RMT/ADC drivers. A separate motor_test image offers explicitly commanded, propeller-off, low-output fixture tests; it is never a flight controller.

See [protocol](communication-protocol.md), [control](flight-control-algorithm.md), [wiring](connection-tables.md), and [safety](safety-and-failsafe.md). Physical gates remain false until evidence is recorded.
