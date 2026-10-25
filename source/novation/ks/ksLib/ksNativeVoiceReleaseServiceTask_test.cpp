#include "ksNativeVoiceReleaseService.h"
#include "ksNativeVoiceReleaseAbi.h"
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
 using T=ks::NativeVoiceReleaseService;using M=ks::VoiceReleaseTask::Mode;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<4096,"Native release storage grew");
 for(auto mode:{M::Note,M::Part,M::FastPart,M::Pedal,M::ProvidedPart})for(unsigned age:{0u,1u,0x4000u,0x7fffu,0x8000u,0xffffu})for(unsigned owner:{1u,2u})for(unsigned note:{59u,60u})for(unsigned pedal:{0u,127u})for(unsigned flags:{0u,4u}){
  std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));const auto word=[&](unsigned a,uint16_t v){m[a]=uint8_t(v>>8);m[a+1]=uint8_t(v);};const auto wide=[&](unsigned a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));};const auto get=[&](unsigned a){return uint16_t((unsigned(m[a])<<8)|m[a+1]);};const unsigned count=4,patch=0x18000,part=2;
  m[0x17526]=part;m[0x174a6]=count;m[patch+88]=uint8_t(flags);m[0x160d2+334*part+309]=uint8_t(pedal);for(unsigned i=0;i<count;++i){const auto a=0x17560+20*i;wide(a,patch);m[a+4]=uint8_t(owner);m[a+5]=uint8_t(note);word(a+8,uint16_t(age));m[a+17]=1;}for(unsigned i=0;i<16;++i)m[0x160d2+334*part+192+i]=255;
  int previous=0x174a6;unsigned local=0;tracking=true;
  {T s(mode,{60,3});while(!s.complete()){
   const auto cost=s.nextClocks(previous);check(cost>0,"Pure release stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;
   check(!s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Release insufficient budget changed graph");++rejects;
   const auto read=[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Pure release read time/span");++reads;return at==0x600004?uint8_t(255):m[at];};
   const auto write=[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size()&&!(at>=0xfff000&&at<0xfffc00),"Pure release write time/span/CPU scratch");m[at]=v;previous=at<0x400000?int(at):-1;++writes;};
   check(s.advanceBefore(cost,read,write,previous)==cost,"Release phase budget");if(s.effect().write)previous=-1;check(++local<100000,"Release graph did not finish");++phases;
  }
  check(m[0x174a7]==count,"Release did not scan exact voice count");const bool matches=age&&owner==part&&(mode!=M::Note||note==60);const bool held=matches&&mode==M::Note&&!flags&&pedal;
  const bool released=matches&&!held;check(s.releasedAny()==released,"Release branch differs");for(unsigned i=0;i<count;++i){const auto a=0x17560+20*i;check(get(a+8)==(released?uint16_t((age&0x3fff)|1):age),"Release age differs");if(held)check(m[a+17]==pedal,"Sustain did not defer release");if(mode==M::Pedal&&matches)check(!m[a+17],"Pedal did not clear pending");}
  if(mode==M::Part||mode==M::FastPart||mode==M::ProvidedPart)for(unsigned i=0;i<16;++i)check(!m[0x160d2+334*part+192+i],"Part release did not clear keymap");
  struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;};A a;projectNativeVoiceRelease(a,s);check(!s.nextClocks(previous),"Completed release has work");components|=s.components();}
  tracking=false;++graphs;
 }
 check(!allocations,"Pure release allocation");std::cout<<"PASS Pure NativeVoiceRelease graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
