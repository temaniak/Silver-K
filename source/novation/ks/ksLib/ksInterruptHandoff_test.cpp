#include "ksCurrentNativeChildren_test.h"
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
 uint64_t frames=0,peak=0,preparationFrames=0;
 for(phase=0;phase<46;++phase)for(mode=0;mode<3;++mode){
  if(phase>=10&&phase<13){
   a=std::make_unique<ks::Board>(rom.data(),wave,flash,true);b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);
   initialMainBoundary(*a);initialMainBoundary(*b);dirtyResetRings(*a);dirtyResetRings(*b);
  }
  if(phase>=30){a=std::make_unique<ks::Board>(rom.data(),wave,flash,true);b=std::make_unique<ks::Board>(rom.data(),wave,flash,true);check(a->boot()&&b->boot(),"Store handoff boot failed");same(*a,*b);}
  b->enableNativeController();check(b->nativeController(),"Native handoff failed");
  drain(*a,*b);
  if((phase>=31&&phase<34)||phase>=44){enqueueMulti(*a);enqueueMulti(*b);for(unsigned n=0;n<24000;++n){check(a->renderSample()==b->renderSample(),"Store mode preparation audio differs");check(a->controllerCycles()==b->controllerCycles(),"Store mode preparation clocks differ");sameInterrupts(*a,*b);++preparationFrames;}drain(*a,*b);same(*a,*b);}
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
   if(phase>=14&&phase<17){
    pedalOutput(*board);board->write(0x156e2,36);
    // Retained timer histories force a moving encoder on the next real tick.
    board->write(0xffee72,uint8_t((unsigned(board->read(0xfffff7))>>2)-1));
   }
   if(phase==17){word(*board,0x1537c,2);word(*board,0x1537e,0);word(*board,0x15380,0x0121);}
   if(phase==18){word(*board,0x156e8,2);word(*board,0x156ea,0);word(*board,0x156ec,0x643c);}
   if(phase>=19&&phase<22){word(*board,0x15406,2);word(*board,0x15408,0);word(*board,0x1540a,phase==20?0x4022:0x7f20);}
   if(phase>=22&&phase<25){
    word(*board,0x156e8,2);word(*board,0x156ea,0);word(*board,0x156ec,0x643c);
    board->write(0x169ed,255);board->write(0x16798,0);
    for(unsigned part=0;part<5;++part)board->write(0x160d2+24*part+327,0);
   }
   if(phase>=26&&phase<29)menuInput(*board,0);
   if(phase==29)menuInput(*board,2);
   if(phase>=30&&phase<34)storeInput(*board,phase>=31);
   if(phase>=34&&phase<37)menuInput(*board,2);
   if(phase>=41)storeInput(*board,false,phase>=44);
   if(phase>=37&&phase<41)applyInput(*board,phase==38);
   if(phase>=7)continue;
   const unsigned vector=!phase?90:phase<3?32:phase<6?28:89;
   board->cpu().exr=0;board->cpu().interruptPriority[vector]=phase>0&&phase<3?6:1;
   if(phase!=6)board->cpu().interrupt(vector);
  }
  if(phase==6){const uint8_t clock=0xf8;check(a->enqueueMidi(&clock,1)&&b->enqueueMidi(&clock,1),"Handoff F8 rejected");
  }
  bool landed=false;
  for(unsigned n=0;n<65536&&!landed;++n){
   advanceBoth(*a,*b);const auto*r=b->nativeController();const auto nested=ks::currentNativeChildrenForTests(*r);const auto*screenDisplay=nested.display;const auto*screenMenu=nested.menu;const auto*fieldApply=nested.apply;const auto currentSave=nested.save;
   const auto expectedSaveKind=phase>=31&&phase<34?ks::StateSaveService::Kind::CommitMulti:ks::StateSaveService::Kind::CommitSingle;
   const bool expectedSave=currentSave.at(expectedSaveKind);bool target=false;
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
   case 41:target=expectedSave&&currentSave.atPage(ks::StateSavePageService::Component::Compare)&&currentSave.at(ks::FlashPageCompareTask::Stage::ReadTarget)&&b->cpu().exr==0;break;
   case 42:target=expectedSave&&currentSave.at(ks::FlashPageProgramTask::Stage::ReadSource)&&b->cpu().exr==6;break;
   case 43:target=expectedSave&&currentSave.atPage(ks::StateSavePageService::Component::Verify)&&currentSave.at(ks::FlashPageCompareTask::Stage::ReadTarget)&&b->cpu().exr==0;break;
   case 44:target=expectedSave&&currentSave.reload()&&currentSave.atReload(ks::StateReloadService::Component::Coordinator)&&currentSave.reload()->coordinator().multi().stage()==ks::StateMultiLoadTask::Stage::ResolveSource&&b->cpu().exr==0;break;
   case 45:target=expectedSave&&currentSave.atReload(ks::StateReloadService::Component::Panel)&&b->cpu().exr==0;break;
   case 37:target=((r->stateApplyForeground.at(ks::StateApplyService::Kind::Apply)&&r->stateApplyForeground.at(ks::StateApplyService::Component::Apply))||(fieldApply&&fieldApply->kind()==ks::StateApplyService::Kind::Apply&&fieldApply->manualTask()&&fieldApply->manualTask()->stage()==ks::StateManualEditTask::Stage::RetainRequest))&&b->cpu().exr==0;break;
   case 38:target=((r->stateApplyForeground.at(ks::StateApplyService::Kind::Apply)&&r->stateApplyForeground.at(ks::StateApplyService::Component::Packing))||(fieldApply&&fieldApply->kind()==ks::StateApplyService::Kind::Apply&&fieldApply->component()==ks::StateApplyService::Component::Packing))&&b->cpu().exr==0;break;
   case 39:target=((r->stateApplyForeground.at(ks::StateApplyService::Kind::Apply)&&r->stateApplyForeground.at(ks::StateApplyService::Component::Midi))||(fieldApply&&fieldApply->kind()==ks::StateApplyService::Kind::Apply&&fieldApply->component()==ks::StateApplyService::Component::Midi))&&b->cpu().exr==0;break;
   case 40:target=((r->stateApplyForeground.at(ks::StateApplyService::Kind::Apply)&&r->stateApplyForeground.at(ks::StateApplyService::Component::Post))||(fieldApply&&fieldApply->kind()==ks::StateApplyService::Kind::Apply&&fieldApply->component()==ks::StateApplyService::Component::Post))&&b->cpu().exr==0;break;
   case 34:target=((r->stateDisplayForeground.at(ks::StateDisplayService::Kind::Format)&&r->stateDisplayForeground.atFormat(ks::StateMenuFormatTask::Stage::SetPosition))||(screenDisplay&&screenDisplay->kind()==ks::StateDisplayService::Kind::Format&&screenDisplay->atFormat(ks::StateMenuFormatTask::Stage::SetPosition)))&&b->cpu().exr==0;break;
   case 35:target=((r->stateDisplayForeground.at(ks::StateDisplayService::Kind::Format)&&r->stateDisplayForeground.atPoll(ks::PanelPollTask::Stage::TestReady))||(screenDisplay&&screenDisplay->kind()==ks::StateDisplayService::Kind::Format&&screenDisplay->atPoll(ks::PanelPollTask::Stage::TestReady)))&&b->cpu().exr==0;break;
   case 36:target=((r->stateDisplayForeground.at(ks::StateDisplayService::Kind::Format)&&r->stateDisplayForeground.atByte(ks::PanelByteTask::Kind::CommandWrite,ks::PanelByteTask::Stage::Transfer))||(screenDisplay&&screenDisplay->kind()==ks::StateDisplayService::Kind::Format&&screenDisplay->atByte(ks::PanelByteTask::Kind::CommandWrite,ks::PanelByteTask::Stage::Transfer)))&&b->cpu().exr==0;break;
   case 30:target=((r->stateStoreForeground.at(ks::StateStoreService::Kind::WritePatchName))||(expectedSave&&currentSave.atPreparation(ks::StateStoreService::Kind::WritePatchName)&&currentSave.preparation()->nameTask()&&currentSave.preparation()->nameTask()->stage()==ks::StatePatchNameTask::Stage::AddWritePatch))&&b->cpu().exr==0;break;
   case 31:target=((r->stateStoreForeground.at(ks::StateStoreService::Kind::MultiSerialize)&&r->stateStoreForeground.at(ks::StateStoreService::Component::PartReference))||(expectedSave&&currentSave.atPreparation(ks::StateStoreService::Kind::MultiSerialize)&&currentSave.atPreparation(ks::StateStoreService::Component::PartReference)))&&b->cpu().exr==0;break;
   case 32:target=((r->stateStoreForeground.at(ks::StateStoreService::Kind::MultiSerialize)&&r->stateStoreForeground.at(ks::StateStoreService::Component::PackAll))||(expectedSave&&currentSave.atPreparation(ks::StateStoreService::Kind::MultiSerialize)&&currentSave.atPreparation(ks::StateStoreService::Component::PackAll)))&&b->cpu().exr==0;break;
   case 33:target=((r->stateStoreForeground.at(ks::StateStoreService::Kind::MultiSerialize)&&r->stateStoreForeground.at(ks::StateStoreService::Component::Oscillators))||(expectedSave&&currentSave.atPreparation(ks::StateStoreService::Kind::MultiSerialize)&&currentSave.atPreparation(ks::StateStoreService::Component::Oscillators)))&&b->cpu().exr==0;break;
   case 25:target=r->stateMenuForeground.at(ks::StateMenuService::Kind::Clock)&&b->cpu().exr==0;break;
   case 26:target=r->stateMenuForeground.at(ks::StateMenuService::Kind::Previous)&&r->stateMenuForeground.at(ks::StateMenuService::Component::EditGate)&&b->cpu().exr==0;break;
   case 27:target=r->stateMenuForeground.at(ks::StateMenuService::Kind::Previous)&&r->stateMenuForeground.at(ks::StateMenuService::Component::RepeatTimer)&&b->cpu().exr==0;break;
   case 28:target=(r->stateMenuForeground.at(ks::StateMenuService::Kind::BindContext)||(screenMenu&&screenMenu->kind()==ks::StateMenuService::Kind::BindContext&&screenMenu->contextTask()&&screenMenu->contextTask()->stage()==ks::StateEditContextTask::Stage::SaveContextIndex))&&b->cpu().exr==0;break;
   case 29:target=(r->stateMenuForeground.at(ks::StateMenuService::Kind::IndexedAddress)||(nested.address&&nested.address->stage()==ks::StateEditAddressTask::Stage::ReadSelector))&&b->cpu().exr==0;break;
   case 22:target=r->statePanelForeground.atRoute(ks::MidiOutputTask::Stage::SaveOnContext)&&b->cpu().exr==0;break;
   case 23:target=r->statePanelForeground.atQueue(ks::InternalMidiQueueTask::Stage::SavePush)&&b->cpu().exr==0;break;
   case 24:target=r->statePanelForeground.atSerial(ks::SerialTransmitTask::Stage::RetainFirst)&&b->cpu().exr==0;break;
   case 17:target=r->stateEventsForeground.at(ks::StateEventsService::Kind::ButtonPop)&&b->cpu().exr==0;break;
   case 18:target=(r->stateEventsForeground.at(ks::StateEventsService::Kind::KeyboardPop)||r->statePanelForeground.atKeyboard(ks::KeyboardEventsTask::Stage::ReadPopConsumer))&&b->cpu().exr==0;break;
   case 19:target=r->stateEventsForeground.at(ks::StateEventsService::Kind::ControlPop)&&r->stateEventsForeground.at(ks::StateEventsService::Component::Convert)&&b->cpu().exr==0;break;
   case 20:target=r->stateEventsForeground.at(ks::StateEventsService::Kind::ControlPop)&&r->stateEventsForeground.at(ks::StateEventsService::Component::Scale)&&b->cpu().exr==0;break;
   case 21:target=r->stateEventsForeground.at(ks::StateEventsService::Kind::ControlPop)&&r->stateEventsForeground.at(ks::StateEventsService::Component::Square)&&b->cpu().exr==0;break;
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
  if(phase>=14&&phase<17){const unsigned expected=phase==14?0x188a:phase==15?0x13c2:0x1646;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same panel controls child");}
  if(phase>=17&&phase<22){const unsigned expected=phase==17?0x1416:phase==18?0x1f16:phase==19?0x20fc:phase==20?0x2144:0x218c;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same State event child");}
  if(phase>=22&&phase<25){const unsigned expected=phase==22?0x2b38:phase==23?0x2c88:0x1104;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same upper State MIDI child");}
  if(phase>=25&&phase<30){const unsigned expected=phase==25?0x1300:phase==26?0x4efe:phase==27?0x4134:phase==28?0x5a4a:0x52ae;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same menu/context child");}
  if(phase>=30&&phase<34){const unsigned expected=phase==30?0x6e82:phase==31?0xa670:phase==32?0x687e:0x68b0;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same natural store preparation child");}
  if(phase>=34&&phase<37){const unsigned expected=phase==34?0x5ae4:phase==35?0x8ec:0x86c;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same menu/LCD child");}
  if(phase>=37&&phase<41){const unsigned expected=phase==37?0x6aea:phase==38?0x688e:phase==39?0x2be8:0x6bf0;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same live edit/packing/MIDI/post child");}
  if(phase>=41){const unsigned expected=phase==41||phase==43?0x128a:phase==42?0x122e:phase==44?0x6ff4:0x6c3c;check(unsigned(a->cpu().getPC())==expected,"H8 did not reach the same Flash/verify/reload child");}
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
 std::cout<<"PASS: one hundred thirty-eight H8/hybrid/cancel requests inside IRQ/serial/State reset, panel scan/controls State event conversion and upper State MIDI/UART, menu/context, store preparation, LCD, live edit/packing/MIDI/post, Flash compare/write/verify and full multi reload; no added guest advance; "<<frames<<" exact audible 24-stream frames plus "<<preparationFrames<<" exact host-mode preparation frames, persistent RAM/hardware/state/Flash/cycles/pending/priorities/serial/MIDI exact\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<"phase="<<phase<<", mode="<<mode<<": "<<e.what()<<'\n';return 1;}}
