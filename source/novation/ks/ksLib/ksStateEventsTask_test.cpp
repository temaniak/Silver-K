#include "ksStateEventsService.h"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace{bool track=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace{
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
using S=ks::StateEventsService;using K=S::Kind;using C=S::Component;
uint8_t scaled(uint8_t value,uint8_t offset,uint8_t factor,uint16_t bias){
 const int product=int(int8_t(uint8_t(value+offset+128)))*factor;
 int16_t result=int16_t(product<0?uint16_t(product+bias):uint16_t(product-bias));
 if(product<0)result=result>=0?0:std::max<int16_t>(result,-16384);
 else result=result<0?0:std::min<int16_t>(result,16256);
 return uint8_t((uint16_t(uint16_t(result)*2u)>>8)+128);
}
uint8_t converted(uint8_t value,uint8_t selector,uint8_t offset,uint8_t factor){
 return selector==0x22?scaled(value,offset,factor,0x400):selector==0x23?uint8_t(scaled(uint8_t(~value),offset,factor,0)>>1):selector==0x20?uint8_t((unsigned(value)*value)>>9):value;
}
}
int main(){try{
 auto memory=std::make_unique<std::array<uint8_t,0x200000>>();memory->fill(0);
 const auto word=[&](uint32_t a,uint16_t v){(*memory)[a]=uint8_t(v>>8);(*memory)[a+1]=uint8_t(v);};
 const auto wordAt=[&](uint32_t a){return uint16_t((unsigned((*memory)[a])<<8)|(*memory)[a+1]);};
 uint64_t cases=0,phases=0,rejected=0,callbacks=0,live=0,paired=0;uint32_t components=0;
 const auto run=[&](S&service,bool mutate=false){
  int previousWrite=-1;unsigned count=0,mutation=0;
  while(!service.complete()){
   if(mutate){
    if(service.atPanel(ks::PanelInputTask::Stage::ReadProducer)){word(0x15406,8);mutation|=1;}
    if(service.atPanel(ks::PanelInputTask::Stage::ReadEvent)){word(0x1540a,0x6f22);mutation|=2;}
    if(service.atAnalog(ks::PanelAnalogTask::Stage::ReadFactor)){(*memory)[0x158ed]=137;mutation|=4;}
    if(service.atAnalog(ks::PanelAnalogTask::Stage::ReadOffset)){(*memory)[0x158ec]=23;mutation|=8;}
   }
   const auto cost=service.nextClocks(previousWrite);const auto before=callbacks;check(cost,"Owned graph stalled");
   const auto read=[&](uint32_t a,unsigned offset){check(a<memory->size()&&offset<=cost,"Invalid pure read");++callbacks;return(*memory)[a];};
   const auto write=[&](uint32_t a,uint8_t v,unsigned offset){check(a<memory->size()&&offset<=cost,"Invalid pure write");++callbacks;previousWrite=int(a);(*memory)[a]=v;};
   track=true;const auto refused=service.advanceBefore(cost-1,read,write,previousWrite);track=false;
   check(!refused&&before==callbacks&&cost==service.nextClocks(previousWrite),"Small budget changed continuation/resources");++rejected;
   const auto oldComponent=service.component();const bool shared=service.atAnalog(ks::PanelAnalogTask::Stage::WriteFactor)&&cost==6;
   track=true;const auto used=service.advanceBefore(cost,read,write,previousWrite);track=false;
   check(used==cost&&++count<128,"Pure phase failed");++phases;if(shared)++paired;
   components|=(1u<<unsigned(oldComponent))|service.components();
  }
  const auto before=callbacks;check(!service.nextClocks(previousWrite)&&!service.advanceBefore(100,[](uint32_t,unsigned){return uint8_t(0);},[](uint32_t,uint8_t,unsigned){},previousWrite),"Complete service advanced");
  check(callbacks==before,"Complete service touched resources");++cases;if(mutate){check(mutation==15,"Live child coverage incomplete");++live;}
 };
 // Every input/selector includes both wheel children, square tail and identity.
 for(unsigned selector=0;selector<256;++selector)for(unsigned value=0;value<256;++value){
  const uint8_t offset=uint8_t(selector*37+value),factor=uint8_t(value*73+selector);
  (*memory)[0x158ec]=(*memory)[0x158ee]=offset;(*memory)[0x158ed]=(*memory)[0x158ef]=factor;
  for(auto kind:{K::Convert,K::ControlPop}){
   word(0x15406,2);word(0x15408,0);word(0x1540a,uint16_t((value<<8)|selector));
   S service(kind,{uint8_t(value),uint8_t(selector),0,0,0});run(service);
   const auto expected=converted(uint8_t(value),uint8_t(selector),offset,factor);
   check(service.value()==expected&&service.selector()==selector,"Full convert oracle differs");
   if(kind==K::ControlPop)check(service.available()&&service.event()==uint16_t((unsigned(expected)<<8)|selector)&&wordAt(0x15408)==2,"Converted queue publication differs");
  }
 }
 // All valid byte offsets, including odd reads and wrapping consumer updates.
 for(unsigned kind=0;kind<4;++kind){const auto limit=kind==0?128u:512u,producer=kind==0?0x1537cu:kind==3?0x156e8u:0x15406u;
  for(unsigned consumer=0;consumer<limit;++consumer)for(bool empty:{false,true}){
   const auto value=uint16_t(consumer*197+0x5175);const auto produced=empty?consumer:((consumer+2)&(limit-2));
   word(producer,uint16_t(produced));word(producer+2,uint16_t(consumer));word(producer+4+consumer,value);
   S service{K(kind)};run(service);
   const auto expected=empty?(kind==0?0xff00u:0u):kind==1?(unsigned(converted(uint8_t(value>>8),uint8_t(value),(*memory)[uint8_t(value)==0x23?0x158ee:0x158ec],(*memory)[uint8_t(value)==0x23?0x158ef:0x158ed]))<<8)|(value&255):value;
   check(service.available()==!empty&&service.event()==expected,"Event ring oracle differs");
   check(wordAt(producer+2)==(empty?consumer:(consumer+2)&(limit-2)),"Consumer byte-pair publication differs");
  }
 }
 // Independent signed/clipped arithmetic over zero/overflow/full-width biases.
 for(unsigned value=0;value<256;++value)for(unsigned offset:{0u,1u,127u,128u,255u})for(unsigned factor:{0u,1u,64u,127u,128u,255u})for(unsigned bias:{0u,1u,1024u,16384u,32767u,32768u,65535u}){
  (*memory)[0x158e0]=uint8_t(offset);(*memory)[0x158e1]=uint8_t(factor);
  S service(K::Scale,{uint8_t(value),0,uint16_t(bias),0,0x158e0});run(service);
  check(service.value()==scaled(uint8_t(value),uint8_t(offset),uint8_t(factor),uint16_t(bias)),"Standalone scale oracle differs");
 }
 for(unsigned minimum=0;minimum<256;++minimum)for(unsigned maximum=0;maximum<256;++maximum){
  const uint16_t bias=uint16_t(minimum*251+maximum*769);(*memory)[0x158e0]=uint8_t(minimum);(*memory)[0x158e1]=uint8_t(maximum);
  S service(K::Coefficients,{0,0,bias,0x158e0,0x158e4});run(service);
  const auto span=uint8_t(maximum-minimum);
  check((*memory)[0x158e4]==uint8_t(128-uint8_t(minimum+(span>>1)))&&(*memory)[0x158e5]==(span?uint8_t(bias/span):0),"Calibration coefficients oracle differs");
  for(unsigned value:{0u,minimum,maximum,128u,255u}){
   (*memory)[0x158e0]=uint8_t(minimum);(*memory)[0x158e1]=uint8_t(maximum);
   S range(K::Range,{uint8_t(value),0,0,0x158e0,0});run(range);
   check((*memory)[0x158e0]==std::min(value,minimum)&&(*memory)[0x158e1]==std::max(value,maximum),"Range min/max oracle differs");
  }
 }
 for(unsigned value=0;value<256;++value){S service(K::Square,{uint8_t(value),0,0,0,0});run(service);check(service.value()==(value*value)>>9,"Square oracle differs");}
 word(0x15406,2);word(0x15408,0);word(0x1540a,0x0122);S changed(K::ControlPop);run(changed,true);
 check(changed.event()==uint16_t((unsigned(scaled(0x6f,23,137,0x400))<<8)|0x22)&&wordAt(0x15406)==8,"Live queue/curve input was frozen");
 check(components==255&&live==1&&paired&&!allocations,"Graph/component/pairing/allocation coverage incomplete");
 std::cout<<"PASS: "<<cases<<" pure event graphs; "<<phases<<" phases; "<<rejected<<" rejected budgets; "<<paired<<" shared byte writes; all nine kinds/eight components, full convert inputs/calibration ranges, live child resources; zero allocations\n";return 0;
}catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
