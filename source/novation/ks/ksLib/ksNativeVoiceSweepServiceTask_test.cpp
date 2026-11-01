#include "ksNativeVoiceSweepService.h"
#include "ksNativeVoiceSweepAbi.h"
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
 using T=ks::NativeVoiceSweepService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<8192,"Sweep storage grew");
 for(auto kind:{K::ReleaseAll,K::RetunePart})for(unsigned seed=0;seed<128;++seed)for(unsigned velocity:{0u,1u,64u,127u}){
  std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));const auto word=[&](unsigned a,uint16_t v){m[a]=uint8_t(v>>8);m[a+1]=uint8_t(v);};const auto wide=[&](unsigned a,uint32_t v){word(a,uint16_t(v>>16));word(a+2,uint16_t(v));};const auto get=[&](unsigned a){return uint16_t((unsigned(m[a])<<8)|m[a+1]);};const unsigned part=2,control=0x174aeu+20*part,patch=0x18000;
  m[0x17526]=part;m[0x174a6]=uint8_t((seed&3)+1);m[0x174a7]=uint8_t(seed&3);m[0x17528]=60;m[0x17529]=uint8_t(velocity);m[0x169f1]=uint8_t(seed);m[patch]=16;m[patch+168]=uint8_t(seed&1?0:60);
  for(unsigned j=0;j<20;++j)m[control+j]=uint8_t(seed+j);wide(control,patch);m[control+14]=uint8_t(velocity);m[control+16]=uint8_t(seed);m[control+17]=0;word(0x159f2,0x1234);
  const auto age=uint16_t(seed&1?0:0x8000|(seed<<1));for(unsigned j=0;j<14;++j){wide(0x17560+20*j,patch);word(0x17560+20*j+8,age);word(0x17560+20*j+10,0x1234);m[0x17560+20*j+4]=part;m[0x17560+20*j+5]=60;}
  int previous=-1;unsigned local=0;const uint32_t reply=(seed<<16)|0x3408;tracking=true;
  {T s(kind,{});while(!s.complete()){
   const auto cost=s.nextClocks(previous);check(cost>0,"Pure allocation stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;
   check(!s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Sweep insufficient budget changed graph");++rejects;
   const auto read=[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Pure allocation read time/span");++reads;return at==0x600004?uint8_t(255):at==0x60000a?uint8_t(reply>>16):at==0x60000c?uint8_t(reply>>8):at==0x60000e?uint8_t(reply):m[at];};
   const auto write=[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size()&&!(at>=0xfff000&&at<0xfffc00),"Pure allocation write time/span/CPU scratch");m[at]=v;previous=at<0x400000?int(at):-1;++writes;};
   check(s.advanceBefore(cost,read,write,previous)==cost,"Sweep phase budget");if(s.effect().write)previous=-1;check(++local<100000,"Sweep graph did not finish");++phases;
  }
  const auto count=kind==K::ReleaseAll?14u:unsigned((seed&3)+1);for(unsigned j=0;j<count;++j){const auto slot=0x17560+20*j;if(kind==K::ReleaseAll&&age)check(get(slot+8)==uint16_t((age&0x3fff)|1)&&get(slot+10)==0xffff,"Release-all age transformation differs");else if(kind==K::RetunePart&&age)check(m[slot+13]==uint8_t(60+velocity-64),"Retune note/transpose transformation differs");}
  check(s.sweep().index()==count&&s.sweep().remaining()==0,"Sweep slot count differs");struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;};A a;projectNativeVoiceSweep(a,s);check(!s.nextClocks(previous),"Sweep final completion");components|=s.components();}
  tracking=false;++graphs;
 }
 check(!allocations,"Pure allocation allocation");std::cout<<"PASS Pure NativeVoiceSweep graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
