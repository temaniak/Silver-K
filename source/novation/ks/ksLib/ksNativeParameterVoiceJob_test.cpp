#include "ksNativeParameterVoiceJob.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <array>
#include <memory>
#include <vector>
#include <tuple>
#include <random>
#include <iostream>
#include <stdexcept>
using Job=ks::NativeParameterVoiceJob;using Event=std::tuple<bool,unsigned,unsigned,uint64_t>;
void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
struct Original:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
struct Bus:H8SDevice{
 unsigned wait=0,reads=0;std::vector<Event>events;
 static bool dsp(unsigned a){return(a>=0x400000&&a<0x400010)||(a>=0x600000&&a<0x600010);}
 static bool watched(unsigned a){return(a>=0x15000&&a<0x19000)||(a>=0x30000&&a<0x30200)||dsp(a)||a==0xffec22||a==0xffff61||a==0xffff62;}
 uint8_t read(uint32_t a)override{auto value=state->memory[a];if(dsp(a))value=(a&15)==4?(reads++<wait?0:7):0;if(watched(a))events.emplace_back(false,a,value,state->cycles);return value;}
 void write(uint32_t a,uint8_t v)override{state->memory[a]=v;if(watched(a))events.emplace_back(true,a,v,state->cycles);}
};
struct IO{
 std::array<uint8_t,0x4000>data{};std::array<uint8_t,512>patches{};uint8_t latch=0,control=0;unsigned wait=0,reads=0;uint64_t now=1000;std::vector<Event>events;
 uint8_t value(unsigned a)const{a&=0xffffff;if(a>=0x15000&&a<0x19000)return data[a-0x15000];check(a>=0x30000&&a<0x30200,"invalid patch resource");return patches[a-0x30000];}
 uint32_t read(unsigned a,unsigned bytes,unsigned at){uint32_t v=0;for(unsigned i=0;i<bytes;++i){auto b=value(a+i);events.emplace_back(false,(a+i)&0xffffff,b,now+at-(bytes==4&&i<2?3:0));v=(v<<8)|b;}return v;}
 uint8_t readVoiceIndex(unsigned at){return uint8_t(read(0x174a7,1,at));}
 uint32_t readPitch(ks::ParameterPitchTask::Read what,unsigned output,uint8_t slot,uint32_t selected,unsigned at){using R=ks::ParameterPitchTask::Read;switch(what){
 case R::StaticPitch:return read(0x15130,2,at);case R::Octaves:return read(0x30006,1,at);case R::Coarse:return read(0x30007+8*output,1,at);case R::GlobalFine:return read(0x169f0,1,at);case R::PatchFine:return read(0x30008+8*output,1,at);case R::WheelFine:return read(0x17521,1,at);case R::Flags:return read(0x30000,1,at);case R::UnisonCounter:return read(0x17560+20*slot+12,1,at);case R::SelectedPatch:return read(0x17560+20*slot,4,at);case R::Spread:return read(selected+1,1,at);case R::PitchDepth:return read(0x30009+8*output,1,at);case R::Wheel:return read(0x17516,2,at);}throw std::runtime_error("unknown pitch read");}
 uint32_t readAmplitude(ks::ParameterVoiceScaleTask::Read what,uint16_t field,uint8_t slot,uint32_t selected,unsigned at){using R=ks::ParameterVoiceScaleTask::Read;switch(what){case R::Amplitude:check(field>=14&&field<=16,"amplitude binding");return read(0x17560+20*slot+field,1,at);case R::Patch:return read(0x17560+20*slot,4,at);case R::Gain:return read(selected+2,1,at);}throw std::runtime_error("unknown amplitude read");}
 uint8_t readDsp(ks::NativeDspJob::Port port,unsigned off,unsigned at){uint8_t v=off==4?(reads++<wait?0:7):0;events.emplace_back(false,(port==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,v,now+at);return v;}
 void writeDsp(ks::NativeDspJob::Port port,unsigned off,uint8_t v,unsigned at){events.emplace_back(true,(port==ks::NativeDspJob::Port::A?0x400000:0x600000)+off,v,now+at);}
 uint8_t readFault(ks::DspFaultTask::Read r,unsigned at){auto v=r==ks::DspFaultTask::Read::Latch?latch:control;events.emplace_back(false,r==ks::DspFaultTask::Read::Latch?0xffec22:0xffff62,v,now+at);return v;}
 void writeFault(ks::DspFaultTask::Write w,uint8_t v,unsigned at){if(w==ks::DspFaultTask::Write::Latch)latch=v;else if(w==ks::DspFaultTask::Write::Control)control=v;events.emplace_back(true,w==ks::DspFaultTask::Write::Latch?0xffec22:w==ks::DspFaultTask::Write::Data?0xffff61:0xffff62,v,now+at);}
};
int main(int argc,char**argv){try{if(argc!=2)return 77;ks::Rom os(argv[1]);check(os.isValid(),"ROM");auto h=std::make_unique<Original>();h->loadmem(os.data().data(),os.data().size(),0);Bus bus;h->memmap(&bus,0,1<<24);IO io;io.events.reserve(4);bus.events.reserve(4);std::mt19937 rng(0x4e50564f);for(unsigned i=0;i<io.data.size();++i)h->memory[0x15000+i]=io.data[i]=uint8_t(rng());for(unsigned i=0;i<io.patches.size();++i)h->memory[0x30000+i]=io.patches[i]=uint8_t(rng());for(unsigned i=0;i<128;++i)h->interruptPriority[i]=0;h->interruptPriority[28]=6;h->memory[112]=h->memory[113]=0;h->memory[114]=1;h->memory[115]=0x70;
 uint64_t cases=0,boundaries=0,interrupts=0;const unsigned entries[]{0x7e8c,0x8100,0x8374,0x8066,0x82da,0x854e},ends[]{0x7f66,0x81da,0x844e,0x8094,0x8308,0x857c};
 auto put=[&](unsigned a,uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i){uint8_t b=uint8_t(value>>((bytes-1-i)*8));h->memory[a+i]=b;if(a+i>=0x30000)io.patches[a+i-0x30000]=b;else io.data[a+i-0x15000]=b;}};
 auto run=[&](unsigned kind,unsigned port,unsigned input,unsigned wait,unsigned irqAt=~0u,bool mutate=false){auto p=port?ks::NativeDspJob::Port::B:ks::NativeDspJob::Port::A;Job job=kind<3?Job(Job::Pitch{kind},p):Job(Job::Amplitude{uint16_t(kind+11)},p);uint8_t slot=uint8_t(input*149+kind*53);put(0x174a7,slot,1);put(0x17560+20*slot,0xab030100,4);put(0x17560+20*slot+12,uint8_t(input>>8),1);for(unsigned i=14;i<17;++i)put(0x17560+20*slot+i,uint8_t(input),1);put(0x30101,input>>8,1);put(0x30102,input>>8,1);put(0x30000,input,1);put(0x30006,input>>8,1);put(0x15130,input,2);put(0x17516,uint16_t(input*149),2);for(unsigned output=0;output<3;++output){put(0x30007+8*output,input,1);put(0x30008+8*output,uint8_t(input>>8),1);put(0x30009+8*output,input,1);}
  io.now=1000;io.wait=bus.wait=wait;io.reads=bus.reads=0;io.latch=io.control=0;h->memory[0xffec22]=h->memory[0xffff62]=0;for(unsigned i=0;i<8;++i)h->regs[i].er=int32_t(rng());h->setSP(0xa18000);h->pc=h->makepc(entries[kind]);h->regs[6].er=0x30000;h->regs[5].er=port?0x600000:0x400000;h->ccr=uint8_t(input*149);h->exr=0;h->cycles=1000;h->lastread=int(rng());h->lastwrite=int(rng());h->indent=0;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;
  unsigned count=0;while(!job.complete()){io.events.clear();bus.events.clear();auto cost=job.nextClocks();check(!job.advanceBefore(cost-1,io).clocks&&io.events.empty(),"early voice IO");if(count==irqAt){h->pending_irqs=1ull<<28;h->step();io.now+=51;++interrupts;check(h->cycles==io.now&&bus.events.empty(),"voice IRQ pause");++count;++boundaries;continue;}if(mutate&&count%7==0){put(0x169f0,uint8_t(count+input),1);put(0x30101,uint8_t(count*13),1);put(0x30102,uint8_t(count*149),1);put(0x17560+20*slot+12,uint8_t(count),1);put(0x17560+20*slot+14+kind%3,uint8_t(count*53),1);}auto progress=job.advanceBefore(cost,io);h->step();io.now+=progress.clocks;if(h->cycles!=io.now||io.events!=bus.events){std::cerr<<"kind="<<kind<<" input="<<input<<" step="<<count<<" pc="<<std::hex<<h->getPC()<<std::dec<<" time="<<h->cycles<<'/'<<io.now<<" events="<<bus.events.size()<<'/'<<io.events.size()<<'\n';throw std::runtime_error("native voice bytes/clock differ");}check(progress.completed==job.complete(),"voice completion escaped");++boundaries;check(++count<450000,"voice stalled");}
  check(unsigned(h->getPC())==ends[kind]&&h->getSP()==0xa18000,"voice inline continuation");check(io.reads==bus.reads,"voice status reads");++cases;
 };
 for(unsigned kind=0;kind<6;++kind)for(unsigned port=0;port<2;++port){for(unsigned input=0;input<65536;++input)run(kind,port,input,input&7);run(kind,port,93,65537);for(unsigned pause=0;pause<128;++pause)run(kind,port,0xab5d,3,pause,true);}
 std::cout<<"PASS: "<<cases<<" direct native pitch/amplitude jobs / "<<boundaries<<" original H8 boundaries / "<<interrupts<<" IRQ pauses; full real voice lookup/patch resource/DSP/fault children, live mutation and exact byte/address/order/time/continuation\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
