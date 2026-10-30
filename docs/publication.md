# Publication record

Requested owner: diyan07-ctrl. Requested repository: esp32-gesture-controlled-drone, public. Description: Custom gesture-controlled quadcopter using two ESP32-WROOM-32 boards, MPU6050 IMUs, ESP-NOW communication, and a custom onboard flight-control algorithm without a dedicated flight controller.

GitHub CLI authenticated as **diyan07-ctrl** on 2026-10-30. The requested repository name was checked and absent before creation. Creation succeeded with **PUBLIC** visibility and **ADMIN** access for the authenticated owner: [public repository](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone). The main branch was pushed successfully. All 73 source paths and Git blob hashes matched the remote tree at the initial publication; subsequent updates retain this structure. Anonymous requests to the repository and raw README returned HTTP 200.

The [successful build/test run](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/runs/41005573875) compiled both firmware targets and passed shared C++, Python, static and toy-model checks. [Current workflow runs](https://github.com/diyan07-ctrl/esp32-gesture-controlled-drone/actions/workflows/firmware-ci.yml) report each later commit. Three real engineering issues track unresolved hardware, airborne failsafe and physical-validation work. No release or flight-readiness claim is made.

The initial session was blocked by restricted filesystem/network access and unavailable connector runtime tools. Updated permissions allow gh's existing keyring authentication to work. Git's sandbox-owner mismatch is handled through a process-scoped safe.directory value for this exact trusted workspace; no global trust exception, credential exposure or new token was needed. For subsequent updates, inspect the remote head and existing contents before mutation. Do not force-push unrelated work.

Commands for a correctly authenticated authorized CLI, after inspection:
```sh
gh api user --jq .login
gh repo view diyan07-ctrl/esp32-gesture-controlled-drone --json nameWithOwner,visibility
gh repo create diyan07-ctrl/esp32-gesture-controlled-drone --public --description "Custom gesture-controlled quadcopter using two ESP32-WROOM-32 boards, MPU6050 IMUs, ESP-NOW communication, and a custom onboard flight-control algorithm without a dedicated flight controller." --source . --remote origin --push
gh repo view diyan07-ctrl/esp32-gesture-controlled-drone --json url,visibility
gh run list
```
The create command above documents the initial operation and must not be repeated on the existing repository. Do not paste access tokens into project files. Inspect remote commit against git rev-parse HEAD. Track unknown hardware identities, ADC protection design, airborne failsafe, controller tuning and physical timing tests. Do not create a flight-ready release. A pre-release is unnecessary until actual builds and review pass.
