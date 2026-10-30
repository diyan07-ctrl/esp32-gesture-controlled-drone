# ESP-NOW protocol v1

Use fixed channel 1, station mode, encrypted unicast with per-build peer MAC, private 16-byte PMK and LMK. Both endpoints must be provisioned locally. Default is unconfigured: no broadcast control and no public example keys. MAC allowlisting is additional filtering; it is not authentication on its own. CRC and sequence numbers are not cryptographic authentication.

| Command offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 1 | Magic 0x47 |
| 1 | 1 | Version 1 |
| 2 | 1 | Type 1 command |
| 3 | 1 | Flags bit0 enable, bit1 arm, bit2 stop; other bits rejected |
| 4 | 4 | Nonzero random boot session, uint32 LE |
| 8 | 4 | Sequence uint32 LE |
| 12 | 2 | Collective uint16 0–1000 |
| 14/16/18 | 2 each | Roll/pitch/yaw int16 −1000…1000 |
| 20 | 2 | Reserved zero |
| 22 | 2 | CRC16-CCITT-FALSE over bytes 0…21, LE |

Status has magic/version/type=2, byte3 state; session at4, acknowledged sequence at8, uint16 faults at12, battery mV at14, age ms at16, uint32 rejected count at18, CRC at22. Fixed size is 24 bytes in both directions. Encoding is explicit little endian; no C++ struct packing assumptions.

Callbacks enforce length and peer MAC then overwrite a one-slot mailbox. Main tasks validate CRC, version, type, flags, ranges, session and sequence. A first/new session is accepted only disabled with zero throttle; session changes while armed are rejected. Same-session freshness uses unsigned wrap-safe sequence delta in (0,2^31). Only accepted packets update the age timestamp, which is captured in the callback, not on dequeue. At 200 ms command age the armed drone latches FAILSAFE. An armed session change cannot extend the deadline. Safety latch needs power-cycle inspection, not a reconnect.

Session identifiers prevent accidental previous-session mixing; they are not a durable anti-replay database. No receiver boot challenge/persistent replay protection is implemented. This is a security limitation requiring protocol revision for hostile-link operation. Sequence gaps are reported as loss estimates; latest-mailbox overwrites also contribute. They are not a calibrated RF packet-loss measurement.

Transmit at most one outstanding ESP-NOW send. Completion callback releases the slot; immediate errors are counted. A missing completion stalls transmissions and causes timeout, not unbounded queued commands. MAC-layer send success is not application acknowledgment. Glove checks drone status session, advancing ack sequence and local status age. Drone status age remains the authoritative command-age indicator.

Provisioning: create ignored firmware/common/local_secrets.h defining namespace pairing with configured=true and constexpr uint8_t peer[6], pmk[16], lmk[16]. Select the drone peer MAC under BUILD_GLOVE and the glove peer MAC under BUILD_DRONE so both targets build consistently from one private header. Generate private random keys off-repository and use matching PMK/LMK. Do not commit the header or keys. Determine MAC via a standalone serial diagnostic before provisioning. Do not log keys. Default firmware builds without that header but intentionally cannot establish a link.
