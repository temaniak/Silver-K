#include "ksMidiFileEventProofPrelude.h"
#include "ksNativeArpOwnedService.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
struct Observer{uint64_t notices=0;template<class...A>void operator()(const A&...){++notices;}};
struct Access{uint32_t address;unsigned dt;uint8_t value;bool write;bool operator==(const Access&a)const{return address==a.address&&dt==a.dt&&value==a.value&&write==a.write;}};
int main(){try{using T=ks::NativeArpOwnedService;std::vector<uint8_t>m(1<<24),m2(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0,notices=0;uint32_t components=0;FileResolver resolver;auto gs=std::make_unique<uint8_t[]>(sizeof(T)),hs=std::make_unique<uint8_t[]>(sizeof(T)),snapshot=std::make_unique<uint8_t[]>(sizeof(T));
 for(unsigned seed=0;seed<64;++seed){std::fill(m.begin(),m.end(),uint8_t(0));const unsigned part=seed%5,rec=0x160d2+334*part;const auto word=[&](unsigned at,uint16_t v){m[at]=uint8_t(v>>8);m[at+1]=uint8_t(v);};const auto wide=[&](unsigned at,uint32_t v){word(at,uint16_t(v>>16));word(at+2,uint16_t(v));};
 for(unsigned p=0;p<5;++p)wide(0x174aeu+20*p,0x18000);m[0x17526]=uint8_t(part);m[0x174a6]=4;m[0x18000]=16;m[0x18000+173]=8;m[0x18000+85]=60;m[0x18000+86]=uint8_t(seed);m[0x18000+88]=uint8_t(seed%2?20:0);m[rec+328]=60;m[rec+324]=uint8_t(seed);m[rec+327]=uint8_t(seed^17);m[rec+317]=255;wide(rec+8,0x2e000);m[rec+315]=4;word(0x2e000,0x0400);word(0x2e002,0x0001);wide(rec+48,0x2f000);wide(rec+52,0x2f001);wide(rec+56,0x10000000);m[0x2f000]=0;m[0xffff8a]=0xf0;m2=m;
 Observer observer;unsigned local=0;int previous=-1,previous2=-1,lastread=-1,lastread2=-1;tracking=true;{
 auto*gptr=::new(gs.get())T(seed%2?T::Kind::Schedule:T::Kind::Main,{0x1234643c,0x45});auto*hptr=::new(hs.get())T(seed%2?T::Kind::Schedule:T::Kind::Main,{0x1234643c,0x45});auto&g=*gptr;auto&h=*hptr;while(!g.complete()){
 struct Unknown{};check(!g.task<Unknown>(),"Unknown Main task");components|=1u<<unsigned(g.component());const auto cost=g.nextClocks(previous,lastread);check(cost&&h.nextClocks(previous2,lastread2)==cost,"Main clock/noop stalled");check(g.critical()==h.critical(),"Main noop priority differs");std::memcpy(snapshot.get(),&h,sizeof(T));unsigned callbacks=0;const auto oldNotices=observer.notices;
 check(!h.advanceObservedBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},resolver,observer,previous2,lastread2)&&!callbacks&&observer.notices==oldNotices&&!std::memcmp(snapshot.get(),&h,sizeof(T)),"Main underbudget mutated graph/observer");++rejects;
 std::array<Access,16>events{},events2{};unsigned n=0,n2=0;
 const auto run=[&](T&job,std::vector<uint8_t>&memory,int&p,int&r,auto&list,unsigned&count,bool observed){
 const auto read=[&](uint32_t a,unsigned dt){check(a<memory.size()&&dt&&dt<=cost&&count<list.size(),"Main read bounds/time");const auto v=a==0x400004||a==0x600004?uint8_t(255):memory[a];list[count++]={a,dt,v,false};r=history(a);if(!observed)++reads;return v;};
 const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(a<memory.size()&&dt&&dt<=cost&&!(a>=0xfff000&&a<0xfffc00)&&count<list.size(),"Main write bounds/time");list[count++]={a,dt,v,true};memory[a]=v;p=a<0x400000?int(a):-1;if(!observed)++writes;};
 const auto used=observed?job.advanceObservedBefore(cost,read,write,resolver,observer,p,r):job.advanceBefore(cost,read,write,resolver,p,r);check(used==cost,"Main phase admission");if(job.effect().write)p=-1;if(job.effect().read)r=-1;};
 run(g,m,previous,lastread,events,n,false);run(h,m2,previous2,lastread2,events2,n2,true);check(n==n2&&std::equal(events.begin(),events.begin()+n,events2.begin()),"Main observing/noop resource difference");check(previous==previous2&&lastread==lastread2&&g.effect().read==h.effect().read&&g.effect().write==h.effect().write&&g.complete()==h.complete(),"Main observing/noop history/lifetime");check(++local<100000,"Main pure noncompletion");++phases;}
 check(g.expressionSourceAfter(0)==h.expressionSourceAfter(0)&&g.activationAfter(0)==h.activationAfter(0),"Main pure observer influenced context");check(!std::memcmp(m.data(),m2.data(),m.size()),"Main pure observing memory difference");notices+=observer.notices;g.~T();h.~T();
 }tracking=false;++graphs;}
 check(!allocations,"Main heap");std::cout<<"PASS Pure NativeArpOwnedMainSchedule graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" observer_notices="<<notices<<" noop_exact=1 sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
