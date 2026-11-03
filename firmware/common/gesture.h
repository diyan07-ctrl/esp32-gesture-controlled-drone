#pragma once
#include "control.h"
#include "protocol/packet.h"
namespace flight {
struct Gesture {
  float neutralRoll = 0, neutralPitch = 0;
  bool released = false, zeroed = false;
  void center(float r, float p) {
    neutralRoll = r;
    neutralPitch = p;
  }
  void reset() {
    released = zeroed = false;
  }
  bool permit(bool link, bool enable, bool arm, float throttle) {
    if (!link) {
      reset();
      return false;
    }
    if (!arm)
      released = true;
    if (!arm && !enable && throttle < 0.02f)
      zeroed = true;
    return released && zeroed && enable;
  }
};
struct GestureConfig {
  float bendOn = .75f, bendOff = .55f, tiltOn = 12, tiltOff = 8, yawOn = 35, yawOff = 20;
  float throttlePerSecond = .15f, smoothingSeconds = .1f;
  uint32_t debounceMs = 150, armMs = 1000;
};
struct GesturePipeline {
  GestureConfig config{};
  protocol::Gesture pending = protocol::Gesture::Neutral, stable = protocol::Gesture::Neutral;
  uint32_t since = 0;
  bool bent[2] = {}, armed = false, stopped = false;
  float throttle = 0, roll = 0, pitch = 0, yaw = 0;
  void reset() {
    armed = false;
    throttle = roll = pitch = yaw = 0;
    pending = stable = protocol::Gesture::Neutral;
    bent[0] = bent[1] = false;
    since = 0;
  }
  protocol::Gesture classify(float r, float p, float gz, float f0, float f1) {
    using G = protocol::Gesture;
    bent[0] = f0 >= (bent[0] ? config.bendOff : config.bendOn);
    bent[1] = f1 >= (bent[1] ? config.bendOff : config.bendOn);
    if (bent[0] && bent[1]) {
      if (p <= -config.tiltOn)
        return G::Emergency;
      if (p >= config.tiltOn)
        return G::Disarm;
      if (std::fabs(r) < config.tiltOff && std::fabs(p) < config.tiltOff)
        return G::Arm;
      return G::Neutral;
    }
    if (bent[0])
      return G::ThrottleUp;
    if (bent[1])
      return G::ThrottleDown;
    float tiltThreshold =
        (stable == G::Left || stable == G::Right || stable == G::Forward || stable == G::Backward)
            ? config.tiltOff
            : config.tiltOn;
    if (std::fabs(r) > std::fabs(p) && std::fabs(r) >= tiltThreshold)
      return r > 0 ? G::Right : G::Left;
    if (std::fabs(p) >= tiltThreshold)
      return p > 0 ? G::Backward : G::Forward;
    float yawThreshold =
        (stable == G::YawLeft || stable == G::YawRight) ? config.yawOff : config.yawOn;
    if (std::fabs(gz) >= yawThreshold)
      return gz > 0 ? G::YawRight : G::YawLeft;
    return G::Neutral;
  }
  protocol::Gesture update(float r, float p, float gz, float f0, float f1, float dt, uint32_t now) {
    using G = protocol::Gesture;
    if (!validDt(dt) || !std::isfinite(r) || !std::isfinite(p) || !std::isfinite(gz) ||
        !std::isfinite(f0) || !std::isfinite(f1) || f0 < 0 || f0 > 1 || f1 < 0 || f1 > 1) {
      reset();
      return G::Disarm;
    }
    G next = classify(r, p, gz, f0, f1);
    if (next == G::Emergency) {
      stopped = true;
      reset();
      stable = G::Emergency;
      return stable;
    }
    if (stopped)
      return G::Emergency;
    if (next != pending) {
      pending = next;
      since = now;
    }
    if (uint32_t(now - since) >= (next == G::Arm ? config.armMs : config.debounceMs))
      stable = next;
    if (stable == G::Disarm) {
      armed = false;
      throttle = 0;
    }
    if (stable == G::Arm && throttle <= .02f)
      armed = true;
    if (armed) {
      if (stable == G::ThrottleUp)
        throttle = clamp(throttle + config.throttlePerSecond * dt, 0, 1);
      if (stable == G::ThrottleDown)
        throttle = clamp(throttle - config.throttlePerSecond * dt, 0, 1);
    }
    float a = dt / (config.smoothingSeconds + dt);
    roll += a * ((stable == G::Right ? 1.0f : stable == G::Left ? -1.0f : 0) - roll);
    pitch += a * ((stable == G::Backward ? 1.0f : stable == G::Forward ? -1.0f : 0) - pitch);
    yaw += a * ((stable == G::YawRight ? 1.0f : stable == G::YawLeft ? -1.0f : 0) - yaw);
    return stable;
  }
};
} // namespace flight
