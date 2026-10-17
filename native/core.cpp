#include "core.hpp"
#include <algorithm>
#include <cmath>
static constexpr double FACTORS[5][5]={{.6,.6,.72,.84,.6},{.44,.55,.55,.66,.77},{.3,.4,.5,.5,.6},{.18,.27,.36,.45,.45},{.08,.16,.24,.32,.4}};
int virtueLevel(double v){return v<30?1:v<45?2:v<65?3:v<85?4:5;}
int agentLevel(const std::array<double,4>& values){int sum=0;for(double v:values)sum+=virtueLevel(v);return sum>=16?5:std::max(1,sum/3);}
double healthFactor(int work,double hp,double sp){
  if(work==3)return .5;
  double x=work==0?hp:work==1?sp:(hp+sp)/2;
  if(x<=.1)return 1.5;if(x<=.2)return 1.3;if(x<.7)return 1;if(x<.8)return .8;if(x<.9)return .6;return .4;
}
double trainingFactor(bool tech,int resident,int clerks){
  static constexpr double r[]={0,.05,.1,.15},c[]={0,.01,.03,.05};
  return 1+(tech?.5:0)+r[std::clamp(resident,0,3)]+c[std::clamp(clerks,0,3)];
}
WorkResult simulate(const PlanInput& in,int work){
  WorkResult result;double value=std::clamp(in.current[work],1.,(double)in.cap);
  const double target=std::clamp(in.target[work],value,(double)in.cap);
  result.rate=in.rates[work][virtueLevel(value)-1];
  result.projected=value;
  if(result.rate==-101)return result;
  if(value+1e-9<target){
    // Keep the starting quality coefficient for the entire plan, even across
    // quality thresholds. Preserve the existing work-specific health factor.
    const double gain=in.boxes*FACTORS[virtueLevel(value)-1][std::clamp(in.level,0,4)]
        *healthFactor(work,in.hp,in.sp)
        *trainingFactor(in.tech,in.resident,in.clerks);
    if(gain>0){
      result.firstGain=gain;
      result.count=(int)std::ceil((target-value-1e-9)/gain);
      value=std::min((double)in.cap,value+result.count*gain);
    }
  }
  result.projected=value;result.reached=value+1e-9>=target;result.remaining=std::max(0,result.count-in.completed[work]);
  return result;
}
