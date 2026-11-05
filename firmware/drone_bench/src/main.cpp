#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "configuration/config.h"
#include "bno055.h"
#include "transport.h"
#include "bench_state.h"
#include "safety.h"
#include "protocol/telemetry.h"
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

// This image has no ESC header, motor mixer, PID or actuator output calls.
static_assert(!cfg::outputsVerified && !cfg::powerVerified &&
              !cfg::propulsionVerified && !cfg::controllerValidated,
              "The bench image must not be built with flight output gates enabled");
using namespace flight;
Bno055 imu;
protocol::Receiver receiver;
WiFiUDP udp;
IPAddress destination;
bench::State state = bench::State::Boot;
bool imuInitialized = false, calibrated = false, sampleValid = false;
bool radioAttempted = false, wifiPrevious = false;
uint32_t started = 0, previousUs = 0, lastSerial = 0, lastUdp = 0, lastStatus = 0;
uint32_t lastWifiRetry = 0, missed = 0, maximumUs = 0, accepted = 0, wifiReconnects = 0;
uint32_t telemetrySequence = 0, udpErrors = 0, lastInterval = 0, lastExecution = 0;
uint32_t invalidStreak = 0;
BnoSample latest{};
constexpr uint32_t benchPeriodUs = 20000; // 50 Hz sensor/telemetry bench target, not flight-loop timing.

void transition(bench::State next, uint32_t now) {
  if (next != state) {
    Serial.printf("STATE,%lu,%s,%s\n", (unsigned long)now,
                  bench::name(state), bench::name(next));
    state = next;
  }
}

void scanI2c() {
  Wire.begin(cfg::sda, cfg::scl, 100000);
  Wire.setTimeOut(2);
  unsigned found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C_FOUND,0x%02X\n", address);
      ++found;
    }
  }
  Serial.printf("I2C_SCAN,count=%u,BNO055_candidates=0x28|0x29,SDA=%d,SCL=%d\n",
                found, cfg::sda, cfg::scl);
}

void checkWifi(uint32_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiPrevious) {
    wifiPrevious = connected;
    Serial.printf("WIFI,%lu,%s,ip=%s,channel=%u,required=%u\n",
                  (unsigned long)now, connected ? "CONNECTED" : "DISCONNECTED",
                  connected ? WiFi.localIP().toString().c_str() : "unavailable",
                  connected ? WiFi.channel() : 0, cfg::radioChannel);
    if (connected && WiFi.channel() != cfg::radioChannel)
      Serial.println("WARN,Wi-Fi channel conflicts with ESP-NOW; commands rejected");
  }
  if (local_wifi::configured && !connected && uint32_t(now - lastWifiRetry) >= 10000) {
    lastWifiRetry = now;
    ++wifiReconnects;
    WiFi.reconnect();
    Serial.printf("WIFI_RETRY,%lu,count=%lu\n", (unsigned long)now,
                  (unsigned long)wifiReconnects);
  }
}

void checkRadio(uint32_t now) {
  // With STA credentials, let the AP choose the channel before initializing ESP-NOW.
  if (!radioAttempted && (!local_wifi::configured || WiFi.status() == WL_CONNECTED)) {
    radioAttempted = true;
    bool paired = radio::begin();
    Serial.printf("ESPNOW,%lu,initialized=%d,paired=%d,channel_ok=%d,peer_configured=%d\n",
                  (unsigned long)now, radio::initialized, paired,
                  radio::channelCompatible(), pairing::configured);
  }
}

void publish(uint32_t now, bool estimateValid) {
  telemetry::Snapshot t;
  t.flags = (imuInitialized ? telemetry::ImuInitialized : 0) |
            (calibrated ? telemetry::Calibrated : 0) |
            (estimateValid ? telemetry::EstimateValid : 0) |
            (sampleValid ? telemetry::SensorValid : 0) | telemetry::Bno055 |
            (radio::initialized ? telemetry::EspNowInitialized : 0) |
            (pairing::configured && radio::ready ? telemetry::PeerConfigured : 0) |
            (WiFi.status() == WL_CONNECTED ? telemetry::WifiConnected : 0) |
            (receiver.fresh(now, cfg::timeoutMs) ? telemetry::LinkFresh : 0);
  // OutputsEnabled is deliberately never set in this image.
  t.state = uint8_t(state);
  t.channel = WiFi.status() == WL_CONNECTED ? WiFi.channel() : cfg::radioChannel;
  t.sequence = telemetrySequence++;
  t.uptimeMs = now;
  t.dtUs = lastInterval; t.executionUs = lastExecution; t.maximumUs = maximumUs;
  t.missed = missed; t.imuErrors = imu.errors;
  t.accepted = accepted; t.rejected = receiver.rejected; t.lost = receiver.lost;
  t.mailboxDropped = radio::dropped.load(); t.sendErrors = radio::sendErrors.load();
  t.commandSequence = receiver.have ? receiver.command.seq : 0;
  t.commandAgeMs = receiver.have ? now - receiver.received : 0xffffffffu;
  t.wifiReconnects = wifiReconnects;
  if (receiver.have) {
    t.gesture = uint8_t(receiver.command.gesture);
    t.commandFlags = receiver.command.flags;
    t.throttle = receiver.command.throttle;
    t.rollCommand = receiver.command.roll;
    t.pitchCommand = receiver.command.pitch;
    t.yawCommand = receiver.command.yaw;
    t.flex[0] = receiver.command.flex[0]; t.flex[1] = receiver.command.flex[1];
  }
  if (sampleValid) {
    for (int i = 0; i < 3; ++i) {
      t.accel[i] = latest.accel[i]; t.gyro[i] = latest.gyro[i];
    }
    t.yawRate = latest.gyro[2];
    t.calibration = latest.calibration;
    t.systemStatus = latest.systemStatus; t.systemError = latest.systemError;
  }
  if (estimateValid) {
    t.roll = latest.roll; t.pitch = latest.pitch;
    for (int i = 0; i < 4; ++i) t.quaternion[i] = latest.quaternion[i];
  }
  uint8_t bytes[telemetry::size];
  telemetry::encode(t, bytes);
  if (WiFi.status() == WL_CONNECTED && destination != IPAddress() &&
      udp.beginPacket(destination, local_wifi::port)) {
    if (udp.write(bytes, sizeof(bytes)) != sizeof(bytes) || !udp.endPacket()) ++udpErrors;
  } else if (WiFi.status() == WL_CONNECTED) ++udpErrors;
}

void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  for (int pin : cfg::motors) pinMode(pin, INPUT);
  WiFi.mode(WIFI_STA);
  delay(100); // USB boot message only; there is no flight loop or ESC output.
  Serial.printf("DRONE_BENCH,firmware=1,MAC=%s,outputs=DISABLED,GPIO25/26/32/33=INPUT\n",
                WiFi.macAddress().c_str());
  transition(bench::State::ImuInitialization, millis());
  scanI2c();
  imuInitialized = imu.begin();
  Serial.printf("BNO055_INIT,ok=%d,CHIP_ID=0x%02X,expected=0xA0,address=0x%02X,failed_register=0x%02X,errors=%lu,mode=IMU_6DOF\n",
                imuInitialized, imu.identity, imu.detectedAddress,
                imu.failedRegister, (unsigned long)imu.errors);
  transition(imuInitialized ? bench::State::ImuCalibration : bench::State::SensorFault,
             millis());
  WiFi.setSleep(false);
  destination.fromString(local_wifi::destination);
  if (local_wifi::configured) {
    WiFi.begin(local_wifi::ssid, local_wifi::password);
    Serial.printf("WIFI_CONFIG,ssid=%s,destination=%s,port=%u,required_channel=%u\n",
                  local_wifi::ssid, local_wifi::destination, local_wifi::port,
                  cfg::radioChannel);
  } else Serial.println("WIFI_CONFIG,unprovisioned; local_wifi.h missing or disabled");
  started = millis(); previousUs = micros();
  Serial.println("DIAG,ms,state,sensor_valid,calibrated,cal_byte,imu_errors,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,roll_sensor,pitch_sensor,yaw_rate,dt_us,max_us,missed,espnow_init,peer,channel_ok,accepted,rejected,lost,age_ms,wifi,udp_errors,outputs");
}

void loop() {
  uint32_t now = millis();
  checkWifi(now);
  checkRadio(now);
  uint32_t begin = micros(), interval = begin - previousUs;
  if (interval < benchPeriodUs) { delay(1); return; }
  previousUs = begin; lastInterval = interval;
  if (interval < benchPeriodUs / 2 || interval > benchPeriodUs * 2) ++missed;
  BnoSample sample;
  sampleValid = imuInitialized && imu.sample(sample);
  invalidStreak = sampleValid ? 0 : invalidStreak + 1;
  if (sampleValid) latest = sample;
  calibrated = sampleValid && latest.calibrated();
  bool estimateValid = calibrated && sampleValid;
  if (radio::ready && radio::channelCompatible()) {
    radio::Message message;
    if (radio::receive(message)) {
      bool ok = receiver.accept(message.bytes, message.length, message.received, false);
      if (ok) ++accepted;
      if (ok && uint32_t(now - lastSerial) >= 200)
        Serial.printf("COMMAND,seq=%lu,throttle=%u,roll=%d,pitch=%d,gesture=%u,age_ms=%lu\n",
                      (unsigned long)receiver.command.seq, receiver.command.throttle,
                      receiver.command.roll, receiver.command.pitch,
                      unsigned(receiver.command.gesture), (unsigned long)(now - message.received));
    }
  }
  bench::Inputs inputs;
  inputs.imuInitialized = imuInitialized && invalidStreak < 3;
  inputs.calibrated = calibrated; inputs.sampleValid = estimateValid;
  inputs.radioInitialized = radio::initialized;
  inputs.paired = radio::ready;
  inputs.channelCompatible = radio::channelCompatible();
  inputs.haveCommand = receiver.have;
  inputs.commandFresh = receiver.fresh(now, cfg::timeoutMs);
  transition(bench::select(inputs), now);
  if (radio::ready && receiver.have && uint32_t(now - lastStatus) >= 100) {
    lastStatus = now;
    protocol::Status status;
    status.session = receiver.command.session;
    status.ack = receiver.command.seq;
    status.state = estimateValid ? 3 : 7; // Glove never sees ARMED from bench image.
    status.faults = estimateValid ? 0 : flight::Sensor;
    status.age = uint16_t(std::min(now - receiver.received, 65535u));
    status.rejected = receiver.rejected;
    uint8_t bytes[protocol::statusSize];
    protocol::encodeStatus(status, bytes);
    radio::send(bytes, sizeof(bytes));
  }
  if (uint32_t(now - lastUdp) >= 20) { lastUdp = now; publish(now, estimateValid); }
  if (uint32_t(now - lastSerial) >= 200) {
    lastSerial = now;
    Serial.printf("DIAG,%lu,%s,%d,%d,0x%02X,%lu,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.2f,%.2f,%.3f,%lu,%lu,%lu,%d,%d,%d,%lu,%lu,%lu,%lu,%d,%lu,0\n",
      (unsigned long)now, bench::name(state), sampleValid, calibrated,
      latest.calibration, (unsigned long)imu.errors,
      latest.accel[0], latest.accel[1], latest.accel[2],
      latest.gyro[0], latest.gyro[1], latest.gyro[2],
      latest.roll, latest.pitch, latest.gyro[2],
      (unsigned long)lastInterval, (unsigned long)maximumUs, (unsigned long)missed,
      radio::initialized, radio::ready, radio::channelCompatible(),
      (unsigned long)accepted, (unsigned long)receiver.rejected,
      (unsigned long)receiver.lost,
      (unsigned long)(receiver.have ? now - receiver.received : 0xffffffffu),
      WiFi.status() == WL_CONNECTED, (unsigned long)udpErrors);
  }
  lastExecution = micros() - begin;
  if (lastExecution > maximumUs) maximumUs = lastExecution;
  if (lastExecution >= benchPeriodUs) ++missed;
}
