#pragma once
#include <cstdint>
#include "configuration/config.h"
namespace flight {
enum class State:uint8_t {BOOT,SENSOR_INITIALIZATION,CALIBRATION,DISARMED,ARMING_CHECKS,ARMED,FAILSAFE,FAULT};
enum Fault:uint16_t{Sensor=1,Timing=2,Battery=4,Link=8,Emergency=16,Numeric=32,Setup=64};
struct Checks{bool sensor=false,calibrated=false,fresh=false,enabled=false,arm=false,kill=false,battery=false,lowThrottle=false,hardware=false;uint16_t hardFaults=0;};
struct Safety{
 State state=State::BOOT;uint16_t faults=0;uint32_t start=0;bool armReleased=false;
 bool armed()const{return state==State::ARMED;}
 void begin(){if(state==State::BOOT)state=State::SENSOR_INITIALIZATION;}
 void initialized(bool ok){if(state==State::SENSOR_INITIALIZATION){state=ok?State::CALIBRATION:State::FAULT;if(!ok)faults|=Sensor;}}
 void calibrated(bool ok){if(state==State::CALIBRATION){state=ok?State::DISARMED:State::FAULT;if(!ok)faults|=Sensor;}}
 void step(const Checks&c,uint32_t now){
  if(state==State::FAULT||state==State::FAILSAFE)return;
  if(c.hardFaults){faults|=c.hardFaults;state=State::FAULT;return;}
  if(armed()){
   if(c.kill){faults|=Emergency;state=State::FAILSAFE;}
   else if(!c.sensor||!c.battery){faults|=!c.sensor?Sensor:Battery;state=State::FAULT;}
   else if(!c.fresh||!c.enabled){faults|=Link;state=State::FAILSAFE;}
   else if(!c.arm){state=State::DISARMED;armReleased=false;}
   return;
  }
  if(state!=State::DISARMED&&state!=State::ARMING_CHECKS)return;
  // A reconnect during the arming hold must require another observed arm release.
  if(!c.fresh){state=State::DISARMED;armReleased=false;return;}
  if(!c.arm&&c.fresh)armReleased=true;
  bool ok=c.sensor&&c.calibrated&&c.fresh&&c.enabled&&c.arm&&!c.kill&&c.battery&&c.lowThrottle&&c.hardware&&armReleased;
  if(!ok){state=State::DISARMED;return;}
  if(state==State::DISARMED){state=State::ARMING_CHECKS;start=now;}
  else if(uint32_t(now-start)>=cfg::armHoldMs){state=State::ARMED;armReleased=false;}
 }
};
}
