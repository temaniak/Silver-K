#include "ksAdcService.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <tuple>
namespace {bool track=false;uint64_t allocations=0;}
void* operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
using Transfer=std::tuple<bool,uint32_t,uint8_t,unsigned>;
struct Resources {
 std::array<uint8_t,0x20000> low{};std::array<uint8_t,512> io{};
 std::array<Transfer,4> transfers{};unsigned count=0,cost=0;
 uint8_t& at(uint32_t address){if(address<low.size())return low[address];check(address>=0xfffe00&&address<=0xffffff,"ADC resource address invalid");return io[address&511];}
 void observe(bool writing,uint32_t address,uint8_t value,unsigned offset){check(offset<=cost&&count<transfers.size(),"ADC transfer clock/capacity invalid");if(count)check(offset>=std::get<3>(transfers[count-1]),"ADC transfer offsets reversed");transfers[count++]={writing,address,value,offset};}
 uint8_t read(uint32_t address,unsigned offset){const auto value=at(address);observe(false,address,value,offset);return value;}
 void write(uint32_t address,uint8_t value,unsigned offset){observe(true,address,value,offset);at(address)=value;}
 void word(uint32_t address,uint16_t value){at(address)=uint8_t(value>>8);at(address+1)=uint8_t(value);}
 uint16_t word(uint32_t address){return uint16_t((unsigned(at(address))<<8)|at(address+1));}
 void seed(unsigned remaining,unsigned group,unsigned raw,unsigned previous,unsigned armed,unsigned curve,unsigned producer){
  low.fill(0);io.fill(0);for(unsigned n=0;n<36;++n){at(0xb918+n)=uint8_t(n);at(0xb93c+n)=uint8_t(curve);}
  const auto offset=uint8_t(group<<1);
  for(unsigned n=0;n<4;++n){word(0xffff90+2*n,uint16_t(raw));word(0x15652+offset+2*n,uint16_t(previous));word(0x1560a+offset+2*n,0);at(0x1569a+offset+2*n)=uint8_t(armed);}
  word(0x15406,uint16_t(producer));at(0x156e2)=uint8_t(group);at(0x156e3)=uint8_t(remaining);at(0x156e4)=255;at(0xffff98)=0xf3;
 }
};
}
int main(){try{
 auto a=std::make_unique<Resources>(),b=std::make_unique<Resources>();uint64_t cases=0,phases=0,rejects=0;uint32_t components=0;
 const auto run=[&](unsigned remaining,unsigned group,unsigned raw,unsigned previous,unsigned armed,unsigned curve,unsigned producer){
  a->seed(remaining,group,raw,previous,armed,curve,producer);*b=*a;ks::AdcService service,reference;unsigned count=0;
  while(!service.complete()){
   const auto cost=service.nextClocks();check(cost&&cost==reference.nextClocks(),"ADC phase availability differs");a->count=b->count=0;a->cost=b->cost=cost;
   const auto component=service.component();const auto mask=service.components();
   track=true;const auto rejected=service.advanceBefore(cost-1,[&](uint32_t at,unsigned clock){return a->read(at,clock);},[&](uint32_t at,uint8_t value,unsigned clock){a->write(at,value,clock);});track=false;
   check(!rejected&&!a->count&&service.component()==component&&service.components()==mask&&service.nextClocks()==cost,"Insufficient ADC budget changed state");++rejects;
   track=true;const auto used=service.advanceBefore(cost,[&](uint32_t at,unsigned clock){return a->read(at,clock);},[&](uint32_t at,uint8_t value,unsigned clock){a->write(at,value,clock);});
   const auto expected=reference.advanceBefore(cost,[&](uint32_t at,unsigned clock){return b->read(at,clock);},[&](uint32_t at,uint8_t value,unsigned clock){b->write(at,value,clock);});track=false;
   check(used==cost&&expected==cost&&service.component()==reference.component()&&service.complete()==reference.complete(),"Budgeted ADC phase differs");
   check(a->count==b->count&&std::equal(a->transfers.begin(),a->transfers.begin()+a->count,b->transfers.begin()),"Budgeted ADC transfers differ");
   check(++count<8192,"Pure ADC graph stalled");++phases;
  }
  check(a->low==b->low&&a->io==b->io,"Budgeted ADC final resources differ");
  if(remaining>1){check(a->at(0x156e3)==remaining-1&&a->at(0xffff98)==0x73&&service.components()==1,"ADC quick completion differs");}
  if(remaining==1){check(a->at(0x156e3)==0&&a->at(0xffff98)==0x73&&service.components()==3,"ADC settle completion differs");}
  components|=service.components();++cases;
 };
 for(unsigned remaining=0;remaining<256;++remaining)for(unsigned group:{0u,28u,32u,255u})for(unsigned armed:{0u,1u})for(unsigned curve:{0u,1u})for(unsigned producer:{0u,1u,0x1feu,0x1ffu})for(unsigned raw:{0u,1u,0x7fffu,0x8000u,0xffffu})run(remaining,group,raw,raw^1,armed,curve,producer);
 for(unsigned group:{0u,28u,32u}){
  run(0,group,0x8000,0x8000,0,0,0x1fe);
  check(a->at(0x156e2)==group+4,"ADC next group differs");
  check(a->word(0x15406)==(group==32?6:14),"ADC event queue wrap differs");
  for(unsigned n=0;n<4;++n)check(a->word(0x1560a+group*2+2*n)==0x7f00,"ADC accepted history differs");
  if(group==32)check(a->at(0xffff98)==0x53&&a->at(0x156e3)==0,"ADC terminal special group restarted conversion");
  else check(a->at(0xffff98)==0x73&&a->at(0x156e3)==9&&a->at(0xffff6e)==(group+4)/4,"ADC mux conversion restart differs");
 }
 check(components==0x3f&&!allocations,"ADC pure graph incomplete or allocated");
 std::cout<<"PASS: "<<cases<<" pure ADC calls; "<<phases<<" phases; "<<rejects<<" insufficient-budget rejections; all six components, queue wrap and threshold/sample/mux state; zero allocations\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
