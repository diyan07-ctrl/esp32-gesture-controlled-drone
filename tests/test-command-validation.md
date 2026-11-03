# Command validation acceptance cases

Executable core tests encode/decode v2, reject length errors, mutate every command byte, reject ranges/flags, check duplicate/stale sequence, accept sequence wrap, reject new armed session, and verify invalid packets do not refresh receive time. Sensor/gesture tests also round-trip flex, gesture and orientation fields and reject invalid sensor/gesture values. Test timeout through millis wrap and explicit status decoding. Expand with fuzzing and receiver-reboot replay tests before hostile-link use.

Hardware link tests must verify source MAC, private encryption, channel, 50 Hz command rate, status acknowledgments, no auto-arm on glove reboot, emergency mailbox priority amid ordinary traffic, and measured packet-age deadline. The bounded latest-value mailboxes are intentional; sequence gaps are not solely RF loss. Refer to the protocol document for wire offsets.
