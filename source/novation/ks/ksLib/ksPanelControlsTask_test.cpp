#include "ksPanelControlsService.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace {bool track=false;uint64_t allocations=0;}
void* operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Resources {
 std::array<uint8_t,0x17000> low{};std::array<uint8_t,0x2000> internal{};std::array<uint8_t,512> io{};
 unsigned cost=0,callbacks=0,lastOffset=0;
 uint8_t& at(uint32_t a){if(a<low.size())return low[a];if(a>=0xffdc00&&a<0xfffc00)return internal[a-0xffdc00];check(a>=0xfffe00&&a<=0xffffff,"Panel controls address invalid");return io[a&511];}
 void word(uint32_t a,unsigned value){at(a)=uint8_t(value>>8);at(a+1)=uint8_t(value);}
 void observe(unsigned offset){check(offset<=cost&&(!callbacks||offset>=lastOffset),"Panel controls byte clocks invalid");++callbacks;lastOffset=offset;}
};
}
int main(){try{
 using T=ks::PanelControlsService;using K=T::Kind;auto r=std::make_unique<Resources>();uint64_t cases=0,phases=0,rejects=0;uint32_t components=0;
 const auto run=[&](K kind,unsigned timer,unsigned half,unsigned activity,unsigned quarter,unsigned delta,unsigned polarity,unsigned port,unsigned history,unsigned group){
  r->word(0xfffff6,timer);r->at(0xffee73)=uint8_t(half);r->at(0xffee74)=uint8_t(activity);r->at(0xffee72)=uint8_t(quarter);r->at(0xffee75)=uint8_t(delta);
  r->at(0x169fa)=uint8_t(polarity);r->at(0xffff50)=uint8_t(port);r->at(0xffff53)=uint8_t(port);r->at(0x1537a)=uint8_t(history);
  r->at(0x156e2)=uint8_t(group);r->at(0x156e3)=17;r->at(0x156e4)=23;r->at(0xffff6e)=0x7b;r->at(0xffff98)=0x53;
  T service(kind);unsigned elapsed=0,count=0;
  while(!service.complete()){
   const auto cost=service.nextClocks();const auto component=service.component();const auto mask=service.components();check(cost,"Panel controls stalled");r->cost=cost;r->callbacks=0;
   track=true;const auto rejected=service.advanceBefore(cost-1,[&](uint32_t a,unsigned offset){r->observe(offset);return r->at(a);},[&](uint32_t a,uint8_t v,unsigned offset){r->observe(offset);r->at(a)=v;});track=false;
   check(!rejected&&!r->callbacks&&service.component()==component&&service.components()==mask&&service.nextClocks()==cost,"Insufficient controls budget mutated state");++rejects;
   track=true;const auto used=service.advanceBefore(cost,[&](uint32_t a,unsigned offset){r->observe(offset);return r->at(a);},[&](uint32_t a,uint8_t v,unsigned offset){r->observe(offset);check(a==0x1537a||(a>=0xffee72&&a<=0xffee75)||(a>=0x156e2&&a<=0x156e4)||a==0xffff6e||a==0xffff98,"Controls changed unrelated resources");r->at(a)=v;});track=false;
   check(used==cost,"Panel controls phase cost differs");elapsed+=cost;++phases;check(++count<100,"Panel controls looped");
  }
  switch(kind){
  case K::Switches:check(service.switches()==(port&0x30)&&elapsed==24,"Switches result differs");break;
  case K::PedalReset:check(r->at(0x1537a)==255&&elapsed==32,"Pedal reset differs");break;
  case K::Pedal:{const auto normalized=uint8_t(port^uint8_t((polarity>>2)|(polarity<<6)));const bool changed=bool((history^normalized)&64);const auto result=changed?(normalized&64?0:127):128;check(r->at(0x1537a)==normalized&&service.pedalResult()==result&&service.pedalCarry()==bool(polarity&2),"Pedal transition/polarity differs");check(elapsed==unsigned(changed?(normalized&64?136:139):127),"Pedal path clocks differ");break;}
  case K::Encoder:{auto distance=uint8_t(half-uint8_t(timer>>1));if(distance&128)distance=uint8_t(0-distance);auto nextActivity=uint8_t(activity+distance);if(nextActivity)--nextActivity;if(nextActivity>13)nextActivity=13;auto gain=uint8_t((nextActivity>4?nextActivity-4:0)+1);const auto movement=uint8_t(quarter-uint8_t(timer>>2));if(movement&&((movement^delta)&128)){nextActivity=0;gain=1;}if(movement&&!(movement&128))gain=uint8_t(0-gain);check(service.delta()==movement&&service.encoderGain()==gain,"Encoder gain/direction differs");check(r->at(0xffee73)==uint8_t(timer>>1)&&r->at(0xffee72)==uint8_t(timer>>2)&&r->at(0xffee74)==nextActivity&&r->at(0xffee75)==(movement?movement:delta),"Encoder history/activity differs");break;}
  case K::AdcStart:if(group<=32){check(r->at(0x156e2)==group&&r->at(0xffff98)==0x53&&r->at(0x156e3)==17&&elapsed==79,"Busy ADC start changed conversion");}else check(r->at(0x156e2)==0&&r->at(0xffff98)==0x73&&r->at(0xffff6e)==0&&r->at(0x156e3)==9&&elapsed==170,"ADC start/mux conversion differs");break;
  case K::AdcMux:check(r->at(0xffff6e)==group/4&&r->at(0x156e3)==9&&elapsed==54,"ADC mux differs");break;
  case K::AdcResetLast:check(r->at(0x156e4)==255&&elapsed==32,"ADC last-control reset differs");break;
  }
  components|=service.components();++cases;
 };
 for(unsigned port=0;port<256;++port)run(K::Switches,0,0,0,0,0,0,port,0,0);
 run(K::PedalReset,0,0,0,0,0,0,0,0,0);run(K::AdcResetLast,0,0,0,0,0,0,0,0,0);
 for(unsigned polarity=0;polarity<256;++polarity)for(unsigned port=0;port<256;++port)for(unsigned history:{0u,64u,128u,255u})run(K::Pedal,0,0,0,0,0,polarity,port,history,0);
 for(unsigned timer=0;timer<1024;++timer)for(unsigned half:{0u,1u,127u,128u,255u})for(unsigned activity:{0u,1u,3u,4u,13u,14u,255u})for(unsigned movement:{0u,1u,127u,128u,255u})for(unsigned previousDelta:{0u,128u})run(K::Encoder,timer,half,activity,uint8_t((timer>>2)+movement),previousDelta,0,0,0,0);
 for(unsigned group=0;group<256;++group){run(K::AdcStart,0,0,0,0,0,0,0,0,group);run(K::AdcMux,0,0,0,0,0,0,0,0,group);}
 check(components==0x7f&&!allocations,"Controls graph incomplete or allocated");std::cout<<"PASS: "<<cases<<" pure controls calls; "<<phases<<" phases; "<<rejects<<" budget rejects; all seven roots, exhaustive pedal polarity/contact and encoder wrap/activity/direction matrices, exact ADC start/mux; zero allocations\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
