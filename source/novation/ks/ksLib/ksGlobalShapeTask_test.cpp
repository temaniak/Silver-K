#include "ksGlobalShapeTask.h"
#include <iostream>
#include <stdexcept>
static void check(bool b){if(!b)throw std::runtime_error("Global shape semantic property failed");}
int main(){using T=ks::GlobalShapeTask;uint64_t cases=0;auto verify=[&](unsigned kind,unsigned value,unsigned base){T t{T::Kind(kind)};unsigned reads=0,bases=0;auto read=[&](bool b){if(b){++bases;return base;}++reads;return value;};check(t.advanceBefore(~0u,read)>0&&t.awaiting());uint32_t expected;
 if(kind==0)expected=uint32_t(uint16_t(value<<8)>>1)<<8;else if(kind==2)expected=uint32_t(uint16_t(unsigned(uint8_t(value-64))<<9))<<8;else{auto product=(value<<8)*0xcccd;auto high=uint16_t((product>>16)-0x3f50+base);if(high&0x8000)high=0x7fff;expected=(uint32_t(high)<<8)|uint8_t(product>>8);}check(t.wireWord()==expected&&reads==1&&bases==unsigned(kind==1));check(t.resumeChild()&&t.stage()==T::Stage::Complete);++cases;};
 for(unsigned value=0;value<256;++value){verify(0,value,0);verify(2,value,0);for(unsigned base=0;base<65536;++base)verify(1,value,base);}
 for(unsigned kind=0;kind<3;++kind)for(unsigned value:{0u,63u,64u,127u,128u,255u}){T t{T::Kind(kind)};auto read=[&](bool base){return base?65535u:value;};unsigned guard=0;while(!t.awaiting()){check(++guard<20);auto c=t.nextClocks(),s=unsigned(t.stage());check(t.advanceBefore(c-1,read)==0&&unsigned(t.stage())==s);check(t.advanceBefore(c,read)==c);}}
 std::cout<<"PASS: "<<cases<<" pure global shape properties; all byte inputs and all65536 coefficient-base combinations, fixed-width product low byte, signed clamp, half/center shapes and budget refusal\n";}
