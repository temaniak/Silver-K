#include "ksNativeVoiceStartService.h"
#include "ksNativeVoiceStartAbi.h"
#include <array>
#include <algorithm>
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
 using T=ks::NativeVoiceStartService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<16384,"Start storage grew");
 for(auto kind:{K::Unison,K::Voice,K::ResumeGate,K::FullGate})for(unsigned seed=0;seed<32;++seed)for(unsigned velocity:{0u,1u,64u,127u}){
  std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));const auto word=[&](unsigned a,uint16_t v){m[a]=uint8_t(v>>8);m[a+1]=uint8_t(v);};const auto wide=[&](unsigned a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));};const auto get=[&](unsigned a){return uint16_t((unsigned(m[a])<<8)|m[a+1]);};const unsigned part=2,control=0x174aeu+20*part,patch=0x18000;
  m[0x17526]=part;m[0x174a6]=4;m[0x174a7]=uint8_t(seed&3);m[0x17528]=60;m[0x17529]=uint8_t(velocity);m[0x169f1]=uint8_t(seed);m[patch]=uint8_t(16|(seed&7));m[patch+4]=64;m[patch+3]=uint8_t(seed*7);m[patch+5]=uint8_t(seed&1);m[patch+79]=uint8_t(seed*3);m[patch+78]=uint8_t(seed);m[patch+147]=uint8_t(seed);m[patch+58]=m[patch+61]=m[patch+66]=uint8_t(seed*8);word(0x1513c,0x4470);m[0x16798]=part;m[patch+173]=12;
  for(unsigned j=0;j<20;++j)m[control+j]=uint8_t(seed+j);wide(control,patch);m[control+14]=64;m[control+16]=uint8_t(seed);m[control+17]=0;word(0x159f2,0x1234);
  if(kind==K::Voice&&(seed&1)){m[patch]=0;m[control+17]=0;m[control+18]=0;for(unsigned j=0;j<4;++j){wide(0x17560+20*j,patch);wide(0x17560+20*j+8,0x10000);m[0x17560+20*j+4]=part;m[0x17560+20*j+5]=60;}}
  int previous=-1;unsigned local=0;const uint32_t reply=(seed<<16)|0x3408;tracking=true;
  {T s(kind,{uint16_t((velocity<<8)|60),patch,ks::DspPort::B,0});while(!s.complete()){
   const auto cost=s.nextClocks(previous);check(cost>0,"Pure start stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;
   check(!s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Start insufficient budget changed graph");++rejects;
   const auto read=[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Pure start read time/span");++reads;return at==0x600004?uint8_t(255):at==0x60000a?uint8_t(reply>>16):at==0x60000c?uint8_t(reply>>8):at==0x60000e?uint8_t(reply):m[at];};
   const auto write=[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size()&&!(at>=0xfff000&&at<0xfffc00),"Pure start write time/span/CPU scratch");m[at]=v;previous=at<0x400000?int(at):-1;++writes;};
   check(s.advanceBefore(cost,read,write,previous)==cost,"Start phase budget");if(s.effect().write)previous=-1;check(++local<100000,"Start graph did not finish");++phases;
  }
  check(s.start()&&!s.start()->handoff(),"Start did not complete gate/suppression");check(m[0x17528]==60&&m[0x17529]==velocity,"Start note/velocity facts changed");
  struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;};A a;projectNativeVoiceStart(a,s);check(a.values[6]==patch&&!s.nextClocks(previous),"Start final projection/completion");if(kind==K::Unison)check(s.summary().starts==(seed&7)+1&&(!(seed&7)||!((m[control+18])&7)),"Unison extra count/finaltail differs");components|=s.components();}
  tracking=false;++graphs;
 }
 check(!allocations,"Pure start start");std::cout<<"PASS Pure NativeVoiceStart graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
