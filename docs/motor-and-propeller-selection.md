# Motor and propeller selection

Owned motors: A2212, 1000 KV as reported by owner. All other ratings remain unidentified. “A2212” does not establish a manufacturer's winding, shaft, bearings, continuous current, maximum watts or supported cells. See [audit evidence](hardware-feasibility-audit.md).

Do not choose a propeller from F450 arm clearance alone. Obtain maker-specific motor/prop tests at 3S including 12.6 V, continuous current, thrust, RPM, temperature and duration. Match hub bore, adapter, thread retention, CW/CCW blades and frame clearance. Check arm bolt spacing and bolt length against the motor base; bolts must not touch windings. Confirm all four motors are identical.

Required data sheet:
- Maker, exact SKU, KV marking, supported cells, maximum continuous/burst current and power, no-load current/RPM if documented.
- Recommended propeller diameter/pitch and ESC, load curves and test conditions.
- Motor mass, mount pattern, shaft/hub dimensions and adapter retention.
- ESC actual input range, signal type/levels, update rate, endpoint procedure, no-signal behavior and thermal limits.

A 30 A ESC is conditionally plausible only after the selected motor/prop current and cooling are verified. DShot/OneShot are not assumed. The firmware contains a conventional 50 Hz, 1000–2000 µs PWM candidate but does not emit it in the default configuration. No ESC calibration pulses are emitted automatically.

Weigh the complete aircraft. Required hover thrust per motor is approximately mass_g/4 gram-force. A provisional review target of ≥2:1 total static thrust/weight reserves maneuver margin but is not a flight certificate. Measure thrust safely using an appropriate guarded stand, never exposed props on a desk.
