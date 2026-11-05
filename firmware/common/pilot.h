#pragma once
#include "gesture.h"
#include "flex.h"
#include "potentiometer.h"
namespace flight {
// Shared sensor-to-command orchestration. Hardware, NVS, RF and time are supplied by adapters.
struct Pilot {
  GesturePipeline gestures;
  FlexFilter flex[2];
  PotFilter pot;
  bool neutral = false, flexHealthy = false, potHealthy = false, permit = false;
  float neutralRoll = 0, neutralPitch = 0;
  Pilot() {
    gestures.config.potentiometerThrottle = true;
  }
  protocol::Command update(const Estimator &attitude, const Sample &sample, bool healthy,
                           const uint16_t raw[2], uint16_t potRaw,
                           const FlexCalibration calibration[2],
                           const PotCalibration &potCalibration, bool editing, float dt,
                           uint32_t now, bool fresh, uint8_t droneState, bool enable = true,
                           bool armSwitch = true) {
    flexHealthy = flex[0].update(raw[0], calibration[0], dt);
    flexHealthy = flex[1].update(raw[1], calibration[1], dt) && flexHealthy;
    potHealthy = pot.update(potRaw, potCalibration, dt);
    if (droneState == 6 || droneState == 7) {
      gestures.stopped = true;
      gestures.reset();
    }
    if (!healthy || !flexHealthy || !potHealthy || !fresh || !enable || editing) {
      gestures.reset();
      neutral = false;
    }
    if (!neutral && healthy && flexHealthy && potHealthy && pot.target <= .02f && fresh &&
        !editing && !gestures.armed && flex[0].value < .2f && flex[1].value < .2f &&
        std::fabs(attitude.roll) < 8 && std::fabs(attitude.pitch) < 8) {
      neutral = true;
      neutralRoll = attitude.roll;
      neutralPitch = attitude.pitch;
    }
    protocol::Command c;
    c.flex[0] = raw[0];
    c.flex[1] = raw[1];
    c.orientation[0] = int16_t(clamp(attitude.roll, -180, 180) * 100);
    c.orientation[1] = int16_t(clamp(attitude.pitch, -180, 180) * 100);
    c.orientation[2] = int16_t(clamp(attitude.yaw, -180, 180) * 100);
    permit = healthy && flexHealthy && potHealthy && fresh && enable && neutral &&
             calibration[0].valid() && calibration[1].valid() && potCalibration.valid() && !editing;
    if (permit) {
      c.gesture = gestures.update(attitude.roll - neutralRoll, attitude.pitch - neutralPitch,
                                  sample.gz, flex[0].value, flex[1].value, dt, now, pot.target);
      gestures.throttle = gestures.armed && droneState == 5 ? pot.value : 0;
      if (!armSwitch) {
        gestures.armed = false;
        gestures.throttle = 0;
      }
      c.flags = protocol::enabled | (gestures.armed ? protocol::arm : 0);
      c.throttle = uint16_t(gestures.throttle * 1000);
      c.roll = int16_t(gestures.roll * 1000);
      c.pitch = int16_t(gestures.pitch * 1000);
      c.yaw = int16_t(gestures.yaw * 1000);
    }
    if (gestures.stopped) {
      c.flags = protocol::stop;
      c.gesture = protocol::Gesture::Emergency;
      c.throttle = 0;
    }
    return c;
  }
};
} // namespace flight
