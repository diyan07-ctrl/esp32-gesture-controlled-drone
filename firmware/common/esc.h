#pragma once
#include <Arduino.h>
#include "control.h"
#include "configuration/config.h"
namespace flight {
class Esc {
 bool attached=false;
 public:
 void safeBoot(){for(int pin:cfg::motors)pinMode(pin,INPUT);}
 bool attach(){
  if(!(cfg::outputsVerified&&cfg::powerVerified&&cfg::propulsionVerified&&cfg::controllerValidated))return false;
  for(int i=0;i<4;i++){if(ledcSetup(i,cfg::pwmHz,cfg::pwmBits)==0)return false;ledcWrite(i,0);ledcAttachPin(cfg::motors[i],i);}attached=true;return true;
 }
 void write(const Mix&m,bool armed){if(!attached)return;
  // Disarmed = no pulses. Exact ESC must be verified to stop on absent signal.
  for(int i=0;i<4;i++){uint32_t duty=0;
   if(armed&&m.valid&&std::isfinite(m.motor[i])){float pulse=cfg::pwmMin+(cfg::pwmMax-cfg::pwmMin)*clamp(m.motor[i],0,1);duty=uint32_t(pulse*cfg::pwmHz*65535.0f/1000000.0f);}
   ledcWrite(i,duty);
  }
 }
};
}
