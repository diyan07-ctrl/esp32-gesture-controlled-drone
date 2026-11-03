# Mixer acceptance cases

Executable: unit/core_tests.cpp includes the actual firmware/common/control.h. Owner-confirmed order is FR, FL, RL, RR, with CCW, CW, CCW, CW spins. At T=.5, a positive .1 roll gives [.4,.6,.6,.4]; pitch gives [.6,.6,.4,.4]; yaw gives [.6,.4,.6,.4]. Test all four values, desaturation flags, finite rejection, out-of-range throttle, and equal collective over 101 levels. Match hardware/motor-mapping.csv, motor SVG and flight-control-algorithm.md.

These cases test software signs. Physical corrections require verifying sensor and motor axes with propellers removed; a mixer unit test cannot verify installed rotation, thrust, torque constant or propeller orientation. Record execution in validation-results.md.
