#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
std::vector<uint8_t> bytes(const char*p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing fixture");return{std::istreambuf_iterator<char>(f),{}};}
void same(ks::Board&a,ks::Board&b){
 check(a.controllerCycles()==b.controllerCycles(),"Handoff moved controller time");
 check(ks::stateMessage(a.state())==ks::stateMessage(b.state()),"Handoff changed musical state");
 check(a.saveUserFlash()==b.saveUserFlash(),"Handoff changed Flash");
 const auto&x=a.cpu();const auto&y=b.cpu();
 check(std::equal(x.memory+0xa00000,x.memory+0xc00000,y.memory+0xa00000)&&std::equal(x.memory+0xffdc00,x.memory+0xfff000,y.memory+0xffdc00),"Handoff changed persistent memory");
 check(a.serialUnderruns==b.serialUnderruns&&a.serialOverflows==b.serialOverflows,"Handoff changed serial accounting");
}
}
int main(int argc,char**argv){try{
 if(argc!=4){std::cout<<"SKIP: private KS OS/Flash/Wave required\n";return 77;}
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 check(a->boot()&&b->boot(),"Reference boot failed");same(*a,*b);
 uint64_t frames=0,peak=0;
 for(unsigned mode=0;mode<3;++mode){
  b->enableNativeController();check(b->nativeController(),"Native handoff failed");
  // Begin a valid transmit interrupt, then select the next controller exactly
  // between its admission/save credits and its first queue observation.
  for(auto*board:{a.get(),b.get()}){
   board->write(0xffed6c,0);board->write(0xffed6d,1);board->write(0xffed6e,0);board->write(0xffed6f,0);board->write(0xffed70,0xf8);
   board->cpu().exr=0;board->cpu().interruptPriority[90]=1;board->cpu().interrupt(90);
  }
  a->advance(1);b->advance(1);check(b->nativeController()->activeNativeInterrupts()==1,"Switch did not land inside native IRQ");
  const auto cycle=b->controllerCycles(),interpreted=b->h8InterpretedSteps();
  b->enableLiveMidiController(mode==1);
  check(!b->nativeController()&&std::string(b->controllerBackend())==(mode==1?"live-cpp-midi-hybrid":"original-h8"),"Requested tail mode not published");
  check(b->controllerCycles()==cycle,"Controller request added guest time");
  if(mode==2){b->enableNativeController();check(b->nativeController(),"Pending reference selection not cancelled");}
  const uint8_t on[]{0x90,uint8_t(60+mode*4),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"MIDI after switch rejected");
  for(unsigned n=0;n<4096;++n){auto x=a->renderSample(),y=b->renderSample();check(x==y,"IRQ handoff changed audio");for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;}
  same(*a,*b);
  if(mode==2)check(b->h8InterpretedSteps()==interpreted,"Cancelled switch executed H8");
  else check(b->h8InterpretedSteps()>interpreted,"Requested reference did not resume");
  const uint8_t off[]{0x80,uint8_t(60+mode*4),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");
 }
 check(peak>1000,"Handoff fixture was silent");
 std::cout<<"PASS: H8/hybrid/cancel requests inside native IRQ; no added guest advance; "<<frames<<" exact audible 24-stream frames, persistent RAM/state/Flash/cycles/serial exact\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
