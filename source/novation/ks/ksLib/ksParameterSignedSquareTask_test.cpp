#include "ksParameterSignedSquareTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::ParameterSignedSquareTask;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
int main(){try{for(unsigned value=0;value<256;++value){T task;unsigned reads=0,clocks=0;auto read=[&]{++reads;return value;};
 while(!task.awaitingWord()){const auto cost=task.nextClocks(),before=reads;check(cost>0,"Stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early read");check(task.advanceBefore(cost,read)==cost,"Admission differs");clocks+=cost;}
 const unsigned biased=(value-64)&255;const int signedByte=biased<128?int(biased):int(biased)-256;const uint16_t word=uint16_t((signedByte<0?-1:1)*signedByte*signedByte*8);check(task.dspWord()==uint32_t(word)<<8,"Signed square differs");check(clocks==80+unsigned(signedByte<0?3:0)&&reads==1,"Schedule differs");check(task.resumeWord()&&task.complete()&&!task.resumeWord(),"Lifetime differs");}
 std::cout<<"PASS:256 signed-square packets; all biased signed bytes including negative128, sign-restored square,16bit scaled wrap,24bit packing and exact deadlines/clocks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
