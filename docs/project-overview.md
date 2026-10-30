# Gesture-Controlled Drone Using ESP32

The project explores a student-built F450 Quad-X aircraft with an ESP32-WROOM-32 as its sole onboard flight-control computer. A second ESP32 and MPU6050 form the wearable pilot interface. ESP-NOW carries pilot commands; the drone independently estimates attitude and stabilizes the aircraft.

The motivation is to make sensing, control, communication and power distribution inspectable. No dedicated flight controller or fallback flight-controller board is included.

Inventory: F450 frame, two ESP32 DevKits, two MPU6050 modules, four A2212 motors (1000 KV reported by owner), four nominal 30 A ESCs and a 3S 2200 mAh LiPo. Exact product identities and propulsion performance remain unknown.

This is a software and provisional engineering baseline. Compilation, hardware measurements, tuning and flight approval are separate gates. See [audit](hardware-feasibility-audit.md), [validation](testing-and-validation.md) and [roadmap](roadmap.md).
