#include "ksParameterLfoTask.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using T=ks::ParameterLfoTask;using S=T::Stage;
void check(bool b,const char* w){if(!b)throw std::runtime_error(w);}
uint32_t convert(unsigned input,bool delay){return (uint32_t(input)*0x9e3779b1u)^(delay?0x87654321u:0xfedcba98u);}
uint32_t preset(unsigned offset,bool delay){return (uint32_t(offset)*0x1234567u)^(delay?0x76543210u:0xfedcba98u);}
int main(){try{uint64_t cases=0;auto verify=[&](unsigned lane,unsigned mode,unsigned lateMode,unsigned shape,unsigned polarity,unsigned rateSelector,unsigned rate,unsigned rateBase,unsigned delaySelector,unsigned delay,unsigned delayBase){T task(lane);unsigned reads=0,clocks=0,packets=0,coefficients=0;const unsigned rateInput=uint16_t(((uint32_t(rate<<7)*0xaaab)>>16)+0x4700+uint16_t(rateBase-0x4470));const unsigned inv=delay^127,delayInput=uint16_t(inv*80+(inv>115?(inv-115)*256:0)+delayBase);
 const uint32_t rateCoefficient=convert(rateInput,false),delayCoefficient=convert(delayInput,true);const uint32_t computedRate=uint32_t((uint64_t(rateCoefficient)*(std::min(rate,32u)<<10))>>16)<<1;
 const unsigned shapeValue=(shape*2)&255;const uint32_t flags=(polarity?0x400000:0)|(lateMode==4||lateMode==5?1:lateMode==2?2:0);
 const std::array<uint32_t,7> words{{0x520u+((mode*4)&255),(shapeValue<<16)|shapeValue,rateSelector?0:computedRate>>8,rateSelector?preset(((rateSelector-1)*4)&255,false)>>8:0,flags,delaySelector?0:(delayCoefficient>>1)>>8,delaySelector?(preset(((delaySelector-1)*4)&255,true)>>1)>>8:0}};
 auto read=[&](T::Read r)->uint32_t{++reads;switch(r){case T::Read::Mode:return mode;case T::Read::Shape:return shape;case T::Read::RateSelector:return rateSelector;case T::Read::Rate:return rate;case T::Read::RateBase:return rateBase;case T::Read::RatePreset:return preset(task.presetOffset(),false);case T::Read::Polarity:return polarity;case T::Read::LateMode:return lateMode;case T::Read::DelaySelector:return delaySelector;case T::Read::Delay:return delay;case T::Read::DelayBase:return delayBase;case T::Read::DelayPreset:return preset(task.presetOffset(),true);}return 0;};
 while(!task.complete()){if(task.awaitingChild()){if(task.coefficientChild()){const bool isDelay=task.resumeStage()==S::HalfDelayCoefficient;check(task.scaleInput()==(isDelay?delayInput:rateInput),"LFO coefficient input differs");check(task.resumeChild(isDelay?delayCoefficient:rateCoefficient),"Coefficient rejected");++coefficients;}else{check(task.outputIndex()==packets&&packets<7&&task.dspWord()==words[packets],"LFO packet differs");check(task.resumeChild(),"Word rejected");++packets;}continue;}const auto cost=task.nextClocks(),before=reads;check(cost>0,"LFO stalled");check(!task.advanceBefore(cost-1,read)&&reads==before,"Early LFO read");check(task.advanceBefore(cost,read)==cost,"LFO admission differs");clocks+=cost;}
 const unsigned flagClocks=89-(polarity?0:9)-(lateMode==4?6:0)-(lateMode==4||lateMode==5?0:3)-(lateMode==2?0:3);
 const unsigned expected=94+(rateSelector?100:256+(rate>32?3:0))+flagClocks+(delaySelector?103:150+(inv>115?6:0));
 if(clocks!=expected){std::cerr<<"clocks="<<clocks<<" expected="<<expected<<" rsel="<<rateSelector<<" dsel="<<delaySelector<<" mode="<<lateMode<<'\n';throw std::runtime_error("LFO schedule differs");}
 check(packets==7&&coefficients==unsigned(!rateSelector)+unsigned(!delaySelector)&&reads==8+unsigned(!rateSelector)+unsigned(!delaySelector)&&!task.resumeChild(),"LFO observation/lifetime differs");++cases;};
 for(unsigned lane=0;lane<2;++lane){for(unsigned rs=0;rs<256;++rs)for(unsigned ds=0;ds<256;++ds)for(unsigned shape:{0u,127u,128u,255u})verify(lane,(rs+ds)&255,(rs*17+ds*31)&255,shape,rs&1,rs,(rs*13+ds*17)&255,(rs*257+ds*31)&65535,ds,(rs*19+ds*23)&255,(rs*43+ds*257)&65535);
  for(unsigned base=0;base<65536;++base)for(unsigned value:{0u,1u,32u,33u,127u,128u,255u})verify(lane,value,base&255,value,base&1,0,value,base,0,(value+base)&255,base^0xaaaa);
  for(unsigned mode=0;mode<256;++mode)for(unsigned polarity=0;polarity<256;++polarity)verify(lane,mode,mode,mode,polarity,mode&1?0:255,mode,0x4470,mode&2?0:255,mode,0x8000);
 }
 std::cout<<"PASS: "<<cases<<" complete7-packet LFO lanes; all selector pairs/offset wrap, shape/polarity/modes, rate clamp and static16 bases, native coefficient input/result/64bit reference weighting, late presets/fields, exact clocks/deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
