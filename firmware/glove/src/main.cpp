#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_system.h>
#include "configuration/config.h"
#include "imu.h"
#include "gesture.h"
#include "transport.h"
using namespace flight;
Imu imu;Estimator attitude;Gesture mapping;bool sensorOk=false,calibrated=false,neutral=false,radioOk=false;
uint32_t session=0,sequence=0,lastUs=0,lastTx=0,lastAck=0,ackSeq=0,calStart=0;bool haveAck=false;
void setup(){Serial.begin(115200);pinMode(cfg::arm,INPUT_PULLUP);pinMode(cfg::kill,INPUT_PULLUP);pinMode(cfg::led,OUTPUT);analogSetPinAttenuation(cfg::throttle,ADC_11db);analogSetPinAttenuation(cfg::yaw,ADC_11db);session=esp_random();if(!session)session=1;sensorOk=imu.begin();radioOk=radio::begin();lastUs=micros();calStart=millis();}
void loop(){uint32_t us=micros(),now=millis();if(uint32_t(us-lastUs)<cfg::periodUs){delay(1);return;}float dt=(us-lastUs)*1e-6f;lastUs=us;
 Sample s;bool healthy=sensorOk&&imu.sample(s);
 if(!calibrated&&healthy){calibrated=imu.calibrate(s);}
 if(!calibrated&&uint32_t(now-calStart)>15000)sensorOk=false;
 healthy=healthy&&calibrated&&attitude.update(s,dt);
 if(!healthy){mapping.reset();neutral=false;}
 bool arm=digitalRead(cfg::arm)==LOW,enable=digitalRead(cfg::kill)==LOW;
 float throttle=clamp(analogReadMilliVolts(cfg::throttle)/2500.0f,0,1);
 float yaw=deadband(clamp((analogReadMilliVolts(cfg::yaw)-1250.0f)/1250.0f,-1,1));
 // Recenter only with enable and arm released and throttle physically at zero.
 if(healthy&&!arm&&!enable&&throttle<0.02f){mapping.center(attitude.roll,attitude.pitch);neutral=true;}
 radio::Message m;protocol::Status status;
 if(radio::receive(m)&&protocol::decodeStatus(m.bytes,24,status)&&status.session==session&&(!haveAck||protocol::newer(status.ack,ackSeq))){lastAck=m.received;ackSeq=status.ack;haveAck=true;}
 now=millis();
 bool fresh=haveAck&&uint32_t(now-lastAck)<=300;
 bool permit=healthy&&neutral&&mapping.permit(fresh,enable,arm,throttle);
 protocol::Command c;c.session=session;c.seq=sequence++;
 if(permit){c.flags=protocol::enabled|(arm?protocol::arm:0);c.throttle=uint16_t(throttle*1000);}
 // Mapping is calculated only after all interlocks pass.
 if(permit){c.roll=int16_t(flight::gesture(attitude.roll,mapping.neutralRoll)*1000);c.pitch=int16_t(flight::gesture(attitude.pitch,mapping.neutralPitch)*1000);c.yaw=int16_t(yaw*1000);}
 if(uint32_t(now-lastTx)>=20){lastTx=now;uint8_t bytes[24];protocol::encode(c,bytes);if(radioOk)radio::send(bytes);}
 digitalWrite(cfg::led,permit);static uint32_t log=0;if(uint32_t(now-log)>=500){log=now;if(Serial.availableForWrite()>64)Serial.printf("G,%lu,%d,%d,%d,%u,%ld\n",(unsigned long)now,healthy,fresh,permit,c.throttle,(long)(haveAck?now-lastAck:-1));}
}
