#include "ksParameterCurveTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterCurveTask;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{uint64_t cases=0;for(unsigned value=0;value<256;++value)for(unsigned curve=0;curve<65536;++curve){T task;unsigned clocks=0,reads=0,index=0;const unsigned biased=(value-64)&255;const bool negative=biased>=128;const unsigned expectedIndex=((negative?(256-biased)&255:biased)*2)&255;
 auto read=[&](bool isCurve)->unsigned{++reads;if(isCurve){index=task.index();return curve;}return value;};
 while(!task.awaitingWord()){const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 const int signedCurve=curve<32768?int(curve):int(curve)-65536;const uint32_t want=(uint32_t(signedCurve*(negative?-21845:21845))<<1)>>8;
 check(task.dspWord()==want&&index==expectedIndex,"Curve/index differs");check(clocks==100+(negative?6:0)&&reads==2,"Schedule differs");check(task.resumeWord()&&task.complete()&&!task.resumeWord(),"Lifetime differs");++cases;}
 std::cout<<"PASS: "<<cases<<" whole curve packets, all256 values x65536 signed table words, negative128 byte abs/doubled index wrap, exact sign/payload/read deadlines/clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
