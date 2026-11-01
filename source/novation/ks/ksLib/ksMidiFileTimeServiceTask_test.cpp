#include "ksMidiFileTimeService.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace{bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::logic_error(m);}
int main(){try{using T=ks::MidiFileTimeService;auto memory=std::make_unique<uint8_t[]>(1u<<24);uint64_t phases=0,rejects=0,calls=0,reads=0,writes=0;unsigned components=0;
 for(unsigned kind=0;kind<5;++kind)for(unsigned seed=0;seed<128;++seed){
 std::memset(memory.get(),0,1u<<24);const auto wide=[&](unsigned a,uint32_t v){for(unsigned n=0;n<4;++n)memory[a+n]=uint8_t(v>>(24-8*n));};
const auto word=[&](unsigned a,uint16_t v){memory[a]=uint8_t(v>>8);memory[a+1]=uint8_t(v);};wide(0x17322,0x7ffff000u+seed*17);wide(0xffee82,0x80000000u-seed*17);word(0x17326,uint16_t(seed*257));wide(0x19078+(seed&1),0xfffff000u+seed*17);const auto v=uint8_t(seed&127);switch(seed%5){case 0:memory[0x19000]=v;break;case 1:memory[0x19000]=128|v;memory[0x19001]=v;break;case 2:memory[0x19000]=128|v;memory[0x19001]=128|v;memory[0x19002]=(255-seed)&127;break;case 3:memory[0x19000]=128|v;memory[0x19001]=128|v;memory[0x19002]=128|v;memory[0x19003]=uint8_t(seed);break;case 4:for(unsigned n=0;n<4;++n)memory[0x19000+n]=255;break;}
 T::Input i;i.stream=0xc1019000;i.tempo=0x12345678u*seed;i.record=0xc1019040u+(seed&1);T s(T::Kind(kind),i);int previous=-1;
 while(!s.complete()){const auto cost=s.nextClocks(previous);check(cost,"Monitor made no progress");const auto read=[&](uint32_t a,unsigned dt){check(dt<=cost,"Read beyond admitted budget");++reads;return memory[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(dt<=cost,"Write beyond admitted budget");++writes;memory[a]=v;};
 const auto oldReads=reads,oldWrites=writes;const T before=s;tracking=true;check(!s.advanceBefore(cost-1,read,write,previous),"Underbudget monitor admitted");tracking=false;check(!std::memcmp(&s,&before,sizeof s)&&oldReads==reads&&oldWrites==writes,"Rejected monitor mutated");++rejects;
 tracking=true;check(s.advanceBefore(cost,read,write,previous)==cost,"Monitor phase differs");tracking=false;if(s.effect().write)previous=-1;components|=s.components();check(!s.task<std::array<int,3>>(),"Unknown task exposed");check(++phases<5000000,"Monitor stalled");}
check(s.task<ks::MidiFileTimeTask>()&&s.task<ks::MidiFileTimeTask>()->complete(),"Completed time task not queryable");
 const T before=s;tracking=true;check(!s.advanceBefore(~0u,[&](auto,auto){++reads;return 0;},[&](auto,auto,auto){++writes;}),"Completed graph restarted");tracking=false;check(!std::memcmp(&s,&before,sizeof s),"Completed monitor mutated");++calls;
 }
 check(!allocations&&components==3,"Monitor allocation/coverage");std::cout<<"PASS pure MidiFileTime graphs="<<calls<<" phases="<<phases<<" wholeobject rejects="<<rejects<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
