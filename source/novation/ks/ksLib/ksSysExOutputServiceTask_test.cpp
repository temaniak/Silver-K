#include "ksSysExOutputService.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
namespace {bool tracking=false;uint64_t allocations=0;void check(bool b,const char*m){if(!b)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
using T=ks::SysExOutputService;
struct Resource {
 std::vector<uint8_t>m=std::vector<uint8_t>(1<<24),wire;uint64_t cycles=0,ready=0,waits=0;int previous=-1;unsigned accesses=0;std::array<unsigned,8>offsets{};
 Resource(){wire.reserve(65536);}
 uint8_t read(uint32_t a,unsigned offset){check(accesses<8&&offset>0,"Pure output read bounds");offsets[accesses++]=offset;if(a==0xffff8c){if(cycles+offset<ready){++waits;return m[a]&127;}return m[a]|128;}return m[a&0xffffff];}
 void write(uint32_t a,uint8_t v,unsigned offset){check(accesses<8&&offset>0,"Pure output write bounds");offsets[accesses++]=offset;m[a&0xffffff]=v;if(a==0xffff8b){wire.push_back(v);ready=cycles+offset+200;}previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
};
}
int main(){try{
 auto r=std::make_unique<Resource>();auto storage=std::make_unique<std::aligned_storage_t<sizeof(T),alignof(T)>>();std::array<uint8_t,sizeof(T)>saved{};uint64_t graphs=0,phases=0,rejects=0,waits=0,guards=0;uint32_t components=0;
 auto run=[&](T::Kind kind,uint16_t count,uint32_t source,unsigned seed){
  r->wire.clear();r->cycles=0;r->ready=100+seed%7;r->previous=-1;r->waits=0;r->m[0xffff8c]=uint8_t(seed&127);
  r->m[0xffed68]=0x77;r->m[0xffec64]=0xaa;r->m[0xffec65]=0xbb;r->m[0xffec66]=0xcc;r->m[0xffec67]=0xdd;
  for(unsigned n=0;n<12;++n){r->m[0x1b0+n]=uint8_t(seed+17*n);r->m[0x16044+n]=uint8_t(seed+9*n);}
  const auto length=count?unsigned(count):65536u;
  if(kind==T::Kind::Send)for(unsigned n=0;n<length;++n)r->m[(source+n)&0xffffff]=uint8_t(seed+13*n);
  tracking=true;auto*s=new(storage.get())T(kind,{source,count,uint8_t(seed)});tracking=false;
  while(!s->complete()){
   const auto cost=s->nextClocks(r->previous);check(cost,"Pure output has no phase");const auto previous=r->previous;r->accesses=0;
   std::memcpy(saved.data(),s,sizeof(T));tracking=true;
   const auto rejected=s->advanceBefore(cost-1,[&](uint32_t a,unsigned dt){return r->read(a,dt);},[&](uint32_t a,uint8_t v,unsigned dt){r->write(a,v,dt);},0,previous);tracking=false;
   check(!rejected&&!r->accesses&&!std::memcmp(saved.data(),s,sizeof(T)),"Rejected output budget changed graph/resources");++rejects;
   tracking=true;const auto used=s->advanceBefore(cost,[&](uint32_t a,unsigned dt){return r->read(a,dt);},[&](uint32_t a,uint8_t v,unsigned dt){r->write(a,v,dt);},0,previous);tracking=false;
   check(used==cost,"Pure output phase cost");for(unsigned n=0;n<r->accesses;++n)check(r->offsets[n]<=cost,"Output access after phase deadline");
   if(s->effect().write)r->previous=-1;r->cycles+=cost;++phases;
  }
  if(kind==T::Kind::Header){for(unsigned n=0;n<12;++n)check(r->m[0x16044+n]==uint8_t(seed+17*n),"Header copy independent oracle");check(s->summary().headerIndex==12&&s->summary().sourceEnd==0x1bc,"Header semantic cursors");}
  if(kind==T::Kind::Send){check(r->wire.size()==length,"Send count independent oracle");for(unsigned n=0;n<length;++n)check(r->wire[n]==uint8_t((seed+13*n)&127),"Masked send independent oracle");check(s->summary().sourceEnd==source+length&&!s->summary().countRemaining,"Send cursor/count semantic outcome");}
  if(kind==T::Kind::SendHeader){check(r->wire.size()==13&&r->wire[0]==0xf0,"Header status independent oracle");for(unsigned n=0;n<12;++n)check(r->wire[n+1]==uint8_t((seed+9*n)&127),"Header wire data oracle");}
  if(kind==T::Kind::Byte)check(r->wire.size()==1&&r->wire[0]==uint8_t(seed),"Direct byte is not 7-bit masked");
  if(kind==T::Kind::ResetRx)check(!r->m[0xffed68]&&!r->m[0xffec64]&&!r->m[0xffec65]&&!r->m[0xffec66]&&!r->m[0xffec67],"Reset RX semantic oracle");
  check(s->summary().bytesSent==r->wire.size(),"Output byte count");waits+=r->waits;components|=s->components();tracking=true;s->~T();tracking=false;++graphs;
 };
 for(unsigned kind=0;kind<5;++kind)for(unsigned seed=0;seed<64;++seed)run(T::Kind(kind),uint16_t(1+seed%17),0x30000,seed*19);
 for(auto count:{uint16_t(255),uint16_t(256),uint16_t(1024),uint16_t(0)})run(T::Kind::Send,count,0x81ff00,37);
 for(auto source:{0x1fffffu,0x85ffffu,0xbfffffu,0xffefffu}){check(T::admitted(T::Kind::Send,{source,1,0}),"Final permitted byte rejected");check(!T::admitted(T::Kind::Send,{source,2,0}),"Span crossing resource domain accepted");++guards;}
 for(auto source:{0x200000u,0x81feffu,0x860000u,0xc00000u,0xfff000u,0xffff8cu,0x42fff400u}){check(!T::admitted(T::Kind::Send,{source,1,0}),"Unsupported output source accepted");++guards;}
 check(!allocations&&waits&&components==15,"Output allocation/closure/poll coverage");
 std::cout<<"PASS pure SysEx output graphs="<<graphs<<" phases="<<phases<<" budgetRejects="<<rejects<<" waits="<<waits<<" guards="<<guards<<" components="<<components<<" storage="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
 }catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
