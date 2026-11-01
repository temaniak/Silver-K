#include "ksPeriodicService.h"
#include "ksPeriodicAbi.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 using T=ks::PeriodicService;using K=T::Kind;std::vector<uint8_t>memory(1<<24);uint64_t graphs=0,phases=0,rejects=0,writes=0;unsigned roots=0,components=0;
 for(auto kind:{K::Mark,K::Fast,K::Triggered,K::Manual})for(unsigned mode=0;mode<256;++mode)for(unsigned pattern=0;pattern<4;++pattern){
  memory[0x169f5]=uint8_t(mode);memory[0x1755c]=uint8_t(pattern);memory[0x169d7]=uint8_t(mode);memory[0x169d8]=uint8_t(mode^0x55);memory[0x169d9]=uint8_t(mode^0xaa);memory[0x1694b]=uint8_t(pattern*85);memory[0x1694a]=uint8_t(pattern*37);
  for(unsigned n=0;n<32;++n)memory[0x1532e + n]=uint8_t(mode*17+n*13);
  unsigned count=0;int previous=-1;tracking=true;{T service(kind);
   while(!service.complete()){
    const auto cost=service.nextClocks(previous);check(cost>0,"Periodic stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&service,sizeof(T));unsigned callbacks=0;
    check(!service.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous),"Periodic early admission");check(!callbacks&&!std::memcmp(before.data(),&service,sizeof(T)),"Periodic rejected budget changed object");++rejects;
    check(service.advanceBefore(cost,[&](uint32_t a,unsigned dt){check(a<(1u<<24)&&dt<=cost,"Periodic read clocks/span");if(a==0x400004||a==0x600004)return uint8_t(255);return memory[a];},[&](uint32_t a,uint8_t value,unsigned dt){check(dt<=cost&&!(a>=0xffef00&&a<0xfffd00),"Periodic write clocks/scratch");memory[a]=value;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;++writes;},previous)==cost,"Periodic full budget");if(service.effect().write)previous=-1;check(++count<4096,"Periodic did not finish");++phases;
   }
   const unsigned want=kind==K::Mark?256:kind==K::Fast?0:kind==K::Triggered?1:2;check(service.summary().children==((mode==want&&(kind!=K::Triggered||pattern))?5:0),"Periodic real childcount");if(kind==K::Triggered&&pattern)check(!memory[0x1755c],"Periodic trigger not cleared");if(kind==K::Mark&&mode==1)check(memory[0x1755c]==1,"Periodic mark not set");
   struct A{std::array<uint32_t,7>values{};uint8_t flags=0;}a;for(unsigned n=0;n<7;++n)a.values[n]=0x12345678u+n*13;const auto saved=a.values;ks::projectPeriodic(a,service);check(a.values==saved,"Periodic did not preserve caller values");
   roots|=1u<<unsigned(kind);components|=service.components();
  }tracking=false;++graphs;
 }
 check(roots==15&&components==7&&!allocations,"Periodic roots/components/allocations");std::cout<<"PASS Pure Periodic graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" writes="<<writes<<" allocations="<<allocations<<" sizeof="<<sizeof(T)<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
