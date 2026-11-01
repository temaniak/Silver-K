#include "ksStateReloadCoordinator.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace {bool tracking=false;uint64_t allocations=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
// Targeted coordinator data-domain checks. Unrelated typed effects are
// explicitly acknowledged here; their complete native graphs are independently
// exercised by StateReloadService_test, rather than modeled in this guard test.
int main(){try{using T=ks::StateReloadCoordinator;uint64_t total=0,phases=0,rejects=0,acknowledged=0;
 for(unsigned site=0;site<4;++site){const unsigned count=site==0?209:site==1?255:228;
  for(unsigned variant=0;variant<12;++variant){const std::array<unsigned,12>addresses{{0xffefff,0xfff000,0xfff700,0xfffbff,0xffffff,0xfff000-count+1,0xb8e8,0x200000-count,0xa0b8e8,0xc00000-count,0xfff000-count,0x80024001}};const auto raw=addresses[variant];const bool unsupported=variant<6;auto m=std::make_unique<std::array<uint8_t,1<<24>>();
   const auto put=[&](unsigned a,unsigned v){for(unsigned n=0;n<4;++n)(*m)[a+n]=uint8_t(v>>(24-8*n));};(*m)[0x167ba]=1;(*m)[0x167bb]=255;(*m)[0x167d0]=(*m)[0x167d1]=15;(*m)[0x845800 + 189]=(*m)[0x845800 + 190]=15;put(0x167bc,0x169fc);put(0x167c0,0x16f12);T service;unsigned observed=0,cpu=0;bool failed=false;int previous=-1;
   try{for(unsigned n=0;n<10000&&!service.complete()&&!observed;++n){if(service.request()!=T::Request::None){check(service.resumeEffects(),"Reload guard effect ownership");++acknowledged;continue;}
    const auto component=service.component();const auto mask=service.components();const auto beforeEffect=service.effect();const bool selected=site==0?component==T::Component::Patch:site==1?component==T::Component::Invalidate:component==T::Component::Expand;const unsigned source=site==1||site==3?0x167c0:0x167bc;const auto cost=service.nextClocks(previous);check(cost,"Reload guard deadlock");unsigned callbacks=0;tracking=true;
    const auto rejected=service.advanceBefore(cost-1,[&](uint32_t,unsigned){++callbacks;return uint8_t(0);},[&](uint32_t,uint8_t,unsigned){++callbacks;},previous);tracking=false;check(!rejected&&!callbacks&&service.component()==component&&service.components()==mask&&service.effect().read==beforeEffect.read&&service.effect().write==beforeEffect.write&&service.nextClocks(previous)==cost,"Rejected reload budget changed resources/owner");++rejects;
    tracking=true;const auto used=service.advanceBefore(cost,[&](uint32_t a,unsigned offset){check(offset<=cost,"Reload guard observation time");if(a>=0xfff000&&a<0xfffc00)++cpu;if(selected&&a>=source&&a<source+4){++observed;if(unsupported)tracking=false;return uint8_t(raw>>(24-8*(a-source)));}return (*m)[a];},[&](uint32_t a,uint8_t v,unsigned offset){check(offset<=cost,"Reload guard publication time");if(a>=0xfff000&&a<0xfffc00)++cpu;(*m)[a]=v;previous=a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;},previous);tracking=false;check(used==cost,"Reload guard phase duration");if(service.effect().write)previous=-1;++phases;
   }}catch(const std::logic_error&){tracking=false;failed=true;}
   check(observed==4&&!cpu&&failed==unsupported,"Reload actual-read span guard differs");++total;
  }
 }
 check(total==48&&!allocations,"Reload guard allocation/count contract differs");std::cout<<"PASS: "<<total<<" independent reload actual-read guard cases / "<<phases<<" admitted phases / "<<rejects<<" rejected insufficient budgets / "<<acknowledged<<" acknowledged unrelated typed effects; exact-span endpoints and full32aliases accepted, scratch/crossing endpoints rejected before derived access; zero admitted-phase allocations\n";return 0;
 }catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
