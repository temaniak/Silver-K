#include "ksClockSyncService.h"
#include <array>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <new>
#include <iostream>
#include <stdexcept>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
struct Input{uint8_t divider=1,source=0,flags=0,internal=60;uint16_t gain=0x80,signal=300;uint32_t period=0x1000000,synchronized=0x1000000,coefficient=0,previous=0;uint64_t busy=0;int late=-1;bool live=false;};
uint32_t coefficient(uint32_t rate){uint32_t numerator=0x4000000;for(unsigned n=0;n<5&&!(rate&0x80000000u);++n){rate<<=1;numerator<<=1;}const auto denominator=uint16_t(rate>>16);const auto q=denominator?numerator/denominator:0;const auto remainder=numerator-q*denominator;const uint32_t fraction=remainder<<16;const auto low=denominator?fraction/denominator:0;return(uint32_t(uint16_t(q))<<16)|uint16_t(low);}
}
int main(){try{
 using J=ks::ClockSyncService;using T=ks::ClockSyncTask;std::vector<uint8_t>memory(1<<24);std::array<uint8_t,sizeof(J)>snapshot{};uint64_t graphs=0,phases=0,rejected=0,faults=0,rmw=0,live=0;uint32_t components=0;
 auto run=[&](Input in){
  std::fill(memory.begin(),memory.end(),0);auto put=[&](uint32_t a,uint32_t v,unsigned bytes){for(unsigned n=0;n<bytes;++n)memory[a+n]=uint8_t(v>>(8*(bytes-1-n)));};auto get=[&](uint32_t a,unsigned bytes){uint32_t v=0;for(unsigned n=0;n<bytes;++n)v=(v<<8)|memory[a+n];return v;};
  put(0xffee98,in.divider,1);put(0x169ef,in.source,1);put(0xffee97,in.flags,1);put(0x16950,in.internal,1);put(0xffee7e,in.period,4);put(0xffee8e,in.synchronized,4);put(0xffeea0,in.signal,2);put(0xffeea4,in.gain,2);put(0xffee92,in.coefficient,4);put(0xffee8a,in.previous,4);put(0xffff62,0x50,1);put(0xffec22,0x25,1);
  alignas(J)std::array<unsigned char,sizeof(J)>storage{};tracking=true;auto*s=new(storage.data())J;tracking=false;uint64_t reads=0,clock=0,busy=in.busy;int lastwrite=-1;bool fault=false,mutated=false,late=false;unsigned count=0;
  while(!s->complete()){
   const auto cost=s->nextClocks(lastwrite);check(cost,"No progress cost");std::memcpy(snapshot.data(),s,sizeof(J));bool touched=false;tracking=true;
   const auto denied=s->advanceBefore(cost-1,[&](uint32_t,unsigned)->uint8_t{touched=true;return 0;},[&](uint32_t,uint8_t,unsigned){touched=true;},lastwrite);tracking=false;
   check(!denied&&!touched&&!std::memcmp(snapshot.data(),s,sizeof(J)),"Insufficient budget changed whole object or callbacks");++rejected;
   fault|=s->template task<ks::DspFaultTask>()!=nullptr;
   if(!late&&int(s->component())==in.late){busy=reads+65538;late=true;}
   if(in.live&&!mutated&&s->component()==J::Component::Sync&&s->syncTask().stage()==T::Stage::ReadPublishPeriod){in.period=0x7ffff123;put(0xffee7e,in.period,4);mutated=true;++live;}
   unsigned callbacks=0;uint32_t previousAddress=0;unsigned previousOffset=0;bool previousWasRead=false;const auto stage=s->syncTask().stage();
   tracking=true;const auto used=s->advanceBefore(cost,[&](uint32_t a,unsigned offset){if(offset>cost)throw std::logic_error("Read timing bounds");if(s->component()==J::Component::Sync&&(stage==T::Stage::ClearSourceFlag||stage==T::Stage::WriteSourceFlag)){if(a!=0xffee97||offset!=cost-3)throw std::logic_error("Source RMW read deadline");}++callbacks;previousAddress=a;previousOffset=offset;previousWasRead=true;return(a==0x400004||a==0x600004)?uint8_t(reads++<busy?0:255):memory[a];},[&](uint32_t a,uint8_t v,unsigned offset){if(offset>cost)throw std::logic_error("Write timing bounds");if(s->component()==J::Component::Sync&&(stage==T::Stage::ClearSourceFlag||stage==T::Stage::WriteSourceFlag)){if(!previousWasRead||previousAddress!=a||previousOffset!=cost-3||offset!=cost)throw std::logic_error("Source RMW write deadline");++rmw;}++callbacks;memory[a]=v;lastwrite=a<0x400000?int(a):-1;},lastwrite);tracking=false;
   check(used==cost&&callbacks<=16,"Phase progress/callback bound");if(s->effect().write)lastwrite=-1;clock+=cost;++phases;check(++count<2000000,"Graph stalled");
  }
  const uint8_t divider=in.divider^1;check(memory[0xffee98]==divider,"Divider toggle");if(!(divider&1)){
   const uint8_t active=in.source?uint8_t(in.source-1)&1:in.flags&1;const uint8_t flags=uint8_t((in.flags&~2u)|(in.source?uint8_t(in.source-1)&2:0));check(memory[0xffee97]==flags&&memory[0xffee96]==active,"Clock source flags");
   const uint32_t firstPeriod=active?in.period:uint32_t(uint8_t(in.internal+64))*32u*0xd1b7u;
   uint32_t normalized=firstPeriod<<2;if(active){const uint16_t difference=uint16_t(in.signal-uint16_t(in.synchronized>>16));const uint32_t correction=uint32_t(int32_t(int16_t(difference))*int32_t(int16_t(in.gain)));normalized=in.synchronized+(correction<<2);check(get(0xffee8e,4)==normalized,"External signed feedback");}
   const auto expected=coefficient(normalized);check(s->summary().coefficient==expected,"Packed coefficient/zero divisor");uint32_t distance=in.coefficient-expected;if(distance&0x80000000u)distance=0u-distance;const bool compare=!active||in.gain==0x80;check(get(0xffee92,4)==(compare&&distance>=0x1000?expected:in.coefficient),"Coefficient threshold/transition path");check(get(0xffeea4,2)==(active&&in.gain==0x800?0x7ff:in.gain),"Gain transition");
   const uint32_t published=(in.live?in.period:firstPeriod)&0xfffff000u;check(get(0xffee8a,4)==published,"Period quantization/publication");check(s->summary().coefficientSent==(compare&&distance>=0x1000),"Coefficient transaction decision");check(s->summary().periodSent==(published!=in.previous),"Period transaction decision");if(in.live)check(mutated,"Live period deadline");
  }
  if(in.late>=0)check(late,"Late DSP child not reached");if(in.busy>65536||in.late>=0){check(fault,"Real DSP fault not reached");++faults;}components|=s->components();tracking=true;s->~J();tracking=false;check(clock>0,"No elapsed clocks");++graphs;
 };
 for(unsigned v=0;v<256;++v){Input in;in.internal=uint8_t(v);run(in);in=Input{};in.source=uint8_t(v);in.flags=uint8_t(v*19);run(in);in=Input{};in.divider=uint8_t(v);run(in);}
 for(unsigned gain:{0u,1u,0x7fu,0x80u,0x81u,0x7ffu,0x800u,0x801u,0xffffu})for(uint32_t rate:{0u,1u,0x1ffffu,0x7fffffffu,0x80000000u,0xffffffffu}){Input in;in.source=2;in.gain=uint16_t(gain);in.synchronized=rate;in.signal=uint16_t(rate>>16);run(in);}
 for(unsigned internal:{0u,63u,64u,127u,128u,191u,192u,255u})for(uint32_t delta:{0u,1u,4095u,4096u,4097u,0xfffff000u,0xfffff001u,0x80000000u}){Input in;in.internal=uint8_t(internal);const auto period=uint32_t(uint8_t(in.internal+64))*32u*0xd1b7u;in.coefficient=coefficient(period<<2)+delta;in.previous=period&0xfffff000u;run(in);}

 for(unsigned gain:{0u,1u,0x80u,0x7fffu,0x8000u,0xffffu})for(unsigned signal:{0u,1u,32767u,32768u,65535u})for(uint32_t rate:{0u,0x7fff0000u,0x80000000u,0xffffffffu}){Input in;in.source=2;in.gain=uint16_t(gain);in.signal=uint16_t(signal);in.synchronized=rate;run(in);}
 for(int component:{1,2,3}){Input in;in.late=component;run(in);}
 for(uint32_t address:{0xffee98u,0x169efu,0x16950u,0x159f2u,0x600004u,0xffff62u})check(J::endpoint(address)&&J::endpoint(address|0x42000000u),"Endpoint normalization");
 for(uint32_t address:{0xfff000u,0xfff3ffu,0xfff700u,0xfffc00u,0x420fff00u})check(!J::endpoint(address),"Reserved or unrelated resource admitted");
 static_assert(sizeof(J)<4096,"Clock-sync storage bound");
 for(unsigned busy:{1u,7u,65538u}){Input in;in.busy=busy;run(in);}Input in;in.live=true;run(in);
 check(components==15,"All four graph components missing");check(!allocations,"Pure construction/phase/destruction allocated");std::cout<<"PASS ClockSync pure graphs="<<graphs<<" phases="<<phases<<" rejected_budgets="<<rejected<<" RMW="<<rmw<<" live="<<live<<" faults="<<faults<<" components="<<components<<" sizeof="<<sizeof(J)<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
