#include "ksParameterVoiceScaleTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterVoiceScaleTask;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;for(unsigned amplitude=0;amplitude<256;++amplitude)for(unsigned gain=0;gain<256;++gain){T task;unsigned reads=0,clocks=0,lookups=0;
 auto read=[&](T::Read what)->uint32_t{++reads;check(task.slot()==uint8_t(amplitude+gain),"Slot differs");return what==T::Read::Amplitude?amplitude:what==T::Read::Gain?gain:0xfedcba98;};
 while(!task.awaitingWord()){if(task.awaitingVoice()){check(task.resumeVoice(uint8_t(amplitude+gain)),"Handle rejected");++lookups;continue;}const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 const int signedWord=(amplitude<128?int(amplitude):int(amplitude)-256)*256;const uint32_t want=uint32_t(signedWord*int(gain>>2))>>8;check(task.dspWord()==want,"Gain packet differs");check(clocks==129&&reads==3&&lookups==1,"Schedule differs");check(task.resumeWord()&&task.complete()&&!task.resumeWord()&&!task.resumeVoice(0),"Lifetime differs");++cases;}
 std::cout<<"PASS: "<<cases<<" complete voice-scale jobs, all amplitude/gain bytes, signed highword multiply, quartergain, selected-handle/patch read deadlines and exact129 own clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
