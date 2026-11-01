#include "ksSquareRootTask.h"
#include <iostream>
#include <stdexcept>
static void check(bool b){if(!b)throw std::runtime_error("Square root semantic property failed");}
int main(){using T=ks::SquareRootTask;for(unsigned target=0;target<65536;++target){T t;unsigned comparisons=0,rejections=0,clocks=0,guard=0;auto read=[&](){++comparisons;return target;};while(t.stage()!=T::Stage::Complete){check(++guard<90);auto stage=t.stage();auto cost=t.nextClocks();check(t.advanceBefore(cost-1,read)==0&&t.stage()==stage);check(t.advanceBefore(cost,read)==cost);if(stage==T::Stage::Reject)++rejections;clocks+=cost;}auto r=unsigned(t.result());check(r*r<=target&&(r+1)*(r+1)>target&&comparisons==8&&clocks==309+3*rejections);}std::cout<<"PASS: 65536 pure square root properties; full16-bit domain exact floor inequality, exactly8 late target reads,309+3/rejected-bit clocks, insufficient-budget no progress\n";}
