#include "ksMemoryDiagnosticsTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::MemoryDiagnosticsTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Memory diagnostics property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned pattern=0;pattern<65536;++pattern)for(unsigned mode:{0u,1u})for(unsigned mismatch:{0u,3u,8u}){const unsigned stride=pattern^0x47,mask=pattern^0xffff;T t(T::Kind::Pass,0x30000,8,uint16_t(pattern),uint16_t(stride),uint16_t(mask),uint8_t(mode));unsigned reads=0,writes=0;auto read=[&](T::Read,uint32_t ref)->uint16_t{unsigned i=(ref-0x30000)/2;++reads;return uint16_t((pattern+i*stride)^mask)^uint16_t(i==mismatch);};auto write=[&](T::Write,uint32_t ref,uint16_t value){unsigned i=(ref-0x30000)/2;++writes;check(value==uint16_t((pattern+i*stride)^mask));};while(!t.complete()){auto stage=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&stage==t.stage()&&reads+writes==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;}check(mode?(reads==(mismatch<8?mismatch+1:8)&&bool(t.mode()&4)==(mismatch<8)):writes==8);++cases;}
 {T t(T::Kind::Memory);auto read=[](T::Read,uint32_t)->uint16_t{return 0;};auto write=[](T::Write,uint32_t,uint16_t){};unsigned n=0;while(t.stage()!=S::HaltFailure){auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost&&++n<100);++intervals;if(t.awaitingChild())check(t.resumeChild(4));}auto cost=t.nextClocks();check(t.advanceBefore(cost,read,write)==cost&&t.stage()==S::HaltFailure);++cases;}
 {T t(T::Kind::ControllerFault);auto read=[](T::Read,uint32_t)->uint16_t{return 0;};auto write=[](T::Write,uint32_t,uint16_t){};auto stage=t.stage();for(unsigned n=0;n<16;++n){auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost&&t.stage()==stage);++intervals;}++cases;}
 std::cout<<"PASS: "<<cases<<" memory diagnostics policies / "<<intervals<<" intervals; all patterns/masks/strides, first/middle/last success/failure and failure halt, live reads and budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
