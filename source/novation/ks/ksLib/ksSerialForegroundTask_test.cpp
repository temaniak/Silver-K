#include "ksSerialForegroundService.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
namespace {
bool measuring=false;uint64_t allocations=0,cases=0,phases=0,rejections=0;unsigned kinds=0,components=0;
void check(bool value,const char*why){if(!value)throw std::runtime_error(why);}
using K=ks::SerialForegroundService::Kind;
struct Resources{
 std::array<uint8_t,0x1400>bytes{};
 uint8_t&at(uint32_t a){check(a>=0xffec00&&a<=0xffffff,"Unexpected foreground resource");return bytes[a-0xffec00];}
 uint16_t word(uint32_t a){return uint16_t((unsigned(at(a))<<8)|at(a+1));}
 void word(uint32_t a,uint16_t v){at(a)=uint8_t(v>>8);at(a+1)=uint8_t(v);}
};
struct Config{K kind=K::PopRx;unsigned producer=45,consumer=17,status=0,data=0x42;bool blocking=false;};
struct Transfer{uint32_t address;uint8_t value;unsigned offset;bool writing;bool operator==(const Transfer&b)const{return address==b.address&&value==b.value&&offset==b.offset&&writing==b.writing;}};
struct Harness{
 ks::SerialForegroundService task;Resources resource;std::array<Transfer,8>transfers{};size_t count=0;
 uint64_t clocks=0,digest=0xcbf29ce484222325ull;unsigned cost=0,steps=0,consumerReads=0,criticalSteps=0,dataWrites=0;
 explicit Harness(const Config&c):task(c.kind,uint8_t(c.data)){
  resource.word(0xffec64,uint16_t(c.producer));resource.word(0xffec66,uint16_t(c.consumer));resource.word(0xffed6c,uint16_t(c.producer));resource.word(0xffed6e,uint16_t(c.consumer));
  for(unsigned n=0;n<256;++n){resource.at(0xffec68+n)=uint8_t(n*149);resource.at(0xffed70+n)=uint8_t(n*37);}
  resource.at(0xffec68+c.consumer)=uint8_t(c.data);resource.at(0xffed68)=uint8_t(c.status);resource.at(0xffff8a)=0xf0;
  if(c.blocking){resource.word(0xffec64,0);resource.word(0xffec66,0);resource.at(0xffed68)=2;}
 }
 void event(uint32_t a,uint8_t v,unsigned offset,bool writing){
  check(offset&&offset<=cost,"Foreground callback crossed clock budget");if(count)check(offset>=transfers[count-1].offset,"Foreground callbacks reversed clock order");check(count<transfers.size(),"Foreground fanout exceeded bound");
  transfers[count++]={a,v,offset,writing};digest^=uint64_t(a)|(uint64_t(v)<<24)|(uint64_t(writing)<<32);digest*=1099511628211ull;digest^=clocks+offset;digest*=1099511628211ull;
 }
 unsigned step(unsigned available){count=0;cost=task.nextClocks();const auto used=task.advanceBefore(available,
  [&](uint32_t a,unsigned offset){const auto v=resource.at(a);if(a==0xffed6e)++consumerReads;event(a,v,offset,false);return v;},
  [&](uint32_t a,uint8_t v,unsigned offset){event(a,v,offset,true);if(a>=0xffed70&&a<0xffee70)++dataWrites;if(a==0xffff8a)check(task.critical()&&v==0xf0,"TX enable escaped critical section");resource.at(a)=v;});
  check(!used||used==cost,"Foreground crossed logical phase boundary");if(used){clocks+=used;++steps;criticalSteps+=task.critical();}return used;
 }
};
void advance(Harness&a,Harness&b){
 const auto cost=a.task.nextClocks();check(cost&&a.steps<1024,"Foreground graph did not make bounded progress");const auto component=a.task.component();const auto critical=a.task.critical();const auto digest=a.digest,clock=a.clocks;const auto reads=a.consumerReads;
 for(auto budget:{0u,cost-1}){check(!a.step(budget)&&!a.count&&a.task.component()==component&&a.task.critical()==critical&&a.task.nextClocks()==cost&&a.digest==digest&&a.clocks==clock&&a.consumerReads==reads,"Rejected foreground budget changed resources/continuation");++rejections;}
 check(a.step(cost)==cost&&b.step(std::numeric_limits<unsigned>::max())==cost,"Foreground phase did not finish exactly once");
 check(a.task.component()==b.task.component()&&a.task.critical()==b.task.critical()&&a.task.complete()==b.task.complete()&&a.task.nextClocks()==b.task.nextClocks()&&a.count==b.count&&a.digest==b.digest&&a.clocks==b.clocks,"Foreground phase budget changed continuation/resources/timing");
 for(size_t i=0;i<a.count;++i)check(a.transfers[i]==b.transfers[i],"Foreground callback differs with phase budget");components|=1u<<unsigned(a.task.component());++phases;
}
void run(Config c){
 Harness a(c),b(c);Resources expected=a.resource;unsigned attempts=0;bool released=false;
 while(!a.task.complete()){
  if(c.kind==K::Transmit&&((c.producer+1)&255)==c.consumer&&!released&&a.consumerReads==3&&a.task.atTransmit(ks::SerialTransmitTask::Stage::ReadConsumer)){
   check(a.dataWrites==1&&!a.task.critical()&&a.resource.word(0xffed6c)==c.producer,"Full TX queue did not wait without duplication/masking");
   c.consumer=(c.consumer+1)&255;a.resource.word(0xffed6e,uint16_t(c.consumer));b.resource.word(0xffed6e,uint16_t(c.consumer));expected.word(0xffed6e,uint16_t(c.consumer));released=true;
  }
  if(c.blocking&&a.task.atReceive(ks::SerialReceiveTask::Stage::CompareStatus)&&attempts<3){
   const auto n=attempts++;for(auto*h:{&a,&b}){h->resource.at(0xffec68+n)=uint8_t(n==0?0xf8:n==1?0x35:0xf7);h->resource.word(0xffec64,uint16_t(n+1));h->resource.at(0xffed68)=n==1?2:0;}
   expected.at(0xffec68+n)=uint8_t(n==0?0xf8:n==1?0x35:0xf7);
  }
  advance(a,b);
 }
 check(!a.task.critical()&&!a.task.nextClocks()&&!a.step(100000)&&!a.count,"Completed foreground graph restarted or retained critical section");
 check(a.resource.bytes==b.resource.bytes,"Foreground final resources depend on phase budget");
 uint16_t result=0;
 if(c.kind==K::ResetRx){expected.word(0xffec64,0);expected.word(0xffec66,0);expected.at(0xffed68)=0;}
 else if(c.kind==K::ResetTx){expected.word(0xffed6c,0);expected.word(0xffed6e,0);}
 else if(c.kind==K::Transmit){expected.at(0xffed70+c.producer)=uint8_t(c.data);expected.word(0xffed6c,uint16_t((c.producer+1)&255));expected.at(0xffff8a)=0xf0;check(a.dataWrites==1&&a.criticalSteps==3,"TX emitted duplicate data or wrong critical window");}
 else if(c.blocking){expected.word(0xffec64,3);expected.word(0xffec66,3);expected.at(0xffed68)=0;result=0x1f7;check(attempts==3,"Blocking receive omitted rejection/retry graph");}
 else{expected.at(0xffed68)=0;result=uint16_t(c.status<<8);if(c.producer!=c.consumer){expected.word(0xffec66,uint16_t((c.consumer+1)&255));result=uint16_t(((c.status|1)<<8)|c.data);}}
 check(a.resource.bytes==expected.bytes,"Foreground semantic resource result differs");
 if(c.kind==K::PopRx)check(a.task.result()==result,"Pop result/status/data differs");
 if(c.kind==K::Receive)check(a.task.result()==result,"Blocking receive accepted result differs");
 kinds|=1u<<unsigned(c.kind);++cases;
}
}
void*operator new(std::size_t n){if(measuring)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){try{
 measuring=true;Config c;
 for(auto kind:{K::PopRx,K::Transmit})for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer){c=Config{};c.kind=kind;c.producer=producer;c.consumer=consumer;run(c);}
 for(unsigned status=0;status<256;++status)for(unsigned data=0;data<256;++data){c=Config{};c.status=status;c.data=data;run(c);}
 for(auto kind:{K::ResetRx,K::ResetTx})for(unsigned n=0;n<256;++n){c=Config{};c.kind=kind;c.producer=n;c.consumer=255-n;c.status=n;run(c);}
 for(unsigned data=0;data<256;++data){c=Config{};c.kind=K::Transmit;c.data=data;c.producer=data;c.consumer=(data+23)&255;run(c);}
 for(unsigned data=0;data<248;++data){c=Config{};c.kind=K::Receive;c.data=data;run(c);}
 c=Config{};c.kind=K::Receive;c.blocking=true;run(c);
 measuring=false;check(kinds==0x1f&&components==7&&!allocations,"Foreground graph/allocation coverage incomplete");
 std::printf("PASS: %llu CPU-independent foreground UART graphs; %llu phases; %llu rejected budgets; kinds=0x%x components=0x%x allocations=%llu; all queue positions/status/data, full TX release, blocking Receive error/F8 retries, exact callback timing and phase budgets verified\n",static_cast<unsigned long long>(cases),static_cast<unsigned long long>(phases),static_cast<unsigned long long>(rejections),kinds,components,static_cast<unsigned long long>(allocations));return 0;
 }catch(const std::exception&e){measuring=false;std::fprintf(stderr,"%s\n",e.what());return 1;}}
