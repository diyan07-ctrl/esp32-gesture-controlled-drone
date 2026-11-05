#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_system.h>
#include "configuration/config.h"
#include "imu.h"
#include "gesture.h"
#include "transport.h"
#include "calibration.h"
using namespace flight;
Imu imu;
Estimator attitude;
GesturePipeline gestures;
GloveCalibration calibration;
FlexFilter flexFilter[2];
PotFilter potFilter;
float neutralRoll = 0, neutralPitch = 0;
bool sensorOk = false, calibrated = false, neutral = false, radioOk = false, haveAck = false;
uint32_t session = 0, sequence = 0, lastUs = 0, lastTx = 0, lastAck = 0, ackSeq = 0, calStart = 0;
uint8_t droneState = 0;
void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  gestures.config.potentiometerThrottle = true;
  pinMode(cfg::arm, INPUT_PULLUP);
  pinMode(cfg::kill, INPUT_PULLUP);
  pinMode(cfg::led, OUTPUT);
  for (int pin : cfg::flex)
    analogSetPinAttenuation(pin, ADC_11db);
  analogReadResolution(12);
  analogSetPinAttenuation(cfg::throttle, ADC_11db);
  calibration.load();
  WiFi.mode(WIFI_STA); // Make the station MAC available before private pairing exists.
  session = esp_random();
  if (!session)
    session = 1;
  sensorOk = imu.begin();
  radioOk = radio::begin();
  lastUs = micros();
  calStart = millis();
  Serial.printf("Glove MPU6050 init=%d radio=%d sensors-cal=%d MAC=%s\n", sensorOk, radioOk,
                calibration.valid(), WiFi.macAddress().c_str());
}
void loop() {
  uint32_t us = micros(), now = millis();
  if (uint32_t(us - lastUs) < cfg::periodUs)
    return;
  float dt = (us - lastUs) * 1e-6f;
  lastUs = us;
  Sample s;
  bool healthy = sensorOk && imu.sample(s);
  if (!calibrated && healthy && uint32_t(now - calStart) > 100)
    calibrated = imu.calibrate(s);
  if (!calibrated && uint32_t(now - calStart) > 15000)
    sensorOk = false;
  healthy = healthy && calibrated && attitude.update(s, dt);
  uint16_t raw[2] = {uint16_t(analogRead(cfg::flex[0])), uint16_t(analogRead(cfg::flex[1]))};
  uint16_t potRaw = uint16_t(analogRead(cfg::throttle));
  bool flexHealthy = flexFilter[0].update(raw[0], calibration.flex[0], dt);
  flexHealthy = flexFilter[1].update(raw[1], calibration.flex[1], dt) && flexHealthy;
  bool potHealthy = potFilter.update(potRaw, calibration.pot, dt);
  radio::Message m;
  protocol::Status status;
  if (radio::receive(m) && protocol::decodeStatus(m.bytes, m.length, status) &&
      status.session == session && (!haveAck || protocol::newer(status.ack, ackSeq)) &&
      !protocol::newer(status.ack, sequence)) {
    lastAck = m.received;
    ackSeq = status.ack;
    haveAck = true;
    droneState = status.state;
    if (status.state == 6 || status.state == 7) {
      gestures.stopped = true;
      gestures.reset();
    }
  }
  now = millis();
  bool fresh = haveAck && uint32_t(now - lastAck) <= 300;
  calibration.poll(!gestures.armed && droneState != 4 && droneState != 5, raw, potRaw);
  bool enable = !cfg::gloveSwitchesInstalled || digitalRead(cfg::kill) == LOW;
  if (!healthy || !flexHealthy || !potHealthy || !fresh || !enable || calibration.editing) {
    gestures.reset();
    neutral = false;
  }
  if (!neutral && healthy && flexHealthy && potHealthy && potFilter.target <= .02f && fresh &&
      !calibration.editing && !gestures.armed && flexFilter[0].value < .2f &&
      flexFilter[1].value < .2f && std::fabs(attitude.roll) < 8 && std::fabs(attitude.pitch) < 8) {
    neutral = true;
    neutralRoll = attitude.roll;
    neutralPitch = attitude.pitch;
  }
  protocol::Command c;
  c.session = session;
  c.seq = sequence;
  c.flex[0] = raw[0];
  c.flex[1] = raw[1];
  c.orientation[0] = int16_t(clamp(attitude.roll, -180, 180) * 100);
  c.orientation[1] = int16_t(clamp(attitude.pitch, -180, 180) * 100);
  c.orientation[2] = int16_t(clamp(attitude.yaw, -180, 180) * 100);
  bool permit = healthy && flexHealthy && potHealthy && fresh && enable && neutral &&
                calibration.valid() && !calibration.editing;
  if (permit) {
    c.gesture =
        gestures.update(attitude.roll - neutralRoll, attitude.pitch - neutralPitch, s.gz,
                        flexFilter[0].value, flexFilter[1].value, dt, now, potFilter.target);
    gestures.throttle = gestures.armed ? potFilter.value : 0;
    if (droneState != 5)
      gestures.throttle = 0; // Hold zero until the drone acknowledges ARMED.
    if (cfg::gloveSwitchesInstalled && digitalRead(cfg::arm) != LOW) {
      gestures.armed = false;
      gestures.throttle = 0;
    }
    c.flags = protocol::enabled | (gestures.armed ? protocol::arm : 0);
    c.throttle = uint16_t(gestures.throttle * 1000);
    c.roll = int16_t(gestures.roll * 1000);
    c.pitch = int16_t(gestures.pitch * 1000);
    c.yaw = int16_t(gestures.yaw * 1000);
  }
  // Emergency latch is transmitted even when normal command permission is lost.
  if (gestures.stopped) {
    c.flags = protocol::stop;
    c.gesture = protocol::Gesture::Emergency;
    c.throttle = 0;
  }
  if (uint32_t(now - lastTx) >= 20) {
    lastTx = now;
    uint8_t bytes[protocol::commandSize];
    protocol::encode(c, bytes);
    if (radioOk && radio::send(bytes, sizeof(bytes)))
      ++sequence;
  }
  digitalWrite(cfg::led, permit && !gestures.stopped);
  static uint32_t log = 0;
  if (uint32_t(now - log) >= 500) {
    log = now;
    char line[192];
    int n = snprintf(line, sizeof(line), "G,%lu,%d,%d,%d,%d,%u,%u,%u,%u,%.3f,%u\n",
                     (unsigned long)now, healthy, flexHealthy, potHealthy, fresh,
                     unsigned(c.gesture), raw[0], raw[1], potRaw, potFilter.value, c.throttle);
    if (n > 0 && n < int(sizeof(line)) && Serial.availableForWrite() >= n)
      Serial.write((uint8_t *)line, n);
  }
}
