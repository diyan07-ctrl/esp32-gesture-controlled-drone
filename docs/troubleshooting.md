# Troubleshooting

| Symptom | Check | Stop condition |
|---|---|---|
| IMU init fault | 0x68 address, 3.3 V supply/pullups, AD0, WHO_AM_I, config readback | Never bypass ID or voltage checks |
| Calibration never completes | Stable level support, vibration, 1 g norm, temperature, gyro offsets | Timeout needs inspection/reset |
| No ESP-NOW packets | Private provisioning header, actual station MACs, same channel1, key pairing | No broadcast fallback |
| Status LED off | Sensor health, zero-throttle/arm-release interlocks, fresh status | Do not force enable |
| Arming always blocked | Intentional default verification gates | Hardware and controller evidence required |
| Timing fault | I2C timeout/noise, CPU/flash/log load, measured wake jitter | Do not raise limits to hide failures |
| Voltage implausible | Divider polarity/value, ADC1 pin, DMM scale, common ground | Disconnect pack before wiring changes |
| Motors do not respond | Default no-PWM behavior; exact ESC manual; scope | Never test by fitting props |
| Wrong correction sign | Sensor transform, FRD axes, mixer order | Stop before powered propulsion |
| Frequent failsafe | Packet age, callback queue gaps, interference, permit wiring | Reconnection cannot clear latch |

Diagnostic state integers follow safety.h. Save revision, build logs and CSV telemetry. “It works after reset” is not a resolved failure.
