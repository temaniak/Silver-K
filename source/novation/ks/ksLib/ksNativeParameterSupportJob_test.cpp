#include "ksNativeParameterSupportJob.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <memory>
#include <vector>
#include <tuple>
#include <iostream>
#include <stdexcept>
#include <random>
using Job=ks::NativeParameterSupportJob;using Event=std::tuple<bool,unsigned,unsigned,uint64_t>;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
struct Original:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice {std::vector<Event>events;uint32_t fieldAddress=0,baseAddress=0;unsigned wait=0,reads=0;
 static bool dsp(unsigned a){return(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010);}
 bool watched(unsigned a){return a==0x159f2||a==0x159f3||a==0x30000||a==0x3007d||a==0x3008c||dsp(a)||a==fieldAddress||a==0xffec22||a==0xffff61||a==0xffff62;}
 uint8_t read(uint32_t a)override{uint8_t v=state->memory[a];if(dsp(a))v=(a&15)==4?(reads++<wait?0:7):0;if(watched(a))events.emplace_back(false,a,v,state->cycles);return v;}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;if(watched(a))events.emplace_back(true,a,v,state->cycles);}
};
struct IO {uint64_t now=1000;std::vector<Event>events;unsigned fieldAddress=0,wait=0,reads=0;uint8_t latch=0,control=0;std::array<uint8_t,256> patch{};
 uint8_t readSupport(ks::ParameterSupportTask::Read what,uint16_t field,unsigned at){using R=ks::ParameterSupportTask::Read;unsigned offset=what==R::Rate?field:what==R::Clock?125:what==R::Sync?140:0;events.emplace_back(false,0x30000+offset,patch[offset],now+at);return patch[offset];}
 void writeRoute(ks::DspRoutingTask::Write what,uint16_t value,unsigned at){check(what==ks::DspRoutingTask::Write::CachedAddress,"support route publication");events.emplace_back(true,0x159f2,value>>8,now+at);events.emplace_back(true,0x159f3,value&255,now+at);}
 uint8_t readDsp(ks::NativeDspJob::Port p,unsigned off,unsigned at){uint8_t value=off==4?(reads++<wait?0:7):0;events.emplace_back(false,(p==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,value,now+at);return value;}
 void writeDsp(ks::NativeDspJob::Port p,unsigned off,uint8_t v,unsigned at){events.emplace_back(true,(p==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,v,now+at);}
 uint8_t readFault(ks::DspFaultTask::Read r,unsigned at){auto value=r==ks::DspFaultTask::Read::Latch?latch:control;events.emplace_back(false,r==ks::DspFaultTask::Read::Latch?0xffec22:0xffff62,value,now+at);return value;}
 void writeFault(ks::DspFaultTask::Write w,uint8_t v,unsigned at){if(w==ks::DspFaultTask::Write::Latch)latch=v;else if(w==ks::DspFaultTask::Write::Control)control=v;events.emplace_back(true,w==ks::DspFaultTask::Write::Latch?0xffec22:w==ks::DspFaultTask::Write::Data?0xffff61:0xffff62,v,now+at);}
};
int main(int argc,char**argv){try{if(argc!=2)return 77;ks::Rom os(argv[1]);check(os.isValid(),"ROM");auto h=std::make_unique<Original>();h->loadmem(os.data().data(),os.data().size(),0);Bus bus;h->memmap(&bus,0,1<<24);std::mt19937 rng(0x4e504152);uint64_t cases=0,boundaries=0,interrupts=0;for(unsigned i=0;i<128;++i)h->interruptPriority[i]=0;h->interruptPriority[28]=6;h->memory[112]=h->memory[113]=0;h->memory[114]=1;h->memory[115]=0x70;
 auto run=[&](ks::ParameterSupportTask::Kind kind,unsigned entry,unsigned end,unsigned field,unsigned input,unsigned port,unsigned wait,unsigned flags=~0u,unsigned irqAt=~0u){
 ks::NativeDspContext context;context.port=port?ks::NativeDspJob::Port::B:ks::NativeDspJob::Port::A;Job job(kind,uint16_t(field),context);
  IO io;io.fieldAddress=bus.fieldAddress=0x30000+field;for(unsigned i=0;i<256;++i)h->memory[0x30000+i]=io.patch[i]=uint8_t(rng());io.patch[field]=uint8_t(input);if(flags!=~0u)io.patch[0]=uint8_t(flags);h->memory[io.fieldAddress]=io.patch[field];h->memory[0x30000]=io.patch[0];io.wait=bus.wait=wait;bus.reads=0;io.events.reserve(4);h->memory[0xffec22]=h->memory[0xffff62]=0;
  for(unsigned i=0;i<8;++i)h->regs[i].er=int32_t(rng());h->setSP(0xa18000);if(kind==ks::ParameterSupportTask::Kind::Exit)h->pushPC(h->makepc(0x4338));h->pc=h->makepc(entry);h->regs[6].er=0x30000;h->regs[5].er=port?0x600000:0x400000;h->ccr=uint8_t(input*149);h->exr=0;h->cycles=1000;h->lastread=int(rng());h->lastwrite=int(rng());h->indent=0;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;
  unsigned count=0;while(!job.complete()){
   bus.events.clear();io.events.clear();const auto cost=job.nextClocks();check(!job.advanceBefore(cost-1,io).clocks&&io.events.empty(),"early parameter IO");if(count==irqAt){h->pending_irqs=1ull<<28;h->step();io.now+=51;++interrupts;check(h->cycles==io.now&&bus.events.empty(),"support IRQ pause time/events");++count;++boundaries;continue;}auto progress=job.advanceBefore(cost,io);h->step();io.now+=progress.clocks;if(h->cycles!=io.now||bus.events!=io.events){std::cerr<<"entry="<<std::hex<<entry<<" pc="<<h->getPC()<<std::dec<<" input="<<input<<" step="<<count<<" time="<<h->cycles<<'/'<<io.now<<" events="<<bus.events.size()<<'/'<<io.events.size()<<'\n';throw std::runtime_error("native parameter timeline/events differ");}check(progress.completed==job.complete(),"support child completion escaped");++boundaries;check(++count<450000,"parameter stalled");}
  check(unsigned(h->getPC())==end&&h->getSP()==0xa18000,"inline block continuation/return differs");check(io.reads==bus.reads,"support status reads");check(unsigned(h->regs[5].er)==(context.port==ks::NativeDspJob::Port::A?0x400000u:0x600000u),"support selected port");if(kind==ks::ParameterSupportTask::Kind::Exit)check(context.cached==uint16_t((unsigned(h->memory[0x159f2])<<8)|h->memory[0x159f3]),"support cached address");++cases;
 };

 struct Binding{ks::ParameterSupportTask::Kind kind;unsigned entry,end,field;};
 using K=ks::ParameterSupportTask::Kind;const Binding bindings[]{{K::Rate,0x8898,0x88ae,47},{K::Sync,0x88ae,0x88d6,140},{K::Clock,0x8ac4,0x8ae8,125},{K::Rate,0x8bca,0x8be0,69},{K::Exit,0x8fb6,0x4338,0}};
 for(auto c:bindings)for(unsigned port=0;port<2;++port){for(unsigned input=0;input<256;++input)run(c.kind,c.entry,c.end,c.field,input,port,input&7);if(c.kind!=K::Exit)run(c.kind,c.entry,c.end,c.field,93,port,65537);}

 for(auto c:bindings)for(unsigned port=0;port<2;++port){
  if(c.kind==K::Clock||c.kind==K::Sync)for(unsigned pair=0;pair<65536;++pair)run(c.kind,c.entry,c.end,c.field,pair&255,port,pair&7,pair>>8);
  for(unsigned pause=0;pause<64;++pause)run(c.kind,c.entry,c.end,c.field,93,port,3,0x35,pause);
 }
 std::cout<<"PASS: "<<cases<<" native rate/sync/clock/exit jobs / "<<boundaries<<" original H8 boundaries / "<<interrupts<<" IRQ pauses; fiveblocks/allbytes/bothports/timeoutchildren/fullrouteandsyncchildren; live input/base, DSP bytes and exact clock/continuation/typed results, no CPU projections in native jobs\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
