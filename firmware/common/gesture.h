#pragma once
#include "control.h"
namespace flight {
struct Gesture {
 float neutralRoll=0,neutralPitch=0;bool released=false,zeroed=false;
 void center(float r,float p){neutralRoll=r;neutralPitch=p;}
 void reset(){released=zeroed=false;}
 bool permit(bool link,bool enable,bool arm,float throttle){
  if(!link){reset();return false;}
  if(!arm)released=true;
  if(!arm&&!enable&&throttle<0.02f)zeroed=true;
  return released&&zeroed&&enable;
 }
};
}
