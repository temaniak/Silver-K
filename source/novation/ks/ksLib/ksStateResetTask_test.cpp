#include "ksStateResetService.h"
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
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){try{
 using T=ks::StateResetService;using E=std::tuple<uint32_t,uint8_t,unsigned>;
 auto memory=std::make_unique<std::array<uint8_t,0x16000>>();
 uint64_t cases=0,phases=0,rejects=0;uint32_t components=0;
 for(unsigned kind=0;kind<4;++kind)for(unsigned seed=0;seed<1024;++seed){
  memory->fill(uint8_t(seed));const auto before=*memory;T service{static_cast<T::Kind>(kind)};
  std::array<E,12> writes{};unsigned count=0,elapsed=0;
  while(!service.complete()){
   const auto cost=service.nextClocks();check(cost,"Reset stalled");const auto component=service.component();const auto mask=service.components();
   for(unsigned budget=0;budget<cost;++budget){
    track=true;const auto used=service.advanceBefore(budget,[](uint32_t,unsigned){throw std::runtime_error("Reset read a byte");return uint8_t(0);},[](uint32_t,uint8_t,unsigned){throw std::runtime_error("Under-budget reset wrote a byte");});track=false;
    check(!used&&service.nextClocks()==cost&&service.component()==component&&service.components()==mask,"Under-budget reset mutated its continuation");++rejects;
   }
   track=true;const auto used=service.advanceBefore(cost,[](uint32_t,unsigned){throw std::runtime_error("Reset read a byte");return uint8_t(0);},[&](uint32_t address,uint8_t value,unsigned offset){
    check(address<memory->size()&&count<writes.size()&&offset==cost,"Reset byte resource shape differs");(*memory)[address]=value;writes[count++]={address,value,elapsed+offset};
   });track=false;
   check(used==cost,"Reset phase cost differs");elapsed+=cost;++phases;
  }
  const std::array<uint32_t,3> producers{{0x1537c,0x15406,0x156e8}};
  std::array<E,12> expected{};unsigned n=0;
  const unsigned first=kind?kind-1:0,last=kind?kind:3;
  for(unsigned ring=first;ring<last;++ring){
   const unsigned start=kind?0:ring*86+20;
   for(unsigned byte=0;byte<4;++byte)expected[n++]={producers[ring]+byte,0,start+(byte<2?26:38)};
  }
  check(count==n&&std::equal(writes.begin(),writes.begin()+count,expected.begin()),"Reset order/value/clock differs");
  check(elapsed==(kind?66u:275u),"Complete reset clocks differ");
  auto wanted=before;for(unsigned i=0;i<count;++i)wanted[std::get<0>(writes[i])]=0;
  check(*memory==wanted,"Reset changed data outside its indices");
  check(service.components()==(kind?1u<<kind:15u),"Reset component coverage differs");components|=service.components();++cases;
 }
 check(components==15&&!allocations,"Reset graph incomplete or allocated");
 std::cout<<"PASS: "<<cases<<" pure reset calls; "<<phases<<" phases; "<<rejects<<" rejected budgets; all four components, exact ordered bytes/clocks, zero allocations\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
