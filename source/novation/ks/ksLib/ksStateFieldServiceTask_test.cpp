#include "ksStateFieldService.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>
namespace {bool tracking=false;uint64_t allocations=0;void check(bool b,const char*m){if(!b)throw std::logic_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
struct Resolver {ks::StateFieldCoordinator::Handler field(uint8_t category,uint32_t)const{check(category<8,"Unsupported category");return ks::StateFieldCoordinator::Handler(category);}ks::StatePackFieldsTask::Kind operator()(uint32_t raw)const{check(raw<6,"Recipe");return ks::StatePackFieldsTask::Kind(raw);}};
int main(){try{using T=ks::StateFieldService;static_assert(sizeof(T)<128*1024,"Field fixed semantic storage unexpectedly large");std::vector<uint8_t>m(1<<24);uint64_t graphs=0,phases=0,rejects=0,reads=0,writes=0;uint32_t components=0;unsigned maxDepth=0;
for(unsigned kind=0;kind<25;++kind)for(unsigned seed=0;seed<256;++seed){std::fill(m.begin(),m.end(),0);auto wide=[&](uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)m[a+n]=uint8_t(v>>(24-8*n));};wide(0x167bc,0x32000);wide(0x167c0,0x34000);wide(0x1748e,0x32000);m[0x167ac]=1;m[0x167ad]=3;m[0x169ed]=127;m[0x169f6]=seed&1;m[0x169f8]=seed&2?1:0;m[0x169f9]=seed&1;m[0x16793]=1;m[0x1679f]=1;m[0xffec14]=1;wide(0xffec10,12345);
for(unsigned n=0;n<256;++n){m[0x32000+n]=uint8_t(n*13+seed);m[0x34000+n]=255;}for(unsigned n:{139u,222u,223u,224u,225u,226u})m[0x32000+n]=0;
for(unsigned key=0;key<47;++key){m[0xfd66+8*key]=seed%8;m[0xfd68+8*key]=255;m[0xfd69+8*key]=3;m[0xfd6c+8*key]=0;m[0xfd6d+8*key]=1;}
for(unsigned n=0;n<8;++n)m[0x30000+n]=m[0xfd66+8+n];m[0x30000]=seed%8;
for(unsigned n=0;n<6;++n)wide(71306+4*n,n);
for(unsigned n=0;n<48;++n){m[0x1c0+5*n]=uint8_t(n%4);m[0x1c1+5*n]=1;m[0x1c2+5*n]=255;m[0x1c3+5*n]=7;m[0x1c4+5*n]=uint8_t(n%4);}
T::Input in;in.descriptor=0x30000;in.pointer=0x32003;in.event=uint16_t(((seed&1?255:1)<<8)|1);in.mode=uint16_t((seed&2?256:0)|1);in.limits=0x7f00;in.request=uint8_t(seed%23);in.action.value=0x5006;auto s=std::make_unique<T>(T::Kind(kind),in);Resolver resolver;int lastwrite=-1;unsigned count=0;
while(!s->complete()){const auto cost=s->nextClocks(lastwrite);check(cost,"Stalled child");const auto component=s->component();const auto mask=s->components();const auto depth=s->depth();unsigned callbacks=0;tracking=true;
check(s->advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},resolver,lastwrite)==0,"Insufficient budget accepted");check(!callbacks&&component==s->component()&&mask==s->components()&&depth==s->depth()&&cost==s->nextClocks(lastwrite),"Rejected budget changed graph/resources");++rejects;
check(s->advanceBefore(cost,[&](uint32_t raw,unsigned){++reads;const auto a=raw&0xffffff;return a==0x400004||a==0x600004?uint8_t(255):m[a];},[&](uint32_t raw,uint8_t v,unsigned){++writes;const auto a=raw&0xffffff;m[a]=v;lastwrite=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},resolver,lastwrite)==cost,"Admitted budget differs");tracking=false;if(s->effect().write)lastwrite=-1;maxDepth=std::max(maxDepth,s->depth());check(++count<200000,"Graph did not terminate");++phases;}
components|=s->components();++graphs;}
check(!allocations,"Pure Field allocated");check(maxDepth<=6&&maxDepth>=2,"Semantic nesting fixture missed");std::cout<<"PASS pure Field graphs="<<graphs<<" phases="<<phases<<" budget_rejects="<<rejects<<" reads="<<reads<<" writes="<<writes<<" components="<<components<<" depth="<<maxDepth<<" bytes="<<sizeof(T)<<" allocations="<<allocations<<'\n';return 0;}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
