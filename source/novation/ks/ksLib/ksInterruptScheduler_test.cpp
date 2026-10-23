#include "ksInterruptScheduler.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
bool trackAllocations=false;
uint64_t allocations=0;
void check(bool value,const char*why){if(!value)throw std::runtime_error(why);}
using Scheduler=ks::InterruptScheduler;
using Service=ks::InterruptService;
using Kind=Service::Kind;

// Search priority bands first, then choose the lowest numbered asserted
// vector in that band. This oracle does not use the scheduler's running max.
Scheduler::Selection oracle(uint64_t low,uint64_t high,const std::array<uint8_t,128>&levels,uint8_t mask){
 for(int priority=7;priority>int(mask&7);--priority)
  for(unsigned vector=0;vector<128;++vector)
   if(levels[vector]==priority&&((vector<64?low:high)&(uint64_t(1)<<(vector&63))))
    return{int(vector),uint8_t(priority)};
 return{-1,uint8_t(mask&7)};
}
void selected(uint64_t low,uint64_t high,const std::array<uint8_t,128>&levels,uint8_t mask){
 const auto expected=oracle(low,high,levels,mask),actual=Scheduler::select(low,high,levels.data(),mask);
 check(actual.vector==expected.vector&&actual.priority==expected.priority,"Priority/vector/tie selection differs");
}
uint32_t randomWord(uint32_t&state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
struct Event{
 uint32_t address=0;unsigned offset=0;uint8_t value=0;bool write=false;
 bool operator==(const Event&other)const{return address==other.address&&offset==other.offset&&value==other.value&&write==other.write;}
};
// Only peripheral resources, held in bounded sparse storage. There is no CPU
// state, emulated stack or instruction memory in this independent fixture.
struct Resources{
 struct Cell{uint32_t address=0;uint8_t value=0;};
 std::array<Cell,512> cells{};unsigned count=0;
 std::array<Event,32> events{};unsigned eventCount=0,limit=0;
 void put(uint32_t address,uint8_t value){
  for(unsigned n=0;n<count;++n)if(cells[n].address==address){cells[n].value=value;return;}
  check(count<cells.size(),"Resource fixture capacity exceeded");cells[count++]={address,value};
 }
 uint8_t get(uint32_t address)const{
  for(unsigned n=0;n<count;++n)if(cells[n].address==address)return cells[n].value;
  if(address>=0xffff90&&address<0xffff98)return address&1?0:0x80;
  return 0;
 }
 void word(uint32_t address,uint16_t value){put(address,uint8_t(value>>8));put(address+1,uint8_t(value));}
 void longword(uint32_t address,uint32_t value){word(address,uint16_t(value>>16));word(address+2,uint16_t(value));}
 void append(uint32_t address,uint8_t value,unsigned offset,bool write){
  check(offset<=limit,"Resource transfer crossed its phase");
  check(!eventCount||offset>=events[eventCount-1].offset,"Resource offsets moved backwards");
  check(eventCount<events.size(),"Resource phase log overflow");events[eventCount++]={address,offset,value,write};
 }
 uint8_t read(uint32_t address,unsigned offset){const auto value=get(address);append(address,value,offset,false);return value;}
 void write(uint32_t address,uint8_t value,unsigned offset){append(address,value,offset,true);put(address,value);}
 void begin(unsigned clocks){eventCount=0;limit=clocks;}
 bool same(const Resources&other)const{
  if(count!=other.count||eventCount!=other.eventCount)return false;
  for(unsigned n=0;n<count;++n)if(cells[n].address!=other.cells[n].address||cells[n].value!=other.cells[n].value)return false;
  for(unsigned n=0;n<eventCount;++n)if(!(events[n]==other.events[n]))return false;
  return true;
 }
};
bool same(const Scheduler::Advance&a,const Scheduler::Advance&b){
 return a.clocks==b.clocks&&a.returned==b.returned&&a.mask==b.mask&&a.timer==b.timer&&a.adc==b.adc&&a.receive==b.receive;
}
Scheduler::Advance advance(Scheduler&s,Resources&r,unsigned clocks){
 r.begin(clocks);
 return s.advanceBefore(clocks,[&](uint32_t address,unsigned offset){return r.read(address,offset);},
  [&](uint32_t address,uint8_t value,unsigned offset){r.write(address,value,offset);});
}
void seed(Resources&r,Kind kind){
 r.put(0xffff8c,0xff);r.word(0xffed6c,1);r.word(0xffed6e,0);r.put(0xffed70,0xa5);r.put(0xffff8a,0xf0);
 if(kind==Kind::Tpu0){
  r.put(0xffec14,1);r.put(0xffee70,128);r.longword(0xffec10,7);
  r.longword(0xffee7a,0xffffffff);r.longword(0xffee7e,1);r.word(0xffee78,0xffff);
  r.word(0xffed6c,0);r.word(0xffed6e,0);r.put(0xffee97,3);r.put(0xffee9a,255);r.put(0xffec23,1);
  for(unsigned n=0;n<4;++n){r.put(0xffec16+n,uint8_t(1u<<n));r.put(0xffec24+n,uint8_t(128u>>n));}
 }else if(kind==Kind::Adc){
  r.put(0x156e2,28);r.put(0x156e3,0);r.put(0x156e4,255);r.word(0x15406,0x1fe);r.word(0x15408,0x1fe);
  for(unsigned n=0;n<4;++n){const auto channel=28+n;r.word(0x15652+2*channel,0x8000);r.word(0x1560a+2*channel,0);r.put(0xb93c+channel,uint8_t(n&1));r.put(0xb918+channel,uint8_t(n));}
 }else if(kind==Kind::SerialReceive){
  r.put(0xffff8d,0xf8);r.put(0xffee96,1);r.put(0xffee9a,1);r.word(0xffffe6,20000);r.word(0xffee9e,1000);
  r.longword(0xffee78,0x18000);r.longword(0xffeea0,0x04000000);r.word(0xffeea4,0x800);r.word(0xffeea6,0x100);
 }
}
}
void*operator new(std::size_t size){if(trackAllocations)++allocations;if(void*p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}

int main(){try{
 uint64_t selections=0,phases=0,rejections=0,returns=0,admissions=0,critical=0;
 trackAllocations=true;
 std::array<uint8_t,128> levels{};
 for(unsigned mask=0;mask<256;++mask){selected(0,0,levels,uint8_t(mask));++selections;}
 for(unsigned vector=0;vector<128;++vector)for(unsigned priority=0;priority<8;++priority){
  levels.fill(0);levels[vector]=uint8_t(priority);
  const uint64_t low=vector<64?uint64_t(1)<<vector:0,high=vector>=64?uint64_t(1)<<(vector-64):0;
  for(unsigned mask=0;mask<256;++mask){selected(low,high,levels,uint8_t(mask));++selections;}
 }
 for(unsigned first=0;first<128;++first)for(unsigned second=first+1;second<128;++second)
  for(unsigned priority=1;priority<8;++priority){
   levels.fill(0);levels[first]=levels[second]=uint8_t(priority);uint64_t low=0,high=0;
   (first<64?low:high)|=uint64_t(1)<<(first&63);(second<64?low:high)|=uint64_t(1)<<(second&63);
   for(unsigned mask=0;mask<8;++mask){selected(low,high,levels,uint8_t(0xa0|mask));++selections;}
  }
 uint32_t random=0x524f5554;
 for(unsigned n=0;n<131072;++n){
  for(auto&level:levels)level=uint8_t(randomWord(random)&7);
  const auto low=(uint64_t(randomWord(random))<<32)|randomWord(random),high=(uint64_t(randomWord(random))<<32)|randomWord(random);
  selected(low,high,levels,uint8_t(randomWord(random)));++selections;
 }
 // The upper mask bits are retained for the caller, but only its low three
 // bits participate in admission. Invalid levels cannot mutate an empty job.
 for(unsigned before=0;before<256;++before)for(unsigned priority=0;priority<256;++priority){
  Scheduler scheduler;const bool expected=priority&&priority<8&&priority>(before&7);
  check(scheduler.admit(Kind::Unused,uint8_t(priority),uint8_t(before))==expected,"Empty admission validity differs");
  if(expected){
   ++admissions;Resources resources;check(scheduler.depth()==1&&scheduler.mask()==priority&&scheduler.nextClocks()==23,"Unused admission state differs");
   const auto result=advance(scheduler,resources,23);++phases;++returns;
   check(result.clocks==23&&result.returned&&result.mask==before&&!scheduler.active()&&!scheduler.current()&&!scheduler.nextClocks()&&!scheduler.mask()&&!resources.eventCount,"Unused return/mask/context differs");
  }else check(!scheduler.active()&&!scheduler.depth()&&!scheduler.current()&&!scheduler.nextClocks(),"Rejected empty admission created work");
 }
 {
  Scheduler scheduler;Resources resources;seed(resources,Kind::SerialError);
  check(scheduler.admit(Kind::SerialError,1,0xa0),"Error admission failed");++admissions;
  advance(scheduler,resources,11);advance(scheduler,resources,4);phases+=2; // Retain the error status before nesting.
  check(scheduler.admit(Kind::SerialTransmit,2,0xa1),"Transmit nesting failed");++admissions;
  for(unsigned priority=3;priority<=7;++priority){check(scheduler.admit(Kind::Unused,uint8_t(priority),uint8_t(0xa0|priority-1)),"Priority nesting failed");++admissions;}
  check(scheduler.depth()==Scheduler::Capacity&&scheduler.mask()==7,"Seven-level nesting differs");
  for(unsigned priority=0;priority<256;++priority)check(!scheduler.admit(Kind::Unused,uint8_t(priority),0),"Full/high-priority stack accepted work");
  for(unsigned depth=7;depth>=3;--depth){
   const auto result=advance(scheduler,resources,23);++phases;++returns;
   check(result.returned&&result.mask==uint8_t(0xa0|depth-1)&&scheduler.depth()==depth-1&&scheduler.mask()==depth-1,"Nested return order/mask differs");
  }
  while(scheduler.active()){
   const auto kind=scheduler.current()->kind();const auto depth=scheduler.depth();
   const auto result=advance(scheduler,resources,scheduler.nextClocks());++phases;
   if(result.returned){++returns;check(result.mask==uint8_t(kind==Kind::SerialTransmit?0xa1:0xa0)&&scheduler.depth()==depth-1,"Serial nested return differs");}
  }
  check(resources.get(0xffff8b)==0xa5&&resources.get(0xffed6f)==1&&resources.get(0xffff8c)==0xc7&&resources.get(0xffed68)==2,"Paused IRQ snapshot/live nested publication differs");
 }
 // Pair each rejected-budget run with an untouched control. Comparing every
 // phase/result/resource catches hidden progress even if the next cost repeats.
 for(unsigned kindValue=0;kindValue<6;++kindValue){
  const auto kind=static_cast<Kind>(kindValue);Scheduler actual,control;Resources left,right;seed(left,kind);right=left;
  check(actual.admit(kind,3,0xa1)&&control.admit(kind,3,0xa1),"Service admission failed");admissions+=2;
  unsigned count=0;
  while(actual.active()){
   check(++count<100000,"Peripheral lifecycle stalled");
   const auto cost=actual.nextClocks(),depth=actual.depth();const auto mask=actual.mask();
   for(const auto budget:std::array<unsigned,3>{{0,cost/2,cost-1}}){
    const auto result=advance(actual,left,budget);++rejections;
    check(same(result,{})&&!left.eventCount&&actual.nextClocks()==cost&&actual.depth()==depth&&actual.mask()==mask,"Rejected budget mutated continuation/resources");
   }
   const auto a=advance(actual,left,cost),b=advance(control,right,cost);++phases;
   check(same(a,b)&&left.same(right)&&actual.depth()==control.depth()&&actual.mask()==control.mask()&&actual.nextClocks()==control.nextClocks(),"Budget rejection changed later phase/resource effects");
   if(a.returned){++returns;check(a.mask==0xa1&&!actual.active(),"Service did not restore original mask");}
   if(actual.mask()==7){
    ++critical;
    check(kind==Kind::Tpu0&&actual.current()->critical(),"Unexpected temporary mask owner");
    for(unsigned priority=1;priority<=7;++priority)check(!actual.admit(Kind::Unused,uint8_t(priority),0),"Stale caller mask bypassed a critical section");
   }
  }
  check(!advance(actual,left,1000).clocks&&!left.eventCount,"Completed scheduler restarted");
 }
 check(critical>0,"Critical TPU0 fixture did not enter its MIDI section");
 trackAllocations=false;check(!allocations,"Pure scheduler allocated while selecting/admitting/advancing");
 std::cout<<"PASS: "<<selections<<" independent selection cases, "<<admissions<<" admitted jobs, "<<phases<<" phases, "<<returns<<" returns, "<<rejections<<" insufficient-budget rejects; seven-level nesting, exact previous masks, retained/live resources and "<<critical<<" critical phases; zero allocations\n";
 return 0;
 }catch(const std::exception&e){trackAllocations=false;std::cerr<<e.what()<<'\n';return 1;}}
