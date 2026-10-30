# Contributing

Preserve the ESP32-WROOM-32 onboard controller and independent glove application. No dedicated flight-controller substitution or fallback is accepted. Open a focused change with hardware identity, references, units, assumptions and reproducible validation. Update wiring, GPIO CSV, diagrams and mixer tests together whenever any mapping changes.

Run both PlatformIO builds, shared C++ tests, Python utility tests and repository checker using docs/build-and-upload.md. Report checks not run and compiler/framework versions; never convert untested hardware into a compatibility claim. Keep output gates disabled in general-purpose contributions. Explain saturation, timing and safety effects of controller changes. Gains require recorded evidence, not subjective “stable” labels.

Do not commit private keys, pairing headers, personal/site data, binaries or raw sensitive logs. Use concise commits that describe actual changes. Respect CODE_OF_CONDUCT.md and report security issues according to SECURITY.md.
