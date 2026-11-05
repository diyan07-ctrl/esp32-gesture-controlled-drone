#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "configuration/config.h"
#include "bno055.h"
#include "stabilizer.h"
#include "protocol/shadow_telemetry.h"
#if __has_include("local_wifi.h")
#include "local_wifi.h"
#else
namespace local_wifi {
constexpr bool configured = false;
constexpr char ssid[] = "";
constexpr char password[] = "";
constexpr char destination[] = "";
constexpr uint16_t port = 14550;
}
#endif

// Dedicated diagnostic image: no esc.h, LEDC, actuator task or arming path.
static_assert(!cfg::outputsVerified && !cfg::powerVerified &&
              !cfg::propulsionVerified && !cfg::controllerValidated,
              "PID shadow image requires every flight-output gate disabled");
static_assert(cfg::periodUs == 4000, "Native PID timing contract is 4 ms");
enum Mode : uint8_t { Sensor = 0, Shadow = 1, MotorLocked = 2 };
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
flight::Bno055 imu;
WiFiUDP udp;
IPAddress destination;
flight::BnoSample sharedSensor{};
shadow_wire::Packet sharedPacket{};
uint32_t sensorStamp = 0;
bool sensorHealthy = false;
bool startupOk = false;
Mode requestedMode = Sensor;
float requestedRoll = 0, requestedPitch = 0;
uint32_t missedControl = 0, invalidControl = 0;

void sensorTask(void *) {
  TickType_t next = xTaskGetTickCount();
  for (;;) {
    flight::BnoSample candidate;
    bool ok = imu.sample(candidate);
    portENTER_CRITICAL(&lock);
    sensorHealthy = ok;
    if (ok) { sharedSensor = candidate; sensorStamp = millis(); }
    portEXIT_CRITICAL(&lock);
    vTaskDelayUntil(&next, pdMS_TO_TICKS(20));
  }
}

void controlTask(void *) {
  flight::Stabilizer controller;
  uint32_t previous = micros();
  TickType_t next = xTaskGetTickCount();
  uint32_t sequence = 0;
  for (;;) {
    uint32_t current = micros();
    uint32_t interval = current - previous;
    previous = current;
    flight::BnoSample sensor;
    uint32_t stamp;
    bool healthy;
    Mode mode;
    float targetRoll, targetPitch;
    portENTER_CRITICAL(&lock);
    sensor = sharedSensor; stamp = sensorStamp; healthy = sensorHealthy;
    mode = requestedMode; targetRoll = requestedRoll; targetPitch = requestedPitch;
    portEXIT_CRITICAL(&lock);
    bool valid = healthy && stamp != 0 && uint32_t(millis() - stamp) <= 60 &&
                 sensor.calibrated() && flight::validDt(interval * 1e-6f) &&
                 std::fabs(sensor.roll) <= 60 && std::fabs(sensor.pitch) <= 60;
    if (!flight::validDt(interval * 1e-6f)) ++missedControl;
    if (!valid) ++invalidControl;
    flight::Estimator orientation;
    orientation.initialized = valid;
    orientation.roll = sensor.roll; orientation.pitch = sensor.pitch;
    orientation.yaw = sensor.yaw;
    flight::Sample rates;
    rates.ax = sensor.accel[0]; rates.ay = sensor.accel[1]; rates.az = sensor.accel[2];
    rates.gx = sensor.gyro[0]; rates.gy = sensor.gyro[1]; rates.gz = sensor.gyro[2];
    // 'armed' is true only inside the pure Stabilizer computation; this image
    // has no ESC object and has no route to a physical output.
    bool compute = mode == Shadow && valid;
    flight::Mix calculated = controller.update(compute, .35f,
      targetRoll, targetPitch, 0, orientation, rates, interval * 1e-6f);
    shadow_wire::Packet packet;
    packet.mode = mode; packet.sequence = sequence++;
    packet.uptimeMs = millis(); packet.dtUs = interval;
    packet.imuErrors = imu.errors; packet.calibration = sensor.calibration;
    packet.sensorValid = valid;
    const flight::Pid *pids[] = {&controller.rollPid, &controller.pitchPid,
                                  &controller.yawPid};
    const float desired[] = {targetRoll, targetPitch, 0};
    const float measured[] = {sensor.roll, sensor.pitch, sensor.yaw};
    const float measuredRate[] = {rates.gx, rates.gy, rates.gz};
    for (unsigned a = 0; a < 3; ++a) {
      auto &axis = packet.axis[a];
      axis.setpoint = a == 2 ? 0 : desired[a];
      axis.measured = measured[a];
      axis.error = a == 2 ? -measuredRate[a] : desired[a] - measured[a];
      axis.p = pids[a]->pTerm; axis.i = pids[a]->iTerm;
      axis.d = pids[a]->dTerm; axis.output = pids[a]->output;
      axis.rateTarget = controller.rateTarget[a]; axis.measuredRate = measuredRate[a];
    }
    for (unsigned i = 0; i < 4; ++i) {
      packet.motor[i] = compute && calculated.valid ? calculated.motor[i] : 0;
      packet.quaternion[i] = sensor.quaternion[i];
    }
    for (unsigned i = 0; i < 3; ++i) {
      packet.accel[i] = sensor.accel[i]; packet.gyro[i] = sensor.gyro[i];
    }
    portENTER_CRITICAL(&lock);
    sharedPacket = packet;
    portEXIT_CRITICAL(&lock);
    vTaskDelayUntil(&next, pdMS_TO_TICKS(4));
  }
}

void setup() {
  Serial.begin(115200);
  for (int pin : cfg::motors) pinMode(pin, INPUT);
  delay(100);
  Serial.println("PID_BENCH,outputs=DISABLED,motor_mode=LOCKED,GPIO25/26/32/33=INPUT");
  bool found = imu.begin();
  Serial.printf("BNO055_INIT,ok=%d,address=0x%02X,identity=0x%02X\n",
                found, imu.detectedAddress, imu.identity);
  if (!found) {
    Serial.println("SENSOR_FAULT,halted; no control task started");
    return;
  }
  startupOk = true;
  WiFi.mode(WIFI_STA); WiFi.setSleep(false);
  destination.fromString(local_wifi::destination);
  if (local_wifi::configured) WiFi.begin(local_wifi::ssid, local_wifi::password);
  xTaskCreatePinnedToCore(sensorTask, "bno_sensor", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(controlTask, "pid_shadow", 4096, nullptr, 3, nullptr, 1);
  Serial.println("COMMANDS,s=shadow,n=sensor,r/l=roll+/-5,f/b=pitch+/-5,c=neutral,m=motor-locked,x=stop");
}

void loop() {
  static uint32_t lastPrint = 0, lastUdp = 0, udpSequence = 0;
  if (!startupOk) { delay(100); return; }
  while (Serial.available()) {
    char c = Serial.read();
    portENTER_CRITICAL(&lock);
    if (c == 's') requestedMode = Shadow;
    if (c == 'n' || c == 'x') { requestedMode = Sensor; requestedRoll = requestedPitch = 0; }
    if (c == 'm') requestedMode = MotorLocked;
    if (c == 'r') requestedRoll = 5;
    if (c == 'l') requestedRoll = -5;
    if (c == 'f') requestedPitch = 5;
    if (c == 'b') requestedPitch = -5;
    if (c == 'c') requestedRoll = requestedPitch = 0;
    portEXIT_CRITICAL(&lock);
    Serial.printf("COMMAND,mode=%u,roll=%.1f,pitch=%.1f,outputs=DISABLED\n",
                  unsigned(requestedMode), requestedRoll, requestedPitch);
  }
  uint32_t now = millis();
  if (uint32_t(now - lastPrint) >= 200 || uint32_t(now - lastUdp) >= 20) {
    shadow_wire::Packet packet;
    portENTER_CRITICAL(&lock);
    packet = sharedPacket;
    portEXIT_CRITICAL(&lock);
    if (uint32_t(now - lastPrint) >= 200) {
      lastPrint = now;
      Serial.printf("SHADOW,%lu,%u,%u,roll=%.2f,pitch=%.2f,rP=%.5f,rI=%.5f,rD=%.5f,rOut=%.5f,m=%.3f|%.3f|%.3f|%.3f,dt=%lu,imuErrors=%lu,missed=%lu,outputs=0\n",
        (unsigned long)now, unsigned(packet.mode), unsigned(packet.sensorValid),
        packet.axis[0].measured, packet.axis[1].measured,
        packet.axis[0].p, packet.axis[0].i, packet.axis[0].d, packet.axis[0].output,
        packet.motor[0], packet.motor[1], packet.motor[2], packet.motor[3],
        (unsigned long)packet.dtUs, (unsigned long)packet.imuErrors,
        (unsigned long)missedControl);
    }
    if (uint32_t(now - lastUdp) >= 20) {
      lastUdp = now;
      packet.sequence = udpSequence++;
      uint8_t bytes[shadow_wire::size];
      shadow_wire::encode(packet, bytes);
      if (WiFi.status() == WL_CONNECTED && destination != IPAddress() &&
          udp.beginPacket(destination, local_wifi::port)) {
        udp.write(bytes, sizeof(bytes)); udp.endPacket();
      }
    }
  }
  delay(1);
}
