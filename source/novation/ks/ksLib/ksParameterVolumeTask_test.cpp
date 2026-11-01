#include "ksParameterVolumeTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterVolumeTask;void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;for(unsigned amount=0;amount<256;++amount)for(unsigned level=0;level<256;++level){T task;unsigned reads=0,clocks=0,packets=0;auto read=[&](bool isLevel)->unsigned{++reads;return isLevel?level:amount;};
 while(!task.complete()){if(task.awaitingWord()){const uint32_t want=packets?uint32_t((level^127)*181)<<8:uint32_t(uint16_t(0x7fff-uint16_t(amount*level*2)))<<8;check(task.dspWord()==want,"Volume packet differs");check(task.resumeWord(),"Completion rejected");++packets;continue;}const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 check(clocks==133&&reads==2&&packets==2&&!task.resumeWord(),"Schedule differs");++cases;}
 std::cout<<"PASS: "<<cases<<" complete two-packet volume jobs; all amount/level pairs, unsigned multiply/16bit doubling wrap, retained level across child,181 shape mapping/deadlines133clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
