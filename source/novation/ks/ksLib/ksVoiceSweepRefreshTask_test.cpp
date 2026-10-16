#include "ksVoiceSweepTask.h"
#include "ksVoiceRefreshTask.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <iostream>
static void check(bool b){if(!b)throw std::runtime_error("Native voice semantic property failed");}
int main(){using T=ks::VoiceSweepTask;using F=ks::VoiceRefreshTask;uint64_t cases=0;
 for(unsigned n=0;n<256;++n)for(unsigned transpose=0;transpose<256;++transpose)for(unsigned pitch:{0u,255u}){
 T t(T::Kind::RetunePart);unsigned index=0,notes=0,cuts=0;uint32_t noteOut=0;unsigned guard=0;
 auto store=[](T::Write w){return w==T::Write::Age?6:3;};auto reader=[&](T::Read r)->uint32_t{switch(r){case T::Read::Part:case T::Read::Match:case T::Read::Owner:return 3;case T::Read::Count:return 1;case T::Read::Age:return 0x8000;case T::Read::Note:return n;case T::Read::Transpose:return transpose;case T::Read::Index:return index;case T::Read::Pitch:return pitch;case T::Read::TrackedPitch:return noteOut;case T::Read::Patch:return 0x30000;}return 0;};
 while(t.stage()!=T::Stage::Complete){check(++guard<1000);const auto child=t.child();if(child!=T::Child::None){if(child==T::Child::Cut){check(t.wireWord()==(uint32_t(uint8_t((pitch?pitch:noteOut)-60))<<16));++cuts;}check(t.resumeChild(uint8_t(index)));continue;}auto cost=t.nextClocks(store);check(cost);check(t.advanceBefore(cost,reader,[&](T::Write w,uint32_t value){if(w==T::Write::Index)index=value;else if(w==T::Write::Note){noteOut=value;++notes;}else check(false);},store)==cost);}
 check(index==1&&notes==1&&cuts==1&&noteOut==uint8_t(n+transpose-64));++cases;}
 for(unsigned kind=0;kind<3;++kind)for(unsigned count=0;count<256;++count)for(unsigned mode=0;mode<256;++mode){
 F t{F::Kind(kind)};unsigned index=0,uploads=0,tracking=0,global=0,guard=0;
 auto store=[](){return 3;};auto read=[&](F::Read r)->uint32_t{switch(r){case F::Read::Part:case F::Read::Match:case F::Read::Owner:return 3;case F::Read::Mode:return mode;case F::Read::Count:return count;case F::Read::Age:return (index&1)?0:0x8000;case F::Read::Patch:return 0x30000;case F::Read::Index:return index;}return 0;};
 while(t.stage()!=F::Stage::Complete){check(++guard<16000);auto child=t.child();if(child!=F::Child::None){if(child==F::Child::Upload)++uploads;if(child==F::Child::Tracking)++tracking;if(child==F::Child::Global)++global;check(t.resumeChild(uint8_t(index)));continue;}auto cost=t.nextClocks(store);check(cost);check(t.advanceBefore(cost,read,[&](uint8_t v){index=v;},store)==cost);}
 unsigned total=(kind==1&&mode)?0:(count?count:256);check(index==uint8_t(total));check(uploads==(total+1)/2);check(tracking==(kind==2));check(global==(kind==2));++cases;}
 std::cout<<"PASS: "<<cases<<" pure semantic properties; all byte note/transpose pairs and pitch routes; all counts/modes for each refresh kind; zero-count 256 traversal, sparse active slots, upload/tracking/global lifetimes\n";}
