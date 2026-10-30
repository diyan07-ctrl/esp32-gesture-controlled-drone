#include <iostream>
#include "control.h"
int main(){
 // Toy single-axis rate plant. Coefficients are assumptions, not aircraft measurements.
 flight::Pid p;float rate=0,angle=0;
 for(int i=0;i<2500;i++){float target=i<250?0:10;float torque=p.update(target,rate,.004f);rate+=(100*torque-0.5f*rate)*.004f;angle+=rate*.004f;
  if(!std::isfinite(rate)||std::fabs(torque)>p.limit)return 1;
 }
 std::cout<<"Toy roll model finite; final rate="<<rate<<", angle="<<angle<<"; NOT flight validation\n";
}
