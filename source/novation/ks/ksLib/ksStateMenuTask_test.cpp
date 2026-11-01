#include "ksStateMenuService.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
namespace {bool track=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}struct Resources {std::array<uint8_t,1<<24>memory{};unsigned callbacks=0,last=0,cost=0;void see(uint32_t a,unsigned at){check(a<memory.size()&&at<=cost&&(!callbacks||last<=at),"Menu resource address/order/time invalid");++callbacks;last=at;}void longValue(uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)memory[a+n]=uint8_t(v>>(24-8*n));}uint32_t longValue(uint32_t a)const{uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|memory[a+n];return v;}unsigned word(uint32_t a)const{return(unsigned(memory[a])<<8)|memory[a+1];}};}
int main(){try{
 using T=ks::StateMenuService;using K=T::Kind;using E=T::Exit;auto r=std::make_unique<Resources>();uint64_t calls=0,phases=0,rejected=0;uint32_t components=0;
 const auto run=[&](K kind,unsigned page,unsigned special,unsigned edit,unsigned selector,unsigned index,unsigned offset,uint32_t descriptor,uint32_t clock,bool pressed){
  T::Input input;input.pressed=pressed;input.event=0x817f;input.interval=uint16_t(clock);input.index=index;input.descriptor=descriptor;input.action=(clock&1)?T::RepeatAction::Next:T::RepeatAction::Previous;
  const uint32_t base=0x24000;r->memory[0x16790]=uint8_t(page);r->memory[0x1679e]=uint8_t(special);r->memory[0x16784]=uint8_t(edit);r->memory[0x167a1]=uint8_t(offset);
  r->longValue(0xffec10,clock);r->longValue(0x167bc,base);r->longValue(0x167a6,0xdeadbeef);r->memory[71586+index]=uint8_t(selector);r->memory[0x16785]=uint8_t(page);
  r->memory[(descriptor+2)&0xffffff]=uint8_t(selector);r->memory[base+selector]=uint8_t(index);r->memory[(descriptor+index+3)&0xffffff]=uint8_t(offset);r->memory[(descriptor+3)&0xffffff]=uint8_t(offset);
  r->longValue(0x16770,0x7b7b7b7b);r->longValue(0x16774,0x7b7b7b7b);T service(kind,input);unsigned count=0,elapsed=0;int lastWrite=0x23456;
  while(!service.complete()){
   const auto cost=service.nextClocks(lastWrite),mask=service.components();const auto component=service.component();check(cost,"Menu pure graph stalled");r->callbacks=0;r->cost=cost;
   const auto read=[&](uint32_t a,unsigned at){r->see(a,at);return r->memory[a];};
   const auto write=[&](uint32_t a,uint8_t value,unsigned at){r->see(a,at);check((a>=0x16770&&a<0x16778)||(a>=0x167a6&&a<0x167aa)||a==0x167a1,"Menu changed unrelated data");r->memory[a]=value;lastWrite=a<0x400000?int(a):-1;};
   const auto action=[&](T::RepeatAction value,unsigned at){const uint32_t bytes=value==T::RepeatAction::Previous?0x46da:0x471a;for(unsigned n=0;n<4;++n)write(0x16774+n,uint8_t(bytes>>(24-8*n)),at-3*((3-n)/2));};
   track=true;const auto no=service.advanceBefore(cost-1,read,write,action,lastWrite);track=false;check(!no&&!r->callbacks&&service.nextClocks(lastWrite)==cost&&service.component()==component&&service.components()==mask,"Rejected menu phase mutated continuation");++rejected;
   track=true;const auto used=service.advanceBefore(cost,read,write,action,lastWrite);track=false;check(used==cost,"Menu admitted phase clocks differ");elapsed+=cost;++phases;check(++count<128,"Menu pure graph looped");
  }
  if(unsigned(kind)<6){E expected=E::Return;if(kind==K::Toggle)expected=pressed?E::RefreshToggle:E::Release;else if(kind==K::Auxiliary)expected=pressed?E::Auxiliary:E::Release;
   else if(kind==K::RepeatPrevious||kind==K::RepeatNext)expected=kind==K::RepeatPrevious?E::PreviousProgram:E::NextProgram;
   else {const bool next=kind==K::Next;expected=!pressed?E::ReleaseNavigation:page==2?(next?E::NextName:E::PreviousName):special?(next?E::NextSpecial:E::PreviousSpecial):!edit?E::Edit:next?E::NextProgram:E::PreviousProgram;}
   check(service.exit()==expected,"Menu routing policy differs");if(kind==K::Toggle)check(r->memory[0x167a1]==uint8_t(pressed?offset^1:offset),"Menu toggle mutation differs");
   if(expected==E::PreviousProgram||expected==E::NextProgram){const bool next=expected==E::NextProgram;check(r->longValue(0x16774)==(next?0x471au:0x46dau)&&r->word(0x16772)==(kind==K::RepeatPrevious||kind==K::RepeatNext?200u:400u)&&r->word(0x16770)==uint16_t(clock)&&service.observedClock()==clock,"Menu repeat schedule differs");}
  }else {check(service.exit()==E::Return,"Leaf menu graph did not return");switch(kind){case K::EditActive:check(service.edit()==edit&&elapsed==29,"Edit-active observation differs");break;
  case K::RepeatArm:check(service.observedClock()==clock&&r->word(0x16772)==uint16_t(clock)&&r->word(0x16770)==uint16_t(clock)&&elapsed==123,"Standalone repeat arm differs");break;
  case K::Clock:check(service.observedClock()==clock&&elapsed==44,"Menu clock read differs");break;
  case K::BindContext:check(r->longValue(0x167a6)==(selector>=126||page==0||page==9?0x168fcu:0u),"Edit context selection differs");break;
  case K::ClearContext:check(!r->longValue(0x167a6)&&elapsed==78,"Clear context differs");break;
  case K::IndexedAddress:case K::DirectAddress:case K::GlobalAddress:check(service.addressTask()->target()==(kind==K::GlobalAddress?0x168fcu:base)+offset,"Edit address resolution differs");break;default:break;}}
  components|=service.components();++calls;
 };
 for(unsigned kind=0;kind<14;++kind)for(unsigned value=0;value<256;++value)for(unsigned selector:{0u,1u,125u,126u,127u,255u})for(bool pressed:{false,true})run(K(kind),value,value&3,value^0x80,selector,value,(value*97)&255,0x18000,value*0x01010101u,pressed);
 for(K kind:{K::Previous,K::Next})for(unsigned page:{0u,1u,2u,9u,255u})for(unsigned special=0;special<256;++special)for(unsigned edit:{0u,1u,128u,255u})run(kind,page,special,edit,0,0,0,0x18001,0xffffffff,true);
 for(uint32_t descriptor:{0x18000u,0x18001u,0xa18000u,0xa18001u,0xffdc00u,0xffdc01u})for(unsigned selector=0;selector<256;++selector)for(unsigned index:{0u,1u,127u,128u,255u})run(K::IndexedAddress,0,0,0,selector,index,(selector+index)&255,descriptor,0x80000000,true);
 check(components==63&&!allocations,"Menu pure graph incomplete or allocated");std::cout<<"PASS: "<<calls<<" pure menu/edit calls; "<<phases<<" phases; "<<rejected<<" budget rejects; all fourteen roots, navigation/repeat routing, context/indexed/direct/global resolution with low/overlay/internal odd/even resources; zero allocations\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
