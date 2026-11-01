#include "ksVoiceFeedbackService.h"
#include "ksVoiceFeedbackAbi.h"
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
 using T=ks::VoiceFeedbackService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;static_assert(sizeof(T)<4096,"Feedback storage grew");
 for(unsigned cursor:{0u,1u,127u,128u,254u,255u})for(unsigned count:{0u,1u,128u,255u})for(unsigned age:{0u,1u,0x4000u,0x7fffu,0x8000u,0xffffu})for(unsigned status:{0u,1u,8u,9u})for(auto kind:{K::Reset,K::Poll}){
  std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));m[0x1755a]=uint8_t(cursor);m[0x174a6]=uint8_t(count);const auto word=[&](unsigned at,uint16_t v){m[at]=uint8_t(v>>8);m[at+1]=uint8_t(v);};const auto get=[&](unsigned at){return uint16_t((unsigned(m[at])<<8)|m[at+1]);};const unsigned slot=0x17560+20*cursor;const uint16_t countdown=uint16_t(cursor*257);word(slot+8,uint16_t(age));word(slot+10,countdown);
  const uint16_t feedback=uint16_t(count*257);const uint32_t reply=(unsigned(feedback)<<8)|status;int previous=0x174a6;unsigned local=0;tracking=true;
  {T s(kind);while(!s.complete()){
   const auto cost=s.nextClocks(previous);check(cost>0,"Pure feedback stalled");std::array<unsigned char,sizeof(T)>before{};std::memcpy(before.data(),&s,sizeof(T));unsigned callbacks=0;
   check(s.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)==0&&!callbacks&&!std::memcmp(before.data(),&s,sizeof(T)),"Feedback insufficient budget changed graph");++rejects;
   const auto read=[&](uint32_t at,unsigned dt){check(dt&&dt<=cost&&at<m.size(),"Pure read time/span");++reads;return at==0x600004?uint8_t(255):at==0x60000a?uint8_t(reply>>16):at==0x60000c?uint8_t(reply>>8):at==0x60000e?uint8_t(reply):m[at];};
   const auto write=[&](uint32_t at,uint8_t v,unsigned dt){check(dt&&dt<=cost&&at<m.size()&&!(at>=0xfff000&&at<0xfffc00),"Pure write time/span/CPU scratch");m[at]=v;previous=at<0x400000?int(at):-1;++writes;};
   check(s.advanceBefore(cost,read,write,previous)==cost,"Feedback phase budget");if(s.effect().write)previous=-1;check(++local<1000,"Feedback graph did not finish");++phases;
  }
  if(kind==K::Reset)check(m[0x1755a]==0,"Reset cursor not cleared");else {
   const uint8_t incremented=uint8_t(cursor+1);const uint8_t expected=incremented<uint8_t(count)?incremented:0;check(m[0x174a7]==cursor&&m[0x1755a]==expected,"Cursor scan order/wrap differs");
   if(!age)check(get(slot+8)==0&&get(slot+10)==countdown,"Empty age changed voice");
   else if((age&0x8000)&&!(status&1))check(get(slot+8)==0x7fff&&get(slot+10)==0xffff,"Pending voice not activated");
   else {if(!(age&0x8000)){const uint16_t expectedAge=uint16_t(((status&8)?age&0x4000:0)|(feedback>>1));check(get(slot+8)==expectedAge,"Age feedback arithmetic differs");}const uint16_t next=uint16_t(countdown-1);check(get(slot+10)==(next?next:countdown),"Zero countdown suppression differs");}
  }
  struct A{std::array<uint32_t,7>values{};uint8_t flags=0xa5;};A a;projectVoiceFeedback(a,s);check(!s.nextClocks(previous),"Completed feedback has work");components|=s.components();}
  tracking=false;++graphs;
 }
 check(components==31&&!allocations,"Pure feedback coverage/allocations");std::cout<<"PASS Pure VoiceFeedback graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
