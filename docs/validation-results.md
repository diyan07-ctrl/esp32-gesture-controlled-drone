# Validation results — 2026-10-30

This record distinguishes performed checks from prepared tests. No hardware was physically tested.

| Check | Result | Evidence/limitation |
|---|---|---|
| Hardware feasibility review | Complete as provisional audit | Exact ESC, PCB variants, motor maker, prop data, pack C and mass absent |
| Local compiler discovery | No C++ compiler found | git/node/uv/gh present; pio/python/g++/clang++ not found on PATH |
| Toolchain preparation | Blocked | uv Python download failed with socket permission error 10013; protected local program folders denied |
| Glove firmware compile | NOT RUN | PlatformIO/toolchain unavailable; no successful build claimed |
| Drone firmware compile | NOT RUN | Same blocker |
| Shared C++ unit tests | WRITTEN, NOT RUN | Compiler unavailable; CI is configured |
| Python utility tests | WRITTEN, NOT RUN | Python unavailable |
| Toy simulation | WRITTEN, NOT RUN | Compiler unavailable; never a flight model |
| Repository static checks | PASS: 413 assertions across 72 source files, Node v24.21.0 | Actual final Node execution; local Markdown links, motor mapping, GPIO/default gates, CI commands, BOM IDs, secret-pattern scan and basic Mermaid structure checked. Not compilation, exhaustive secret detection or visual rendering |
| Mermaid visual rendering | NOT RUN | Renderer unavailable; editable sources and motor SVG supplied |
| CI | NOT RUN | Repository publication not completed |
| Sensor/RF/ESC/bench/HIL/flight | NOT RUN | Physical hardware/instruments unavailable |
| GitHub connector | Blocked after initial account inspection | Runtime returned Unknown tool on subsequent calls |
| GitHub browser path | Blocked | Computer Use was not approved to use Brave, including retry after user authorization |
| GitHub CLI | Blocked | Configuration path access denied; no credentials exposed or changed |
| Local Git commit | Blocked | git init -b main succeeded; git add and commit could not create .git/index.lock (permission denied). No commit or hash exists |

Initial connector profile was vishwam17-ctrl. User subsequently specified diyan07-ctrl. A public read-only web check verified the diyan07-ctrl profile exists and the requested repository returned 404. This does not authenticate write permission or exclude a private repository. The requested owner has not been authenticated by a successful connector runtime call. Do not publish into the previously connected account merely to bypass this owner requirement.

A repository URL, remote visibility, pushed commit or CI pass must be added only after actual successful publication and public verification.
