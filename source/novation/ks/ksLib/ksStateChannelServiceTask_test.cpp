#include "ksStateChannelService.h"
#include "ksStateChannelAbi.h"
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
 using T=ks::StateChannelService;using K=T::Kind;std::vector<uint8_t>memory(1<<24);uint64_t graphs=0,phases=0,rejects=0;unsigned roots=0;
 for(auto kind:{K::ModeStatus,K::GlobalMirror})for(unsigned input=0;input<256;++input)for(unsigned mode:{0u,1u,255u})for(int previous:{-1,0x167d5,0x169b0,0x174b8}){
  memory[0x167ba]=uint8_t(mode);memory[0x167d5]=uint8_t(input);memory[0x169ee]=uint8_t(input);unsigned count=0;int history=previous;tracking=true;{T service(kind);
   while(!service.complete()){
    const auto cost=service.nextClocks(history);check(cost>0,"Channel stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&service,sizeof(T));unsigned callbacks=0;
    check(!service.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},history),"Channel early admission");check(!callbacks&&!std::memcmp(before.data(),&service,sizeof(T)),"Channel rejected budget changed object");++rejects;
    check(service.advanceBefore(cost,[&](uint32_t a,unsigned dt){check(dt<=cost,"Channel read clocks");return memory[a];},[&](uint32_t a,uint8_t value,unsigned dt){check(dt<=cost&&a<0x18000,"Channel write clocks/span");memory[a]=value;history=int(a);},history)==cost,"Channel full budget");check(++count<64,"Channel did not finish");++phases;
   }
   struct A{std::array<uint32_t,7>values{};uint8_t flags=0;}a;a.values.fill(0x12345678);const auto original=a.values;ks::projectStateChannel(a,service);
   if(kind==K::GlobalMirror){check(memory[0x169b1]==input&&memory[0x174b9]==input,"Channel mirrors");check(a.values[0]==0x174ae&&uint8_t(a.values[1])==input,"Channel result");}else check(uint8_t(a.values[0])==((!mode||input)?0x91:0),"Channel mode result");
   for(unsigned n=2;n<7;++n)check(a.values[n]==original[n],"Channel unrelated caller value changed");roots|=1u<<unsigned(kind);
  }tracking=false;++graphs;
 }
 check(roots==3&&!allocations,"Channel roots/allocations");std::cout<<"PASS Pure StateChannel graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" allocations="<<allocations<<" sizeof="<<sizeof(T)<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
