#include "ksSystemServicesTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::SystemServicesTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("System service property differs");}
int main(){try{uint64_t cases=0,intervals=0;
 for(auto kind:{T::Kind::Initialize,T::Kind::KeyMap,T::Kind::Keyboard,T::Kind::Glyphs,T::Kind::Bar,T::Kind::Signature,T::Kind::Save})for(unsigned input=0;input<256;++input)for(unsigned mismatch=0;mismatch<=48;++mismatch){
  T t(kind,uint8_t(input));unsigned reads=0,writes=0,patches=0,saves=0;std::vector<unsigned> bars;auto read=[&](T::Read field,unsigned at){++reads;return field==T::Read::Hardware?input:field==T::Read::Expected?at:field==T::Read::Actual?(at==mismatch?at^1:at):input*257;};auto write=[&](T::Write field,unsigned value){++writes;if(field!=T::Write::KeyboardPort){check(value==255);++patches;}else check(value==1);};
  unsigned steps=0;while(!t.complete()){const auto before=t.stage();const auto cost=t.nextClocks(),n=reads+writes;check(cost&&++steps<500);check(!t.advanceBefore(cost-1,read,write)&&t.stage()==before&&reads+writes==n);check(t.advanceBefore(cost,read,write)==cost);++intervals;
   if(before==S::WriteSolidBar)bars.push_back(5);if(before==S::WritePartialBar)bars.push_back(t.value());if(before==S::WriteBlankBar)bars.push_back(0);if(before==S::WriteSystemPage)++saves;if(t.awaitingChild())check(t.resumeChild());}
  if(kind==T::Kind::KeyMap)check(patches==(input?3u:0u));if(kind==T::Kind::Signature)check(t.version()==(mismatch>=40?input*257u:0u));if(kind==T::Kind::Save)check(saves==unsigned(mismatch<48));if(kind==T::Kind::Bar){check(bars.size()==5);unsigned sum=0;for(auto v:bars){check(v<=5);sum+=v;}check(sum==(input>25?25:input));}++cases;
 }
 std::cout<<"PASS: "<<cases<<" system service policies / "<<intervals<<" intervals; startup child order, key-map bytes, glyph/bar bounds, live signatures and deadlines\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
