# Assembly guide

Assembly remains provisional until the hardware audit gates close. Do not populate a guessed ESC connector or assume an F450 center plate is a rated PDB.

1. Inventory exact SKUs, markings and mass; inspect frame and motor fasteners. Select motor-compatible props but keep them removed.
2. Confirm DevKit schematic and GPIO labels against [tables](connection-tables.md). Verify MPU supply and 3.3 V pullups. Solder short sensor wiring with strain relief.
3. Assemble PDB/harness with correctly rated wire, connectors and insulation. Check polarity and continuity with no battery. Route battery-to-ESC current separately from signal returns.
4. Verify buck with a current-limited test supply before connecting ESP32. Check 5 V output, 3.3 V rail, temperature and load transients. Use only one DevKit power source.
5. Individually identify all ESC leads from documentation and continuity as appropriate. Insulate each unused BEC positive. Connect signal and verified reference grounds only after level compatibility is established.
6. Mount sensor near center of mass with documented axes. Secure electronics away from prop arcs and antenna obstructions. Add isolation only with measured vibration benefit.
7. Fit battery divider after input/back-power protection review. Verify with DMM. Add normally closed run-permit loop, LED and external ESC signal pulldowns.
8. Build glove with protected 5 V supply, switches and separate throttle/yaw controls. Test sensor and controls independently.
9. Perform every validation stage in order, stop at the first failure, and record actual results. Firmware output gates remain closed through documentation and sensor/link tests.

Do not install propellers until an independent flight-readiness review authorizes the exact assembly and test environment.
