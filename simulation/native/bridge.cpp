#include <chrono>
#include <new>
#include "imu.h"
#include "pilot.h"
#include "stabilizer.h"
#include "safety.h"
#ifdef _WIN32
#define API extern "C" __declspec(dllexport)
#else
#define API extern "C" __attribute__((visibility("default")))
#endif
using namespace flight;
// Only tests/mocks Arduino and Wire are linked. No ESC, serial port or radio API exists here.
struct Context {
  Imu gloveImu, droneImu;
  Estimator gloveAttitude, droneAttitude;
  Pilot pilot;
  FlexCalibration flex[2];
  PotCalibration pot;
  Stabilizer control;
  Safety safety;
  protocol::Receiver receiver;
  bool gloveCal = false, droneCal = false, virtualPermit = false;
  uint32_t sequence = 0, lastTx = 0;
  Pid probePid;
};
static Sample acquire(Imu &imu, const double *v, int fault, bool &ok) {
  Wire.fail = fault == 1;
  Wire.registers[0x3a] = fault == 2 ? 0 : 1;
  bool finite = true;
  const unsigned reg[] = {0x3b, 0x3d, 0x3f, 0x43, 0x45, 0x47};
  const double scale[] = {-4096, 4096, 4096, 65.5, -65.5, -65.5};
  for (unsigned i = 0; i < 6; ++i) {
    finite = finite && std::isfinite(v[i]);
    double count = std::isfinite(v[i]) ? v[i] * scale[i] : 32767;
    Wire.setWord(reg[i], int16_t(std::max(-32768.0, std::min(32767.0, std::round(count)))));
  }
  Sample s;
  ok = imu.sample(s) && finite;
  Wire.fail = false;
  return s;
}
API void *sil_create(const double *endpoints, int virtualPermit) {
  Context *c = new (std::nothrow) Context;
  if (!c)
    return nullptr;
  Wire.fail = false;
  Wire.shortRead = false;
  Wire.corrupt = -1;
  Wire.registers[0x75] = 0x68;
  bool good = c->gloveImu.begin() && c->droneImu.begin();
  c->safety.begin();
  c->safety.initialized(good);
  c->flex[0] = {float(endpoints[0]), float(endpoints[1]), true};
  c->flex[1] = {float(endpoints[2]), float(endpoints[3]), true};
  c->pot = {float(endpoints[4]), float(endpoints[5]), true};
  c->virtualPermit = virtualPermit != 0; // Virtual sink only, never cfg::*Verified.
  return c;
}
API void sil_destroy(void *p) {
  delete static_cast<Context *>(p);
}
API int sil_glove(void *p, const double *s, const double *adc, double dt, uint32_t ms, int fault,
                  int fresh, int droneState, uint8_t *packet, double *out) {
  auto &c = *static_cast<Context *>(p);
  bool good;
  Sample sample = acquire(c.gloveImu, s, fault, good);
  if (!c.gloveCal && good && ms > 100)
    c.gloveCal = c.gloveImu.calibrate(sample);
  good = good && c.gloveCal && c.gloveAttitude.update(sample, float(dt));
  uint16_t raw[2] = {uint16_t(clamp(float(adc[0]), 0, 65535)),
                     uint16_t(clamp(float(adc[1]), 0, 65535))};
  auto command =
      c.pilot.update(c.gloveAttitude, sample, good, raw, uint16_t(clamp(float(adc[2]), 0, 65535)),
                     c.flex, c.pot, false, float(dt), ms, fresh != 0, uint8_t(droneState));
  command.session = 12345;
  command.seq = c.sequence;
  protocol::encode(command, packet);
  out[0] = c.gloveAttitude.roll;
  out[1] = c.gloveAttitude.pitch;
  out[2] = c.gloveAttitude.yaw;
  out[3] = good;
  out[4] = c.gloveCal;
  out[5] = c.pilot.flexHealthy;
  out[6] = c.pilot.potHealthy;
  out[7] = c.pilot.flex[0].value;
  out[8] = c.pilot.flex[1].value;
  out[9] = c.pilot.gestures.bent[0];
  out[10] = c.pilot.gestures.bent[1];
  out[11] = c.pilot.pot.target;
  out[12] = c.pilot.pot.value;
  out[13] = command.throttle / 1000.0;
  out[14] = unsigned(command.gesture);
  out[15] = c.pilot.gestures.armed;
  out[16] = command.roll / 1000.0;
  out[17] = command.pitch / 1000.0;
  out[18] = command.yaw / 1000.0;
  if (uint32_t(ms - c.lastTx) < 20)
    return 0;
  c.lastTx = ms;
  ++c.sequence;
  return 1;
}
API int sil_receive(void *p, const uint8_t *packet, unsigned n, uint32_t arrival) {
  auto &c = *static_cast<Context *>(p);
  return c.receiver.accept(packet, n, arrival, c.safety.armed());
}
API void sil_drone(void *p, const double *s, double dt, uint32_t ms, int fault, double *out) {
  const auto start = std::chrono::steady_clock::now();
  auto &c = *static_cast<Context *>(p);
  bool good;
  Sample sample = acquire(c.droneImu, s, fault, good);
  if (!c.droneCal && good && ms > 100) {
    c.droneCal = c.droneImu.calibrate(sample);
    if (c.droneCal)
      c.safety.calibrated(true);
  }
  bool estimated = c.droneCal && good && c.droneAttitude.update(sample, float(dt));
  Checks checks;
  if (!validDt(float(dt)))
    checks.hardFaults |= Timing;
  if ((c.droneCal && !estimated) || (!c.droneCal && ms > 15000))
    checks.hardFaults |= Sensor;
  const auto &cmd = c.receiver.command;
  checks.sensor = estimated;
  checks.calibrated = c.droneCal;
  checks.fresh = c.receiver.fresh(ms, cfg::timeoutMs);
  checks.enabled = cmd.flags & protocol::enabled;
  checks.arm = cmd.flags & protocol::arm;
  checks.kill = cmd.flags & protocol::stop;
  checks.battery = true;
  checks.lowThrottle = cmd.throttle <= 20;
  checks.hardware = c.virtualPermit;
  c.safety.step(checks, ms);
  float throttle = cmd.throttle / 1000.0f, r = cmd.roll / 1000.0f * cfg::maxTilt;
  float pitch = cmd.pitch / 1000.0f * cfg::maxTilt, y = cmd.yaw / 1000.0f * cfg::maxYaw;
  auto m =
      c.control.update(c.safety.armed(), throttle, r, pitch, y, c.droneAttitude, sample, float(dt));
  if (!m.valid) {
    checks.hardFaults |= Numeric;
    c.safety.step(checks, ms);
  }
  bool active = c.safety.armed() || c.safety.state == State::ARMING_CHECKS;
  out[0] = c.droneAttitude.roll;
  out[1] = c.droneAttitude.pitch;
  out[2] = c.droneAttitude.yaw;
  out[3] = unsigned(c.safety.state);
  out[4] = c.safety.faults;
  out[5] = estimated;
  out[6] = c.droneCal;
  out[7] = c.receiver.have ? uint32_t(ms - c.receiver.received) : 4294967295u;
  out[8] = c.receiver.lost;
  out[9] = c.receiver.rejected;
  out[11] = m.saturated;
  for (int i = 0; i < 4; i++) {
    uint32_t duty = pwmDuty(m.motor[i], active, m.valid);
    out[12 + i] = duty ? m.motor[i] : 0;
    out[16 + i] = duty ? double(duty) * 1000000 / (cfg::pwmHz * ((1u << cfg::pwmBits) - 1)) : 0;
  }
  out[20] = r;
  out[21] = pitch;
  out[22] = y;
  Pid *pid[] = {&c.control.rollPid, &c.control.pitchPid, &c.control.yawPid};
  float rates[] = {sample.gx, sample.gy, sample.gz};
  for (int i = 0; i < 3; i++) {
    out[23 + i] = c.control.rateTarget[i];
    out[26 + i] = pid[i]->kp * (c.control.rateTarget[i] - rates[i]);
    out[29 + i] = pid[i]->integral;
    out[32 + i] = pid[i]->kd * pid[i]->derivative;
    out[35 + i] = c.control.correction[i];
  }
  const float values[] = {sample.ax, sample.ay, sample.az, sample.gx, sample.gy, sample.gz};
  for (int i = 0; i < 6; i++)
    out[38 + i] = values[i];
  out[44] = cmd.seq;
  out[45] = throttle;
  out[46] = dt;
  out[47] = c.droneImu.calibrationProgress();
  out[10] =
      std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
}
API void sil_encode(const double *v, uint32_t seq, uint32_t session, unsigned flags, uint8_t *out) {
  protocol::Command c;
  c.session = session;
  c.seq = seq;
  c.flags = uint8_t(flags);
  c.throttle = uint16_t(v[0]);
  c.roll = int16_t(v[1]);
  c.pitch = int16_t(v[2]);
  c.yaw = int16_t(v[3]);
  protocol::encode(c, out);
}
API void *sil_filter_create(double tau) {
  auto *e = new Estimator;
  e->tau = float(tau);
  return e;
}
API void sil_filter_destroy(void *p) {
  delete static_cast<Estimator *>(p);
}
API int sil_filter(void *p, const double *v, double dt, double *out) {
  Sample s{float(v[0]), float(v[1]), float(v[2]), float(v[3]), float(v[4]), float(v[5])};
  auto &e = *static_cast<Estimator *>(p);
  bool ok = e.update(s, float(dt));
  out[0] = e.roll;
  out[1] = e.pitch;
  out[2] = e.yaw;
  return ok;
}
API void sil_mix(const double *v, double *out) {
  auto m = mix(float(v[0]), float(v[1]), float(v[2]), float(v[3]));
  out[0] = m.valid;
  out[1] = m.saturated;
  for (int i = 0; i < 4; i++) {
    out[2 + i] = m.motor[i];
    out[6 + i] = pwmDuty(m.motor[i], true, m.valid);
  }
}
API void sil_analog(void *p, const double *v, double dt, double *out) {
  auto &c = *static_cast<Context *>(p);
  out[0] = c.pilot.flex[0].update(float(v[0]), c.flex[0], float(dt));
  out[1] = c.pilot.flex[1].update(float(v[1]), c.flex[1], float(dt));
  out[2] = c.pilot.pot.update(float(v[2]), c.pot, float(dt));
  c.pilot.gestures.classify(0, 0, 0, c.pilot.flex[0].value, c.pilot.flex[1].value);
  out[3] = c.pilot.flex[0].value;
  out[4] = c.pilot.flex[1].value;
  out[5] = c.pilot.gestures.bent[0];
  out[6] = c.pilot.gestures.bent[1];
  out[7] = c.pilot.pot.target;
  out[8] = c.pilot.pot.value;
}
API double sil_pid(void *p, double target, double measured, double dt, int freeze, double *out) {
  auto &pid = static_cast<Context *>(p)->probePid;
  double v = pid.update(float(target), float(measured), float(dt), freeze != 0);
  out[0] = pid.integral;
  out[1] = pid.derivative;
  return v;
}
API void sil_config(double *out) {
  double v[] = {cfg::periodUs / 1e6,
                cfg::attitudeTau,
                cfg::flexTau,
                cfg::flexBendOn,
                cfg::flexBendOff,
                cfg::potMinSpan,
                cfg::potMargin,
                cfg::potDeadband,
                cfg::potTau,
                cfg::potRisePerSecond,
                cfg::timeoutMs,
                cfg::pwmHz,
                cfg::pwmMin,
                cfg::pwmMax,
                cfg::maxTilt,
                cfg::maxRate,
                cfg::maxYaw,
                cfg::angleKp,
                cfg::rateKp,
                cfg::rateKi,
                cfg::rateKd,
                cfg::rateLimit,
                cfg::outputsVerified,
                cfg::powerVerified,
                cfg::propulsionVerified,
                cfg::controllerValidated};
  for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++)
    out[i] = v[i];
}
