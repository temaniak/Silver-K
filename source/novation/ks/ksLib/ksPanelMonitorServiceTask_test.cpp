#include "ksPanelMonitorService.h"
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
int main(){try{using T=ks::PanelMonitorService;auto memory=std::make_unique<uint8_t[]>(1u<<24);uint64_t phases=0,rejects=0,calls=0,reads=0,writes=0;unsigned components=0;
 for(unsigned kind=0;kind<6;++kind)for(unsigned seed=0;seed<128;++seed){
 std::memset(memory.get(),0,1u<<24);const auto wide=[&](unsigned a,uint32_t v){for(unsigned n=0;n<4;++n)memory[a+n]=uint8_t(v>>(24-8*n));};
 for(unsigned n=0;n<3;++n){memory[0x2a0c+n*4]='I';memory[0x2a0d+n*4]='N';memory[0x2a0e + n*4]='T';}memory[0x19000]=uint8_t(seed*17);memory[0x19001]=uint8_t(seed*31);memory[0xffee96]=seed%2;memory[0xffee97]=seed%2;wide(0xffee7e,0x12345*(seed+1));memory[0xffff62]=255;
 T::Input i;i.presentation=kind==2||kind==3?0x9000:uint16_t(0x3200|seed);i.source=0xc1019000;i.level=uint8_t(seed*17);i.patch=uint8_t(seed*31);i.numerator=uint16_t(seed*257);i.divisor=uint16_t(seed*3);T s(T::Kind(kind),i);int previous=-1;
 while(!s.complete()){const auto cost=s.nextClocks(previous);check(cost,"Monitor made no progress");const auto read=[&](uint32_t a,unsigned dt){check(dt<=cost,"Read beyond admitted budget");++reads;return memory[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(dt<=cost,"Write beyond admitted budget");++writes;memory[a]=v;};
 const auto oldReads=reads,oldWrites=writes;const T before=s;tracking=true;check(!s.advanceBefore(cost-1,read,write,previous),"Underbudget monitor admitted");tracking=false;check(!std::memcmp(&s,&before,sizeof s)&&oldReads==reads&&oldWrites==writes,"Rejected monitor mutated");++rejects;
 tracking=true;check(s.advanceBefore(cost,read,write,previous)==cost,"Monitor phase differs");tracking=false;if(s.effect().write)previous=-1;components|=s.components();check(!s.task<std::array<int,3>>(),"Unknown task exposed");check(++phases<5000000,"Monitor stalled");}
 check(!s.task<ks::PanelVersionTask>()||s.kind()==T::Kind::Clock||s.kind()==T::Kind::Bar||s.kind()==T::Kind::Percent,"Completed version query exposed frame");(void)s.task<ks::ClockDisplayTask>();(void)s.task<ks::SystemServicesTask>();(void)s.task<ks::PanelPercentTask>();(void)s.task<ks::PanelSettingsLcdService>();
 const T before=s;tracking=true;check(!s.advanceBefore(~0u,[&](auto,auto){++reads;return 0;},[&](auto,auto,auto){++writes;}),"Completed graph restarted");tracking=false;check(!std::memcmp(&s,&before,sizeof s),"Completed monitor mutated");++calls;
 }
 check(!allocations&&components==31,"Monitor allocation/coverage");std::cout<<"PASS pure PanelMonitor graphs="<<calls<<" phases="<<phases<<" wholeobject rejects="<<rejects<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
