#include <Arduino.h>
#include "imu.h"
#include "gesture.h"
#include "../../glove/src/calibration.h"
// Independent sensor image: no radio, ESC initialization, motor mixing or PID execution.
using namespace flight;
Imu imu;
Estimator attitude;
#ifdef SENSOR_GLOVE
GloveCalibration calibration;
FlexFilter flex[2];
PotFilter pot;
GesturePipeline fingers;
#endif
bool initialized = false, calibrated = false;
uint32_t previous = 0, started = 0, lastLog = 0, misses = 0;
void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  // Explicitly leave all ESC signal GPIOs high-impedance on this bench image.
  for (int pin : cfg::motors)
    pinMode(pin, INPUT);
#ifdef SENSOR_GLOVE
  analogReadResolution(12);
  for (int pin : cfg::flex)
    analogSetPinAttenuation(pin, ADC_11db);
  analogSetPinAttenuation(cfg::throttle, ADC_11db);
  calibration.load();
  fingers.config.potentiometerThrottle = true;
#endif
  initialized = imu.begin();
  previous = micros();
  started = millis();
  Serial.printf("SENSOR ONLY MPU6050 init=%d who=0x%02x; no ESC/radio; o,b,l,h,s,x calibration\n",
                initialized, imu.identity);
  Serial.println("S,ms,read,cal,estimate,cal_count,errors,dt_us,misses,raw_ax,raw_ay,raw_az,"
                 "raw_gx,raw_gy,raw_gz,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,roll,pitch,yaw");
}
void loop() {
  uint32_t us = micros(), now = millis();
  uint32_t interval = us - previous;
  if (interval < cfg::periodUs)
    return;
  previous = us;
  float dt = interval * 1e-6f;
  if (!validDt(dt))
    ++misses;
  Sample sample;
  bool read = initialized && imu.sample(sample);
  if (!calibrated && read && uint32_t(now - started) > 100)
    calibrated = imu.calibrate(sample);
  if (!calibrated && uint32_t(now - started) > 15000)
    initialized = false; // Reboot after inspecting a failed startup calibration.
  bool estimated = read && calibrated && attitude.update(sample, dt);
#ifdef SENSOR_GLOVE
  uint16_t raw[2] = {uint16_t(analogRead(cfg::flex[0])), uint16_t(analogRead(cfg::flex[1]))};
  uint16_t potRaw = uint16_t(analogRead(cfg::throttle));
  calibration.poll(true, raw, potRaw); // This image has no command or actuator authority.
  bool f0 = flex[0].update(raw[0], calibration.flex[0], dt);
  bool f1 = flex[1].update(raw[1], calibration.flex[1], dt);
  bool p = pot.update(potRaw, calibration.pot, dt);
  if (f0 && f1)
    fingers.classify(0, 0, 0, flex[0].value, flex[1].value);
#endif
  if (uint32_t(now - lastLog) < 1000 / cfg::logHz)
    return;
  lastLog = now;
  char line[512];
  int n = snprintf(line, sizeof(line),
                   "S,%lu,%d,%d,%d,%u,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d,"
                   "%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f\n",
                   (unsigned long)now, read, calibrated, estimated, imu.calibrationProgress(),
                   (unsigned long)imu.errors, (unsigned long)interval, (unsigned long)misses,
                   imu.raw.ax, imu.raw.ay, imu.raw.az, imu.raw.gx, imu.raw.gy, imu.raw.gz,
                   sample.ax, sample.ay, sample.az, sample.gx, sample.gy, sample.gz, attitude.roll,
                   attitude.pitch, attitude.yaw);
  if (n > 0 && n < int(sizeof(line)) && Serial.availableForWrite() >= n)
    Serial.write((uint8_t *)line, n);
#ifdef SENSOR_GLOVE
  n = snprintf(line, sizeof(line), "A,%u,%u,%u,%d,%d,%d,%.3f,%.3f,%d,%d,%.3f,%.3f,%d,%d\n", raw[0],
               raw[1], potRaw, f0, f1, p, flex[0].value, flex[1].value, fingers.bent[0],
               fingers.bent[1], pot.value, pot.target, calibration.valid(), calibration.editing);
  if (n > 0 && n < int(sizeof(line)) && Serial.availableForWrite() >= n)
    Serial.write((uint8_t *)line, n);
#endif
}
