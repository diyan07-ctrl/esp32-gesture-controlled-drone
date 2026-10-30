# Security policy

This experimental controller is not certified or approved for flight. No supported production release currently exists. Treat unexpected output, stale-command acceptance, arming bypass, protocol replay, key disclosure and watchdog failures as security/safety concerns.

Keep props removed and actuator power isolated while reproducing issues. Provide minimal code, affected revision, expected behavior and sanitized evidence. Never publish PMK/LMK keys or local_secrets.h. If GitHub private vulnerability reporting is enabled, use it; otherwise request a private maintainer channel without disclosing exploit details. No private-reporting feature is claimed enabled before repository publication.

Encrypted ESP-NOW does not cure sensor/power failure. Session/sequence/CRC checks do not provide persistent receiver-reboot anti-replay protection. See docs/communication-protocol.md and docs/known-limitations.md. Rotate any exposed pairing material locally and review the public Git history before reuse.
