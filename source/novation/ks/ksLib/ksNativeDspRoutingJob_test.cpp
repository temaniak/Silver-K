#include "ksNativeDspRoutingJob.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <memory>
#include <vector>
#include <tuple>
#include <iostream>
#include <stdexcept>
#include <random>
using Event=std::tuple<bool,unsigned,unsigned,uint64_t>;
using T=ks::DspRoutingTask;using K=T::Kind;using C=ks::NativeDspContext;using Job=ks::NativeDspRoutingJob;
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
struct Original:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice {std::vector<Event>events;uint32_t reply=0;unsigned wait=0,reads=0;
 static bool dsp(unsigned a){return(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010);}
 static bool watched(unsigned a){return dsp(a)||a==0x159f2||a==0x159f3||(a>=0x174a7&&a<=0x174ab)||a==0x17526||a==0x167ba||a==0xffec22||a==0xffff61||a==0xffff62;}
 uint8_t read(uint32_t a)override{uint8_t v=state->memory[a];if(dsp(a)){auto off=a&15;v=off==4?(reads++<wait?0:7):off==10?uint8_t(reply>>16):off==12?uint8_t(reply>>8):uint8_t(reply);}if(watched(a))events.emplace_back(false,a,v,state->cycles);return v;}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;if(watched(a))events.emplace_back(true,a,v,state->cycles);}
};
struct IO {C&context;uint64_t now=1000;std::vector<Event>events;uint32_t reply=0;unsigned reads=0,wait=0;uint8_t latch=0,control=0;
 static unsigned route(T::Read r){switch(r){case T::Read::PrimaryOffset:return 0x174a8;case T::Read::AlternateOffset:return 0x174aa;case T::Read::VoiceB:return 0x174a7;case T::Read::VoiceA:return 0x17526;case T::Read::VoiceBase:return 0x167ba;case T::Read::CachedAddress:return 0x159f2;}return 0;}
 uint16_t readRoute(T::Read r,unsigned at){uint16_t value=r==T::Read::PrimaryOffset?context.primary:r==T::Read::AlternateOffset?context.alternate:r==T::Read::CachedAddress?context.cached:r==T::Read::VoiceB?context.voiceB:r==T::Read::VoiceA?context.part:context.basePart;auto a=route(r);if(r==T::Read::PrimaryOffset||r==T::Read::AlternateOffset||r==T::Read::CachedAddress){events.emplace_back(false,a,value>>8,now+at);events.emplace_back(false,a+1,value&255,now+at);}else events.emplace_back(false,a,value,now+at);return value;}
 void writeRoute(T::Write w,uint16_t v,unsigned at){auto a=w==T::Write::PrimaryOffset?0x174a8:w==T::Write::AlternateOffset?0x174aa:0x159f2;events.emplace_back(true,a,v>>8,now+at);events.emplace_back(true,a+1,v&255,now+at);}
 uint8_t readDsp(ks::NativeDspJob::Port p,unsigned off,unsigned at){uint8_t v=off==4?(reads++<wait?0:7):off==10?uint8_t(reply>>16):off==12?uint8_t(reply>>8):uint8_t(reply);events.emplace_back(false,(p==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,v,now+at);return v;}
 void writeDsp(ks::NativeDspJob::Port p,unsigned off,uint8_t v,unsigned at){events.emplace_back(true,(p==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,v,now+at);}
 uint8_t readFault(ks::DspFaultTask::Read r,unsigned at){auto value=r==ks::DspFaultTask::Read::Latch?latch:control;events.emplace_back(false,r==ks::DspFaultTask::Read::Latch?0xffec22:0xffff62,value,now+at);return value;}
 void writeFault(ks::DspFaultTask::Write w,uint8_t v,unsigned at){if(w==ks::DspFaultTask::Write::Latch)latch=v;else if(w==ks::DspFaultTask::Write::Control)control=v;events.emplace_back(true,w==ks::DspFaultTask::Write::Latch?0xffec22:w==ks::DspFaultTask::Write::Data?0xffff61:0xffff62,v,now+at);}
};
int main(int argc,char**argv){try{if(argc!=2)return 77;ks::Rom os(argv[1]);check(os.isValid(),"ROM");auto h=std::make_unique<Original>();h->loadmem(os.data().data(),os.data().size(),0);Bus bus;h->memmap(&bus,0,1<<24);std::mt19937 rng(0x4e524f55);uint64_t cases=0,boundaries=0;
 for(unsigned kind=0;kind<9;++kind)for(unsigned port=0;port<2;++port)for(unsigned trial=0;trial<=1024;++trial){
  C c;c.port=port?ks::NativeDspJob::Port::B:ks::NativeDspJob::Port::A;c.primary=uint16_t(rng());c.alternate=uint16_t(rng());c.cached=uint16_t(rng());c.voiceB=uint8_t(trial);c.part=uint8_t(trial>>2);c.basePart=uint8_t(trial>>1);const uint16_t address=uint16_t(rng());const uint32_t word=rng()&0xffffff;Job job(static_cast<K>(kind),address,word,c);IO io{c};io.reply=bus.reply=rng()&0xffffff;io.wait=bus.wait=trial==1024?65537:trial&7;bus.reads=0;
  auto set16=[&](unsigned at,uint16_t value){h->memory[at]=uint8_t(value>>8);h->memory[at+1]=uint8_t(value);};set16(0x174a8,c.primary);set16(0x174aa,c.alternate);set16(0x159f2,c.cached);h->memory[0x174a7]=c.voiceB;h->memory[0x17526]=c.part;h->memory[0x167ba]=c.basePart;h->memory[0xffec22]=h->memory[0xffff62]=0;
  constexpr unsigned entries[]{0x77dc,0x785a,0x784c,0x77f4,0x781a,0x24b2,0x24cc,0x24c6,0x24be};for(unsigned i=0;i<8;++i)h->regs[i].er=int32_t(rng());h->setSP(0xa18000);h->pushPC(h->makepc(0x4338));h->pc=h->makepc(entries[kind]);h->regs[4].r=int16_t(address);h->regs[1].er=int32_t(word<<8);h->regs[5].er=port?0x600000:0x400000;h->ccr=uint8_t(trial);h->exr=0;h->cycles=1000;h->lastread=int(rng());h->lastwrite=int(rng());h->indent=0;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;
  unsigned count=0;while(!job.complete()){bus.events.clear();io.events.clear();auto cost=job.nextClocks();check(!job.advanceBefore(cost-1,io).clocks&&io.events.empty(),"early routing IO");auto progress=job.advanceBefore(cost,io);h->step();io.now+=progress.clocks;if(h->cycles!=io.now||bus.events!=io.events){std::cerr<<"kind="<<kind<<" trial="<<trial<<" step="<<count<<" pc="<<std::hex<<h->getPC()<<std::dec<<" cycles="<<h->cycles<<'/'<<io.now<<" events="<<bus.events.size()<<'/'<<io.events.size()<<'\n';throw std::runtime_error("native route timing/events differ");}check(progress.completed==job.complete(),"nested completion escaped");++boundaries;check(++count<(trial==1024?450000u:2048u),"route stalled");}
  check(unsigned(h->getPC())==0x4338,"native routing return");check(uint16_t((unsigned(h->memory[0x159f2])<<8)|h->memory[0x159f3])==c.cached,"cached mirror");check(uint16_t((unsigned(h->memory[0x174a8])<<8)|h->memory[0x174a9])==c.primary,"primary mirror");check(uint16_t((unsigned(h->memory[0x174aa])<<8)|h->memory[0x174ab])==c.alternate,"alternate mirror");check(h->regs[5].er==(c.port==ks::NativeDspJob::Port::A?0x400000:0x600000),"port");if(kind==2)check(job.result()==io.reply,"read result");++cases;
 }
 std::cout<<"PASS: "<<cases<<" native routing/select/address/word graphs / "<<boundaries<<" independent H8 boundaries; every route/HDI byte event, reply, port/mirrors, clock, typed completion exact; no architectural CPU in native graph\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
