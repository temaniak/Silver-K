#include "ksServiceCycleTask.h"
#include <iostream>
#include <stdexcept>
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{using T=ks::ServiceCycleTask;T task;unsigned clocks=0;
 for(unsigned cycle=0;cycle<1024;++cycle){for(unsigned child=0;child<17;++child){check(task.stage()==T::Stage::Dispatch&&unsigned(task.child())==child,"Service order differs");const auto cost=child==7||child==8?15u:20u;check(task.nextClocks()==cost,"Dispatch cost differs");check(!task.advanceBefore(cost-1),"Early admission");check(task.advanceBefore(cost)==cost,"Dispatch refused");check(task.stage()==T::Stage::AwaitChild&&!task.advanceBefore(10000),"Child not awaited");check(task.resumeChild(),"Child reply refused");clocks+=cost;}
 check(task.stage()==T::Stage::NextCycle&&task.nextClocks()==3,"Loop admission differs");check(!task.advanceBefore(2),"Early loop");check(task.advanceBefore(3)==3,"Loop refused");clocks+=3;check(task.cyclesCompleted()==cycle+1,"Cycle count differs");check(!task.resumeChild(),"Stray reply accepted");}
 check(clocks==1024*333,"Whole cycle clocks differ");std::cout<<"PASS: 1024 complete native service cycles/17408 child completions, strict order, deadlines and exact own clocks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
