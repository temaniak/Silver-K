#include "ksDiagnosticCodecTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::DiagnosticCodecTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("Diagnostic codec property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(unsigned value=0;value<65536;++value)for(auto kind:{T::Kind::Word,T::Kind::Long}){const uint32_t input=value*65537u;T t(kind,input);std::vector<unsigned> bytes;unsigned reads=0;auto read=[&](T::Read,uint32_t)->uint16_t{++reads;return 0;};auto write=[](int,int){};
  while(!t.complete()){auto stage=t.stage();const auto cost=t.nextClocks(),n=reads;check(cost&&!t.advanceBefore(cost-1,read,write)&&stage==t.stage()&&reads==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){if(stage==S::SendWordLow||stage==S::SendWordHigh||stage==S::SendLongTop)bytes.push_back(t.byte());if(stage==S::SendLongLow||stage==S::SendLongHigh){const auto v=stage==S::SendLongLow?input&16383:(input>>14)&16383;bytes.push_back(v&127);bytes.push_back((v>>7)&255);}check(t.resumeChild(0,0,0,0));}}
  std::vector<unsigned> expected=kind==T::Kind::Word?std::vector<unsigned>{value&127,(value>>7)&255}:std::vector<unsigned>{input&127,(input>>7)&127,(input>>14)&127,(input>>21)&127,input>>28};check(bytes==expected&&reads==0);++cases;}
 for(unsigned value=0;value<65536;++value)for(unsigned weight:{1u,4u,16u,64u,256u,1024u,4096u}){const unsigned carry=value^0x5a5a;T t(T::Kind::Sample,0,0x30000,0xffffff00, uint16_t(weight),uint16_t(carry));std::vector<unsigned> bytes;auto read=[&](T::Read,uint32_t ref)->uint16_t{check(ref==0x30000);return uint16_t(value);};auto write=[](int,int){};while(!t.complete()){auto stage=t.stage();const auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild()){bytes.push_back(t.byte());check(t.resumeChild(0,0,0,0));}}
  uint32_t expected=uint32_t(value)*weight;expected=(expected&0xffff0000u)|uint16_t(expected|carry);expected<<=2;const auto nextCarry=expected>>16;check(bytes.size()==2&&bytes[0]==((expected>>2)&127)&&bytes[1]==((expected>>9)&127)&&t.carry()==nextCarry&&t.sum()==uint32_t(0xffffff00u+value)&&t.source()==0x30002);++cases;}
 std::cout<<"PASS: "<<cases<<" diagnostic codec policies / "<<intervals<<" intervals; all word/long inputs and sample words at seven weights, independent wire bits/checksum/carry and budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
