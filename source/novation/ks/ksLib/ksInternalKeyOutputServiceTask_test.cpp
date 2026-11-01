#include "ksInternalKeyOutputService.h"
#include "ksInternalKeyOutputAbi.h"
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
int main(){try{using T=ks::InternalKeyOutputService;using K=T::Kind;std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;
for(unsigned kind=0;kind<4;++kind)for(unsigned key=0;key<256;++key)for(unsigned pattern=0;pattern<4;++pattern){std::fill(m.begin()+0x15000,m.begin()+0x30000,uint8_t(0));std::fill(m.begin()+0xffec00,m.end(),uint8_t(0));const unsigned part=key%5,base=0x160d2+334*part;m[0x17526]=m[0x16798]=uint8_t(part);m[0x169ed]=0;m[0x169b1]=uint8_t(key&15);m[0x174ae +20*part+11]=uint8_t(key&15);m[0x15b07]=128;m[0xffed6f]=128;m[base+327]=uint8_t(key);std::array<uint8_t,32>original{};unsigned marked=0;for(unsigned i=0;i<32;++i){m[base+224+i]=original[i]=pattern==0?0:pattern==1?(i==15?128:0):pattern==2?uint8_t(i*31+key):255;if(i<16)for(unsigned b=0;b<8;++b)marked+=(original[i]>>b)&1;}
unsigned count=0,polls=0;int previous=-1;tracking=true;{T t(K(kind),{uint8_t(part),uint16_t((key^99)<<8|key)});while(!t.complete()){struct Unowned{};check(!t.task<Unowned>(),"Unknown internal task query");const auto cost=t.nextClocks(previous);check(cost>0,"Internal pure stalled");std::array<unsigned char,sizeof(T)>saved{};std::memcpy(saved.data(),&t,sizeof(T));unsigned callbacks=0;check(t.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous)==0&&!callbacks&&!std::memcmp(saved.data(),&t,sizeof(T)),"Internal budget changed object");++rejects;
const auto read=[&](uint32_t a,unsigned dt){check(a<(1u<<24)&&dt<=cost,"Internal pure read bound");++reads;if(a==0xffed6f&&++polls%5==0)m[a]=uint8_t(m[0xffed6d]+128);return m[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(a<(1u<<24)&&dt<=cost&&!(a>=0xfff000&&a<0xfffc00),"Internal pure write bound");++writes;m[a]=v;previous=a<0x400000?int(a):-1;};check(t.advanceBefore(cost,read,write,previous)==cost,"Internal pure admission");if(t.effect().write)previous=-1;check(++count<131072,"Internal pure did not complete");++phases;}
const auto&s=t.summary();check(s.part==part,"Internal selected part");for(unsigned i=0;i<32;++i){uint8_t expected=original[i];if(kind<2&&i==key/8)expected=kind==0?uint8_t(expected|(1u<<(key&7))):uint8_t(expected&~(1u<<(key&7)));else if(kind>=2&&i<16)expected=0;check(m[base+224+i]==expected,"Internal scalar bitmap oracle");}if(kind==3)check(s.notes==((key&96)?marked:0),"Internal packet count oracle");struct A{std::array<uint32_t,7>values{};uint8_t flags=0;};A a;projectInternalKeyOutput(a,t);components|=t.components();}tracking=false;++graphs;}
check(components==7&&!allocations,"Internal pure coverage/heap");std::cout<<"PASS Pure InternalKeyOutput graphs="<<graphs<<" phases="<<phases<<" whole_object_budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
