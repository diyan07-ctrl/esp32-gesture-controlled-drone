#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "configuration/config.h"
#include "imu.h"
#include "esc.h"
#include "transport.h"
#include "safety.h"
using namespace flight;
struct Diagnostic{uint32_t ms,age,execUs,misses,rejected,lost;uint16_t faults;uint8_t state;float v;Sample imu;float roll,pitch,yaw;float command[4],pid[3],motor[4];};
QueueHandle_t diagnostics;
void controlTask(void*){
 Imu imu;Esc esc;Safety safety;Estimator attitude;Pid rp,pp,yp;protocol::Receiver rx;
 esc.safeBoot();safety.begin();safety.initialized(imu.begin());bool calibrated=false,initialized=safety.state==State::CALIBRATION;
 bool radioOk=radio::begin();uint32_t calStart=millis(),previous=micros(),misses=0,lastStatus=0,lastDiag=0;
 bool saturated=false;uint16_t watchdogFault=0;
 if(esp_task_wdt_init(1,true)!=ESP_OK||esp_task_wdt_add(nullptr)!=ESP_OK)watchdogFault=Setup;
 TickType_t wake=xTaskGetTickCount();
 for(;;){
  vTaskDelayUntil(&wake,pdMS_TO_TICKS(4));uint32_t begin=micros(),now=millis(),interval=begin-previous;previous=begin;
  Checks c;c.hardFaults=watchdogFault;float dt=interval*1e-6f;
  if(interval<2000||interval>6000){c.hardFaults|=Timing;++misses;}
  Sample sample;bool sensor=initialized&&imu.sample(sample);
  if(!calibrated&&sensor){calibrated=imu.calibrate(sample);if(calibrated)safety.calibrated(true);}
  if(!calibrated&&uint32_t(now-calStart)>15000)c.hardFaults|=Sensor;
  bool estimated=calibrated&&sensor&&attitude.update(sample,dt);
  if(calibrated&&!estimated)c.hardFaults|=Sensor;
  radio::Message incoming;
  if(radio::receive(incoming))rx.accept(incoming.bytes,24,incoming.received,safety.armed());
  // A callback may arrive during I2C work; compare its timestamp to a later clock read.
  now=millis();
  uint32_t age=rx.have?now-rx.received:0xffffffffu;
  float volts=batteryVoltage(analogReadMilliVolts(cfg::battery),cfg::dividerRatio,cfg::batteryScale);
  // A divider plausibility check cannot detect every stuck-but-plausible ADC failure.
  c.sensor=estimated;c.calibrated=calibrated;c.fresh=rx.fresh(now,cfg::timeoutMs);
  c.enabled=rx.command.flags&protocol::enabled;c.arm=rx.command.flags&protocol::arm;
  c.kill=digitalRead(cfg::kill)==HIGH||(rx.command.flags&protocol::stop);
  c.battery=batteryValid(volts)&&volts>=cfg::lowBattery;c.lowThrottle=rx.command.throttle<=20;
  c.hardware=cfg::outputsVerified&&cfg::powerVerified&&cfg::propulsionVerified&&cfg::controllerValidated&&radioOk;
  if(safety.armed()&&(!batteryValid(volts)||volts<cfg::criticalBattery))c.hardFaults|=Battery;
  safety.step(c,now);
  float throttle=rx.command.throttle/1000.0f;
  float rcmd=rx.command.roll/1000.0f*cfg::maxTilt,pcmd=rx.command.pitch/1000.0f*cfg::maxTilt,ycmd=rx.command.yaw/1000.0f*cfg::maxYaw;
  Mix motors;float r=0,p=0,y=0;
  if(safety.armed()&&throttle>0.02f){
   r=rp.update(clamp(cfg::angleKp*(rcmd-attitude.roll),-cfg::maxRate,cfg::maxRate),sample.gx,dt,saturated);
   p=pp.update(clamp(cfg::angleKp*(pcmd-attitude.pitch),-cfg::maxRate,cfg::maxRate),sample.gy,dt,saturated);
   y=yp.update(ycmd,sample.gz,dt,saturated);motors=mix(throttle,r,p,y);
   if(!motors.valid){c.hardFaults|=Numeric;safety.step(c,now);}
  }else{rp.reset();pp.reset();yp.reset();motors=mix(0,0,0,0);}
  saturated=motors.saturated;
  // Attach only on an explicit successful arming transition after all checks exist.
  static bool attached=false;if(safety.armed()&&!attached){attached=esc.attach();if(!attached){c.hardFaults|=Setup;safety.step(c,now);}}
  if(uint32_t(micros()-begin)>=cfg::periodUs){++misses;c.hardFaults|=Timing;safety.step(c,now);}
  esc.write(motors,safety.armed());
  if(uint32_t(now-lastStatus)>=100){lastStatus=now;protocol::Status s;s.session=rx.command.session;s.ack=rx.command.seq;s.state=uint8_t(safety.state);s.faults=safety.faults;s.millivolts=uint16_t(clamp(volts*1000,0,65535));s.age=uint16_t(std::min(age,65535u));s.rejected=rx.rejected;uint8_t bytes[24];protocol::encodeStatus(s,bytes);radio::send(bytes);}
  if(uint32_t(now-lastDiag)>=1000/cfg::logHz){lastDiag=now;Diagnostic d{};d.ms=now;d.age=age;d.execUs=micros()-begin;d.misses=misses;d.rejected=rx.rejected;d.lost=rx.lost;d.faults=safety.faults;d.state=uint8_t(safety.state);d.v=volts;d.imu=sample;d.roll=attitude.roll;d.pitch=attitude.pitch;d.yaw=attitude.yaw;d.command[0]=throttle;d.command[1]=rcmd;d.command[2]=pcmd;d.command[3]=ycmd;d.pid[0]=r;d.pid[1]=p;d.pid[2]=y;for(int i=0;i<4;i++)d.motor[i]=motors.motor[i];xQueueOverwrite(diagnostics,&d);}
  if(uint32_t(micros()-begin)>=cfg::periodUs){++misses;c.hardFaults|=Timing;safety.step(c,now);esc.write(Mix{},false);}
  esp_task_wdt_reset();
 }
}
void setup(){Serial.setTxBufferSize(1024);Serial.begin(115200);pinMode(cfg::kill,INPUT_PULLUP);pinMode(cfg::led,OUTPUT);analogSetPinAttenuation(cfg::battery,ADC_11db);diagnostics=xQueueCreate(1,sizeof(Diagnostic));if(!diagnostics)return;if(xTaskCreatePinnedToCore(controlTask,"flight",8192,nullptr,4,nullptr,1)!=pdPASS){for(int p:cfg::motors)pinMode(p,INPUT);}}
void loop(){Diagnostic d;if(diagnostics&&xQueueReceive(diagnostics,&d,0)==pdTRUE){
 char line[768];int n=snprintf(line,sizeof(line),"D,%lu,%u,%u,%.3f,%lu,%lu,%lu,%lu,%lu",(unsigned long)d.ms,unsigned(d.state),unsigned(d.faults),d.v,(unsigned long)d.age,(unsigned long)d.execUs,(unsigned long)d.misses,(unsigned long)d.rejected,(unsigned long)d.lost);
 const float values[]={d.imu.ax,d.imu.ay,d.imu.az,d.imu.gx,d.imu.gy,d.imu.gz,d.roll,d.pitch,d.yaw,d.command[0],d.command[1],d.command[2],d.command[3],d.pid[0],d.pid[1],d.pid[2],d.motor[0],d.motor[1],d.motor[2],d.motor[3]};
 for(float v:values){if(n<0||n>=int(sizeof(line)))break;int added=snprintf(line+n,sizeof(line)-n,",%.3f",v);if(added<0){n=-1;break;}n+=added;}
 if(n>0&&n+1<int(sizeof(line))){line[n++]='\n';if(Serial.availableForWrite()>=n)Serial.write((uint8_t*)line,n);}
 digitalWrite(cfg::led,d.state==uint8_t(State::ARMED));}delay(1);}
