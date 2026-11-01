#include "ksScheduledTimerService.h"
#include "ksServiceCycleService.h"
#include "ksServiceCycleBindings.h"
#include "ksServiceCycleResolver.h"
#include "ksNativeArpOwnedService.h"
#include "ksMidiFileTransportService.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace TimerProperty {

static void check(bool ok,const char*what){if(!ok)throw std::logic_error(what);}
template<class T>static std::vector<unsigned char> snapshot(const T&v){std::vector<unsigned char> b(sizeof(T));std::memcpy(b.data(),&v,sizeof(T));return b;}
template<class T>static bool unchanged(const T&v,const std::vector<unsigned char>&b){return !std::memcmp(&v,b.data(),sizeof(T));}
int run(){try{
 using S=ks::ScheduledTimerService;using C=ks::ScheduledTimerCoordinator;using K=S::Kind;
 ks::ServiceCycleResolver::Metadata meta{};ks::ServiceCycleResolver resolver(meta);
 std::vector<uint8_t> mem(1u<<24);unsigned reads=0,writes=0;uint64_t servicePhases=0,coordinatorPhases=0;
 const auto read=[&](uint32_t at,unsigned){++reads;return mem.at(at);};
 const auto write=[&](uint32_t at,uint8_t v,unsigned){++writes;mem.at(at)=v;};
 for(unsigned slot=0;slot<5;++slot)for(auto kind:{K::Cancel,K::Arm,K::Service}){
  std::fill(mem.begin(),mem.end(),0);S::Input in;in.delay=7;auto storage=std::make_unique<S>(kind,slot,in);auto&s=*storage;
  while(!s.complete()){
   const auto cost=s.nextClocks(-1);check(cost&&s.nextClocks(-1)==cost,"service next cost");const auto snap=snapshot(s);const auto r=reads,w=writes;
   check(!s.advanceBefore(0,read,write,resolver,-1)&&!s.advanceBefore(cost-1,read,write,resolver,-1)&&unchanged(s,snap)&&r==reads&&w==writes,"service rejection");
   check(s.advanceBefore(cost,read,write,resolver,-1)==cost,"service exact");check(++servicePhases<10000,"service runaway");
  }
  const auto snap=snapshot(s);const auto r=reads,w=writes;check(!s.nextClocks()&&!s.advanceBefore(~0u,read,write,resolver)&&unchanged(s,snap)&&r==reads&&w==writes,"service complete");
 }
 // Coordinator mismatch is checked only after budget admission, before any IO.
 C arm(K::Arm,0,{1,C::Action::FieldClear});const auto cost=arm.nextClocks();const auto before=snapshot(arm);const auto r=reads,w=writes;
 const ks::ScheduledTimerTask::ActionHandle mismatch{C::binding(C::Action::StatusTick)};
 check(!arm.advanceBefore(cost-1,read,write,-1,mismatch)&&unchanged(arm,before)&&r==reads&&w==writes,"mismatch underbudget");
 bool threw=false;try{arm.advanceBefore(cost,read,write,-1,mismatch);}catch(const std::logic_error&){threw=true;}
 check(threw&&unchanged(arm,before)&&r==reads&&w==writes,"mismatch admitted");
 // Drive a service to a real awaiting-action state with a valid bound action.
 std::fill(mem.begin(),mem.end(),0);mem[0x1675b]=1;mem[0xffec13]=2;const auto action=C::binding(C::Action::StatusTick);
 for(unsigned i=0;i<4;++i)mem[0x1675c+i]=uint8_t(action>>(24-8*i));C c(K::Service,0);
 while(!c.awaitingAction()){
  const auto n=c.nextClocks();check(n,"coordinator stalled");const auto snap=snapshot(c);const auto nr=reads,nw=writes;
  check(!c.advanceBefore(0,read,write)&&!c.advanceBefore(n-1,read,write)&&unchanged(c,snap)&&nr==reads&&nw==writes,"coordinator rejection");
  check(c.advanceBefore(n,read,write)==n,"coordinator exact");check(++coordinatorPhases<1000,"coordinator runaway");
 }
 const auto waiting=snapshot(c);const auto nr=reads,nw=writes;check(!c.nextClocks()&&!c.advanceBefore(~0u,read,write)&&unchanged(c,waiting)&&nr==reads&&nw==writes,"awaiting action guard");
 check(c.action()==C::Action::StatusTick&&c.resumeAction(1),"action resume");
 while(!c.complete()){const auto n=c.nextClocks();check(n&&c.advanceBefore(n,read,write)==n,"coordinator finish");check(++coordinatorPhases<1000,"coordinator finish runaway");}
 const auto done=snapshot(c);const auto doneReads=reads,doneWrites=writes;check(!c.nextClocks()&&!c.advanceBefore(~0u,read,write)&&unchanged(c,done)&&reads==doneReads&&writes==doneWrites,"coordinator complete guard");
 std::cout<<"PASS native timer guards servicePhases="<<servicePhases<<" coordinatorPhases="<<coordinatorPhases<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}

namespace MainProperty {
using NativeMain=ks::ServiceCycleService<ks::ScheduledTimerService,ks::NativeMidiPumpService,ks::NativeArpOwnedService,ks::MidiFileTransportService>;
using NativeMainBindings=ks::ServiceCycleBindings<NativeMain,ks::NativeArpOwnedService,ks::MidiFileTransportService>;

static void check(bool ok,const char* msg){if(!ok)throw std::logic_error(msg);}
// A distinct concrete TimerEngine with only the public checked interface.
// It intentionally does not expose or implement advancePlannedPhase.
struct CheckedTimer {
 using Kind=ks::ScheduledTimerService::Kind;
 using Effect=ks::ScheduledTimerService::Effect;
 static unsigned checkedCalls;
 ks::ScheduledTimerService timer;
 CheckedTimer(Kind k,unsigned slot):timer(k,slot){}
 bool complete()const{return timer.complete();}
 unsigned priority()const{return timer.priority();}
 unsigned nextClocks(int previous=-1)const{return timer.nextClocks(previous);}
 const Effect&effect()const{return timer.effect();}
 template<class T>const T*task()const{return timer.template task<T>();}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&r,W&&w,Resolver&&res,int previous=-1){++checkedCalls;return timer.advanceBefore(budget,r,w,res,previous);}
};
unsigned CheckedTimer::checkedCalls=0;
using GenericMain=ks::ServiceCycleService<CheckedTimer,ks::NativeMidiPumpService,ks::NativeArpOwnedService,ks::MidiFileTransportService>;
using GenericBase=ks::ServiceCycleBindings<GenericMain,ks::NativeArpOwnedService,ks::MidiFileTransportService>;
struct GenericBindings {
 using Context=GenericBase::Context;
 GenericBase actual;
 Context after(const CheckedTimer&s,Context c)const{return actual.after(s.timer,c);}
 template<class... A>auto after(A&&...a)const->decltype(actual.after(std::forward<A>(a)...)){return actual.after(std::forward<A>(a)...);}
 template<class... A>auto beforeExternal(A&&...a)const->decltype(actual.beforeExternal(std::forward<A>(a)...)){return actual.beforeExternal(std::forward<A>(a)...);}
};
struct Event {bool write;uint32_t at;uint8_t value;unsigned offset;};
struct Bus {
 std::vector<uint8_t> memory=std::vector<uint8_t>(1u<<24);
 std::array<Event,16> events{};unsigned count=0;
 void reset(){count=0;}
 uint8_t read(uint32_t at,unsigned dt){check(count<events.size(),"event capacity");const auto v=memory.at(at);events[count++]={false,at,v,dt};return v;}
 void write(uint32_t at,uint8_t v,unsigned dt){check(count<events.size(),"event capacity");memory.at(at)=v;events[count++]={true,at,v,dt};}
};
static bool sameEvents(const Bus&a,const Bus&b){if(a.count!=b.count)return false;for(unsigned i=0;i<a.count;++i){const auto&x=a.events[i];const auto&y=b.events[i];if(x.write!=y.write||x.at!=y.at||x.value!=y.value||x.offset!=y.offset)return false;}return true;}
int run(){try{
 ks::ServiceCycleResolver::Metadata metadata{};ks::ServiceCycleResolver resolver(metadata);
 auto main=std::make_unique<NativeMain>();auto direct=std::make_unique<ks::ScheduledTimerService>(ks::ScheduledTimerService::Kind::Service,0);
 auto a=std::make_unique<Bus>();auto b=std::make_unique<Bus>();NativeMainBindings bindings;ks::NativeCompletionNoop observer;
 auto readA=[&](uint32_t at,unsigned dt){return a->read(at,dt);};auto writeA=[&](uint32_t at,uint8_t v,unsigned dt){a->write(at,v,dt);};
 auto readB=[&](uint32_t at,unsigned dt){return b->read(at,dt);};auto writeB=[&](uint32_t at,uint8_t v,unsigned dt){b->write(at,v,dt);};
 const auto dispatch=main->nextClocks(-1,-1);check(dispatch&&main->advanceBefore(dispatch,readA,writeA,resolver,bindings,observer,-1,-1)==dispatch,"Main initial dispatch");
 check(main->current<ks::ScheduledTimerService>(),"Main did not dispatch Timer0");
 a->reset();unsigned phases=0;std::vector<unsigned char> before(sizeof(*main));
 while(const auto* timer=main->current<ks::ScheduledTimerService>()){
  const auto cost=main->nextClocks(-1,-1);check(cost&&cost==timer->nextClocks(-1)&&cost==direct->nextClocks(-1),"Main/direct cost");
  std::memcpy(before.data(),main.get(),before.size());const auto ac=a->count;
  check(!main->advanceBefore(0,readA,writeA,resolver,bindings,observer,-1,-1)&&!main->advanceBefore(cost-1,readA,writeA,resolver,bindings,observer,-1,-1)&&!std::memcmp(before.data(),main.get(),before.size())&&a->count==ac,"Main rejected Timer phase changed state/IO");
  a->reset();b->reset();check(main->advanceBefore(cost,readA,writeA,resolver,bindings,observer,-1,-1)==cost,"Main Timer exact");check(direct->advanceBefore(cost,readB,writeB,resolver,-1)==cost,"direct Timer exact");
  check(sameEvents(*a,*b),"Main/direct Timer IO/timestamps");
  if(const auto* next=main->current<ks::ScheduledTimerService>()){check(next->component()==direct->component()&&next->coordinator().component()==direct->coordinator().component()&&next->nextClocks(-1)==direct->nextClocks(-1)&&next->effect().read==direct->effect().read&&next->effect().write==direct->effect().write,"Main/direct Timer state");}else check(direct->complete(),"Main Timer returned early");
  check(++phases<1000,"Main Timer runaway");
 }
 check(phases&&a->memory==b->memory,"Main/direct Timer memory");
 auto generic=std::make_unique<GenericMain>();GenericBindings genericBindings;auto g=std::make_unique<Bus>();
 auto readG=[&](uint32_t at,unsigned dt){return g->read(at,dt);};auto writeG=[&](uint32_t at,uint8_t v,unsigned dt){g->write(at,v,dt);};
 const auto init=generic->nextClocks(-1,-1);check(init&&generic->advanceBefore(init,readG,writeG,resolver,genericBindings,observer,-1,-1)==init,"Generic Main initial dispatch");check(generic->current<CheckedTimer>(),"Generic Main Timer0 missing");
 const auto n=generic->nextClocks(-1,-1);const auto calls=CheckedTimer::checkedCalls;check(n&&!generic->advanceBefore(n-1,readG,writeG,resolver,genericBindings,observer,-1,-1)&&CheckedTimer::checkedCalls==calls,"Generic rejection called Timer");
 check(generic->advanceBefore(n,readG,writeG,resolver,genericBindings,observer,-1,-1)==n&&CheckedTimer::checkedCalls==calls+1,"Generic checked fallback not called");
 std::cout<<"PASS Main planned Timer vs direct phases="<<phases<<" genericCheckedCalls="<<CheckedTimer::checkedCalls<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}

int main(){if(const auto result=TimerProperty::run())return result;return MainProperty::run();}
