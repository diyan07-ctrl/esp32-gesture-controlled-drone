#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include "configuration/config.h"
namespace flight {
inline float clamp(float x, float a, float b) {
  return std::max(a, std::min(b, x));
}
inline float deadband(float x, float d = 0.08f) {
  if (!std::isfinite(x) || !std::isfinite(d) || d < 0 || d >= 1)
    return 0;
  return std::fabs(x) <= d ? 0 : std::copysign(clamp((std::fabs(x) - d) / (1 - d), 0, 1), x);
}
inline float gesture(float angle, float neutral) {
  return deadband(clamp((angle - neutral) / 30.0f, -1, 1));
}
struct Sample {
  float ax = 0, ay = 0, az = 1, gx = 0, gy = 0, gz = 0;
};
inline bool valid(const Sample &s) {
  return std::isfinite(s.ax) && std::isfinite(s.ay) && std::isfinite(s.az) && std::isfinite(s.gx) &&
         std::isfinite(s.gy) && std::isfinite(s.gz) && std::fabs(s.ax) < 7.95f &&
         std::fabs(s.ay) < 7.95f && std::fabs(s.az) < 7.95f && std::fabs(s.gx) < 495 &&
         std::fabs(s.gy) < 495 && std::fabs(s.gz) < 495;
}
inline bool validDt(float dt) {
  return std::isfinite(dt) && dt >= 0.002f && dt <= 0.006f;
}
struct Estimator {
  float tau = cfg::attitudeTau, gravityMin = cfg::gravityMin, gravityMax = cfg::gravityMax;
  float roll = 0, pitch = 0, yaw = 0;
  bool initialized = false;
  bool update(const Sample &s, float dt) {
    if (!valid(s) || !validDt(dt) || !std::isfinite(tau) || tau <= 0 ||
        !std::isfinite(gravityMin) || !std::isfinite(gravityMax) || gravityMin <= 0 ||
        gravityMin >= 1 || gravityMax <= 1)
      return false;
    float norm = std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    bool gravity = norm > gravityMin && norm < gravityMax;
    float ar = std::atan2(s.ay, s.az) * 57.2957795f;
    float ap = std::atan2(-s.ax, std::sqrt(s.ay * s.ay + s.az * s.az)) * 57.2957795f;
    if (!initialized) {
      if (!gravity || std::fabs(ap) > 60)
        return false;
      roll = ar;
      pitch = ap;
      initialized = true;
    }
    // Euler kinematics with body rates in FRD. Limit use away from gimbal lock.
    if (std::fabs(pitch) > 60)
      return false;
    float r = roll / 57.2957795f, p = pitch / 57.2957795f;
    float rd = s.gx + std::sin(r) * std::tan(p) * s.gy + std::cos(r) * std::tan(p) * s.gz;
    float pd = std::cos(r) * s.gy - std::sin(r) * s.gz;
    float yd = (std::sin(r) * s.gy + std::cos(r) * s.gz) / std::cos(p);
    float nextRoll = roll + rd * dt, nextPitch = pitch + pd * dt;
    float nextYaw = std::remainder(yaw + yd * dt, 360.0f);
    if (gravity) {
      float a = tau / (tau + dt);
      nextRoll = std::remainder(nextRoll + (1 - a) * std::remainder(ar - nextRoll, 360.0f), 360.0f);
      nextPitch = a * nextPitch + (1 - a) * ap;
    }
    // Validate the candidate before publication: a boundary crossing must not be
    // marked healthy for one cycle or poison recovery after rejected input.
    if (!std::isfinite(nextRoll) || !std::isfinite(nextPitch) || !std::isfinite(nextYaw) ||
        std::fabs(nextPitch) > 60)
      return false;
    roll = nextRoll;
    pitch = nextPitch;
    yaw = nextYaw;
    return true;
  }
};
struct Pid {
  // Bench starting values only. Units: normalized output per degree/s and seconds.
  float kp = cfg::rateKp, ki = cfg::rateKi, kd = cfg::rateKd, limit = cfg::rateLimit;
  float integral = 0, previous = 0, derivative = 0;
  // Last-cycle diagnostics; these do not alter the controller calculation.
  float error = 0, pTerm = 0, iTerm = 0, dTerm = 0, output = 0;
  bool initialized = false;
  void reset() {
    integral = previous = derivative = 0;
    error = pTerm = iTerm = dTerm = output = 0;
    initialized = false;
  }
  float update(float target, float measured, float dt, bool freeze = false) {
    if (!std::isfinite(target) || !std::isfinite(measured) || !validDt(dt)) {
      reset();
      return 0;
    }
    error = target - measured;
    float raw = initialized ? -(measured - previous) / dt : 0;
    previous = measured;
    initialized = true;
    derivative += dt / (0.02f + dt) * (raw - derivative);
    float candidate = clamp(integral + ki * error * dt, -limit, limit);
    float candidateOutput = kp * error + candidate + kd * derivative;
    if (!freeze && (std::fabs(candidateOutput) <= limit || candidateOutput * error < 0))
      integral = candidate;
    pTerm = kp * error;
    iTerm = integral;
    dTerm = kd * derivative;
    output = clamp(pTerm + iTerm + dTerm, -limit, limit);
    return output;
  }
};
struct Mix {
  std::array<float, 4> motor{};
  bool valid = false, saturated = false;
};
inline Mix mix(float t, float r, float p, float y) {
  Mix m;
  if (!std::isfinite(t) || !std::isfinite(r) || !std::isfinite(p) || !std::isfinite(y) || t < 0 ||
      t > 1)
    return m;
  // FRD axes, positive roll right-wing-down, pitch nose-up, yaw clockwise from above.
  // Owner-confirmed positions FR, FL, RL, RR; spins CCW, CW, CCW, CW from above.
  // Reaction yaw opposes spin. Independently verify on the frame before output enablement.
  std::array<float, 4> correction{};
  for (int i = 0; i < 4; i++)
    correction[i] = cfg::rollSign[i] * r + cfg::pitchSign[i] * p + cfg::yawSign[i] * y;
  auto bounds = std::minmax_element(correction.begin(), correction.end());
  float span = *bounds.second - *bounds.first;
  if (!std::isfinite(span))
    return m;
  // Scale differential demand only when it cannot fit the available output span.
  float scale = span > 1 ? 1 / span : 1;
  float low = 0, high = 1;
  for (float v : correction) {
    low = std::max(low, -v * scale);
    high = std::min(high, 1 - v * scale);
  }
  float collective = clamp(t, low, high);
  m.saturated = scale < 1 || std::fabs(collective - t) > 1e-6f;
  for (int i = 0; i < 4; i++)
    m.motor[i] = clamp(collective + correction[i] * scale, 0, 1);
  m.valid = true;
  return m;
}
inline uint32_t pwmDuty(float normalized, bool active, bool validMix = true) {
  if (!active || !validMix || !std::isfinite(normalized))
    return 0;
  float pulse = cfg::pwmMin + (cfg::pwmMax - cfg::pwmMin) * clamp(normalized, 0, 1);
  return uint32_t(pulse * cfg::pwmHz * ((1u << cfg::pwmBits) - 1) / 1000000.0f);
}
inline float batteryVoltage(float millivolts, float ratio = 5.7f, float scale = 1) {
  return millivolts * 0.001f * ratio * scale;
}
inline bool batteryValid(float v) {
  return std::isfinite(v) && v >= 6 && v <= 13.2f;
}
} // namespace flight
