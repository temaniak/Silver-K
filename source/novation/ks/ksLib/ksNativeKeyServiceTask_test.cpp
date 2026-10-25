#include "ksNativeKeyService.h"
#include "ksNativeKeyAbi.h"
#include <array>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
namespace{bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(){try{using T=ks::NativeKeyService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;
for(unsigned kind=0;kind<8;++kind)for(unsigned key=0;key<256;++key)for(unsigned pattern=0;pattern<4;++pattern){std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));const auto base=0x160d2u+334*uint8_t(key%5);m[0x17526]=uint8_t(key%5);std::array<uint8_t,16>originalPhysical{},originalHeld{};for(unsigned i=0;i<128;++i)m[base+64+i]=uint8_t(i^key);for(unsigned i=0;i<16;++i){m[base+192+i]=pattern==0?0:pattern==1?(i==15?128:0):pattern==2?uint8_t(i*31+key):255;originalPhysical[i]=m[base+192+i];m[base+208+i]=uint8_t(~m[base+192+i]);originalHeld[i]=m[base+208+i];}m[base+306]=m[base+307]=8;for(unsigned i=0;i<8;++i){m[base+240+2*i]=uint8_t(i^77);m[base+241+2*i]=uint8_t(key+i-3);}m[base+308]=4;
unsigned count=0;int previous=-1;tracking=true;
{T t(K(kind),{uint16_t((key^99)<<8|key),base+192});while(!t.complete()){struct Unowned{};check(!t.task<Unowned>(),"Unknown key task query");const auto cost=t.nextClocks(previous);check(cost>0,"Key pure stalled");std::array<unsigned char,sizeof(T)>saved{};std::memcpy(saved.data(),&t,sizeof(T));unsigned callbacks=0;check(t.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)==0&&!callbacks&&!std::memcmp(saved.data(),&t,sizeof(T)),"Key budget changed object");++rejects;
const auto read=[&](uint32_t a,unsigned dt){check(a<(1u<<24)&&dt<=cost,"Key pure read bound");++reads;return m[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(a<(1u<<24)&&dt<=cost&&!(a>=0xfff000&&a<0xfffc00),"Key pure write bound");++writes;m[a]=v;previous=int(a);};check(t.advanceBefore(cost,read,write,previous)==cost,"Key pure admission");if(t.effect().write)previous=-1;check(++count<16384,"Key pure did not complete");++phases;}
const auto&s=t.summary();check(s.part==uint8_t(key%5)||kind>=6,"Key selected part");if(kind<2){uint16_t expected=0;const auto&bytes=kind?originalHeld:originalPhysical;for(unsigned i=0;i<16;i+=2){expected=uint16_t((unsigned(bytes[i])<<8)|bytes[i+1]);if(expected)break;}check(s.activity==expected,"Key activity result");}if(kind==2||kind==3){for(unsigned i=0;i<16;++i)check(!m[base+(kind==2?208:192)+i],"Key reset target");for(unsigned i=0;i<16;++i)check(m[base+(kind==2?192:208)+i]==(kind==2?originalPhysical[i]:originalHeld[i]),"Key reset other map changed");if(kind==2)check(!m[base+306]&&!m[base+307]&&!m[base+308],"Held reset counters");}if((kind==6||kind==7)&&key<128){const auto expected=kind==6?ks::nativeControl::nextKey(originalPhysical,key):ks::nativeControl::previousKey(originalPhysical,key);check(s.searchTask->result().note==expected.note,"Key search scalar oracle");}
struct A{std::array<uint32_t,7>values{};uint8_t flags=0;};A a;projectNativeKey(a,t);components|=t.components();}
tracking=false;++graphs;}
check(components==63&&!allocations,"Key pure coverage/heap");std::cout<<"PASS Pure NativeKey graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
