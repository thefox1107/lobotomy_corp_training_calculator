#include "../native/core.hpp"
#include <cassert>
#include <cmath>
int main(){
  assert(virtueLevel(29)==1&&virtueLevel(30)==2&&virtueLevel(45)==3&&virtueLevel(65)==4&&virtueLevel(85)==5);
  assert(agentLevel({85,85,65,65})==5);
  assert(healthFactor(0,1,1)==.4&&healthFactor(3,1,1)==.5);
  PlanInput p;p.level=1;p.current[0]=29;p.target[0]=45;p.boxes=8;
  for(auto& row:p.rates)for(int& x:row)x=60;
  auto result=simulate(p,0);
  assert(result.count==9&&result.reached&&std::abs(result.firstGain-1.92)<1e-10);
  p.completed[0]=3;assert(simulate(p,0).remaining==6);
  p.current[0]=40;p.target[0]=95;
  result=simulate(p,0);
  assert(result.count==32&&result.reached&&std::abs(result.firstGain-1.76)<1e-10);
  p.level=0;p.boxes=7;p.cap=130;p.current[0]=84;p.target[0]=104;
  result=simulate(p,0);
  assert(result.count==40&&result.reached&&std::abs(result.firstGain-.504)<1e-10);
  p.current[3]=40;p.target[3]=95;p.level=1;p.boxes=8;
  result=simulate(p,3);
  assert(result.count==25&&result.reached&&std::abs(result.firstGain-2.2)<1e-10);
  p.rates[0][3]=-40;assert(simulate(p,0).rate==-40);
  p.rates[0][3]=-101;assert(simulate(p,0).count==0);
}
