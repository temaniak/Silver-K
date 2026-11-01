#include "ksNativeDspJob.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <memory>
#include <vector>
#include <tuple>
#include <iostream>
#include <stdexcept>
#include <random>
using Job=ks::NativeDspJob;
using Event=std::tuple<bool,unsigned,unsigned,uint64_t>;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
struct Original:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice {static bool observed(unsigned a){return(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)||a==0xffec22||a==0xffff61||a==0xffff62;}std::vector<Event> events;unsigned reads=0,wait=0;uint32_t reply=0;
 uint8_t read(uint32_t a)override{if(a==0xffec22||a==0xffff61||a==0xffff62){auto v=state->memory[a];events.emplace_back(false,a,v,state->cycles);return v;}if((a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010)){unsigned off=a&15;uint8_t v=off==4?(reads++<wait?0:7):off==10?uint8_t(reply>>16):off==12?uint8_t(reply>>8):uint8_t(reply);events.emplace_back(false,a,v,state->cycles);return v;}return state->memory[a];}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;if(observed(a))events.emplace_back(true,a,v,state->cycles);}
};
int main(int argc,char**argv){try{if(argc!=2)return 77;ks::Rom os(argv[1]);check(os.isValid(),"ROM");auto h=std::make_unique<Original>();h->loadmem(os.data().data(),os.data().size(),0);Bus bus;h->memmap(&bus,0,1<<24);for(unsigned i=0;i<128;++i)h->interruptPriority[i]=0;h->interruptPriority[28]=6;h->memory[112]=h->memory[113]=0;h->memory[114]=1;h->memory[115]=0x70;std::mt19937 rng(0x4e445350);uint64_t cases=0,boundaries=0,interrupts=0,faults=0;
 auto verify=[&](unsigned kind,unsigned port,unsigned seed,unsigned wait,unsigned irqAt,bool fault){
  const uint32_t word=rng()&0xffffff,reply=rng()&0xffffff;const uint16_t address=uint16_t(rng());
  Job job=kind<2?Job(Job::Word{word,kind==1,address},port?Job::Port::B:Job::Port::A):Job(Job::Address{address,kind!=2,kind==4?ks::DspAddressTask::PortSelection::A:kind==5?ks::DspAddressTask::PortSelection::B:ks::DspAddressTask::PortSelection::Keep},port?Job::Port::B:Job::Port::A);
  unsigned entry=kind==0?0x2520:kind==1?0x24d8:kind==2?0x2416:kind==3?0x256e:kind==4?0x2568:0x2560;
  for(unsigned i=0;i<8;++i)h->regs[i].er=int32_t(rng());h->setSP(0xa18000);h->pushPC(h->makepc(0x4338));h->pc=h->makepc(entry);h->regs[4].r=int16_t(address);h->regs[1].er=int32_t(word<<8);h->regs[5].er=port?0x600000:0x400000;h->ccr=uint8_t(seed);h->exr=0;h->cycles=1000;h->lastread=int(rng());h->lastwrite=int(rng());h->indent=0;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;bus.reads=0;bus.wait=wait;bus.reply=reply;unsigned reads=0,index=0;uint64_t now=1000;
  auto runTransfer=[&]{while(!job.complete()&&!job.awaitingFault()){
   bus.events.clear();std::vector<Event> actual;actual.reserve(4);const auto base=[&](Job::Port p){return p==Job::Port::A?0x400000u:0x600000u;};
   auto read=[&](Job::Port p,unsigned off,unsigned at){uint8_t value=off==4?(reads++<wait?0:7):off==10?uint8_t(reply>>16):off==12?uint8_t(reply>>8):uint8_t(reply);actual.emplace_back(false,base(p)+off,value,now+at);return value;};
   auto write=[&](Job::Port p,unsigned off,uint8_t value,unsigned at){actual.emplace_back(true,base(p)+off,value,now+at);};unsigned published=0;auto publish=[&](uint16_t value,unsigned at){check(kind==1&&value==uint16_t(address+1),"address publication");check(at==12,"address publication time");++published;};
   const auto cost=job.nextClocks();check(!job.advanceBefore(cost-1,read,write,publish).clocks&&actual.empty(),"early native IO");
   if(index==irqAt){h->pending_irqs=1ull<<28;h->step();now+=51;++interrupts;if(h->cycles!=now||!bus.events.empty())std::cerr<<"IRQ kind="<<kind<<" index="<<index<<" actual="<<h->cycles<<" expected="<<now<<" pc="<<std::hex<<h->getPC()<<std::dec<<" events="<<bus.events.size()<<"\n";check(h->cycles==now&&bus.events.empty(),"native pause IRQ timing");}
   else{auto progress=job.advanceBefore(cost,read,write,publish);h->step();now+=progress.clocks;check(h->cycles==now,"native job time differs");check(actual==bus.events,"native DSP bytes/order/time differ");if(published)check(uint16_t((unsigned(h->memory[0x159f2])<<8)|h->memory[0x159f3])==uint16_t(address+1),"native cached address");}
   ++index;++boundaries;check(index<400000,"stalled");
  }};runTransfer();
  if(fault){check(job.awaitingFault()&&unsigned(h->getPC())==0x2304,"fault request");++faults;
   ks::NativeDspFaultJob child;uint8_t latch=uint8_t(seed),control=uint8_t(seed^0x55);h->memory[0xffec22]=latch;h->memory[0xffff62]=control;
   while(!child.complete()){
    bus.events.clear();std::vector<Event> actual;actual.reserve(4);const auto cost=child.nextClocks();
    auto read=[&](ks::DspFaultTask::Read what,unsigned at){const bool isLatch=what==ks::DspFaultTask::Read::Latch;auto value=isLatch?latch:control;actual.emplace_back(false,isLatch?0xffec22:0xffff62,value,now+at);return value;};
    auto write=[&](ks::DspFaultTask::Write what,uint8_t value,unsigned at){unsigned address=what==ks::DspFaultTask::Write::Latch?0xffec22:what==ks::DspFaultTask::Write::Data?0xffff61:0xffff62;if(what==ks::DspFaultTask::Write::Latch)latch=value;else if(what==ks::DspFaultTask::Write::Control)control=value;actual.emplace_back(true,address,value,now+at);};
    check(!child.advanceBefore(cost-1,read,write).clocks&&actual.empty(),"early fault IO");auto progress=child.advanceBefore(cost,read,write);h->step();now+=progress.clocks;check(h->cycles==now&&actual==bus.events,"native fault control/time differs");++boundaries;
   }
   check(job.resumeFault(),"fault resume");bus.wait=bus.reads;wait=reads;runTransfer();
  }
  check(job.complete()&&unsigned(h->getPC())==0x4338,"typed return");if(kind>=3)check(job.result()==reply,"native 24bit reply");
  check(reads==bus.reads,"status reads differ");++cases;return index;
 };
 for(unsigned kind=0;kind<6;++kind)for(unsigned port=0;port<2;++port){for(unsigned flags=0;flags<256;++flags)verify(kind,port,flags,flags&7,unsigned(-1),false);const auto n=verify(kind,port,0,0,unsigned(-1),false);for(unsigned i=0;i<n;++i)verify(kind,port,0xa5,0,i,false);verify(kind,port,0xa5,unsigned(-1),unsigned(-1),true);}
 std::cout<<"PASS: "<<cases<<" direct native DSP jobs / "<<boundaries<<" H8 reference boundaries / "<<interrupts<<" independent pauses / "<<faults<<" complete fault children; command bytes/replies/event clocks/returns match, native interface has no registers/flags/SP/PC\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
