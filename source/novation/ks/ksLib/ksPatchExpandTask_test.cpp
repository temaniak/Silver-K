#include "ksPatchExpandTask.h"
#include <array>
#include <vector>
#include <iostream>
#include <stdexcept>
using T=ks::PatchExpandTask;using S=T::Stage;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{uint64_t cases=0;auto verify=[&](unsigned packed,unsigned unison,unsigned flags,unsigned gate,unsigned offset,unsigned command,unsigned latency){
 std::array<unsigned,256>data{},expected{};std::array<unsigned,81>source{};for(unsigned i=0;i<256;++i)data[i]=uint8_t(i*57+packed+unison+offset);for(unsigned i=0;i<81;++i)source[i]=uint8_t(i*29+command);data[5]=packed;data[78]=unison;data[79]=flags;data[80]=gate;data[125]=offset;expected=data;for(unsigned i=0;i<81;++i)expected[128+i]=source[i];for(unsigned i=209;i<256;++i)expected[i]=0;
 expected[128]=packed&3;expected[129]=(packed>>2)&3;expected[130]=(packed>>4)&3;expected[5]=(packed>>6)&1;expected[131]=((unison>>2)&3)+1;expected[132]=((unison>>4)&3)+1;expected[79]=flags&246;expected[147]=(gate&120)/2;expected[125]=offset>=64&&offset<192?offset-64:0;if(command==64){expected[43]=0;expected[151]=65;}
 T t(0xffa30001u);std::vector<unsigned>published;unsigned reads=0;while(!t.complete()){
  auto read=[&](S s)->unsigned{++reads;switch(s){case S::ReadTemplate:return source[t.index()];case S::ReadOscillator:return data[5];case S::ReadUnison:return data[78];case S::ReadFlags:return data[79];case S::ReadGate:return data[80];case S::ReadOffset:return data[125];case S::ReadCommand:return command;default:throw std::runtime_error("Unexpected expand read");}};
  auto write=[&](S s,unsigned v){unsigned index=0;switch(s){case S::PublishTemplate:case S::PublishZero:index=128+t.index();break;case S::PublishOscillator0:index=128;break;case S::PublishOscillator1:index=129;break;case S::PublishOscillator2:index=130;break;case S::PublishOscillator6:index=5;break;case S::PublishUnison0:index=131;break;case S::PublishUnison1:index=132;break;case S::PublishFlags:index=79;break;case S::PublishGate:index=147;break;case S::PublishOffset:index=125;break;case S::PublishLegacyField:index=43;break;case S::PublishLegacyMode:index=151;break;default:throw std::runtime_error("Unexpected expand write");}data[index]=v;published.push_back(index);};auto timing=[&](S){return latency;};const auto cost=t.nextClocks(t.publication()?latency:3);check(cost>0,"Expansion stalled");const auto before=reads+published.size();check(!t.advanceBefore(cost-1,read,write,timing)&&reads+published.size()==before,"Early expansion access");check(t.advanceBefore(cost,read,write,timing)==cost,"Expansion deadline differs");
 }
 std::vector<unsigned>ep;for(unsigned i=128;i<256;++i)ep.push_back(i);for(unsigned i:{128u,129u,130u,5u,131u,132u,79u,147u})ep.push_back(i);if(offset)ep.push_back(125);if(command==64){ep.push_back(43);ep.push_back(151);}check(published==ep,"Expanded publication order differs");check(data==expected,"Expanded packed patch differs");check(t.patch()==0xffa30001u,"Data binding changed");++cases;
 };
 for(unsigned latency:{0u,3u}){
  for(unsigned packed=0;packed<256;++packed)for(unsigned unison=0;unison<256;++unison)verify(packed,unison,uint8_t(packed+unison),uint8_t(packed^unison),uint8_t(packed*3+unison),packed&1?64:17,latency);
  for(unsigned offset=0;offset<256;++offset)for(unsigned gate=0;gate<256;++gate)verify(offset,gate,uint8_t(offset^gate),gate,offset,64,latency);
  for(unsigned flags=0;flags<256;++flags)for(unsigned command=0;command<256;++command)verify(flags,command,flags,command,127,command,latency);
 }
 std::cout<<"PASS: "<<cases<<" pure patch expansions, independent full256-byte packed-field/template/legacy oracle, all byte fields, publication order and early deadlines\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
