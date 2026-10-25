#include "ksStateRefreshService.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
namespace{bool tracking=false;uint64_t allocations=0;void check(bool v,const char*m){if(!v)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){try{using T=ks::StateRefreshService;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0,guards=0,flashGraphs=0,flashReads=0;uint32_t components=0;
 const auto byte=[&](uint32_t a,uint8_t v){m[a&0xffffff]=v;};const auto word=[&](uint32_t a,uint16_t v){byte(a,v>>8);byte(a+1,v);};const auto wide=[&](uint32_t a,uint32_t v){word(a,v>>16);word(a+2,v);};
 const auto resolver=[](uint8_t c,uint32_t raw){switch(c){case 0:return raw==0x4f4a;case 2:return raw==0x51d8;case 3:return raw==0x51f4;case 4:return raw==0x5252;case 6:return raw==0x51fe;default:return false;}};
 auto seed=[&](unsigned n,uint32_t source,unsigned dirty,unsigned delay,unsigned pending,unsigned divider){
  std::fill(m.begin(),m.end(),0);for(const auto&s:ks::StateReloadPanelService::schema)for(unsigned j=0;j<8;++j)byte(0xfd66+8*s.key+j,s.bytes[j]);for(auto c:{0u,2u,3u,4u,6u})wide(0x1013e +4*c,c==0?0x4f4a:c==2?0x51d8:c==3?0x51f4:c==4?0x5252:0x51fe);
  for(unsigned key=0;key<32;++key){byte(0x1c0+5*key,uint8_t(key%4));byte(0x1c1+5*key,1);byte(0x1c2+5*key,255);byte(0x1c3+5*key,127);byte(0x1c4+5*key,uint8_t(n%4));}
  for(unsigned slot=0;slot<256;++slot){const auto at=0x17560+20*slot;wide(at,source+256*(slot%5));byte(at+4,uint8_t(slot%5));word(at+8,uint16_t(slot<4?1:0));byte(at+6,uint8_t(n));byte(at+12,uint8_t(n&7));}
  for(unsigned part=0;part<5;++part){wide(0x174ae +20*part,source+256*part);for(unsigned j=4;j<20;++j)byte(0x174ae +20*part+j,uint8_t(n+j));for(unsigned j=0;j<256;++j)byte(source+256*part+j,uint8_t((n*19+j*3)%128));for(unsigned j:{0u,139u,222u,223u,224u,225u,226u})byte(source+256*part+j,0);byte(source+256*part+73,uint8_t(n%3));byte(source+256*part+76,uint8_t(n%4));}
  byte(0x1679a,uint8_t(pending));byte(0x1679b,uint8_t(delay));byte(0x1679c,uint8_t(dirty));byte(0x167d2,15);byte(0x167d3,uint8_t(n));byte(0x17526,0);byte(0x16798,3);byte(0x167ba,uint8_t(n&1));byte(0x174a6,uint8_t(n%3));byte(0x1755e,255);wide(0x167bc,source);wide(0x167c0,0x38000);word(0x15138,0x4470);word(0x1512e,uint16_t(n+1));word(0x159f2,0xffff);byte(0xffff62,255);byte(0xfffeb1,255);
  for(unsigned j=0;j<256;++j)word(0x15200+2*j,uint16_t(j*127));for(unsigned j=0;j<7;++j)word(0x15130+2*j,uint16_t(0x1000+n*31));
  byte(0xffee98,uint8_t(divider));byte(0x169ef,uint8_t(n%4));byte(0x16950,uint8_t(n));byte(0xffee96,uint8_t(n&1));word(0xffeea0,0x400);word(0xffeea4,0x100);wide(0xffee7e,0x100000);wide(0xffee8e,0x100000);
 };
 auto run=[&](unsigned n,unsigned dirty,unsigned delay,unsigned pending,unsigned divider,uint32_t source=0x32000,uint64_t busy=0,bool live=false,uint32_t voiceSource=0){seed(n,source,dirty,delay,pending,divider);if(voiceSource){for(unsigned slot=0;slot<256;++slot)wide(0x17560+20*slot,voiceSource);for(unsigned j=0;j<256;++j)byte(voiceSource+j,m[(source+j)&0xffffff]);}check(T::admitted([&](uint32_t a){return m[a&0xffffff];},resolver),"Safe pure refresh admission");auto s=std::make_unique<T>();std::vector<uint8_t>snapshot(sizeof(T));int previous=-1;unsigned count=0;uint64_t status=0,caseFlashReads=0;uint32_t mutations=0;
  while(!s->complete()){
   if(live)if(const auto*t=s->task<ks::StateForegroundTask>()){using S=ks::StateForegroundTask::Stage;if(t->stage()==S::ReadRefreshFlags&&!(mutations&1)){byte(0x167d3,0xa5);mutations|=1;}if(t->stage()==S::ReadDirty&&!(mutations&2)){byte(0x1679c,1);mutations|=2;}if(t->stage()==S::RefreshParts&&!(mutations&4)){byte(0x1679b,0);byte(0x1679a,3);mutations|=4;}if(t->stage()==S::RefreshClock&&!(mutations&8)){byte(0xffee98,1);byte(0x169ef,1);mutations|=8;}}
   const auto cost=s->nextClocks(previous);check(cost,"Refresh missing phase");std::memcpy(snapshot.data(),s.get(),sizeof(T));unsigned callbacks=0;tracking=true;
   check(!s->advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},resolver,previous),"Refresh accepted short budget");check(!callbacks&&!std::memcmp(snapshot.data(),s.get(),sizeof(T)),"Refresh budget rejection changed whole object");++rejects;unsigned last=0;
   check(s->advanceBefore(cost,[&](uint32_t raw,unsigned dt){check(dt>=last&&dt<=cost,"Refresh read timing");last=dt;++reads;const auto a=raw&0xffffff;check(a<0xfff000||a>=0xfffc00,"Refresh scratch read");if(a>=0x800000&&a<0x900000){check(s->component()==T::Component::Deferred&&s->deferred()->component()==ks::StateDeferredPartsService::Component::Voice,"Flash read outside current Voice child");++caseFlashReads;}return a==0x400004||a==0x600004?uint8_t(status++<busy?0:255):m[a];},[&](uint32_t raw,uint8_t v,unsigned dt){check(dt>=last&&dt<=cost,"Refresh write timing");last=dt;++writes;const auto a=raw&0xffffff;check(a<0xfff000||a>=0xfffc00,"Refresh scratch write");check(a<0x800000||a>=0x900000,"Refresh wrote voice Flash");m[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},resolver,previous)==cost,"Refresh budget differs");tracking=false;if(s->effect().write)previous=-1;check(++count<12000000,"Pure refresh stalled");++phases;
  }
  if(live)check(mutations==15,"Pure refresh evolving reads missed");check(s->packet()->complete()&&s->deferred()->complete()&&s->clock()->complete(),"Pure refresh incomplete child");if(dirty||live)check(s->panel()&&s->panel()->complete(),"Pure refresh panel absent");if(voiceSource){check(caseFlashReads>0,"Pure Flash child not exercised");++flashGraphs;flashReads+=caseFlashReads;}components|=s->components();++graphs;
 };
 for(unsigned mask=0;mask<256;++mask)run(mask,mask&1,0,mask,mask&1);
 for(unsigned delay:{1u,2u,127u,128u,255u})for(unsigned pending:{0u,31u,255u})run(pending,1,delay,pending,1);
 for(unsigned alias:{0x80032001u,0xa32001u,0xffdc01u,0x42032001u})run(0,1,1,0,0,alias);
 for(unsigned busy:{1u,7u,65538u})run(0,0,1,0,0,0x32000,busy);
 run(0,0,1,0,0,0x32000,0,true);
 for(unsigned flash:{0x800000u,0x81fd01u,0x8fff00u,0xc181fd01u})for(unsigned dirty:{0u,1u})run(0,dirty,0,1,0,0x32000,0,false,flash);
 for(const auto&s:ks::StateReloadPanelService::schema)for(unsigned j=0;j<8;++j){seed(0,0x32000,1,1,0,0);byte(0xfd66+8*s.key+j,uint8_t(s.bytes[j]^128));check(!T::admitted([&](uint32_t a){return m[a&0xffffff];},resolver),"Refresh admitted mutated descriptor");++guards;}
 for(uint32_t bad:{0u,0x9662u,0xffefffu,0xfff000u,0xfff700u,0xffff51u,0x800000u,0x81fd01u,0x8fff00u,0xc181fd01u}){seed(0,0x32000,1,1,0,0);wide(0x167bc,bad);check(!T::admitted([&](uint32_t a){return m[a&0xffffff];},resolver),"Refresh admitted invalid panel base");++guards;seed(0,0x32000,0,0,1,0);wide(0x174ae,bad);check(!T::admitted([&](uint32_t a){return m[a&0xffffff];},resolver),"Refresh admitted invalid deferred base");++guards;}
 // Unsupported evolving pointers/schema stop before an unsafe derived access.
 for(unsigned mutation=0;mutation<3;++mutation){seed(0,0x32000,1,1,0,0);auto s=std::make_unique<T>();bool mutated=false,rejected=false,scratch=false;unsigned count=0;int previous=-1;
  try{while(!s->complete()){
   if(s->component()==T::Component::Panel){const auto*p=s->panel();if(!mutated&&mutation==0&&p->component()==ks::StateReloadPanelService::Component::Panel&&p->panelTask().stage()==ks::StatePanelRefreshTask::Stage::ReadCurrent){wide(0x167bc,0xfff700);mutated=true;}
    if(!mutated&&mutation==1&&p->component()==ks::StateReloadPanelService::Component::Workflow){byte(0xfd66+8*38,0xfe);mutated=true;}
    if(!mutated&&mutation==2&&p->component()==ks::StateReloadPanelService::Component::Workflow){wide(0x1013e +4*2,0xfff700);mutated=true;}}
   const auto cost=s->nextClocks(previous);s->advanceBefore(cost,[&](uint32_t raw,unsigned){const auto a=raw&0xffffff;if(a>=0xfff000&&a<0xfffc00){scratch=true;throw std::logic_error("Unsafe evolving refresh read");}return a==0x400004||a==0x600004?uint8_t(255):m[a];},[&](uint32_t raw,uint8_t v,unsigned){const auto a=raw&0xffffff;if(a>=0xfff000&&a<0xfffc00){scratch=true;throw std::logic_error("Unsafe evolving refresh write");}m[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},resolver,previous);if(s->effect().write)previous=-1;if(++count>=100000)throw std::runtime_error("Live guard stalled");
  }}catch(const std::logic_error&){rejected=true;}
  if(!(mutated&&rejected&&!scratch))std::cerr<<"guard mutation="<<mutation<<" mutated="<<mutated<<" rejected="<<rejected<<" scratch="<<scratch<<" phases="<<count<<'\n';check(mutated&&rejected&&!scratch,"Refresh evolving guard failed before unsafe use");++guards;
 }
 for(uint32_t bad:{0u,0x437au,0x8fff01u,0x900000u,0xffefffu,0xfff700u,0xc18fff01u}){seed(0,0x32000,0,0,1,0);auto s=std::make_unique<T>();unsigned count=0;int previous=-1;
  for(;;){const auto*t=s->task<ks::VoiceRefreshTask>();if(t&&t->stage()==ks::VoiceRefreshTask::Stage::ReadPatch)break;const auto cost=s->nextClocks(previous);s->advanceBefore(cost,[&](uint32_t raw,unsigned){const auto a=raw&0xffffff;return a==0x400004||a==0x600004?uint8_t(255):m[a];},[&](uint32_t raw,uint8_t v,unsigned){const auto a=raw&0xffffff;m[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},resolver,previous);if(s->effect().write)previous=-1;check(++count<4096,"Pure voice guard landing absent");}
  wide(0x17560,bad);unsigned observations=0;bool rejected=false;const auto cost=s->nextClocks(previous);try{s->advanceBefore(cost,[&](uint32_t raw,unsigned){const auto a=raw&0xffffff;check(a>=0x17560&&a<0x17564,"Pure invalid patch derived read");++observations;return m[a];},[&](uint32_t,uint8_t,unsigned){throw std::runtime_error("Pure invalid patch write");},resolver,previous);}catch(const std::logic_error&){rejected=true;}check(rejected&&observations==4,"Pure evolving invalid voice patch accepted");++guards;
 }
 check(!allocations,"Refresh pure allocated");check(components==31,"Pure refresh coverage");std::cout<<"PASS Refresh pure graphs="<<graphs<<" phases="<<phases<<" budgetRejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" guards="<<guards<<" flashGraphs="<<flashGraphs<<" flashReads="<<flashReads<<" components="<<components<<" bytes="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
