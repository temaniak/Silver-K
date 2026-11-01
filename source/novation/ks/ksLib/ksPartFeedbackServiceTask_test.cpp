#include "ksPartFeedbackService.h"
#include "ksPartFeedbackAbi.h"
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
 using T=ks::PartFeedbackService;std::vector<uint8_t>memory(1<<24);uint64_t graphs=0,phases=0,rejects=0;uint32_t components=0;
 for(unsigned mask=0;mask<16;++mask)for(unsigned mode:{0u,1u,255u}){
  std::fill(memory.begin()+0x15000,memory.begin()+0x40000,uint8_t(0));
  const auto wide=[&](uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)memory[a+n]=uint8_t(v>>(24-8*n));};
  for(unsigned n=0;n<256;++n){memory[0x30000+n]=uint8_t(n*13);memory[0x168fc+n]=uint8_t(n*7);wide(0x174ae + 20*n,0x30000);}for(unsigned n:{95u,113u,119u,103u})memory[0x30000+n]=0;
  memory[0x167ba]=uint8_t(mode);memory[0x17526]=128;unsigned count=0;uint16_t address=0;int previous=-1;
  tracking=true;{T service;while(!service.complete()){
   const auto cost=service.nextClocks(previous);check(cost>0,"Parts phase stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&service,sizeof(T));unsigned callbacks=0;
   check(!service.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous),"Early parts admission");check(!callbacks&&!std::memcmp(before.data(),&service,sizeof(T)),"Rejected parts budget changed object");++rejects;
   check(service.advanceBefore(cost,[&](uint32_t a,unsigned dt){check(dt<=cost,"Parts read clocks");if(a==0x400004||a==0x600004)return uint8_t(255);if(a==0x40000a||a==0x40000c)return uint8_t(0);if(a==0x40000e)return uint8_t(address>=0x8041&&address<=0x8044&&(mask&(1u<<(address-0x8041)))?3:0);return memory[a];},[&](uint32_t a,uint8_t value,unsigned dt){check(dt<=cost&&!(a>=0xffef00&&a<0xfffd00),"Parts write clocks/scratch");memory[a]=value;if(a==0x40000c)address=uint16_t((unsigned(value)<<8)|(address&255));if(a==0x40000e)address=uint16_t((address&0xff00)|value);previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},previous)==cost,"Parts full budget");if(service.effect().write)previous=-1;check(++count<100000,"Parts did not finish");++phases;
  }check(memory[0x17526]==128&&service.summary().refreshed==mask,"Parts selectedpart restore/refreshed mask");components|=service.components();}tracking=false;++graphs;
 }
 check(!allocations&&components==15,"Parts allocations/components");std::cout<<"PASS Pure PartFeedback graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" allocations="<<allocations<<" sizeof="<<sizeof(T)<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
