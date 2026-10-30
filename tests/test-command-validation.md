# Command validation acceptance cases

Executable core tests encode/decode v1, reject length errors, mutate every byte, reject ranges/flags, check duplicate/stale sequence, accept sequence wrap, reject new armed session, and verify invalid packets do not refresh receive time. Test timeout through millis wrap and explicit status decoding. Expand with fuzzing and receiver-reboot replay tests before hostile-link use.

Hardware link tests must verify source MAC, private encryption, channel, 50 Hz command rate, status acknowledgments, no auto-arm on glove reboot, and measured packet-age deadline. The one-slot queue is intentional; sequence gaps are not solely RF loss. Refer to the protocol document for wire offsets.
