#include "ksStatePanelModesTask.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using T=ks::StatePanelModesTask;using S=T::Stage;
void check(bool v){if(!v)throw std::runtime_error("StatePanelModes policy differs");}
int main(){try{uint64_t cases=0,intervals=0;
 
 for(auto kind:{T::Kind::OctaveDown,T::Kind::OctaveUp,T::Kind::Semitone})for(unsigned v=0;v<256;++v)for(unsigned type:{0u,4u,255u}){T t(kind,uint16_t(v<<8),true,0x30000);unsigned reads=0,writes=0;auto read=[&](T::Read f,uint32_t)->uint32_t{++reads;return f==T::Read::Patch?0x31000:f==T::Read::Type?type:f==T::Read::Semitone?42:f==T::Read::Screen?7:v;};unsigned semitone=0,previous=0;auto write=[&](T::Write f,uint32_t,uint32_t value){++writes;if(f==T::Write::Semitone)semitone=value;if(f==T::Write::PreviousSemitone)previous=value;};while(!t.complete()){auto s=t.stage();auto cost=t.nextClocks(),n=reads+writes;check(cost&&!t.advanceBefore(cost-1,read,write)&&s==t.stage()&&n==reads+writes);check(t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(false));}if(kind==T::Kind::OctaveDown)check(t.value()==(int(int8_t(v))-1<0?6:uint8_t(v-1)));if(kind==T::Kind::OctaveUp)check(t.value()==(uint8_t(v+1)<=6?uint8_t(v+1):0));if(kind==T::Kind::Semitone)check(semitone==uint8_t(int(int8_t(uint8_t(v+(type==4?0:1))))*12)&&previous==42);++cases;}
 for(unsigned value=0;value<256;++value){T t(T::Kind::ResetVoices);std::vector<unsigned> parts,modes;auto read=[&](T::Read,uint32_t)->uint32_t{return value;};auto write=[&](T::Write f,uint32_t,uint32_t v){if(f==T::Write::Part)parts.push_back(v);if(f==T::Write::Mode)modes.push_back(v);};while(!t.complete()){auto cost=t.nextClocks();check(cost&&t.advanceBefore(cost,read,write)==cost);++intervals;if(t.awaitingChild())check(t.resumeChild(false));}check(parts==std::vector<unsigned>({0,1,2,3,4,value})&&modes==std::vector<unsigned>({0,1,1,1,1,1,value}));++cases;}

 std::cout<<"PASS: "<<cases<<" StatePanelModes policies / "<<intervals<<" intervals; logical routes, byte arithmetic, child replies and insufficient budgets\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
