#include "ksCurrentNativeChildren_test.h"
#include "ksBoard.h"
#include "ksRom.h"
#include <algorithm>
#include <array>
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
void menuInput(ks::Board& b,unsigned item){
 b.write(0x16790,0);b.write(0x1679e,0);b.write(0x16784,item==2?0:1);
 if(item==2){word(b,0x15406,2);word(b,0x15408,0);word(b,0x1540a,0x4000);return;}
 const unsigned handler=item?0x46ec:0x46ac;unsigned logical=0,matches=0;
 for(unsigned key=0;key<32;++key){const unsigned descriptor=0xfcc4+key*3,action=b.read(descriptor)&127,index=b.read(descriptor+1),at=0xfd1e +4*index;
  uint32_t target=0;for(unsigned n=0;n<4;++n)target=(target<<8)|b.read(at+n);
  if(action==1&&(target&0xffffff)==handler){logical=key;++matches;}}
 check(matches==1,"Menu action descriptor is ambiguous");word(b,0x1537c,2);word(b,0x1537e,0);word(b,0x15380,uint16_t(0x100|logical));
}
void enqueueMulti(ks::Board& b){
 auto state=b.state();state.performance=true;state.activeMask=15;
 for(unsigned part=0;part<4;++part){state.parts[part]=state.parts[0];state.parts[part][179]=100;state.parts[part][181]=uint8_t(part);state.parts[part][182]=0;state.parts[part][183]=127;}
 state.parts[0][189]=1;state.parts[0][190]=15;
 const uint8_t mode[]{0xb0,32,65,0xc0,0};check(b.enqueueMidi(mode,sizeof(mode)),"Store mode MIDI rejected");
 const auto message=ks::stateMessage(state);check(b.enqueueMidi(message.data(),message.size()),"Store host state rejected");
}
void storeInput(ks::Board& b,bool multi,bool singleInMulti=false){
 // Host-loaded Multi has no selected preset destination; choose bank1/program3 as the Store menu does.
 if(singleInMulti)word(b,0x167ac,0x0103);
 const char* name=singleInMulti?"CXX SAVE PART!!!":multi?"CXX SAVE MULTI!!":"CXX SAVE SINGLE!";for(unsigned n=0;n<16;++n)b.write(0x1733c+n,uint8_t(name[n]));
 b.write(0x16790,multi?2:3);b.write(0x16784,0);b.write(0x16785,0);b.write(0x167ba,multi||singleInMulti?1:0);b.write(0x16799,0);b.write(0x168fc+239,1);
 unsigned logical=0,matches=0;
 for(unsigned key=0;key<32;++key){const unsigned descriptor=0xfcc4+3*key,index=b.read(descriptor+1),at=0xfd1e +4*index;uint32_t handler=0;for(unsigned n=0;n<4;++n)handler=(handler<<8)|b.read(at+n);
  if((b.read(descriptor)&127)==1&&(handler&0xffffff)==0x472c){logical=key;++matches;}}
 check(matches==1,"Store descriptor ambiguous");word(b,0x1537c,2);word(b,0x1537e,0);word(b,0x15380,uint16_t(0x100|logical));
}
void applyInput(ks::Board& b,bool packed){
 b.write(0x16790,0);b.write(0x1679e,0);b.write(0x16784,0);b.write(0x169f9,0);b.write(0x169ed,127);
 uint32_t live=0;for(unsigned n=0;n<4;++n)live=(live<<8)|b.read(0x167c0+n);
 for(unsigned field:{3u,127u,222u})b.write((live+field)&0xffffff,255);
 word(b,0x15406,2);word(b,0x15408,0);word(b,0x1540a,packed?0x0126:0x4000);
}
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
}
int main(int argc,char**argv){unsigned item=0;try{
 if(argc!=5){std::cout<<"SKIP: private KS OS/Flash/Wave and new output directory required\n";return 77;}
 const std::filesystem::path out(argv[4]);check(!std::filesystem::exists(out),"Output directory already exists");std::filesystem::create_directories(out);
 ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");auto flash=bytes(argv[2]),wave=bytes(argv[3]);
 auto a=std::make_unique<ks::Board>(rom.data(),wave,flash,true),b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
 initialMainBoundary(*a);initialMainBoundary(*b);b->enableNativeController();b->resetControllerMetrics();
 dirtyResetRings(*a);dirtyResetRings(*b);
 a->startControllerTrace();b->startControllerTrace();std::ofstream pcmA(out/"h8.pcm",std::ios::binary),pcmB(out/"cpp.pcm",std::ios::binary);
 unsigned resetPhases=0;
 while(!b->nativeController()->stateResetForeground.completed||b->nativeController()->activeNativeWork()){
  check(++resetPhases<=65536,"Natural initial State reset did not finish");advanceBoth(*a,*b);
 }
 emptyResetRings(*a);emptyResetRings(*b);
 check(b->nativeController()->stateResetForeground.kinds[0]==1&&b->nativeController()->stateResetForeground.components==0xf,"Natural initial State reset missed its complete child graph");
 check(a->boot()&&b->boot(),"Normal musical warmup failed after initial State reset");
 uint64_t frames=0,peak=0;
 for(item=0;item<31;++item){
  drain(*a,*b);
  const bool foreground=item>=16;
  const bool panelScan=item==17;
  const bool stateEvents=item>=18&&item<20;
  const bool statePanel=item>=20&&item<22;
  const bool stateMenu=item>=22&&item<25;
  const bool stateStore=(item>=25&&item<27)||item==30;
  const bool stateDisplay=item==27;
  const bool stateApply=item>=28&&item<30;
  const unsigned vector=foreground?128:item==0?88:item<=2?90:item==3?42:item==4?91:item==5?32:item<10?28:89;
  const unsigned kind=vector==88?0:vector==90?1:(vector==42||vector==91)?2:vector==32?3:vector==28?4:5;
  const auto kindCalls=foreground?b->nativeController()->serialForeground.kinds[4]:b->nativeController()->nativeInterruptKinds[kind],received=b->midiReceived;
  const auto calls=b->nativeController()->nativeInterruptStarts;
  const auto panelCalls=b->nativeController()->statePanelForeground.started;const auto fieldCalls=b->nativeController()->stateFieldForeground.started;bool sawFieldAddress=false,sawFieldApply=false;
  const unsigned menuKind=item==22?2:item==23?4:11;const auto menuCalls=b->nativeController()->stateMenuForeground.kinds[menuKind];
  const auto applyCalls=b->nativeController()->stateApplyForeground.kinds[0];
  const auto displayCalls=b->nativeController()->stateDisplayForeground.kinds[0];const auto screenValueCalls=b->nativeController()->stateScreenForeground.kinds[unsigned(ks::StateScreenService::Kind::Value)];bool sawNestedFormat=false;
  const unsigned storeKind=item==26?0:1;auto storeCalls=b->nativeController()->stateSaveForeground.kinds[storeKind];auto flashRevision=b->flashRevision;bool sawReload=false,sawVerify=false;
  if(item==26){enqueueMulti(*a);enqueueMulti(*b);for(unsigned n=0;n<24000;++n){check(a->renderSample()==b->renderSample(),"Timeline store preparation audio differs");check(a->controllerCycles()==b->controllerCycles(),"Timeline store preparation clocks differ");sameInterrupts(*a,*b);}drain(*a,*b);storeCalls=b->nativeController()->stateSaveForeground.kinds[storeKind];flashRevision=b->flashRevision;}
  for(auto*board:{a.get(),b.get()}){
   if(stateStore)storeInput(*board,item==26,item==30);
   if(stateDisplay)menuInput(*board,2);
   if(stateApply)applyInput(*board,item==29);
   if(foreground)pedalOutput(*board);
   if(panelScan)board->write(0x15400,0xfe);
   if(stateMenu)menuInput(*board,item-22);
   if(statePanel){word(*board,0x156e8,2);word(*board,0x156ea,0);word(*board,0x156ec,item==20?0x643c:0xe43c);board->write(0x169ed,255);board->write(0x16798,0);for(unsigned part=0;part<5;++part)board->write(0x160d2+24*part+327,0);}
   if(stateEvents){word(*board,0x15406,2);word(*board,0x15408,0);word(*board,0x1540a,item==18?0x4022:0x7f20);}
   if(vector==32)fullTimer(*board);
   if(vector==28)adc(*board,item==9?32:28,item==6?2:item==7?1:0);
   if(vector==89)receive(*board,item-10);
   if(vector==90){board->write(0xffed6c,0);board->write(0xffed6d,0);board->write(0xffed6e,0);board->write(0xffed6f,item==1?255:0);board->write(0xffee6f,0xf8);}
   if(foreground)continue;
   board->cpu().interruptPriority[vector]=vector==32?6:1;
   // The full-ring case admits an already-latched byte before main can
   // consume its deliberately full ring; all other RX cases use the UART.
   if(vector!=89||item==11){if(item==11){board->write(0xffff8d,0xfe);board->write(0xffff8c,uint8_t(board->read(0xffff8c)|0x40));}board->cpu().interrupt(vector);}
  }
  if(vector==89&&item!=11){const uint8_t byte=item==10?0xfe:0xf8;check(a->enqueueMidi(&byte,1)&&b->enqueueMidi(&byte,1),"Test UART byte rejected");}
  if(item>=6){
   unsigned phases=0;bool sawPacking=false;
   while((foreground?b->nativeController()->serialForeground.kinds[4]:b->nativeController()->nativeInterruptKinds[kind])==kindCalls||b->nativeController()->activeNativeWork()||(b->cpu().exr&7)||(stateEvents&&(b->read(0x15408)||b->read(0x15409)!=2))||(statePanel&&(b->read(0x156ea)||b->read(0x156eb)!=2))||(stateMenu&&b->nativeController()->stateMenuForeground.kinds[menuKind]==menuCalls&&!(item==24&&sawFieldAddress))||(stateStore&&b->nativeController()->stateSaveForeground.kinds[storeKind]==storeCalls)||(stateDisplay&&b->nativeController()->stateDisplayForeground.kinds[0]==displayCalls&&b->nativeController()->stateScreenForeground.kinds[unsigned(ks::StateScreenService::Kind::Value)]==screenValueCalls&&!sawNestedFormat)||(stateApply&&b->nativeController()->stateApplyForeground.kinds[0]==applyCalls&&!sawFieldApply)){
    check(++phases<=(stateStore?4000000u:65536u),"Forced ADC/RX/Store lifecycle did not finish");advanceBoth(*a,*b);
    if(stateApply&&b->nativeController()->stateApplyForeground.at(ks::StateApplyService::Component::Packing))sawPacking=true;
    const auto nested=ks::currentNativeChildrenForTests(*b->nativeController());
    if(nested.address)sawFieldAddress=true;if(nested.apply&&nested.apply->kind()==ks::StateApplyService::Kind::Apply){sawFieldApply=true;if(nested.apply->component()==ks::StateApplyService::Component::Packing)sawPacking=true;}
    if(stateDisplay&&nested.display&&nested.display->kind()==ks::StateDisplayService::Kind::Format)sawNestedFormat=true;
    if(stateStore&&b->nativeController()->stateSaveForeground.at(ks::StateSaveService::Component::Reload))sawReload=true;
    if(stateStore&&b->nativeController()->stateSaveForeground.atPage(ks::StateSavePageService::Component::Verify))sawVerify=true;
   }
   if(item==29)check(sawPacking,"Natural edit did not reach its current packing child");
   if(vector==89&&item!=11)check(a->midiReceived==b->midiReceived&&b->midiReceived>received,"RX case did not consume the actual UART byte");
   if(item==11)check(b->read(0xffed68)==2&&b->read(0xffec65)==255,"Full RX ring did not retain producer/report overflow");
   if(item==12||item==13)check(!b->read(0xffee9c)&&!b->read(0xffee9d)&&!b->read(0xffeeaa)&&!b->read(0xffeeab)&&!b->read(0xffeeac)&&!b->read(0xffeead),"F8 reset did not clear samples/sum");
   if(item>=12&&item<=14)check(b->read(0xffeea0)==4&&!b->read(0xffeea1)&&!b->read(0xffeea2)&&!b->read(0xffeea3),"F8 reset/short feedback unexpectedly changed its estimate");
   if(item==15)check(b->read(0xffeea0)!=4||b->read(0xffeea1)||b->read(0xffeea2)||b->read(0xffeea3),"F8 feedback did not update its estimate");
  }
  if(stateApply)check(b->nativeController()->stateApplyForeground.kinds[0]>applyCalls||(b->nativeController()->stateFieldForeground.started>fieldCalls&&sawFieldApply),"Natural control event missed actual standalone/nested edit application");
  if(stateDisplay)check((b->nativeController()->stateDisplayForeground.kinds[0]>displayCalls&&(b->nativeController()->stateDisplayForeground.components&0xf0)==0xf0)||((b->nativeController()->stateScreenForeground.kinds[unsigned(ks::StateScreenService::Kind::Value)]>screenValueCalls||b->nativeController()->stateFieldForeground.started>fieldCalls)&&sawNestedFormat),"Natural menu display missed actual standalone/nested LCD transport");
  if(stateStore&&(!sawVerify||b->flashRevision<=flashRevision||b->read(0x16790)!=0))std::cerr<<"Store outcome verify="<<sawVerify<<" reload="<<sawReload<<" flash="<<flashRevision<<'/'<<b->flashRevision<<" page="<<unsigned(b->read(0x16790))<<" program="<<std::hex<<unsigned(b->read(0x167ac))<<':'<<unsigned(b->read(0x167ad))<<" components="<<b->nativeController()->stateSaveForeground.components<<std::dec<<'\n';
  if(stateStore){check(b->nativeController()->stateSaveForeground.kinds[storeKind]>storeCalls,"Natural Store event missed its complete save graph");check(sawVerify&&b->flashRevision>flashRevision&&b->read(0x16790)==0,"Natural Store did not finish Flash and return to playback");}
  if(item==30)check(sawReload,"Single save in multi mode missed full native reload");
  if(stateMenu)check(b->nativeController()->stateMenuForeground.kinds[menuKind]>menuCalls||(item==24&&b->nativeController()->stateFieldForeground.started>fieldCalls&&sawFieldAddress),"Natural menu/edit event missed actual standalone/nested address root");
  if(statePanel)check(b->nativeController()->statePanelForeground.started>panelCalls&&(b->nativeController()->statePanelForeground.components&0x180)==0x180,"Natural upper State event missed MIDI queue/UART publication");
  if(stateEvents)check((b->nativeController()->stateEventsForeground.components&(item==18?0x10u:0x20u))!=0,"Natural State event did not run its nested scale/square");
  if(panelScan)check(b->nativeController()->panelScanForeground.kinds[1]>0&&b->nativeController()->panelScanForeground.components&2,"Natural State scan did not execute the matrix graph");
  const uint8_t on[]{0x90,uint8_t(foreground?84:48+item*5),100};check(a->enqueueMidi(on,3)&&b->enqueueMidi(on,3),"Note rejected");
  for(unsigned n=0;n<4096;++n){
   if(n==2048){const uint8_t off[]{0x80,uint8_t(foreground?84:48+item*5),0};check(a->enqueueMidi(off,3)&&b->enqueueMidi(off,3),"Release rejected");}
   auto x=a->renderSample(),y=b->renderSample();check(x==y,"Interrupt timeline audio differs");check(a->controllerCycles()==b->controllerCycles(),"Interrupt timeline clock differs");sameInterrupts(*a,*b);
   pcmA.write(reinterpret_cast<const char*>(x.data()),sizeof(x));pcmB.write(reinterpret_cast<const char*>(y.data()),sizeof(y));
   for(auto v:x)peak=std::max(peak,uint64_t(std::abs(int64_t(v))));++frames;
  }
  if(foreground)check(b->nativeController()->serialForeground.kinds[4]>=kindCalls+3,"Real pedal transition did not publish all three UART bytes");
  else check(b->nativeController()->nativeInterruptStarts>calls&&b->nativeController()->nativeInterruptKinds[kind]>kindCalls,"Scenario did not execute its native IRQ kind");
  if(item==11)check(b->nativeController()->serialForeground.kinds[0]>0,"RX overflow did not run the native foreground reset");
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
 // Raw PCs describe the removed controller implementation, rather than a bus
 // resource. Retain both original tapes; only the authenticated Save/ManualEdit caller
 // may replace these independently verified DSP transport locations.
 constexpr std::array<unsigned,13> saveDspSites{{0x2417,0x2425,0x243d,0x2447,0x244f,0x2457,0x245f,0x246d,0x2485,0x24d9,0x24ff,0x2507,0x250f}};
 uint64_t nativeCpuLocationChanges=0;
 for(size_t n=0;n<x->events.size();++n){const auto&p=x->events[n];const auto&q=y->events[n];check(p.cycle==q.cycle&&p.dsp==q.dsp&&p.offset==q.offset&&p.value==q.value&&p.flags==q.flags,"Interrupt timeline DSP transfer differs");
  if(p.pc!=q.pc){check((q.pc==0x47d6||q.pc==0x4eb0)&&std::find(saveDspSites.begin(),saveDspSites.end(),p.pc)!=saveDspSites.end(),"Unexpected DSP trace CPU location change");++nativeCpuLocationChanges;}}

 for(size_t n=0;n<x->slices.size();++n){const auto&p=x->slices[n];const auto&q=y->slices[n];check(p.begin==q.begin&&p.end==q.end&&p.first==q.first&&p.count==q.count,"Interrupt timeline DSP slice differs");}
 const auto*r=b->nativeController();check(r->nativeInterruptKinds[0]&&r->nativeInterruptKinds[1]&&r->nativeInterruptKinds[2]&&r->nativeInterruptKinds[3]&&r->nativeInterruptKinds[4]&&r->nativeInterruptKinds[5],"Missing native IRQ kind coverage");
 check(r->nativeInterruptStarts+r->nativeInterruptInitial==r->nativeInterruptReturns+r->activeNativeInterrupts(),"Native IRQ accounting differs");
 check(r->serialForeground.kinds[0]&&r->serialForeground.kinds[1]&&r->serialForeground.kinds[4],"Missing native foreground reset/pop/transmit coverage");
 check(r->serialForeground.started+r->serialForeground.initial==r->serialForeground.completed+r->serialForeground.active(),"Native foreground accounting differs");
 check(r->stateResetForeground.started+r->stateResetForeground.initialActive==r->stateResetForeground.completed+r->stateResetForeground.active(),"Native State reset accounting differs");
 check(peak>1000&&b->h8StepsSinceNativeHandoff()==0,"Silent fixture or native interpreter fallback");
 std::cout<<"PASS: thirty-one IRQ/foreground/panel scan/State event/menu/store/display/apply/full Flash-reload timelines including TPU0/ADC/RX/F8 and a real pedal UART publication, "<<frames<<" exact audible 24-stream frames, "<<x->events.size()<<" exact timed DSP transfers; raw CPU location changes="<<nativeCpuLocationChanges<<" (both tapes retained); native error/TX/unused="<<r->nativeInterruptKinds[0]<<'/'<<r->nativeInterruptKinds[1]<<'/'<<r->nativeInterruptKinds[2]<<"; TPU0="<<r->nativeInterruptKinds[3]<<"; ADC="<<r->nativeInterruptKinds[4]<<", RX="<<r->nativeInterruptKinds[5]<<", components=0x"<<std::hex<<r->nativeTpuComponents<<'/'<<r->nativeAdcComponents<<'/'<<r->nativeSerialReceiveComponents<<"; private checkpoints="<<x->checkpoint<<'/'<<y->checkpoint<<std::dec<<"; foreground reset/pop/transmit="<<r->serialForeground.kinds[0]<<'/'<<r->serialForeground.kinds[1]<<'/'<<r->serialForeground.kinds[4]<<"; natural State resets="<<r->stateResetForeground.kinds[0]<<", components=0x"<<std::hex<<r->stateResetForeground.components<<std::dec<<"; cycles/pending/priorities/hardware/resource RAM/state/Flash/MIDI exact, no post-handoff H8\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"scenario="<<item<<": "<<e.what()<<'\n';return 1;}}
