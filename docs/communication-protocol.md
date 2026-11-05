# ESP-NOW protocol v2

## Motor-inhibited PID shadow telemetry

The separate `drone_pid_bench` image sends `PS` version 1 datagrams over the
same Wi-Fi UDP destination as the BNO055 `drone_bench` image. It does not
consume ESP-NOW commands or send ESC signals. The fixed 194-byte little-endian
format is defined in `firmware/common/protocol/shadow_telemetry.h`; its
read-only laptop decoder is `ground_station/shadow_codec.py`. CRC16-CCITT-FALSE
covers bytes 0–191, followed by the CRC at 192–193.

| Offset | Content |
|---:|---|
| 0–7 | `PS`, version 1, mode 0 sensor/1 shadow/2 motor locked, length 194, reserved zero |
| 8–23 | Sequence, uptime ms, measured PID interval µs, IMU error counter |
| 24–131 | Roll, pitch, yaw axis records of nine float32 values each: setpoint, measurement, error, P, I, D, output, rate target, measured rate |
| 132–147 | Four **calculated** normalized motor values, never physical ESC output |
| 148–187 | Sensor-frame accel, gyro and BNO055 WXYZ quaternion |
| 188–191 | Calibration byte, sensor-valid byte, two reserved physical-output bytes fixed zero |
| 192–193 | CRC16 |

The decoder rejects nonzero physical-output bytes and reports ESC PWM and
actual motor operation unavailable. See [PID bench validation](pid-bench-validation.md).

**Current BNO055 drone bench:** the glove side and encrypted command v2 format
below are unchanged. The new `drone_bench` image initializes the receiver and
sends [read-only UDP telemetry](drone-bno055-bench.md). It never acknowledges an
ARMED state, has no ESC output, and waits for a separately provisioned peer.
Wi-Fi STA and ESP-NOW share one 2.4 GHz channel; the connected AP channel must
equal `cfg::radioChannel` before commands are accepted.

## Drone bench UDP telemetry v2

`firmware/common/protocol/telemetry.h` is the fixed 142-byte little-endian
wire format; `ground_station/telemetry_codec.py` strictly decodes it. CRC16
CCITT-FALSE covers bytes 0–139 and sits at 140–141. Packet magic `FT`, protocol
version 2, firmware format version 2 and declared length are checked. Status
flags distinguish IMU initialization, calibration, valid sample/estimate,
ESP-NOW initialization, paired peer, Wi-Fi connection and fresh command.
Physical-output-enabled is reserved and rejected by the bench decoder.

| Offset | Field |
|---:|---|
| 0–3 | `FT`, protocol version 2, firmware format 2 |
| 4–7 | Length 142, status flags |
| 8–19 | Telemetry sequence, uptime ms, state, channel, gesture, command flags |
| 20–67 | Twelve uint32 counters: sample interval, execution/max time, missed deadlines, IMU errors, accepted/rejected/lost commands, mailbox drops, send errors, command sequence and age |
| 68–79 | Throttle, signed roll/pitch/yaw commands, two flex ADC values; only meaningful with fresh link |
| 80–103 | Acceleration (g) and angular rate (degrees/s), sensor frame |
| 104–119 | Sensor-frame roll/pitch/yaw rate floats, Wi-Fi reconnect counter |
| 120–139 | Sensor-frame quaternion WXYZ; BNO055 calibration, system status and error; one reserved byte |
| 140–141 | CRC16 |

No PID, ESC PWM, motor command, battery voltage, absolute heading or measured
thrust is fabricated in this packet. Missing validity flags cause the Python
decoder to mark values unavailable. Sequence gaps count missing UDP datagrams
at the listener; local arrival rate is not synchronized one-way latency.

Single source: firmware/common/protocol/packet.h. Explicit little-endian integers, no native struct/float ABI. Fixed-peer encrypted unicast on configurable channel 1 by default; peers must share a channel and PMK/LMK. MAC allowlisting and CRC alone are not authentication. Protocol v1 is intentionally rejected.

| Command offset | Bytes | Meaning |
|---:|---:|---|
| 0 / 1 / 2 | 1 each | Magic 0x47 / version 2 / type 1 |
| 3 | 1 | Bit0 enabled, bit1 arm request, bit2 emergency stop; others rejected |
| 4 / 8 | 4 each | Nonzero random boot session / uint32 sequence |
| 12 | 2 | Throttle 0–1000 |
| 14 / 16 / 18 | 2 each | Signed roll/pitch/yaw demand, each -1000…1000 |
| 20 | 1 | Gesture enum 0…11: neutral, forward, backward, left, right, yaw-left, yaw-right, throttle-up, throttle-down, arm, disarm, emergency |
| 21 | 1 | Reserved zero |
| 22 / 24 | 2 each | Flex ADC counts 0…4095 |
| 26 / 28 / 30 | 2 each | Signed roll/pitch/yaw centidegrees -18000…18000 |
| 32…37 | 6 | Reserved zero |
| 38 | 2 | CRC16-CCITT-FALSE, polynomial 0x1021, initial 0xffff, no reflection, xorout 0, over bytes 0…37 |

Commands are 40 bytes. Status remains 24 bytes: magic/version/type=2 at 0/1/2; state at 3 (0…7), session at 4, acknowledged sequence at 8, uint16 fault mask at 12, battery millivolts at 14 (0 when unavailable), uint16 accepted-command age at 16 (saturates 65535), uint32 reject count at 18, CRC over 0…21 at 22. State/fault values are defined in the safety header. Range telemetry is serial-only, not a status extension.

Receiver accepts a new session only when not armed and the incoming packet has zero flags/throttle. The first packet after boot is also disabled/zero throttle. Sequence delta must lie in (0,2^31), including wraparound. Duplicate/old/invalid packets never refresh the timestamp. Gaps increment a loss estimate that also includes intentionally overwritten mailbox packets; it is not a calibrated RF-loss measurement. Unsigned subtraction handles timestamp wrap.

ESP-NOW callbacks allowlist MAC and length and copy to bounded mailboxes. A CRC-valid stop packet has a separate priority mailbox, preventing ordinary command overwrite; its session/sequence are still checked in the control task. Only one packet is consumed per control tick. A validated emergency is latched by the supervisor. A newer received ordinary packet cannot clear a latched stop.

Commands transmit at target 50 Hz; status at target 10 Hz. MAC completion is not an application acknowledgment. Only one send is outstanding, with atomic bookkeeping. Missing callbacks stall sends and cause a timeout. Glove requires an advancing ack for the matching session within 300 ms. It holds zero throttle until ARMED is acknowledged. The drone considers a command fresh through 200 ms inclusive; age >200 ms latches immediate-cut failsafe while armed. Recovery requires inspection and reset, never automatic re-arm. Session identifiers are not durable replay protection; no boot challenge or persistent anti-replay database exists.

## Private pairing

Read both station MACs from their 115200-baud boot messages. Both boards initialize station mode even before pairing exists so the address can be obtained without keys. Never infer a station MAC from an unrelated Bluetooth/AP address. Generate private material locally:

~~~sh
python tools/calibration/provision_pairing.py --glove-mac AA:BB:CC:DD:EE:02 --drone-mac AA:BB:CC:DD:EE:04
~~~

The addresses above are format examples, not hardware MACs. The tool validates unicast/different addresses, generates matching random PMK/LMK and exclusively creates ignored firmware/common/local_secrets.h. It refuses overwriting an existing file. BUILD_GLOVE selects the drone peer; other firmware selects the glove peer. Rebuild/flash both boards using the same private header. Do not commit, log or publish it, or upload provisioned binaries to public artifacts. Default builds have pairing disabled. Channel, range and interference must be measured on the real system.

References: [Espressif ESP-NOW IDF 4.4.7](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/network/esp_now.html).
