#include "ksSerialReceiveService.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
namespace {
bool measuring=false;uint64_t allocations=0,cases=0,phases=0,rejections=0;uint32_t components=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Resources{
 std::array<uint8_t,0x1400> bytes{};
 uint8_t& at(uint32_t address){check(address>=0xffec00&&address<=0xffffff,"Unexpected SCI resource");return bytes[address-0xffec00];}
 uint16_t word(uint32_t a){return uint16_t((unsigned(at(a))<<8)|at(a+1));}
 uint32_t wide(uint32_t a){return(uint32_t(word(a))<<16)|word(a+2);}
 void word(uint32_t a,uint16_t v){at(a)=uint8_t(v>>8);at(a+1)=uint8_t(v);}
 void wide(uint32_t a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));}
};
struct Config{unsigned data=0xf8,producer=45,consumer=17,status=0xf7,source=0,active=1,flags=0xa4,
 pulseCount=0xffff,pulses=0x1234,fraction=0x5678,samples=0,baseline=0,counter=20000,
 estimate=0x12345678,gain=0x2000,integral=0,sum=0;};
struct Transfer{uint32_t address;uint8_t value;unsigned offset;bool writing;
 bool operator==(const Transfer&b)const{return address==b.address&&value==b.value&&offset==b.offset&&writing==b.writing;}};
struct Harness{
 ks::SerialReceiveService task;Resources resource;std::array<Transfer,8>transfers{};size_t count=0;
 uint64_t clocks=0,digest=0xcbf29ce484222325ull;unsigned cost=0,steps=0,counterReads=0;
 explicit Harness(const Config&c){
  resource.at(0xffff8c)=uint8_t(c.status);resource.at(0xffff8d)=uint8_t(c.data);resource.at(0xffed68)=0x7b;
  resource.word(0xffec64,uint16_t(c.producer));resource.word(0xffec66,uint16_t(c.consumer));
  for(unsigned n=0;n<256;++n)resource.at(0xffec68+n)=uint8_t(n*149);
  resource.at(0xffee9a)=uint8_t(c.source);resource.at(0xffee96)=uint8_t(c.active);resource.at(0xffee97)=uint8_t(c.flags);
  resource.word(0xffee78,uint16_t(c.pulses));resource.word(0xffee7a,uint16_t(c.fraction));resource.word(0xffeea8,uint16_t(c.pulseCount));
  resource.word(0xffee9c,uint16_t(c.samples));resource.word(0xffee9e,uint16_t(c.baseline));resource.word(0xffffe6,uint16_t(c.counter));
  resource.wide(0xffeea0,c.estimate);resource.word(0xffeea4,uint16_t(c.gain));resource.word(0xffeea6,uint16_t(c.integral));resource.wide(0xffeeaa,c.sum);
 }
 void event(uint32_t a,uint8_t v,unsigned offset,bool writing){
  check(offset&&offset<=cost,"SCI callback crossed phase budget");
  if(count)check(offset>=transfers[count-1].offset,"SCI callbacks reversed clock order");
  check(count<transfers.size(),"SCI callback fanout exceeded bound");
  transfers[count++]={a,v,offset,writing};digest^=uint64_t(a)|(uint64_t(v)<<24)|(uint64_t(writing)<<32);digest*=1099511628211ull;digest^=clocks+offset;digest*=1099511628211ull;
 }
 unsigned step(unsigned available){count=0;cost=task.nextClocks();
  const auto used=task.advanceBefore(available,[&](uint32_t a,unsigned offset){auto v=resource.at(a);if(a==0xffffe6)++counterReads;event(a,v,offset,false);return v;},
   [&](uint32_t a,uint8_t v,unsigned offset){event(a,v,offset,true);resource.at(a)=v;});
  check(!used||used==cost,"SCI crossed a logical phase boundary");
  if(used){clocks+=used;++steps;}return used;
 }
};
void advance(Harness&a,Harness&b){
 const auto cost=a.task.nextClocks();check(cost&&a.steps<256,"SCI did not make bounded progress");
 const auto component=a.task.component();const auto mask=a.task.components();const auto digest=a.digest;const auto counters=a.counterReads;
 for(auto available:{0u,cost-1}){
  check(!a.step(available)&&!a.count&&a.task.component()==component&&a.task.components()==mask&&a.task.nextClocks()==cost&&a.digest==digest&&a.counterReads==counters,"Rejected SCI budget changed continuation/resources");++rejections;
 }
 check(a.step(cost)==cost&&b.step(std::numeric_limits<unsigned>::max())==cost,"SCI admitted phase was not exact");
 check(a.task.component()==b.task.component()&&a.task.components()==b.task.components()&&a.task.complete()==b.task.complete()&&a.task.nextClocks()==b.task.nextClocks(),"SCI continuation depends on budget");
 check(a.digest==b.digest&&a.clocks==b.clocks&&a.count==b.count,"SCI transfer order/time depends on budget");
 for(size_t i=0;i<a.count;++i)check(a.transfers[i]==b.transfers[i],"SCI callback differs with budget");
 if(a.count==2&&a.transfers[0].address==0xffee97){
  check(!a.transfers[0].writing&&a.transfers[0].offset==9&&a.transfers[1].writing&&a.transfers[1].offset==12,"Source flag RMW timing differs");
 }
 if(a.count==2&&a.transfers[0].address==0xffffe6)
  check(a.transfers[0].offset==7&&a.transfers[1].address==0xffffe7&&a.transfers[1].offset==8,"Live counter transfer timing differs");
 ++phases;
}
// A scalar resource-level oracle: signed division and modular arithmetic are
// stated directly, independently of the staged graph and its continuations.
void expected(Resources&r,const Config&c){
 r.at(0xffff8c)=uint8_t(c.status&0xbf);
 if(c.data!=0xf8){
  const auto next=(c.producer+1)&255;r.at(0xffec68+c.producer)=uint8_t(c.data);
  if(next==c.consumer)r.at(0xffed68)=2;else r.word(0xffec64,uint16_t(next));return;
 }
 r.at(0xffee97)=uint8_t(c.flags|1);
 if(!c.active||c.source==255){r.word(0xffee9e,uint16_t(c.counter));r.word(0xffee9c,0);r.wide(0xffeeaa,0);r.word(0xffeea8,uint16_t(c.pulses));}
 else{
  const auto pulse=uint16_t(c.pulseCount+1);r.word(0xffeea8,pulse);
  const uint32_t timer=(c.pulses<<16)|uint16_t(c.fraction),raw=(timer-(uint32_t(pulse)<<16))&0xffffff;
  const auto difference=(raw&0x800000)?raw|0xff000000:raw;
  const uint32_t sum=c.sum+difference;const auto samples=uint16_t(c.samples+512);const auto elapsed=uint16_t(c.counter-c.baseline);
  r.wide(0xffeeaa,sum);r.word(0xffee9c,samples);
  if(elapsed>=16000){
   r.word(0xffee9e,uint16_t(c.counter));r.word(0xffee9c,0);r.wide(0xffeeaa,0);
   const int64_t n=int32_t(sum),d=int16_t(samples);const auto average=uint16_t(d?n/d:(n?uint16_t(sum):1));
   const auto rate=uint16_t((uint32_t(samples)*50000u)/elapsed);
   const auto error=uint16_t(rate-uint16_t(c.estimate>>16));
   const uint32_t correction=uint32_t(int32_t(int16_t(error))*int32_t(int16_t(c.gain)))<<1;
   const uint32_t estimate=c.estimate+correction+correction;r.wide(0xffeea0,estimate);
   r.wide(0xffee7e,(estimate>>2)-uint32_t(int32_t(int16_t(average))*16384));
   const auto integral=uint16_t(c.gain==0x4000?0:uint16_t(c.integral+error));
   const uint32_t integralProduct=uint32_t(int32_t(int16_t(integral))*0x7e00)<<1;
   const auto smoothed=uint16_t(integralProduct>>16);r.word(0xffeea6,smoothed);
   const auto magnitude=uint16_t((integralProduct&0x80000000u)?0u-smoothed:smoothed);
   auto gain=uint16_t(c.gain-0x800);if((gain&0x8000)||gain<0x80)gain=0x80;
   r.word(0xffeea4,magnitude>0x1800?0x4000:gain);
  }
 }
 r.at(0xffee9a)=0;
}
void run(Config c,bool liveCounter=false){
 Harness a(c),b(c);Resources result=a.resource;
 bool changed=false;
 while(!a.task.complete()){
  if(liveCounter&&!changed&&a.task.at(ks::SerialReceiveService::Component::Counter)){
   c.counter=0xf123;a.resource.word(0xffffe6,uint16_t(c.counter));b.resource.word(0xffffe6,uint16_t(c.counter));result.word(0xffffe6,uint16_t(c.counter));changed=true;
  }
  advance(a,b);
 }
 check(a.resource.bytes==b.resource.bytes,"SCI resources depend on phase budget");
 expected(result,c);check(a.resource.bytes==result.bytes,"SCI scalar resource result differs");
 check(a.counterReads==unsigned(c.data==0xf8)&&(!liveCounter||changed),"SCI did not sample its live counter exactly once");
 check(!a.task.nextClocks()&&!a.step(10000)&&!a.count,"Completed SCI task restarted");
 components|=a.task.components();++cases;
}
}
void* operator new(std::size_t n){if(measuring)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){try{
 measuring=true;Config c;
 for(unsigned data=0;data<256;++data)for(unsigned producer=0;producer<256;++producer)for(bool full:{false,true}){c=Config{};c.data=data;c.producer=producer;c.consumer=(producer+unsigned(full?1:2))&255;run(c);}
 for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;++consumer){c=Config{};c.data=0x7f;c.producer=producer;c.consumer=consumer;run(c);}
 for(unsigned active:{0u,1u,255u})for(unsigned source=0;source<256;++source){c=Config{};c.active=active;c.source=source;run(c);}
 for(unsigned samples:{0u,1u,0x7dffu,0x7e00u,0xfdffu,0xfe00u,0xffffu})
 for(unsigned gain:{0u,0x7fu,0x80u,0x7ffu,0x800u,0x3fffu,0x4000u,0x7fffu,0x8000u,0xffffu})
 for(unsigned integral:{0u,1u,0x17ffu,0x1800u,0x7fffu,0x8000u,0xffffu})
 for(unsigned sum:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}){c=Config{};c.samples=samples;c.gain=gain;c.integral=integral;c.sum=sum;run(c);}
 for(unsigned pulseCount:{0u,0x7fffu,0xffffu})for(unsigned pulses:{0u,0x7fu,0x80u,0xffu,0x100u,0x7fffu,0x8000u,0xffffu})for(unsigned fraction:{0u,0x7fffu,0x8000u,0xffffu}){c=Config{};c.pulseCount=pulseCount;c.pulses=pulses;c.fraction=fraction;c.samples=0xfe00;run(c);}
 for(unsigned elapsed:{0u,1u,15999u,16000u,16001u,32767u,32768u,65535u})for(unsigned counter:{0u,1u,0x7fffu,0x8000u,0xffffu}){c=Config{};c.counter=counter;c.baseline=uint16_t(counter-elapsed);run(c);}
 c=Config{};run(c,true);c.active=0;run(c,true);
 measuring=false;check(components==0xf&&!allocations,"SCI coverage/allocation contract failed");
 std::printf("PASS: %llu CPU-independent SCI receive/F8 cases; %llu phases; %llu rejected budgets; components=0x%x; allocations=%llu; all bytes/ring positions, full queue, reset, feedback, gain/integral/signed24/wrap/divzero and live counter verified\n",static_cast<unsigned long long>(cases),static_cast<unsigned long long>(phases),static_cast<unsigned long long>(rejections),components,static_cast<unsigned long long>(allocations));return 0;
 }catch(const std::exception&e){measuring=false;std::fprintf(stderr,"%s\n",e.what());return 1;}}
