#include "ksDiagnosticTransferTask.h"
#include <iostream>
#include <stdexcept>
using T=ks::DiagnosticTransferTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Diagnostic transfer property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(auto kind:{T::Kind::Header,T::Kind::Footer,T::Kind::Block})for(unsigned model=0;model<256;++model)for(unsigned device=0;device<256;++device){T t(kind,0xffff,0x12345678);unsigned reads=0;auto read=[&](T::Read f)->uint32_t{++reads;return f==T::Read::SavedModel?model:f==T::Read::SavedDevice?device:f==T::Read::Blocks?65535:0x81234567;};auto write=[](T::Write,uint8_t){};while(!t.complete()){auto stage=t.stage();auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,read,write)&&stage==t.stage()&&reads==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(0xfffffffe));}check(t.model()==model&&t.device()==device);if(kind==T::Kind::Header)check(t.total()==65535&&t.size()==0x81234567);++cases;}
 for(unsigned blocks:{0u,1u,2u,255u,65535u}){T t(T::Kind::Dump);unsigned writes=0,calls=0;auto read=[&](T::Read f)->uint32_t{return f==T::Read::Blocks?blocks:f==T::Read::Model?17:31;};auto write=[&](T::Write,uint8_t){++writes;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){if(s==S::SendDumpBlock)++calls;check(t.resumeChild(0xfffffffd));}}const unsigned expected=blocks?blocks:65536;check(calls==expected&&writes==2&&t.block()==uint16_t(expected)&&t.sum()==uint32_t(uint64_t(expected)*0xfffffffd));++cases;}
 std::cout<<"PASS: "<<cases<<" diagnostic transfer policies / "<<intervals<<" intervals; identity bytes, late fields, zero/full block wrap and checksum overflow, child waits and budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
