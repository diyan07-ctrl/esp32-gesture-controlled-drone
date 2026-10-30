# Motor-output enablement gate

Default firmware never attaches PWM pins. Four independent constants in firmware/common/configuration/config.h are false: outputsVerified, powerVerified, propulsionVerified and controllerValidated. There is no command to bypass them. Changing constants is not evidence of readiness.

Before enabling even propeller-off motor testing, retain signed test records identifying exact board/ESC/motor models, schematic, measured regulator rails/transients, correct pinout, 3.3 V signal acceptance, verified ESC protocol/rate/endpoints/no-signal behavior, motor order/direction and a reviewed safe test fixture. Before any propeller use, require appropriate guarded thrust equipment, verified propulsion/load data, tuned controller validation, actuator timing analysis and a redesigned/validated airborne failsafe.

The LEDC implementation is a **candidate** 50 Hz PWM with 1000–2000 µs commands, 16-bit duty resolution. At 50 Hz one count is approximately 0.305 µs. This quantization calculation does not establish observed edge timing. No OneShot or DShot support is claimed. No endpoint calibration runs on boot. Confirm calibration only using the identified ESC manual on an isolated controlled setup.

If the actual ESC needs idle pulses while disarmed, modify the output state contract and test every boot/fault transition first. Current code uses absence of pulses; it must not be connected to an unidentified ESC assuming that absence is safe.
