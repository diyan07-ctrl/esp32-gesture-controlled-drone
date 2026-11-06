#include "F450_ESP32_FlightController/Control.h"
#include <assert.h>
#include <stdio.h>
int main() {
  PID p;
  assert(fabs(p.step(1, defaults[0], .004) - 151.5) < .001);
  for (int i = 0; i < 10000; i++)
    p.step(100, {0, 100, 0}, .004);
  assert(p.integral == 400);
  float m[4];
  mix(1500, 10, 0, 0, m);
  assert(m[0] < m[1] && m[3] < m[2]);
  mix(1500, 0, 10, 0, m);
  assert(m[0] > m[3] && m[1] > m[2]);
  mix(1500, 0, 0, 10, m);
  assert(m[1] > m[0] && m[3] > m[2]);
  Quaternion q = {.70710678f, 0, 0, .70710678f};
  float a[3];
  (q.inverse() * q).angles(a);
  assert(fabs(a[0]) + fabs(a[1]) + fabs(a[2]) < .001);
  Quaternion start = {cosf(179 * CONTROL_PI / 360), 0, 0, sinf(179 * CONTROL_PI / 360)},
             end = {cosf(-179 * CONTROL_PI / 360), 0, 0, sinf(-179 * CONTROL_PI / 360)};
  (start.inverse() * end).angles(a);
  assert(fabs(a[2] - 2) < .001);
  assert(fabs(wrap(-358) - 2) < .001);
  assert(fabs(10 + 4.03 - 14.03) < .00001);
  puts("PID, limits, mixer geometry, quaternion identity and heading crossing passed");
}
