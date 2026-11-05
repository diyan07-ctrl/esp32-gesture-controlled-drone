#pragma once
#include "control.h"
namespace flight {
// Shared orchestration extracted verbatim from the drone task. No actuator access.
struct Stabilizer {
  Pid rollPid, pitchPid, yawPid;
  bool saturated = false;
  float correction[3] = {}, rateTarget[3] = {};
  Mix update(bool armed, float throttle, float rollCommand, float pitchCommand, float yawCommand,
             const Estimator &attitude, const Sample &sample, float dt) {
    Mix motors;
    correction[0] = correction[1] = correction[2] = 0;
    if (armed && throttle > .02f) {
      rateTarget[0] =
          clamp(cfg::angleKp * (rollCommand - attitude.roll), -cfg::maxRate, cfg::maxRate);
      rateTarget[1] =
          clamp(cfg::angleKp * (pitchCommand - attitude.pitch), -cfg::maxRate, cfg::maxRate);
      rateTarget[2] = yawCommand;
      correction[0] = rollPid.update(rateTarget[0], sample.gx, dt, saturated);
      correction[1] = pitchPid.update(rateTarget[1], sample.gy, dt, saturated);
      correction[2] = yawPid.update(rateTarget[2], sample.gz, dt, saturated);
      motors = mix(throttle, correction[0], correction[1], correction[2]);
    } else {
      rollPid.reset();
      pitchPid.reset();
      yawPid.reset();
      rateTarget[0] = rateTarget[1] = rateTarget[2] = 0;
      motors = mix(0, 0, 0, 0);
    }
    saturated = motors.saturated;
    return motors;
  }
};
} // namespace flight
