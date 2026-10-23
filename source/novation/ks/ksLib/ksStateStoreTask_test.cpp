#include "ksStateStoreService.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace{bool track=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace{
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}using T=ks::StateStoreService;using K=T::Kind;
uint8_t rr(uint8_t v,unsigned n){return uint8_t((unsigned(v)>>n)|(unsigned(v)<<(8-n)));}uint8_t rl(uint8_t v,unsigned n){return uint8_t((unsigned(v)<<n)|(unsigned(v)>>(8-n)));}
template<class Reader>uint8_t oracle(unsigned k,Reader&&r){switch(k){case 0:return rr(uint8_t(rr(uint8_t(rr(rr(r(210),2),1)|r(209)),2)|r(211)),3);case 1:return rr(uint8_t(rr(uint8_t(rr(uint8_t(rr(uint8_t(rr(r(214),2)|r(215)),1)|r(212)),1)|r(213)),1)|r(216)),3);case 2:return rl(uint8_t(rr(rr(r(217),2),1)|r(218)),3);case 3:return rr(uint8_t(rr(uint8_t(rr(r(219),2)|r(220)),2)|r(221)),4);case 4:return rr(uint8_t(rr(uint8_t(rr(uint8_t(rr(r(222),2)|r(223)),2)|r(224)),2)|r(225)),2);default:return uint8_t(r(226)|((r(227)*8)&127));}}
}
int main(){try{
 auto memory=std::make_unique<std::array<uint8_t,0x200000>>();memory->fill(0);uint64_t cases=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0,kinds=0;
 const auto wide=[&](uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)(*memory)[a+n]=uint8_t(v>>(24-8*n));};
 const auto run=[&](T&service){int previous=-1;unsigned count=0;while(!service.complete()){
  const auto cost=service.nextClocks(previous);check(cost,"Pure store graph stalled");const auto before=reads+writes;
  const auto read=[&](uint32_t a,unsigned offset){check(a<memory->size()&&offset<=cost,"Pure store read bounds/time");++reads;return(*memory)[a];};
  const auto write=[&](uint32_t a,uint8_t v,unsigned offset){check(a<memory->size()&&offset<=cost,"Pure store write bounds/time");++writes;previous=int(a);(*memory)[a]=v;};
  const auto resolve=[](uint32_t descriptor){check(descriptor<6,"Pure recipe semantic token invalid");return ks::StatePackFieldsTask::Kind(descriptor);};
  track=true;const auto refused=service.advanceBefore(cost-1,read,write,resolve,previous);track=false;check(!refused&&before==reads+writes&&cost==service.nextClocks(previous),"Rejected budget changed store continuation/resources");++rejects;
  track=true;const auto used=service.advanceBefore(cost,read,write,resolve,previous);track=false;check(used==cost&&++count<4096,"Pure store phase admission failed");++phases;components|=service.components();
 }check(!service.nextClocks(previous),"Complete store graph remained active");++cases;kinds|=1u<<unsigned(service.kind());};
 constexpr unsigned offsets[]{0,88,121,122,127,126};
 for(unsigned kind=0;kind<17;++kind)for(unsigned seed=0;seed<(kind>=3&&kind<=8?65536u:256u);++seed){
  for(unsigned n=0;n<1024;++n)(*memory)[0x169fc + n]=uint8_t(seed*17+n*31);
  for(unsigned n=0;n<256;++n){(*memory)[0x30000 + n]=uint8_t(seed*13+n*37+(seed>>8));(*memory)[0x168fc + n]=uint8_t(seed*43+n*59);}
  for(unsigned p=0;p<5;++p)for(unsigned n=0;n<20;++n)(*memory)[0x174ae + 20*p+n]=uint8_t(seed*61+p*79+n*97);
  for(unsigned p=0;p<4;++p){(*memory)[0x167b0 + 2*p]=uint8_t(p+1);(*memory)[0x167b1 + 2*p]=uint8_t(seed+p*43);}
  for(unsigned n=0;n<17;++n)(*memory)[0x1733c + n]=uint8_t(seed*101+n*107);
  (*memory)[0x167d2]=uint8_t(seed);(*memory)[0x167d3]=uint8_t(seed*5);wide(0x167bc,0x30000);wide(0x1748e,0x30000);for(unsigned n=0;n<6;++n)wide(71306+4*n,n);
  std::array<uint8_t,1024>expected{};std::copy_n(memory->begin()+0x169fc,expected.size(),expected.begin());
  if(kind==0){constexpr unsigned global[]{84,89,90,91,124,208,227};for(unsigned n=0;n<7;++n)expected[n?global[n]:191]=(*memory)[0x168fc + global[n]];expected[189]=uint8_t(seed);expected[190]=uint8_t(seed*5);
   for(unsigned p=0;p<4;++p){for(unsigned n=0;n<8;++n)expected[p*256+179+n]=(*memory)[0x174ae + 20*(p+1)+9+n];expected[p*256+187]=uint8_t(p+1);expected[p*256+188]=uint8_t(seed+p*43);for(unsigned k=0;k<6;++k)expected[p*256+offsets[k]]=oracle(k,[&](unsigned off){return expected[p*256+off];});}}
  std::array<uint8_t,6>packed{};for(unsigned k=0;k<6;++k)packed[k]=oracle(k,[&](unsigned off){return(*memory)[0x30000 + off];});
  std::array<uint8_t,16>name{};const bool reading=kind==9||kind==10||kind==11||kind==16;const uint32_t storage=kind==9||kind==12?0x30097:kind==10||kind==13?0x16abc:0x30000;
  for(unsigned n=0;n<16;++n)name[n]=(*memory)[reading?storage+n:0x1733c + n];
  T service(K(kind),{0x30000,uint8_t(1+seed%6)});run(service);
  if(kind==0){check(std::equal(expected.begin(),expected.end(),memory->begin()+0x169fc),"Pure evolving multiset serialization differs");check((*memory)[0x167bc]==0&&(*memory)[0x167bd]==3&&(*memory)[0x167be]==0&&(*memory)[0x167bf]==0,"Original current reference not restored");}
  else if(kind==1){for(unsigned k=0;k<6;++k)check((*memory)[0x30000 + offsets[k]]==packed[k],"Full packed byte oracle differs");}
  else if(kind<=8){check(service.value()==packed[kind==2?seed%6:kind-3],"Packed recipe oracle differs");}
  else{for(unsigned n=0;n<16;++n)check((*memory)[reading?0x1733c + n:storage+n]==name[n],"Live name byte copy differs");check(!(*memory)[0x1734c],"Name scratch terminator missing");}
 }
 check(components==0x7ff&&kinds==0x1ffff&&!allocations,"Pure store graph/component/allocation coverage differs");
 check(T::dataSpan(0xffeff0,16)&&!T::dataSpan(0xffeff1,16)&&!T::dataSpan(0xfff000,1)&&!T::dataSpan(0xfff3fc,2)&&!T::dataSpan(0xffff8c,1)&&!T::dataSpan(0x687e,16),"Data window guard crossing differs");
 std::cout<<"PASS: "<<cases<<" pure store preparation/name graphs; "<<phases<<" phases and "<<rejects<<" rejected budgets; all17 kinds/11 components, independent packed/serialization/name oracles, zero allocations\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
