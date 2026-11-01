#include "ksStateStatusService.h"
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
int main(){try{using T=ks::StateStatusService;auto memory=std::make_unique<uint8_t[]>(1u<<24);uint64_t phases=0,rejects=0,calls=0,reads=0,writes=0;unsigned components=0;
 for(unsigned kind=0;kind<4;++kind)for(unsigned seed=0;seed<128;++seed){
 std::memset(memory.get(),0,1u<<24);const auto wide=[&](unsigned a,uint32_t v){for(unsigned n=0;n<4;++n)memory[a+n]=uint8_t(v>>(24-8*n));};
 const auto word=[&](unsigned at,uint16_t v){memory[at]=uint8_t(v>>8);memory[at+1]=uint8_t(v);};wide(0x167bc,0x19000);word(0x167ac,0x0110);wide(0xffec10,seed*17);memory[0x167ba]=seed&1;memory[0x167d3]=seed%3?15:0;memory[0x167ee]=seed&3;memory[0x167d6]=uint8_t(seed*17);memory[0x17494]='!';for(unsigned part=0;part<4;++part)word(0x167b0+2*part,uint16_t(0x0100|part));if(kind==0){const auto route=seed%8;if(route==0)memory[0x16791]=1;if(route==1)memory[0x1679e]=1;if(route>=2&&route<=5){memory[0x16784]=1;memory[0x16787]=route==2?5:route==3?11:route==4?12:13;}}for(unsigned n=0;n<3;++n){memory[0x2a0c+n*4]='I';memory[0x2a0d+n*4]='N';memory[0x2a0e + n*4]='T';}wide(0xffee7e,0x12345*(seed+1));memory[0xffff62]=255;memory[0x400004]=255;
 T::Input i;i.presentation=uint16_t(0x3200|seed);T s(T::Kind(kind),i);int previous=-1;
 while(!s.complete()){const auto cost=s.nextClocks(previous);check(cost,"Monitor made no progress");const auto read=[&](uint32_t a,unsigned dt){check(dt<=cost,"Read beyond admitted budget");++reads;return memory[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(dt<=cost,"Write beyond admitted budget");++writes;memory[a]=v;};
 const auto oldReads=reads,oldWrites=writes;const T before=s;tracking=true;check(!s.advanceBefore(cost-1,read,write,previous),"Underbudget monitor admitted");tracking=false;check(!std::memcmp(&s,&before,sizeof s)&&oldReads==reads&&oldWrites==writes,"Rejected monitor mutated");++rejects;
 tracking=true;check(s.advanceBefore(cost,read,write,previous)==cost,"Monitor phase differs");tracking=false;if(s.effect().write)previous=-1;components|=s.components();check(!s.task<std::array<int,3>>(),"Unknown task exposed");check(++phases<5000000,"Monitor stalled");}
 check(!s.task<ks::StateStatusDisplayTask>(),"Completed status exposes active task");(void)s.task<ks::StateEditWorkflowTask>();(void)s.task<ks::StatePresetAddressTask>();(void)s.task<ks::StateProgramTask>();(void)s.task<ks::PanelMonitorService>();(void)s.task<ks::PanelSettingsLcdService>();(void)s.task<ks::ControllerDspService>();(void)s.task<ks::ScheduledTimerCoordinator>();
 const T before=s;tracking=true;check(!s.advanceBefore(~0u,[&](auto,auto){++reads;return 0;},[&](auto,auto,auto){++writes;}),"Completed graph restarted");tracking=false;check(!std::memcmp(&s,&before,sizeof s),"Completed monitor mutated");++calls;
 }
 check(!allocations&&components==255,"Monitor allocation/coverage");std::cout<<"PASS pure StateStatus graphs="<<calls<<" phases="<<phases<<" wholeobject rejects="<<rejects<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
