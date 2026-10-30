# Simulation scope

The C++ roll_model executable uses the actual shared PID with a toy first-order rate plant, rate_dot=100×normalized_torque−0.5×rate. Run with g++ as shown in CI. It checks finite bounded computations under a rate step. Neither coefficient comes from the F450. There is no validated inertia, thrust curve, motor lag, battery sag, vibration, aerodynamic coupling or RF model. Do not use its response as tuning evidence.

A future software-in-the-loop plant should use measured mass/inertia, thrust-versus-command curves, ESC/motor delay, sensor noise and packet dropout. Run Monte Carlo tests across uncertainty. HIL can feed recorded or simulated sensor samples into a test-only driver while ESC outputs remain electrically disconnected, and scope a logic-only output harness. Preserve independent real hardware tests. Neither simulation nor HIL proves real flight stability.
