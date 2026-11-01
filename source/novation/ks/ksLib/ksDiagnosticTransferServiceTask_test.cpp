#include "ksDiagnosticTransferService.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
namespace{bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void check(bool b,const char*m){if(!b)throw std::logic_error(m);}
int main(){try{using T=ks::DiagnosticTransferService;auto memory=std::make_unique<uint8_t[]>(1u<<24);uint64_t phases=0,rejects=0,calls=0,reads=0,writes=0;unsigned components=0;
for(unsigned kind=0;kind<4;++kind)for(unsigned seed=0;seed<32;++seed){std::memset(memory.get(),0,1u<<24);for(unsigned n=0;n<448;++n)memory[0x19000+n]=uint8_t(n*17+seed);memory[0xffff8c]=255;memory[0xffff62]=255;memory[0x400004]=255;memory[0x804189]=uint8_t(1+seed%3);memory[0x8041b8]=8;memory[0x8041b9]=3;T::Input i;i.source=0xc1019000;i.checksum=0x12345678u*seed;i.block=uint16_t(seed*17);T s(T::Kind(kind),i);int previous=-1;
while(!s.complete()){const auto cost=s.nextClocks(previous);check(cost,"Codec stalled");const auto read=[&](uint32_t a,unsigned dt){check(dt<=cost,"Read outside budget");++reads;return a==0xffff8c?uint8_t(255):memory[a];};const auto write=[&](uint32_t a,uint8_t v,unsigned dt){check(dt<=cost,"Write outside budget");++writes;memory[a]=v;};const auto oldReads=reads,oldWrites=writes;const T before=s;tracking=true;check(!s.advanceBefore(cost-1,read,write,previous),"Underbudget codec admitted");tracking=false;check(!std::memcmp(&s,&before,sizeof s)&&oldReads==reads&&oldWrites==writes,"Rejected codec mutated");++rejects;tracking=true;check(s.advanceBefore(cost,read,write,previous)==cost,"Codec phase differs");tracking=false;if(s.effect().write)previous=-1;components|=s.components();check(!s.task<std::array<int,3>>(),"Unknown task exposed");(void)s.task<ks::DiagnosticTransferTask>();(void)s.task<ks::SerialDirectTask>();check(++phases<10000000,"Codec stalled");}
check(!s.task<ks::DiagnosticTransferTask>(),"Completed codec exposes active task");(void)s.task<ks::DiagnosticUartService>();(void)s.task<ks::SerialDirectTask>();const T before=s;tracking=true;check(!s.advanceBefore(~0u,[&](auto,auto){++reads;return 0;},[&](auto,auto,auto){++writes;}),"Completed codec restarted");tracking=false;check(!std::memcmp(&s,&before,sizeof s),"Completed codec mutated");++calls;}
check(!allocations&&components==31,"Codec allocation/coverage");std::cout<<"PASS pure DiagnosticTransfer graphs="<<calls<<" phases="<<phases<<" wholeobject rejects="<<rejects<<" sizeof="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
