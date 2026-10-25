#include "ksPanelMasksService.h"
#include "ksPanelMasksAbi.h"
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
 using T=ks::PanelMasksService;using K=T::Kind;std::vector<uint8_t>memory(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;unsigned roots=0;
 for(auto kind:{K::Service,K::Mask,K::Invert})for(unsigned input=0;input<256;++input)for(unsigned pattern=0;pattern<16;++pattern){
  memory[0xffec1b]=uint8_t(pattern&1);memory[0xffec29]=uint8_t(pattern&2);memory[0xffec1a]=uint8_t(input);memory[0xffec28]=uint8_t(255-input);memory[0xffec20]=uint8_t(input^pattern);memory[0xffff62]=uint8_t(input+pattern);
  for(unsigned n=0;n<4;++n){memory[0xffec16+n]=uint8_t((pattern&(1u<<n))?0:8);memory[0xffec24+n]=uint8_t((pattern&(1u<<n))?8:0);}
  unsigned count=0;tracking=true;{T service(kind,{uint8_t(input)});
   while(!service.complete()){
    const auto cost=service.nextClocks();check(cost>0,"Mask phase stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&service,sizeof(T));unsigned callbacks=0;
    check(!service.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;}),"Early mask admission");check(!callbacks&&!std::memcmp(before.data(),&service,sizeof(T)),"Rejected mask budget changed object");++rejects;
    check(service.advanceBefore(cost,[&](uint32_t a,unsigned dt){check((a>=0xffec16&&a<=0xffec29)||a==0xffff62,"Mask read span");check(dt<=cost,"Mask read clocks");++reads;return memory[a];},[&](uint32_t a,uint8_t value,unsigned dt){check((a>=0xffec16&&a<=0xffec29)||a==0xffff61||a==0xffff62,"Mask write span");check(dt<=cost,"Mask write clocks");memory[a]=value;++writes;})==cost,"Mask full budget");check(++count<1000,"Mask did not finish");++phases;
   }
   check(service.nextClocks()==0,"Complete mask has work");roots|=1u<<unsigned(kind);
   struct A{std::array<uint32_t,7>values{};uint8_t flags=0;}a;ks::projectPanelMasks(a,service);
   if(service.summary().ran){const auto&t=service.maskTask();check(uint8_t(a.values[0])==t.previous(),"Mask outcome projection");if(service.summary().changed)check(memory[0xffff62]&32,"Final strobe missing");}
  }tracking=false;++graphs;
 }
 check(!allocations&&roots==7,"Mask allocation/roots");std::cout<<"PASS Pure PanelMasks graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" allocations="<<allocations<<" sizeof="<<sizeof(T)<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
