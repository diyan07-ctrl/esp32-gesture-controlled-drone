#include "F450_ESP32_FlightController/Protocol.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
void near(float a, float b, float eps = .001f) {
  assert(fabsf(a - b) <= eps);
}
int main() {
  StartupReference zero;
  Quaternion identity;
  float stillGyro[3] = {}, gravity[3] = {0, 0, 9.80665f};
  for (int i = 0; i < 200; i++)
    assert(zero.update(identity, stillGyro, gravity));
  assert(zero.ready);
  Quaternion jump = {.70710678f, .70710678f, 0, 0};
  assert(!zero.update(jump, stillGyro, gravity));
  near(zero.reference.w, 1);
  const float expected[5][3] = {{1.5, 0, .6}, {1.5, 0, .6}, {.7, 0, .01}, {.7, 0, .01}, {2, 12, 0}};
  for (int i = 0; i < 5; i++) {
    near(defaults[i].p, expected[i][0]);
    near(defaults[i].i, expected[i][1]);
    near(defaults[i].d, expected[i][2]);
  }
  PID p;
  near(p.step(2, {3, 0, 0}, .004), 6);
  p = {};
  near(p.step(2, {0, 2, 0}, .004), .008);
  p = {};
  near(p.step(2, {0, 0, .01}, .004), 5);
  for (int i = 0; i < 10000; i++)
    p.step(100, {0, 100, 0}, .004);
  near(p.integral, 400);
  assert(isnan(p.step(1, {1, 0, 0}, 0)));
  assert(isnan(p.step(1, {1, 0, 0}, NAN)));
  p = {};
  near(p.step(2, {1, 0, 0}, .004), 2);
  p = {};
  near(p.step(2, {2, 0, 0}, .004), 4);
  puts("PID defaults, P/I/D, windup, dt rejection, gain change PASS");
  float m[4];
  mix(1500, 0, 0, 0, m);
  for (float v : m)
    near(v, 1500);
  const float coeff[3][4] = {{-1, 1, 1, -1}, {1, 1, -1, -1}, {-1, 1, -1, 1}};
  for (int axis = 0; axis < 3; axis++)
    for (int sign : {-1, 1}) {
      float c[3] = {};
      c[axis] = sign * 10;
      assert(mix(1500, c[0], c[1], c[2], m));
      for (int i = 0; i < 4; i++)
        near(m[i], 1500 + coeff[axis][i] * sign * 10);
    }
  mix(1500, 10, 20, 30, m);
  near(m[0], 1480);
  near(m[1], 1560);
  near(m[2], 1460);
  near(m[3], 1500);
  mix(1800, 400, 400, 400, m);
  for (float v : m)
    assert(v >= 1180 && v <= 2000);
  near(m[1], 1989);
  assert(!mix(NAN, 0, 0, 0, m));
  for (float v : m)
    near(v, 1000);
  puts("Mixer zero, six directions, combined, saturation, invalid PASS");
  for (float us : {1000.f, 1180.f, 1500.f, 2000.f}) {
    uint32_t d;
    assert(pulseToDuty(us, d));
    near(d * 20000.f / 65536.f, us, .153f);
    printf("PWM %.0f -> %u -> %.6f us\n", us, d, d * 20000.f / 65536.f);
  }
  for (float us : {NAN, INFINITY, 999.f, 2001.f}) {
    uint32_t d;
    assert(!pulseToDuty(us, d));
    assert(d == 3277);
  }
  puts("PWM invalid PASS");
  for (int axis = 0; axis < 3; axis++) {
    StartupReference s;
    float g[3] = {.2f, -.3f, .1f}, acc[3] = {0, 0, 9.80665f};
    Quaternion q;
    float half = 20 * CONTROL_PI / 360;
    q.w = cosf(half);
    if (axis == 0)
      q.x = sinf(half);
    if (axis == 1)
      q.y = sinf(half);
    if (axis == 2)
      q.z = sinf(half);
    for (int i = 0; i < 200; i++)
      assert(s.update(q, g, acc));
    assert(s.ready);
    float a[3];
    (s.reference.inverse() * q).angles(a);
    for (float v : a)
      near(v, 0);
    for (int i = 0; i < 3; i++)
      near(s.bias[i], g[i]);
    Quaternion opposite = {-q.w, -q.x, -q.y, -q.z};
    assert(s.update(opposite, g, acc));
    near(opposite.w, q.w);
  }
  Quaternion bad = {0, 0, 0, 0};
  assert(!bad.normalize());
  bad = {NAN, 0, 0, 0};
  assert(!bad.normalize());
  Quaternion start = {cosf(179 * CONTROL_PI / 360), 0, 0, sinf(179 * CONTROL_PI / 360)},
             end = {cosf(-179 * CONTROL_PI / 360), 0, 0, sinf(-179 * CONTROL_PI / 360)};
  float a[3];
  (start.inverse() * end).angles(a);
  near(a[2], 2);
  puts("Orientation roll/pitch reference, bias, sign, invalid, wrap PASS");
  HeightEstimator h;
  for (int i = 1; i <= 3; i++)
    assert(h.update(100, 0, 0, i * 60000));
  near(h.height, 104.03);
  assert(h.healthy(180000));
  assert(!h.healthy(500000));
  float old = h.height;
  assert(!h.update(NAN, 0, 0, 200000));
  near(h.height, old);
  assert(!h.update(201, 0, 0, 210000));
  for (int i = 4; i <= 6; i++)
    h.update(100, 0, 0, i * 60000);
  old = h.height;
  h.update(190, 0, 0, 420000);
  near(h.height, old);
  HeightEstimator tilted;
  tilted.update(100, 30, 0, 60000);
  near(tilted.height, 100 * cosf(CONTROL_PI / 6) + 4.03f);
  assert(!tilted.update(100, 31, 0, 120000));
  AltitudeController alt;
  Gain gain = {10, 1, 0};
  assert(!alt.enable(110, h, gain, 420000));
  alt.validated = true;
  alt.collective = 1500;
  assert(alt.enable(110, h, gain, 420000));
  near(alt.collective, 1500);
  assert(alt.step(1180, 1, gain, h, 420000, .004));
  assert(alt.requestedTarget > 110);
  float target = alt.requestedTarget;
  alt.step(1180, -1, gain, h, 420000, .004);
  assert(alt.requestedTarget < target);
  float integral = alt.pid.integral;
  assert(!alt.step(1180, 0, gain, h, 900000, .004));
  near(alt.pid.integral, integral);
  for (int i = 8; i < 100; i++) {
    uint32_t t = i * 60000;
    h.update(100, 0, 0, t);
    alt.requestedTarget = 200;
    assert(alt.step(1180, 0, gain, h, t, .06));
    assert(alt.collective <= 1800 && fabsf(alt.correction) <= 400);
  }
  float held = alt.collective;
  alt.manual();
  alt.step(1180, 0, gain, h, 6000000, .004);
  assert(fabsf(alt.collective - held) <= .401);
  puts("Altitude offset, invalid/stale, outlier, tilt, target, saturation, transition PASS");
  assert(newer(0, 0xffffffff));
  assert(!newer(5, 5));
  assert(!newer(4, 5));
  assert(!fresh(400001, 100000, 300000));
  assert(fresh(20, 0xfffffff0, 100));
  StaticJsonDocument<2048> doc;
  float pilot[4] = {}, throttle = 1180;
  assert(
      !deserializeJson(doc, "{\"roll\":0,\"pitch\":1,\"yaw\":-1,\"climb\":0,\"throttle\":1500}"));
  assert(parsePilot(doc, pilot, throttle));
  near(throttle, 1500);
  doc["roll"] = 2;
  assert(!parsePilot(doc, pilot, throttle));
  doc["roll"] = 0;
  doc["throttle"] = "NaN";
  assert(!parsePilot(doc, pilot, throttle));
  doc.clear();
  auto array = doc.createNestedArray("gains");
  for (auto g : defaults) {
    auto row = array.createNestedArray();
    row.add(g.p);
    row.add(g.i);
    row.add(g.d);
  }
  Gain parsed[6];
  assert(parseGains(doc, parsed));
  near(parsed[0].p, 1.5);
  doc["gains"][0][1] = -1;
  assert(!parseGains(doc, parsed));
  near(parsed[0].i, 0);
  assert(deserializeJson(doc, "{broken"));
  puts("UDP parsing, atomic gains, sequence, link deadline PASS");
}
