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
}
int main(int argc,char**argv){try{
 if(argc!=5){std::cout<<"SKIP: private KS OS/Flash/Wave and new output directory required\n";return 77;}
 const std::filesystem::path out(argv[4]);check(!std::filesystem::exists(out),"Output directory already exists");std::filesystem::create_directories(out);
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 check(a->boot()&&b->boot(),"Original boot failed");b->enableNativeController();b->resetControllerMetrics();
 a->startControllerTrace();b->startControllerTrace();std::ofstream pcmA(out/"h8.pcm",std::ios::binary),pcmB(out/"cpp.pcm",std::ios::binary);
 uint64_t frames=0,peak=0;
 for(unsigned item=0;item<5;++item){
  const unsigned vector=item==0?88:item<=2?90:item==3?42:91;
  const auto calls=b->nativeController()->nativeInterruptStarts;
  for(auto*board:{a.get(),b.get()}){
   if(vector==90){board->write(0xffed6c,0);board->write(0xffed6d,0);board->write(0xffed6e,0);board->write(0xffed6f,item==1?255:0);board->write(0xffee6f,0xf8);}
   board->cpu().interruptPriority[vector]=1;board->cpu().interrupt(vector);
  }
  const uint8_t on[]{0x90,uint8_t(48+item*5),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"Note rejected");
  for(unsigned n=0;n<4096;++n){
   if(n==2048){const uint8_t off[]{0x80,uint8_t(48+item*5),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");}
   auto x=a->renderSample(),y=b->renderSample();check(x==y,"Interrupt timeline audio differs");check(a->controllerCycles()==b->controllerCycles(),"Interrupt timeline clock differs");
   pcmA.write(reinterpret_cast<const char*>(x.data()),sizeof(x));pcmB.write(reinterpret_cast<const char*>(y.data()),sizeof(y));
   for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;
  }
  check(b->nativeController()->nativeInterruptStarts>calls,"Scenario did not execute native IRQ");
  const auto&x=a->cpu();const auto&y=b->cpu();check(std::equal(x.memory,x.memory+0xfff000,y.memory)&&std::equal(x.memory+0xfffc00,x.memory+(1<<24),y.memory+0xfffc00),"Interrupt timeline resource memory differs");
  check(ks::stateMessage(a->state())==ks::stateMessage(b->state())&&a->saveUserFlash()==b->saveUserFlash(),"Interrupt timeline state/Flash differs");
  uint8_t v,w;while(a->popMidiOut(v))check(b->popMidiOut(w)&&v==w,"Interrupt timeline MIDI differs");check(!b->popMidiOut(w),"Trailing native MIDI");
 }
 auto x=a->finishControllerTrace(),y=b->finishControllerTrace();x->validate();y->validate();x->save((out/"h8.kst").string());y->save((out/"cpp.kst").string());
 check(bytes(out/"h8.kst")==bytes(out/"cpp.kst"),"Interrupt timeline DSP transfers/slices differ");
 const auto*r=b->nativeController();check(r->nativeInterruptKinds[0]&&r->nativeInterruptKinds[1]&&r->nativeInterruptKinds[2],"Missing native IRQ kind coverage");
 check(r->nativeInterruptStarts+r->nativeInterruptInitial==r->nativeInterruptReturns+r->activeNativeInterrupts(),"Native IRQ accounting differs");
 check(peak>1000&&b->h8StepsSinceNativeHandoff()==0,"Silent fixture or native interpreter fallback");
 std::cout<<"PASS: five forced IRQ timelines, "<<frames<<" exact audible 24-stream frames, "<<x->events.size()<<" exact DSP events; native error/TX/unused="<<r->nativeInterruptKinds[0]<<'/'<<r->nativeInterruptKinds[1]<<'/'<<r->nativeInterruptKinds[2]<<"; cycles/resource RAM/state/Flash/MIDI exact, no post-handoff H8\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
