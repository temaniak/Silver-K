#include "ksGlobalRangeTask.h"
#include <iostream>
#include <stdexcept>
static void check(bool b){if(!b)throw std::runtime_error("Global range semantic property failed");}
int main(){using T=ks::GlobalRangeTask;uint64_t cases=0;auto verify=[&](unsigned first,unsigned second,unsigned base,unsigned mode,unsigned initialMode){T t;unsigned reads=0,bases=0,children=0,guard=0;auto effective=mode!=0;const uint32_t a=effective?uint32_t(first*255/2)<<16:uint32_t(uint16_t(((0x1feu*base)>>16)<<1))*first;
 const uint32_t lower=a+(effective?0x1000000:0x800),upper=(effective?0x7fff0000:0x30000)-lower;uint32_t b=effective?uint32_t(second*255)<<16:uint32_t(uint16_t(((0x200u*(base^0x55aa))>>16)<<2))*second;if(b<lower)b=lower;if(b>upper)b=upper;
 auto read=[&](T::Read r)->uint32_t{switch(r){case T::Read::Mode:return reads++?mode:initialMode;case T::Read::First:return first;case T::Read::Second:return second;case T::Read::Base:return bases++?(base^0x55aa):base;}return 0;};
 while(t.stage()!=T::Stage::Complete){check(++guard<80);auto c=t.child();if(c!=T::Child::None){if(c==T::Child::Mode)check(t.mode()==initialMode);if(c==T::Child::First)check(t.firstWord()==a);if(c==T::Child::Second)check(t.candidate()==b&&t.lower()==lower&&t.upper()==upper);check(t.resumeChild());++children;continue;}auto cost=t.nextClocks(),s=unsigned(t.stage());check(t.advanceBefore(cost-1,read)==0&&unsigned(t.stage())==s);check(t.advanceBefore(cost,read)==cost);}
 check(children==3&&reads==2&&bases==(effective?0:2));++cases;};
 for(unsigned mode:{0u,1u})for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)verify(a,b,44100,mode,mode);
 for(unsigned base=0;base<65536;++base)for(unsigned a:{0u,63u,127u,255u})for(unsigned b:{0u,255u})verify(a,b,base,0,255);
 for(unsigned mode=0;mode<256;++mode)for(unsigned initial:{0u,1u,255u})verify(mode,255-mode,65535,mode,initial);
 std::cout<<"PASS: "<<cases<<" pure range properties; all field pairs/both modes, all first/late-second base values with field edges, all modebytes, mode reread after publication, sequential unsigned lower/upper clamp including inverted bounds, budget refusal\n";}
