#include "ksParameterGateTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterGateTask;void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;auto verify=[&](unsigned gate,unsigned part,unsigned active,unsigned mode){T task;unsigned reads=0,clocks=0,outputs=0;const bool gated=gate&&part==active;constexpr unsigned fields[]{39,143,38,142,37,141,171},required[]{2,2,1,1,0,0,0};auto read=[&](T::Read r)->unsigned{++reads;return r==T::Read::Gate?gate:r==T::Read::Part?part:r==T::Read::Active?active:mode;};
 while(!task.complete()){if(task.awaitingOutput()){const auto i=task.output();const auto want=gated&&mode!=required[i]?T::Child::Zero:(i==1||i==3||i>=5)?T::Child::BipolarHalf:T::Child::Quarter;check(task.child()==want&&task.patchOffset()==fields[i],"Gate child differs");check(task.resumeOutput(),"Output rejected");++outputs;continue;}const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 check(clocks==57+(part!=active?3:0)+(gated?105:42)&&reads==4&&outputs==7&&!task.resumeOutput(),"Gate schedule differs");++cases;};
 for(unsigned gate=0;gate<256;++gate)for(unsigned mode=0;mode<256;++mode){verify(gate,2,2,mode);verify(gate,2,3,mode);}
 for(unsigned part=0;part<256;++part)for(unsigned active=0;active<256;++active)for(unsigned gate:{0u,1u,128u,255u})verify(gate,part,active,(part*17+active*31)&255);
 std::cout<<"PASS: "<<cases<<" complete7-output gate decisions; all gate/mode pairs and part/active pairs, clear flag semantics, seven zero/Quarter/Half requests and exact observation/own clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
