#pragma once
#include "Control.h"
inline bool fresh(uint32_t now, uint32_t then, uint32_t maximum) {
  return uint32_t(now - then) <= maximum;
}
inline bool newer(uint32_t seq, uint32_t previous) {
  return seq != previous && uint32_t(seq - previous) < 0x80000000u;
}
inline float slew(float current, float target, float speed, float dt) {
  return current + limit(target - current, -speed * dt, speed * dt);
}
struct StartupReference {
  Quaternion reference, anchor, last;
  float bias[3] = {}, sum[3] = {};
  unsigned count = 0;
  bool ready = false;
  bool update(Quaternion &q, const float *gyro, const float *acc) {
    if (!q.normalize()) {
      if (!ready)
        reset();
      return false;
    }
    q.align(last);
    if (ready) {
      float delta[3];
      (last.inverse() * q).angles(delta);
      for (float v : delta)
        if (fabsf(v) > 30)
          return false;
    }
    last = q;
    if (ready)
      return true;
    float norm = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
    bool stationary = isfinite(norm) && fabsf(norm - 9.80665f) < .5f;
    for (int i = 0; i < 3; i++)
      stationary = stationary && isfinite(gyro[i]) && fabsf(gyro[i]) < 3;
    if (count) {
      float a[3];
      (anchor.inverse() * q).angles(a);
      for (float v : a)
        stationary = stationary && fabsf(v) < 1;
    }
    if (!stationary) {
      reset();
      return true;
    }
    if (!count)
      anchor = q;
    for (int i = 0; i < 3; i++)
      sum[i] += gyro[i];
    if (++count == 200) {
      reference = q;
      for (int i = 0; i < 3; i++)
        bias[i] = sum[i] / count;
      ready = true;
    }
    return true;
  }
  void reset() {
    count = 0;
    for (float &v : sum)
      v = 0;
  }
};
struct HeightEstimator {
  float samples[3] = {}, height = 0, slant = 0;
  unsigned count = 0, index = 0, rejected = 0;
  uint32_t time = 0;
  bool latestValid = false;
  void invalidate() {
    latestValid = false;
    count = 0;
    index = 0;
    for (float &v : samples)
      v = 0;
    rejected++;
  }
  bool healthy(uint32_t now) const {
    return latestValid && count >= 3 && fresh(now, time, 250000);
  }
  bool update(float distance, float rollDeg, float pitchDeg, uint32_t now) {
    if (!isfinite(distance) || distance < 2 || distance > 200 || !isfinite(rollDeg) ||
        !isfinite(pitchDeg) || fabsf(rollDeg) > 30 || fabsf(pitchDeg) > 30) {
      invalidate();
      return false;
    }
    float vertical =
        distance * cosf(rollDeg * CONTROL_PI / 180) * cosf(pitchDeg * CONTROL_PI / 180) + 4.03f;
    if (!fresh(now, time, 250000))
      count = index = 0;
    samples[index++ % 3] = vertical;
    if (count < 3)
      count++;
    float sorted[3];
    for (unsigned i = 0; i < count; i++)
      sorted[i] = samples[i];
    for (unsigned i = 0; i < count; i++)
      for (unsigned j = i + 1; j < count; j++)
        if (sorted[i] > sorted[j]) {
          float tmp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = tmp;
        }
    float median = sorted[count / 2];
    float dt = (now - time) * 1e-6f;
    height = count == 1 ? median : height + dt / (.15f + dt) * (median - height);
    slant = distance;
    time = now;
    latestValid = true;
    return true;
  }
};
struct AltitudeController {
  PID pid;
  bool active = false, validated = false;
  float target = 0, collective = 1180, anchor = 1180, correction = 0, error = 0;
  uint32_t measurementTime = 0;
  void manual() {
    active = false;
    pid = PID{};
    correction = 0;
  }
  bool enable(float requested, const HeightEstimator &h, Gain gain, uint32_t now) {
    if (!validated || !validGain(gain) || gain.p <= 0 || !h.healthy(now) || !isfinite(requested) ||
        requested < 6.03 || requested > 204.03)
      return false;
    active = true;
    requestedTarget = requested;
    target = h.height;
    anchor = collective;
    pid = PID{};
    measurementTime = h.time;
    error = 0;
    correction = 0;
    return true;
  }
  float requestedTarget = 0;
  bool step(float manualUs, float climb, Gain gain, const HeightEstimator &h, uint32_t now,
            float dt) {
    if (!isfinite(dt) || dt <= 0 || !isfinite(manualUs) || !isfinite(climb) ||
        (active && !validGain(gain)))
      return false;
    float desired = manualUs;
    if (active) {
      if (!validated || !h.healthy(now))
        return false; // no integration on invalid/stale data
      requestedTarget = limit(requestedTarget + climb * 30 * dt, 6.03, 204.03);
      target = slew(target, requestedTarget, 30, dt);
      if (h.time != measurementTime) {
        float sampleDt = (h.time - measurementTime) * 1e-6f;
        measurementTime = h.time;
        error = target - h.height;
        float saved = pid.integral;
        correction = pid.step(error, gain, sampleDt);
        if ((anchor + correction >= 1800 && error > 0) ||
            (anchor + correction <= 1180 && error < 0))
          pid.integral = saved;
      }
      desired = limit(anchor + correction, 1180, 1800);
    }
    collective = slew(collective, desired, 100, dt);
    return isfinite(collective);
  }
};
