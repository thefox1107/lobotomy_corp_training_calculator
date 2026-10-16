#pragma once
#include <array>
struct PlanInput {
  std::array<double,4> current{20,20,20,20};
  std::array<double,4> target{30,30,30,30};
  std::array<int,4> completed{0,0,0,0};
  int level=0,cap=100,resident=0,clerks=0;
  double boxes=5,hp=1,sp=1;
  bool tech=false;
  int rates[4][5]{};
};
struct WorkResult {int count=0,remaining=0,rate=-101;double firstGain=0,projected=0;bool reached=false;};
int virtueLevel(double value);
int agentLevel(const std::array<double,4>& values);
double healthFactor(int work,double hp,double sp);
double trainingFactor(bool tech,int resident,int clerks);
WorkResult simulate(const PlanInput& input,int work);
