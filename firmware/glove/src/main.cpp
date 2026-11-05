#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_system.h>
#include "configuration/config.h"
#include "imu.h"
#include "pilot.h"
#include "transport.h"
#include "calibration.h"
using namespace flight;
Imu imu;
Estimator attitude;
Pilot pilot;
GloveCalibration calibration;
bool sensorOk = false, calibrated = false, radioOk = false, haveAck = false;
uint32_t session = 0, sequence = 0, lastUs = 0, lastTx = 0, lastAck = 0, ackSeq = 0, calStart = 0;
uint8_t droneState = 0;
void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  pinMode(cfg::arm, INPUT_PULLUP);
  pinMode(cfg::kill, INPUT_PULLUP);
  pinMode(cfg::led, OUTPUT);
  for (int pin : cfg::flex)
    analogSetPinAttenuation(pin, ADC_11db);
  analogReadResolution(12);
  analogSetPinAttenuation(cfg::throttle, ADC_11db);
  calibration.load();
  WiFi.mode(WIFI_STA);
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
  Sample sample;
  bool healthy = sensorOk && imu.sample(sample);
  if (!calibrated && healthy && uint32_t(now - calStart) > 100)
    calibrated = imu.calibrate(sample);
  if (!calibrated && uint32_t(now - calStart) > 15000)
    sensorOk = false;
  healthy = healthy && calibrated && attitude.update(sample, dt);
  uint16_t raw[2] = {uint16_t(analogRead(cfg::flex[0])), uint16_t(analogRead(cfg::flex[1]))};
  uint16_t potRaw = uint16_t(analogRead(cfg::throttle));
  radio::Message message;
  protocol::Status status;
  if (radio::receive(message) && protocol::decodeStatus(message.bytes, message.length, status) &&
      status.session == session && (!haveAck || protocol::newer(status.ack, ackSeq)) &&
      !protocol::newer(status.ack, sequence)) {
    lastAck = message.received;
    ackSeq = status.ack;
    haveAck = true;
    droneState = status.state;
  }
  now = millis();
  bool fresh = haveAck && uint32_t(now - lastAck) <= 300;
  calibration.poll(!pilot.gestures.armed && droneState != 4 && droneState != 5, raw, potRaw);
  bool enable = !cfg::gloveSwitchesInstalled || digitalRead(cfg::kill) == LOW;
  bool armSwitch = !cfg::gloveSwitchesInstalled || digitalRead(cfg::arm) == LOW;
  auto command =
      pilot.update(attitude, sample, healthy, raw, potRaw, calibration.flex, calibration.pot,
                   calibration.editing, dt, now, fresh, droneState, enable, armSwitch);
  command.session = session;
  command.seq = sequence;
  if (uint32_t(now - lastTx) >= 20) {
    lastTx = now;
    uint8_t bytes[protocol::commandSize];
    protocol::encode(command, bytes);
    if (radioOk && radio::send(bytes, sizeof(bytes)))
      ++sequence;
  }
  digitalWrite(cfg::led, pilot.permit && !pilot.gestures.stopped);
  static uint32_t log = 0;
  if (uint32_t(now - log) >= 500) {
    log = now;
    char line[192];
    int n =
        snprintf(line, sizeof(line), "G,%lu,%d,%d,%d,%d,%u,%u,%u,%u,%.3f,%u\n", (unsigned long)now,
                 healthy, pilot.flexHealthy, pilot.potHealthy, fresh, unsigned(command.gesture),
                 raw[0], raw[1], potRaw, pilot.pot.value, command.throttle);
    if (n > 0 && n < int(sizeof(line)) && Serial.availableForWrite() >= n)
      Serial.write((uint8_t *)line, n);
  }
}
