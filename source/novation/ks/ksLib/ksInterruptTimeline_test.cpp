#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing fixture");return{std::istreambuf_iterator<char>(f),{}};}
void word(ks::Board& b,uint32_t a,uint16_t v){b.write(a,uint8_t(v>>8));b.write(a+1,uint8_t(v));}
void longword(ks::Board& b,uint32_t a,uint32_t v){word(b,a,uint16_t(v>>16));word(b,a+2,uint16_t(v));}
void fullTimer(ks::Board& b){
 // Released GPIO contacts plus one changed two-contact key exercise the
 // velocity child without overflowing the firmware's keyboard event ring.
 b.write(0xffff55,0xff);b.write(0xffff54,0x0c);b.write(0xffff59,0xe0);
 for(unsigned page=0;page<8;++page)word(b,0xb960+2*page,page?0xffff:0xfffe);
 b.write(0xb970,3);b.write(0xb971,60);word(b,0xb972,uint16_t(-30));
 word(b,0x156e8,0x1fe);word(b,0x156ea,0x1fe);word(b,0xffee76,0xffff);
 b.write(0xffec14,0);b.write(0xffee70,128);longword(b,0xffec10,7);
 longword(b,0xffee7a,0xffffffff);longword(b,0xffee7e,1);word(b,0xffee78,0xffff);
 longword(b,0xffee82,0xffffffff);longword(b,0xffee86,0x34);
 b.write(0xffee97,3);b.write(0xffee9a,0xff);
 word(b,0xffed6c,0);word(b,0xffed6e,0);
 for(unsigned n=0;n<4;++n){b.write(0xffec16+n,uint8_t(1u<<n));b.write(0xffec24+n,uint8_t(128u>>n));}
 b.write(0xffec23,1);
}
void sameInterrupts(ks::Board&a,ks::Board&b){
 const auto&x=a.cpu();const auto&y=b.cpu();
 check(x.pending_irqs==y.pending_irqs&&x.pending_irqs_high==y.pending_irqs_high&&x.exr==y.exr,"Interrupt pending masks/priority differ");
 for(unsigned n=0;n<128;++n)check(x.interruptPriority[n]==y.interruptPriority[n],"Interrupt priority table differs");
}
void advanceBoth(ks::Board&a,ks::Board&b){
 a.advance(1);b.advance(1);check(a.controllerCycles()==b.controllerCycles(),"IRQ phase clock differs");sameInterrupts(a,b);
}
void drain(ks::Board&a,ks::Board&b){
 unsigned phases=0;
 while(b.nativeController()->activeNativeInterrupts()||(b.cpu().exr&7)){
  check(++phases<=65536,"Previous IRQ did not drain");advanceBoth(a,b);
 }
}
void adc(ks::Board&b,unsigned group,unsigned remaining=0){
 // Board ADC returns 0x8000. Both ordinary curves and the preview/accepted
 // pair receive a real threshold crossing; one empty ring wraps at its end.
 b.write(0x156e2,uint8_t(group));b.write(0x156e3,uint8_t(remaining));b.write(0x156e4,255);
 word(b,0x15406,0x1fe);word(b,0x15408,0x1fe);
 for(unsigned n=0;n<4;++n){
  const unsigned channel=group+n,offset=2*channel;
  word(b,0x15652+offset,0x8000);word(b,0x1560a+offset,0);b.write(0x1569a+offset,0);
  b.write(0xb93c+channel,uint8_t(n&1));b.write(0xb918+channel,uint8_t(32+n));
 }
 b.write(0xffff98,0xd3);
}
void receive(ks::Board&b,unsigned variant){
 word(b,0xffec64,variant==1?255:0);word(b,0xffec66,0);b.write(0xffed68,0);
 if(variant==1)for(unsigned n=0;n<256;++n)b.write(0xffec68+n,0xfe);
 // Start the real TPU0 counter at a defined phase. Its clock and priority
 // remain enabled while the independently clocked UART and TPU1 run.
 word(b,0xffffd6,0);word(b,0xffffe6,20000);
 b.write(0xffee96,variant==2?0:1);b.write(0xffee9a,variant==3?255:1);
 b.write(0xffee97,0);word(b,0xffee9c,variant==2||variant==3?0x400:0);word(b,0xffee9e,variant==4?19000:1000);
 word(b,0xffeea8,0);longword(b,0xffee78,0x18000);longword(b,0xffeeaa,variant==2||variant==3?0x12345678:0);
 longword(b,0xffeea0,0x04000000);word(b,0xffeea4,0x800);word(b,0xffeea6,0x100);
}
}
int main(int argc,char**argv){unsigned item=0;try{
 if(argc!=5){std::cout<<"SKIP: private KS OS/Flash/Wave and new output directory required\n";return 77;}
 const std::filesystem::path out(argv[4]);check(!std::filesystem::exists(out),"Output directory already exists");std::filesystem::create_directories(out);
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 check(a->boot()&&b->boot(),"Original boot failed");b->enableNativeController();b->resetControllerMetrics();
 a->startControllerTrace();b->startControllerTrace();std::ofstream pcmA(out/"h8.pcm",std::ios::binary),pcmB(out/"cpp.pcm",std::ios::binary);
 uint64_t frames=0,peak=0;
 for(item=0;item<16;++item){
  drain(*a,*b);
  const unsigned vector=item==0?88:item<=2?90:item==3?42:item==4?91:item==5?32:item<10?28:89;
  const unsigned kind=vector==88?0:vector==90?1:(vector==42||vector==91)?2:vector==32?3:vector==28?4:5;
  const auto kindCalls=b->nativeController()->nativeInterruptKinds[kind],received=b->midiReceived;
  const auto calls=b->nativeController()->nativeInterruptStarts;
  for(auto*board:{a.get(),b.get()}){
   if(vector==32)fullTimer(*board);
   if(vector==28)adc(*board,item==9?32:28,item==6?2:item==7?1:0);
   if(vector==89)receive(*board,item-10);
   if(vector==90){board->write(0xffed6c,0);board->write(0xffed6d,0);board->write(0xffed6e,0);board->write(0xffed6f,item==1?255:0);board->write(0xffee6f,0xf8);}
   board->cpu().interruptPriority[vector]=vector==32?6:1;
   // The full-ring case admits an already-latched byte before main can
   // consume its deliberately full ring; all other RX cases use the UART.
   if(vector!=89||item==11){if(item==11){board->write(0xffff8d,0xfe);board->write(0xffff8c,uint8_t(board->read(0xffff8c)|0x40));}board->cpu().interrupt(vector);}
  }
  if(vector==89&&item!=11){const uint8_t byte=item==10?0xfe:0xf8;check(a->enqueueMidi(&byte,1)&&b->enqueueMidi(&byte,1),"Test UART byte rejected");}
  if(item>=6){
   unsigned phases=0;
   while(b->nativeController()->nativeInterruptKinds[kind]==kindCalls||b->nativeController()->activeNativeInterrupts()||(b->cpu().exr&7)){
    check(++phases<=65536,"Forced ADC/RX lifecycle did not finish");advanceBoth(*a,*b);
   }
   if(vector==89&&item!=11)check(a->midiReceived==b->midiReceived&&b->midiReceived>received,"RX case did not consume the actual UART byte");
   if(item==11)check(b->read(0xffed68)==2&&b->read(0xffec65)==255,"Full RX ring did not retain producer/report overflow");
   if(item==12||item==13)check(!b->read(0xffee9c)&&!b->read(0xffee9d)&&!b->read(0xffeeaa)&&!b->read(0xffeeab)&&!b->read(0xffeeac)&&!b->read(0xffeead),"F8 reset did not clear samples/sum");
   if(item>=12&&item<=14)check(b->read(0xffeea0)==4&&!b->read(0xffeea1)&&!b->read(0xffeea2)&&!b->read(0xffeea3),"F8 reset/short feedback unexpectedly changed its estimate");
   if(item==15)check(b->read(0xffeea0)!=4||b->read(0xffeea1)||b->read(0xffeea2)||b->read(0xffeea3),"F8 feedback did not update its estimate");
  }
  const uint8_t on[]{0x90,uint8_t(48+item*5),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"Note rejected");
  for(unsigned n=0;n<4096;++n){
   if(n==2048){const uint8_t off[]{0x80,uint8_t(48+item*5),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");}
   auto x=a->renderSample(),y=b->renderSample();check(x==y,"Interrupt timeline audio differs");check(a->controllerCycles()==b->controllerCycles(),"Interrupt timeline clock differs");sameInterrupts(*a,*b);
   pcmA.write(reinterpret_cast<const char*>(x.data()),sizeof(x));pcmB.write(reinterpret_cast<const char*>(y.data()),sizeof(y));
   for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;
  }
  check(b->nativeController()->nativeInterruptStarts>calls&&b->nativeController()->nativeInterruptKinds[kind]>kindCalls,"Scenario did not execute its native IRQ kind");
  if(vector==32)check(b->nativeController()->nativeTpuComponents==0x7ff,"Full timer did not visit all eleven semantic components");
  if(item==9)check(b->nativeController()->nativeAdcComponents==0x3f,"ADC cases did not visit all six semantic components");
  if(item==15)check(b->nativeController()->nativeSerialReceiveComponents==0xf,"F8 cases did not visit all four semantic components");
  for(unsigned port=0xfffe00;port<0xffffa0;++port)check(a->read(port)==b->read(port),"Interrupt timeline hardware resource differs");
  const auto&x=a->cpu();const auto&y=b->cpu();check(std::equal(x.memory,x.memory+0xfff000,y.memory)&&std::equal(x.memory+0xfffc00,x.memory+(1<<24),y.memory+0xfffc00),"Interrupt timeline resource memory differs");
  check(ks::stateMessage(a->state())==ks::stateMessage(b->state())&&a->saveUserFlash()==b->saveUserFlash(),"Interrupt timeline state/Flash differs");
  uint8_t v,w;while(a->popMidiOut(v))check(b->popMidiOut(w)&&v==w,"Interrupt timeline MIDI differs");check(!b->popMidiOut(w),"Trailing native MIDI");
 }
 auto x=a->finishControllerTrace(),y=b->finishControllerTrace();x->validate();y->validate();x->save((out/"h8.kst").string());y->save((out/"cpp.kst").string());
 // The private replay checkpoint also hashes discarded CPU scratch bytes.
 // Preserve both raw checkpoints and compare every observable tape field.
 check(x->beginCycle==y->beginCycle&&x->endCycle==y->endCycle&&x->events.size()==y->events.size()&&x->slices.size()==y->slices.size(),"Interrupt timeline tape bounds differ");
 for(size_t n=0;n<x->events.size();++n){const auto&p=x->events[n];const auto&q=y->events[n];check(p.cycle==q.cycle&&p.pc==q.pc&&p.dsp==q.dsp&&p.offset==q.offset&&p.value==q.value&&p.flags==q.flags,"Interrupt timeline DSP transfer differs");}
 for(size_t n=0;n<x->slices.size();++n){const auto&p=x->slices[n];const auto&q=y->slices[n];check(p.begin==q.begin&&p.end==q.end&&p.first==q.first&&p.count==q.count,"Interrupt timeline DSP slice differs");}
 const auto*r=b->nativeController();check(r->nativeInterruptKinds[0]&&r->nativeInterruptKinds[1]&&r->nativeInterruptKinds[2]&&r->nativeInterruptKinds[3]&&r->nativeInterruptKinds[4]&&r->nativeInterruptKinds[5],"Missing native IRQ kind coverage");
 check(r->nativeInterruptStarts+r->nativeInterruptInitial==r->nativeInterruptReturns+r->activeNativeInterrupts(),"Native IRQ accounting differs");
 check(peak>1000&&b->h8StepsSinceNativeHandoff()==0,"Silent fixture or native interpreter fallback");
 std::cout<<"PASS: sixteen IRQ timelines including complete TPU0/ADC and UART byte/F8 reset/feedback graphs, "<<frames<<" exact audible 24-stream frames, "<<x->events.size()<<" exact DSP events; native error/TX/unused="<<r->nativeInterruptKinds[0]<<'/'<<r->nativeInterruptKinds[1]<<'/'<<r->nativeInterruptKinds[2]<<"; TPU0="<<r->nativeInterruptKinds[3]<<"; ADC="<<r->nativeInterruptKinds[4]<<", RX="<<r->nativeInterruptKinds[5]<<", components=0x"<<std::hex<<r->nativeTpuComponents<<'/'<<r->nativeAdcComponents<<'/'<<r->nativeSerialReceiveComponents<<"; private checkpoints="<<x->checkpoint<<'/'<<y->checkpoint<<std::dec<<"; cycles/pending/priorities/hardware/resource RAM/state/Flash/MIDI exact, no post-handoff H8\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"scenario="<<item<<": "<<e.what()<<'\n';return 1;}}
