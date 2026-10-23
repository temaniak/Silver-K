#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <array>
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
void initialMainBoundary(ks::Board&b){
 // boot() includes a musical warmup after ready. Stop before that warmup,
 // exactly where the real main program is about to call its initial reset.
 const uint64_t deadline=uint64_t(ks::Board::ControllerClockHz)*10;
 while(!b.mainEntered&&b.controllerCycles()<deadline){b.advance();b.discardAudio();}
 check(!b.ready,"Coarse boot passed the initial main reset boundary");
 while(!b.ready&&b.controllerCycles()<deadline){b.advance(1);b.discardAudio();}
 check(b.ready&&b.cpu().getPC()==0x4334,"Initial main reset boundary missing");
}
void dirtyResetRings(ks::Board&b){
 word(b,0x1537c,0x7e);word(b,0x1537e,0x42);
 word(b,0x15406,0x1fe);word(b,0x15408,0x80);
 word(b,0x156e8,0x1fe);word(b,0x156ea,0x100);
}
void emptyResetRings(ks::Board&b){
 for(const auto address:std::array<uint32_t,6>{{0x1537c,0x1537e,0x15406,0x15408,0x156e8,0x156ea}})
  check(!b.read(address)&&!b.read(address+1),"Initial State reset did not clear all three event rings");
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
 while(b.nativeController()->activeNativeWork()||(b.cpu().exr&7)){
  check(++phases<=65536,"Previous IRQ did not drain");advanceBoth(a,b);
 }
}
void pedalOutput(ks::Board&b){
 // Physical pedal pins are fixed by Board. Give the next real panel scan
 // the opposite retained level so its normal CC64 publication uses UART TX.
 const auto polarity=b.read(0x168fc+254);
 const auto rotated=uint8_t((polarity>>2)|(polarity<<6));
 const auto normalized=uint8_t(b.read(0xffff50)^rotated);
 b.write(0x1537a,uint8_t(normalized^64));
 const auto tick=uint16_t((unsigned(b.read(0xffec12))<<8)|b.read(0xffec13));
 word(b,0x16780,uint16_t(tick-25));
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
void same(ks::Board&a,ks::Board&b){
 check(a.controllerCycles()==b.controllerCycles(),"Handoff moved controller time");sameInterrupts(a,b);
 check(ks::stateMessage(a.state())==ks::stateMessage(b.state()),"Handoff changed musical state");
 check(a.saveUserFlash()==b.saveUserFlash(),"Handoff changed Flash");
 const auto&x=a.cpu();const auto&y=b.cpu();
 check(std::equal(x.memory+0xa00000,x.memory+0xc00000,y.memory+0xa00000)&&std::equal(x.memory+0xffdc00,x.memory+0xfff000,y.memory+0xffdc00),"Handoff changed persistent memory");
 check(a.serialUnderruns==b.serialUnderruns&&a.serialOverflows==b.serialOverflows,"Handoff changed serial accounting");
 for(unsigned port=0xfffe00;port<0xffffa0;++port)check(a.read(port)==b.read(port),"Handoff changed hardware resource");
 uint8_t v,w;while(a.popMidiOut(v))check(b.popMidiOut(w)&&v==w,"Handoff changed outgoing MIDI");check(!b.popMidiOut(w),"Handoff added outgoing MIDI");
}
}
int main(int argc,char**argv){unsigned phase=0,mode=0;try{
 if(argc!=4){std::cout<<"SKIP: private KS OS/Flash/Wave required\n";return 77;}
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 check(a->boot()&&b->boot(),"Reference boot failed");same(*a,*b);
 uint64_t frames=0,peak=0;
 for(phase=0;phase<17;++phase)for(mode=0;mode<3;++mode){
  if(phase>=10&&phase<13){
   a=std::make_unique<ks::Board>(rom.data(),wave,flash,true);b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
   initialMainBoundary(*a);initialMainBoundary(*b);dirtyResetRings(*a);dirtyResetRings(*b);
  }
  b->enableNativeController();check(b->nativeController(),"Native handoff failed");
  drain(*a,*b);
  b->resetControllerMetrics();
  // Select inside typed children, including ordinary/special ADC tails,
  // the ADC event queue and a received F8's live counter read.
  for(auto*board:{a.get(),b.get()}){
   if(!phase){word(*board,0xffed6c,1);word(*board,0xffed6e,0);board->write(0xffed70,0xf8);}
   else if(phase<3)fullTimer(*board);
   else if(phase<6)adc(*board,phase==4?32:28);
   else if(phase==6)receive(*board,5);
   else if(phase==8)pedalOutput(*board);
   else if(phase==9)board->write(0xffed68,2);
   else if(phase==13){board->write(0x15400,0xfe);const auto tick=uint16_t((unsigned(board->read(0xffec12))<<8)|board->read(0xffec13));word(*board,0x16780,uint16_t(tick-25));}
   if(phase>=14){
    pedalOutput(*board);board->write(0x156e2,36);
    // Retained timer histories force a moving encoder on the next real tick.
    board->write(0xffee72,uint8_t((unsigned(board->read(0xfffff7))>>2)-1));
   }
   if(phase>=7)continue;
   const unsigned vector=!phase?90:phase<3?32:phase<6?28:89;
   board->cpu().exr=0;board->cpu().interruptPriority[vector]=phase>0&&phase<3?6:1;
   if(phase!=6)board->cpu().interrupt(vector);
  }
  if(phase==6){const uint8_t clock=0xf8;check(a->enqueueMidi(&clock,1)&&b->enqueueMidi(&clock,1),"Handoff F8 rejected");
  }
  bool landed=false;
  for(unsigned n=0;n<65536&&!landed;++n){
   advanceBoth(*a,*b);const auto*r=b->nativeController();bool target=false;
   switch(phase){
   case 0:target=true;break;
   case 1:target=r->nativeTpu0At(ks::Tpu0Service::Component::Velocity)&&b->cpu().exr==6;break;
   case 2:target=r->nativeTpu0At(ks::Tpu0Service::Component::Midi,true)&&b->cpu().exr==7;break;
   case 3:target=r->nativeAdcAt(ks::AdcService::Component::OrdinaryScale)&&b->cpu().exr==1;break;
   case 4:target=r->nativeAdcAt(ks::AdcService::Component::SpecialScale)&&b->cpu().exr==1;break;
   case 5:target=r->nativeAdcAt(ks::AdcService::Component::ControlPush)&&b->cpu().exr==1;break;
   case 6:target=r->nativeSerialReceiveAt(ks::SerialReceiveService::Component::Counter)&&b->cpu().exr==1;break;
   case 7:target=r->serialForeground.at(ks::SerialForegroundService::Kind::PopRx,true)&&r->serialForeground.atQueue(ks::SerialQueueTask::Stage::ReadStatus)&&b->cpu().exr==7;break;
   case 8:target=r->serialForeground.at(ks::SerialForegroundService::Kind::Transmit,true)&&r->serialForeground.atTransmit(ks::SerialTransmitTask::Stage::Enable)&&b->cpu().exr==7;break;
   case 9:target=r->serialForeground.at(ks::SerialForegroundService::Kind::ResetRx,true)&&r->serialForeground.atQueue(ks::SerialQueueTask::Stage::RetainWord)&&b->cpu().exr==7;break;
   case 10:target=r->stateResetForeground.at(ks::StateResetService::Component::Buttons)&&b->cpu().exr==0;break;
   case 11:target=r->stateResetForeground.at(ks::StateResetService::Component::Controls)&&b->cpu().exr==0;break;
   case 14:target=r->panelControlsForeground.at(ks::PanelControlsService::Component::Encoder)&&b->cpu().exr==0;break;
   case 15:target=r->panelControlsForeground.at(ks::PanelControlsService::Component::Pedal)&&b->cpu().exr==0;break;
   case 16:target=r->panelControlsForeground.at(ks::PanelControlsService::Component::AdcMux)&&b->cpu().exr==0;break;
   case 13:target=r->panelScanForeground.at(ks::PanelScanService::Component::Push)&&b->cpu().exr==0;break;
   case 12:target=r->stateResetForeground.at(ks::StateResetService::Component::Keyboard)&&b->cpu().exr==0;break;
   }
   landed=r->activeNativeInterrupts()==unsigned(phase<7)&&target;
  }
  check(landed,"Switch did not land at requested native IRQ phase");
  if(phase==2)check(a->cpu().getPC()==0x113c&&a->cpu().exr==7,"H8 did not reach the same MIDI masking boundary");
  if(phase>=3&&phase<7){const unsigned expected=phase==3?0x17a8:phase==4?0x176e:phase==5?0x1848:0x25fe;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same ADC/RX child entry");}
  if(phase>=7&&phase<10){const unsigned expected=phase==7?0x1020:phase==8?0x113c:0xfec;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same foreground critical boundary");}
  if(phase>=10&&phase<13){const unsigned expected=phase==10?0x13fe:phase==11?0x1578:0x1efe;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same State reset child");}
  if(phase==13)check(unsigned(a->cpu().getPC())==0x14e4,"H8 did not reach the same matrix event publication");
  if(phase>=14){const unsigned expected=phase==14?0x188a:phase==15?0x13c2:0x1646;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same panel controls child");}
  const auto*selectedRuntime=b->nativeController();
  const auto cycle=b->controllerCycles(),interpreted=b->h8InterpretedSteps();
  b->enableLiveMidiController(mode==1);
  check(!b->nativeController()&&std::string(b->controllerBackend())==(mode==1?"live-cpp-midi-hybrid":"original-h8"),"Requested tail mode not published");
  check(b->controllerCycles()==cycle,"Controller request added guest time");
  if(mode==2){b->enableNativeController();check(b->nativeController(),"Pending reference selection not cancelled");}
  if(phase>=10&&phase<13){
   check(a->boot()&&b->boot(),"Normal musical warmup failed after State reset handoff");
   check(selectedRuntime->stateResetForeground.completed==1&&selectedRuntime->stateResetForeground.components==0xf,"State reset tail did not finish every logical child");
   same(*a,*b);
  }
  const uint8_t on[]{0x90,uint8_t(phase>=14?36+(phase*3+mode)%72:60+phase*3+mode),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"MIDI after switch rejected");
  for(unsigned n=0;n<4096;++n){auto x=a->renderSample(),y=b->renderSample();check(x==y,"IRQ handoff changed audio");check(a->controllerCycles()==b->controllerCycles(),"Handoff render clock differs");sameInterrupts(*a,*b);for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;}
  same(*a,*b);
  if(mode==2)check(b->h8InterpretedSteps()==interpreted,"Cancelled switch executed H8");
  else check(b->h8InterpretedSteps()>interpreted,"Requested reference did not resume");
  const uint8_t off[]{0x80,uint8_t(phase>=14?36+(phase*3+mode)%72:60+phase*3+mode),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");
 }
 check(peak>1000,"Handoff fixture was silent");
 std::cout<<"PASS: fifty-one H8/hybrid/cancel requests inside IRQ/serial/State reset and panel scan/controls; no added guest advance; "<<frames<<" exact audible 24-stream frames, persistent RAM/hardware/state/Flash/cycles/pending/priorities/serial/MIDI exact\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"phase="<<phase<<", mode="<<mode<<": "<<e.what()<<'\n';return 1;}}
