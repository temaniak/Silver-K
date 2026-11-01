#include "ksStateDeferredPartsService.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
namespace{bool tracking=false;uint64_t allocations=0;void check(bool v,const char*m){if(!v)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){try{using T=ks::StateDeferredPartsService;static_assert(sizeof(T)<65536,"VoiceRefresh fixed storage grew unexpectedly");std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;unsigned maxDepth=0;
auto run=[&](unsigned seed,int countOverride=-1,int ageOverride=-1,int ownerOverride=-1,uint32_t voiceSource=0,uint64_t busy=0,bool live=false,unsigned pending=1,unsigned delay=0,uint32_t controlSource=0x32000){std::fill(m.begin(),m.end(),0);auto put=[&](uint32_t a,uint32_t v,unsigned n){for(unsigned j=0;j<n;++j)m[a+j]=uint8_t(v>>(8*(n-1-j)));};const auto byte=[&](uint32_t a,uint8_t v){m[a&0xffffff]=v;};const auto word=[&](uint32_t a,uint16_t v){byte(a,v>>8);byte(a+1,v);};const auto wide=[&](uint32_t a,uint32_t v){word(a,v>>16);word(a+2,v);};
for(unsigned slot=0;slot<256;++slot){const auto at=0x17560+20*slot;wide(at,voiceSource?voiceSource:0x32000+256*(slot%4));byte(at+4,uint8_t(ownerOverride<0?slot%4:ownerOverride));word(at+8,uint16_t(ageOverride<0?(slot<4?(seed&1?0:1):0):ageOverride));byte(at+6,uint8_t(seed));byte(at+12,uint8_t(seed&7));}
for(unsigned part=0;part<5;++part){wide(0x174ae +20*part,controlSource+256*part);for(unsigned n=4;n<20;++n)byte(0x174ae +20*part+n,uint8_t(seed+n));for(unsigned n=0;n<256;++n)byte(controlSource+256*part+n,uint8_t((seed*19+n*3)%128));for(unsigned n:{0u,139u,222u,223u,224u,225u,226u})byte(controlSource+256*part+n,0);byte(controlSource+256*part+73,uint8_t(seed%3));byte(controlSource+256*part+76,uint8_t(seed%4));}
if(voiceSource)for(unsigned n=0;n<256;++n)byte(voiceSource+n,m[0x32000+n]);
byte(0x1679a,uint8_t(pending));byte(0x1679b,uint8_t(delay));byte(0x17526,uint8_t(seed%4));byte(0x16798,uint8_t(seed&4?seed%4:3));byte(0x167ba,uint8_t(seed&1));byte(0x174a6,uint8_t(countOverride<0?seed%8:countOverride));byte(0x1755e,uint8_t(seed&8?255:seed%4));wide(0x167bc,0x32000);word(0x15138,0x4470);word(0x1512e,uint16_t(seed+1));word(0x159f2,0xffff);byte(0xffec22,0);byte(0xffff62,0);
for(unsigned n=0;n<256;++n)word(0x15200+2*n,uint16_t(n*127));for(unsigned n=0;n<7;++n)word(0x15130+2*n,uint16_t(seed?0x1000+seed*31:0));
auto s=std::make_unique<T>();std::vector<uint8_t>snapshot(sizeof(T));int lastwrite=-1;unsigned count=0;uint64_t statusReads=0;uint32_t liveMask=0;
while(!s->complete()){
if(live){const auto*t=s->task<ks::VoiceRefreshTask>();auto once=[&](unsigned n){const bool fresh=!(liveMask&(1u<<n));liveMask|=1u<<n;return fresh;};
 if(s->component()==T::Component::Voice&&s->voice()->component()==ks::StateVoiceRefreshService::Component::Refresh&&t){using S=ks::VoiceRefreshTask::Stage;const auto record=0x17560u+20*t->slot();switch(t->stage()){
 case S::ReadPart:if(once(0))byte(0x17526,1);break;case S::ReadCount:if(once(1))byte(0x174a6,2);break;case S::ReadMatch:if(once(2))byte(0x17526,0);break;
 case S::ReadAge:if(once(3))word(record+8,1);break;case S::ReadOwner:if(once(4))byte(record+4,0);break;case S::ReadPatch:if(once(5))wide(record,0x32300);break;
 case S::ReadIndex:if(once(6))byte(0x174a7,255);break;default:break;}}
 const auto*mirror=s->task<ks::PartMirrorTask>();if(mirror&&mirror->stage()==ks::PartMirrorTask::Stage::Read&&mirror->position()==8&&once(7))byte(0x174ae +20*mirror->part()+8,0xa5);
}
const auto cost=s->nextClocks(lastwrite);check(cost,"Missing VoiceRefresh phase");const auto component=s->component();const auto mask=s->components();unsigned callbacks=0;std::memcpy(snapshot.data(),s.get(),sizeof(T));tracking=true;check(!s->advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},lastwrite),"Accepted insufficient VoiceRefresh budget");check(!std::memcmp(snapshot.data(),s.get(),sizeof(T)),"Rejected Deferred budget changed whole object");check(!callbacks&&s->component()==component&&s->components()==mask&&s->nextClocks(lastwrite)==cost,"Rejected VoiceRefresh budget changed state");++rejects;unsigned lastOffset=0;check(s->advanceBefore(cost,[&](uint32_t raw,unsigned dt){check(dt>=lastOffset&&dt<=cost,"Deferred read resource timestamp");lastOffset=dt;++reads;const auto a=raw&0xffffff;check(a<0xfff000||a>=0xfffc00,"Deferred read reserved CPU scratch");return a==0x400004||a==0x600004?uint8_t(statusReads++<busy?0:255):m[a];},[&](uint32_t raw,uint8_t v,unsigned dt){check(dt>=lastOffset&&dt<=cost,"Deferred write resource timestamp");lastOffset=dt;++writes;const auto a=raw&0xffffff;check(a<0xfff000||a>=0xfffc00,"Deferred write reserved CPU scratch");m[a]=v;lastwrite=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},lastwrite)==cost,"VoiceRefresh budget differs");tracking=false;if(s->effect().write)lastwrite=-1;maxDepth=std::max(maxDepth,s->voice()&&s->voice()->global()?s->voice()->global()->coordinator().depth():0);check(++count<1500000,"VoiceRefresh graph stalled");++phases;}
if(live)check(liveMask==255,"Live pure voice landing missed");components|=s->components();++graphs;};
for(unsigned mask=0;mask<256;++mask)run(mask,-1,-1,-1,0,0,false,mask);
for(unsigned delay:{1u,2u,127u,128u,255u})for(unsigned pending:{0u,31u,255u})run(0,-1,-1,-1,0,0,false,pending,delay);
for(unsigned count:{0u,1u,255u})for(unsigned age:{0u,1u,65535u})for(unsigned owner:{0u,255u})run(0,count,age,owner);
for(unsigned alias:{0x800000u,0x81fd01u,0x8fff00u,0xc181fd01u})run(0,1,-1,-1,alias);
for(unsigned busy:{1u,7u,65537u})run(0,1,-1,-1,0,busy);
run(0,-1,-1,-1,0,0,true);
for(unsigned alias:{0x80032001u,0xa32001u,0xffdc01u})run(0,-1,-1,-1,0,0,false,31,0,alias);
uint64_t guards=0;for(uint32_t bad:{0u,0x9662u,0xffefffu,0xfff000u,0xfff700u,0xffff51u}){std::fill(m.begin(),m.end(),0);m[0x1679a]=1;for(unsigned n=0;n<4;++n)m[0x174ae + n]=uint8_t(bad>>(24-8*n));check(!T::admitted([&](uint32_t a){return m[a&0xffffff];}),"Deferred admitted invalid pending patch");++guards;}
check(!allocations,"Deferred pure allocated");check(maxDepth==3,"Nested lookup fixture missed");std::cout<<"PASS DeferredParts pure graphs="<<graphs<<" phases="<<phases<<" budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" depth="<<maxDepth<<" bytes="<<sizeof(T)<<" guards="<<guards<<" allocations="<<allocations<<'\n';return 0;}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
