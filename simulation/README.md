# Assumed-plant regression harnesses

Run python tools/run_host_tests.py (or its --zig option on Windows). roll_model.cpp retains the historical finite-output single-axis model. closed_loop.cpp simulates all three independently stated physical torque signs in both directions, checks convergence under assumed decoupled dynamics, and sweeps motor saturation. Integration tests separately inject link loss/corruption, sensor/timing faults, disarm and emergency through encoded packets and safety/PWM logic.

These models omit actual mass/inertia, thrust/ESC latency, aerodynamics, vibration, battery sag, coupling and disturbances. No real F450 stability or flight-readiness claim follows.
