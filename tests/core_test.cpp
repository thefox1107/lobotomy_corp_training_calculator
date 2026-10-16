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
  p.rates[0][0]=-40;assert(simulate(p,0).rate==-40);
  p.rates[0][0]=-101;assert(simulate(p,0).count==0);
}
