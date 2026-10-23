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
void word(ks::Board& b,uint32_t a,uint16_t v){b.write(a,uint8_t(v>>8));b.write(a+1,uint8_t(v));}
void longword(ks::Board& b,uint32_t a,uint32_t v){word(b,a,uint16_t(v>>16));word(b,a+2,uint16_t(v));}
void fullTimer(ks::Board& b){
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
void same(ks::Board&a,ks::Board&b){
 check(a.controllerCycles()==b.controllerCycles(),"Handoff moved controller time");
 check(ks::stateMessage(a.state())==ks::stateMessage(b.state()),"Handoff changed musical state");
 check(a.saveUserFlash()==b.saveUserFlash(),"Handoff changed Flash");
 const auto&x=a.cpu();const auto&y=b.cpu();
 check(std::equal(x.memory+0xa00000,x.memory+0xc00000,y.memory+0xa00000)&&std::equal(x.memory+0xffdc00,x.memory+0xfff000,y.memory+0xffdc00),"Handoff changed persistent memory");
 check(a.serialUnderruns==b.serialUnderruns&&a.serialOverflows==b.serialOverflows,"Handoff changed serial accounting");
 for(unsigned port=0xfffe00;port<0xffffa0;++port)check(a.read(port)==b.read(port),"Handoff changed hardware resource");
 uint8_t v,w;while(a.popMidiOut(v))check(b.popMidiOut(w)&&v==w,"Handoff changed outgoing MIDI");check(!b.popMidiOut(w),"Handoff added outgoing MIDI");
}
}
int main(int argc,char**argv){try{
 if(argc!=4){std::cout<<"SKIP: private KS OS/Flash/Wave required\n";return 77;}
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 check(a->boot()&&b->boot(),"Reference boot failed");same(*a,*b);
 uint64_t frames=0,peak=0;
 for(unsigned phase=0;phase<3;++phase)for(unsigned mode=0;mode<3;++mode){
  b->enableNativeController();check(b->nativeController(),"Native handoff failed");
  for(unsigned n=0;(b->nativeController()->activeNativeInterrupts()||(b->cpu().exr&7))&&n<8192;++n){
   a->advance(1);b->advance(1);check(a->controllerCycles()==b->controllerCycles(),"Pre-scenario IRQ drain clock differs");
  }
  check(!b->nativeController()->activeNativeInterrupts()&&!(b->cpu().exr&7),"Previous IRQ did not drain before handoff scenario");
  b->resetControllerMetrics();
  // Exercise selection at a leaf IRQ entry, inside the timer's velocity
  // child, and while its MIDI publication temporarily masks priority seven.
  for(auto*board:{a.get(),b.get()}){
   if(!phase){word(*board,0xffed6c,1);word(*board,0xffed6e,0);board->write(0xffed70,0xf8);}
   else fullTimer(*board);
   const unsigned vector=phase?32:90;
   board->cpu().exr=0;board->cpu().interruptPriority[vector]=phase?6:1;board->cpu().interrupt(vector);
  }
  bool landed=false;
  for(unsigned n=0;n<8192&&!landed;++n){
   a->advance(1);b->advance(1);check(a->controllerCycles()==b->controllerCycles(),"Timer child clock differs before handoff");
   const auto*r=b->nativeController();
   landed=r->activeNativeInterrupts()==1&&(!phase || (phase==1?
    r->nativeTpu0At(ks::Tpu0Service::Component::Velocity)&&b->cpu().exr==6:
    r->nativeTpu0At(ks::Tpu0Service::Component::Midi,true)&&b->cpu().exr==7));
  }
  check(landed,"Switch did not land at requested native IRQ phase");
  if(phase==2)check(a->cpu().getPC()==0x113c&&a->cpu().exr==7,"H8 did not reach the same MIDI masking boundary");
  const auto cycle=b->controllerCycles(),interpreted=b->h8InterpretedSteps();
  b->enableLiveMidiController(mode==1);
  check(!b->nativeController()&&std::string(b->controllerBackend())==(mode==1?"live-cpp-midi-hybrid":"original-h8"),"Requested tail mode not published");
  check(b->controllerCycles()==cycle,"Controller request added guest time");
  if(mode==2){b->enableNativeController();check(b->nativeController(),"Pending reference selection not cancelled");}
  const uint8_t on[]{0x90,uint8_t(60+phase*3+mode),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"MIDI after switch rejected");
  for(unsigned n=0;n<4096;++n){auto x=a->renderSample(),y=b->renderSample();check(x==y,"IRQ handoff changed audio");for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;}
  same(*a,*b);
  if(mode==2)check(b->h8InterpretedSteps()==interpreted,"Cancelled switch executed H8");
  else check(b->h8InterpretedSteps()>interpreted,"Requested reference did not resume");
  const uint8_t off[]{0x80,uint8_t(60+phase*3+mode),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");
 }
 check(peak>1000,"Handoff fixture was silent");
 std::cout<<"PASS: nine H8/hybrid/cancel requests at leaf entry, TPU0 velocity child and TPU0 MIDI critical phase; no added guest advance; "<<frames<<" exact audible 24-stream frames, persistent RAM/hardware/state/Flash/cycles/serial/MIDI exact\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
