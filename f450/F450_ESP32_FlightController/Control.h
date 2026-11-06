#pragma once
#include <math.h>
constexpr float CONTROL_PI = 3.14159265358979323846f;
inline float limit(float x, float a, float b) {
  return fminf(b, fmaxf(a, x));
}
inline float wrap(float x) {
  return remainderf(x, 360.f);
}
struct Gain {
  float p, i, d;
};
static const Gain defaults[6] = {{1.5, 0, .6}, {1.5, 0, .6}, {.7, 0, .01},
                                 {.7, 0, .01}, {2, 12, 0},   {0, 0, 0}};
struct PID {
  float previous = 0, integral = 0;
  float step(float e, Gain g, float dt) {
    integral = limit(integral + g.i * (e + previous) * dt / 2, -400, 400);
    float out = limit(g.p * e + integral + g.d * (e - previous) / dt, -400, 400);
    previous = e;
    return out;
  }
};
struct Quaternion {
  float w = 1, x = 0, y = 0, z = 0;
  Quaternion inverse() const {
    return {w, -x, -y, -z};
  }
  Quaternion operator*(const Quaternion &b) const {
    return {w * b.w - x * b.x - y * b.y - z * b.z, w * b.x + x * b.w + y * b.z - z * b.y,
            w * b.y - x * b.z + y * b.w + z * b.x, w * b.z + x * b.y - y * b.x + z * b.w};
  }
  void angles(float *a) const {
    constexpr float k = 180 / CONTROL_PI;
    a[0] = atan2f(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * k;
    a[1] = asinf(limit(2 * (w * y - z * x), -1, 1)) * k;
    a[2] = atan2f(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * k;
  }
};
inline void mix(float t, float r, float p, float y, float *m) {
  // Body FRD: +roll right down, +pitch nose up, +yaw nose right.
  m[0] = t - r + p - y;
  m[1] = t + r + p + y;
  m[2] = t + r - p - y;
  m[3] = t - r - p + y;
  for (int i = 0; i < 4; i++) {
    m[i] *= 1.024f;
    if (m[i] > 2000)
      m[i] = 1989;
    m[i] = limit(m[i], 1180, 2000);
  }
}
