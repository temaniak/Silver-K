#include "ksNativeVoiceModulationService.h"
#include "ksNativeVoiceModulationAbi.h"
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
 using T=ks::NativeVoiceModulationService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<8192,"Modulation storage grew");
 for(unsigned kindIndex=0;kindIndex<9;++kindIndex)for(unsigned seed=0;seed<32;++seed)for(unsigned velocity:{0u,1u,127u,255u}){
  const auto kind=K(kindIndex);std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));const auto word=[&](unsigned a,uint16_t v){m[a]=uint8_t(v>>8);m[a+1]=uint8_t(v);};const auto wide=[&](unsigned a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));};const auto get=[&](unsigned a){return uint16_t((unsigned(m[a])<<8)|m[a+1]);};const unsigned part=2,control=0x174aeu+20*part,patch=0x18000;
  m[0x17526]=part;m[0x174a6]=4;m[0x174a7]=uint8_t(seed&3);m[0x17528]=60;m[0x17529]=uint8_t(velocity);m[0x169f1]=uint8_t(seed);for(unsigned j=0;j<256;++j)m[patch+j]=uint8_t(seed*(j+1));m[patch]=uint8_t(seed);for(unsigned j=0;j<66;++j)word(0x15200+2*j,uint16_t(j*511));word(0x15130,0x3c00);word(0x17516,256);m[0x17518]=10;m[0x17519]=20;m[0x1751a]=30;m[0x169f0]=64;m[0x17521]=64;
  for(unsigned j=0;j<20;++j)m[control+j]=uint8_t(seed+j);wide(control,patch);m[control+14]=64;m[control+16]=uint8_t(seed);m[control+17]=uint8_t(seed&1);for(unsigned j=0;j<4;++j){wide(0x17560+20*j,patch);word(0x17560+20*j+8,0x1000);m[0x17560+20*j+4]=part;m[0x17560+20*j+12]=uint8_t(j);}word(0x159f2,0x1234);
  int previous=-1;unsigned local=0;const uint32_t reply=(seed<<16)|0x3408;tracking=true;
  {T s(kind,{uint8_t(velocity),patch,ks::DspPort::B,0});while(!s.complete()){
   const auto cost=s.nextClocks(previous);check(cost>0,"Pure allocation stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;
   check(!s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Modulation insufficient budget changed graph");++rejects;
   const auto read=[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Pure allocation read time/span");++reads;return (at==0x400004||at==0x600004)?uint8_t(255):at==0x60000a?uint8_t(reply>>16):at==0x60000c?uint8_t(reply>>8):at==0x60000e?uint8_t(reply):m[at];};
   const auto write=[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size()&&!(at>=0xfff000&&at<0xfffc00),"Pure allocation write time/span/CPU scratch");m[at]=v;previous=at<0x400000?int(at):-1;++writes;};
   check(s.advanceBefore(cost,read,write,previous)==cost,"Modulation phase budget");if(s.effect().write)previous=-1;check(++local<100000,"Modulation graph did not finish");++phases;
  }
  if(kindIndex<4){const auto field=kind==K::Cutoff?4:kind==K::First?6:kind==K::Second?8:7;if(kind==K::Cutoff)check(get(control+field)==(velocity==255?512:velocity*2),"Cutoff domain scaling/sentinel differs");else check(m[control+field]==(kind==K::Square?uint8_t((velocity*velocity*2)>>8):uint8_t(velocity)),"Modulation domain value differs");}
  if(kind==K::Modulation)check(s.summary().outputs==4,"Modulation must issue four exact packets");if(kind==K::Pitch)check(s.summary().outputs==3,"Pitch must issue three exact packets");if(kind==K::Global)check(s.summary().outputs==6&&s.summary().port==ks::DspPort::A,"Global B-then-A packet count differs");
  struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;};A a;projectNativeVoiceModulation(a,s);check(!s.nextClocks(previous),"Modulation final completion");components|=s.components();}
  tracking=false;++graphs;
 }
 check(components==255&&!allocations,"Pure modulation components/allocation");std::cout<<"PASS Pure NativeVoiceModulation graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
