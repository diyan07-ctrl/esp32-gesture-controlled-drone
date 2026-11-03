#include <iostream>
#include "control.h"
#include "safety.h"
int main() {
  using namespace flight;
  // Assumed decoupled normalized plant, with independently stated physical motor torques.
  const int physical[3][4] = {{-1, 1, 1, -1}, {1, 1, -1, -1}, {1, -1, 1, -1}};
  unsigned steps = 0;
  for (int axis = 0; axis < 3; axis++)
    for (int direction : {-1, 1}) {
      Pid pid;
      float angle = direction * 10.0f, rate = direction * 5.0f;
      for (int i = 0; i < 10000; i++) {
        float target = axis == 2 ? 0 : clamp(-cfg::angleKp * angle, -cfg::maxRate, cfg::maxRate);
        float u = pid.update(target, rate, .004f);
        Mix m = mix(.5f, axis == 0 ? u : 0, axis == 1 ? u : 0, axis == 2 ? u : 0);
        if (!m.valid)
          return 1;
        float torque = 0;
        for (int j = 0; j < 4; j++) {
          if (m.motor[j] < 0 || m.motor[j] > 1)
            return 2;
          torque += physical[axis][j] * m.motor[j] / 4;
        }
        if (i == 0 && torque * direction >= 0)
          return 3;
        rate += (100 * torque - .5f * rate) * .004f;
        angle += rate * .004f;
        if (!std::isfinite(angle) || !std::isfinite(rate) || std::fabs(angle) > 30)
          return 4;
        ++steps;
      }
      if ((axis < 2 && std::fabs(angle) > .5f) || std::fabs(rate) > .5f) {
        std::cerr << "convergence axis=" << axis << " angle=" << angle << " rate=" << rate << '\n';
        return 5;
      }
    }
  for (int t = 0; t <= 100; t++)
    for (float r : {-.8f, 0.f, .8f})
      for (float p : {-.8f, 0.f, .8f})
        for (float y : {-.8f, 0.f, .8f}) {
          auto m = mix(t * .01f, r, p, y);
          if (!m.valid)
            return 6;
          for (float v : m.motor)
            if (v < 0 || v > 1)
              return 7;
          ++steps;
        }
  std::cout << "PASS " << steps
            << " assumed-plant steps/saturation cases; roll/pitch/yaw correction signs and "
               "convergence. NOT aircraft stability evidence.\n";
}
