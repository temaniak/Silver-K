#include "kstationNativeController.h"
#include "kstationControllerBoard.h"
#include "kstationControllerStartupGraph.h"
#include "kstationControllerStartupServicesHolder.h"
#include "kstationControllerDspStartupRuntime.h"
#include "kstationControllerDspGlobalsRuntime.h"
#include "kstationControllerFirmwareUpdateRuntime.h"
#include "kstationControllerMidiServices.h"
#include "kstationControllerUiServices.h"
#include "kstationControllerMainEntryRuntime.h"
#include "kstationControllerKeyServices.h"
#include "kstationControllerArpServices.h"
#include "kstationControllerPeriodicDisplayServices.h"
#include "kstationControllerMidiRealtimeRuntime.h"
#include "kstationControllerScriptPlaybackServices.h"
#include "kstationControllerDeferredUiRuntime.h"
#include "kstationControllerDeferredMenuRuntime.h"
#include "kstationControllerDeferredNoticeRuntime.h"
#include "kstationControllerDeferredParameterRuntime.h"
#include <limits>
#include <string>
#if defined(K_RESEARCH_NATIVE_TRACE)
extern void kNativeControllerPort(uint64_t,uint32_t,uint8_t,bool);
extern void kNativeControllerSlice(uint64_t,uint64_t,uint64_t,uint64_t);
#endif
namespace kstation {
struct NativeBoardAccess {
 static void enable(ControllerBoard&board,ControllerResourceBus&bus){board.expectedBus_=&bus;}
};
struct NativeForegroundAccess {
 static ControllerForegroundRuntime::Result advance(ControllerForegroundRuntime&fg,ControllerDeviceState&d,ControllerInterruptRuntime&irq,unsigned budget){
#if defined(K_RESEARCH_NATIVE_TRACE)
  if(fg.bound_!=&d||!irq.boundTo(d)||!fg.active_||d.interruptMode2||d.pendingIrqsHigh||irq.active()||(d.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))throw std::logic_error("foreground selected entry invariant");
#endif
  if(fg.feedback_){
#if defined(K_RESEARCH_NATIVE_TRACE)
   if(fg.refresh_||fg.expressionValues_)throw std::logic_error("foreground feedback owner priority");
#endif
   return fg.advanceFeedbackBefore(d,irq,budget);
  }
  return fg.advanceSelectedBefore(d,irq,budget);
 }
};
struct NativeController::Impl {
 using F=ControllerForegroundRuntime;using G=ControllerStartupGraph;using SH=ControllerStartupServicesHolder;using M=MainLoopTask;
 std::vector<uint8_t>image,storage;ControllerHostCallbacks host;
 ControllerDeviceState state;ControllerBoard board;
 struct Bus final:ControllerResourceBus {
  ControllerBoard&board;ControllerDeviceState&device;
  Bus(ControllerBoard&b,ControllerDeviceState&d):board(b),device(d){}
  bool mapped(uint32_t a)const override{return board.mapped(a);}
  uint8_t read(uint32_t a)override{auto v=board.read(a);trace(a,v,false);return v;}
  void write(uint32_t a,uint8_t v)override{board.write(a,v);trace(a,v,true);}
  void trace(uint32_t a,uint8_t v,bool write){
#if defined(K_RESEARCH_NATIVE_TRACE)
   if(a>=0xffff1c||(a>=0x600000&&a<0x600010))kNativeControllerPort(device.cycles,a,v,write);
#endif
  }
 }bus;
 ControllerInterruptRuntime irq;ControllerDspTransferRuntime dsp;G startup;SH startupServices;
 ControllerNumericDisplayRuntime numeric;ControllerDspStartupRuntime dspStartup;ControllerDspGlobalsRuntime dspGlobals;ControllerFirmwareUpdateRuntime firmwareUpdate;
 F foreground;ControllerVoiceServices voices;ControllerMidiServices midi;ControllerUiServices ui,directUi;
 enum class DirectParent:uint8_t{None,Midi,Startup,Script,Ui,Callback,DeferredUi};DirectParent directParent=DirectParent::None;ControllerMidiServices::Request directMidiRequest{};ControllerStartupDisplayRuntime::Request directStartupRequest{};ControllerScriptPlaybackServices::Request directScriptRequest{};ControllerUiServices::Request directUiRequest{};
 ControllerMainEntryRuntime entry;ControllerKeyServices keys;ControllerArpServices arp;ControllerPeriodicDisplayServices periodic;ControllerMidiRealtimeRuntime realtime;ControllerDeferredUiRuntime deferredUi;ControllerDeferredMenuRuntime deferredMenu;ControllerDeferredNoticeRuntime deferredNotice;ControllerDeferredParameterRuntime deferredParameter;ControllerScriptPlaybackServices script;ControllerPeriodicUiRuntime scriptPeriodic;ControllerUiServices::Request scriptUiParent{};
 const char*fault=nullptr;ControllerDebugState stats;bool bound=false,entryCall=false,entryDone=false,ready=false,enteredMain=false;StartupChild rootStartup=StartupChild::None;
 // Only the foreground dispatcher starts root-owned services. IRQ/board
 // phases update device state, not these owners. A pending request drops
 // this eligibility before admission; no request or guest data is cached.
 // UI/MIDI selection is cleared before routing nested services or returning.
 enum class ForegroundOwner:uint8_t{Unknown,Core,Ui,Midi};
 ForegroundOwner foregroundOwner_=ForegroundOwner::Unknown;
 M::Child keyParent=M::Child::LocalKeys;ControllerCallback callback=ControllerCallback::Unknown;F::MidiHandler realtimeParent=F::MidiHandler::Unknown;
 Impl(const std::vector<uint8_t>&rom,ControllerHostCallbacks callbacks,const std::vector<uint8_t>&flash)
 :image(rom),storage(flash),host(std::move(callbacks)),board(state,image,ControllerHostCallbacks{host.read,host.write,host.resetDSP},storage),bus(board,state){
  state.attachBus(&bus);NativeBoardAccess::enable(board,bus);irq.setMask(ControllerInterruptRuntime::Mask::Disabled);
  require(startup.bindBootResources(state,image.data(),image.size()),"startup resources");
  require(startupServices.bindBootResources(state,image.data(),image.size()),"startup service resources");
  require(numeric.bindBootResources(state,image.data(),image.size()),"numeric boot resources");require(firmwareUpdate.bindBootResources(state,image.data(),image.size()),"firmware progress boot resources");
  require(midi.bindBootResources(state,image.data(),image.size()),"MIDI boot resources");
  require(startup.startCold(),"cold startup");
 }
 void require(bool ok,const char*what)const{if(!ok)throw std::logic_error(std::string("Native K: ")+what+" [resource bindings="+std::to_string(state.immutableBindingCount())+"]");}
 bool check(bool ok,const char*what){if(!ok&&!fault)fault=what;return ok;}
 unsigned fail(const char*what){if(!fault)fault=what;return 0;}
 bool bindOverlay(){
  if(!check(board.ramOverlay()&&!bound,"overlay handoff"))return 0;auto*p=image.data();auto n=image.size();
  if(!check(irq.bindVerifiedFirmware(state,p,n),"IRQ binding"))return 0;if(!check(dsp.bindVerifiedFirmware(state,p,n),"DSP binding"))return 0;
  if(!check(dspStartup.bindVerifiedFirmware(state,p,n),"DSP startup binding"))return 0;if(!check(dspGlobals.bindVerifiedFirmware(state,p,n),"DSP globals binding"))return 0;if(!check(firmwareUpdate.bindMirroredFirmware(state,p,n),"firmware publisher binding"))return 0;
  if(!check(numeric.bindMirroredFirmware(state,p,n),"numeric mirrored binding"))return 0;if(!check(midi.bindAll(state,p,n),"MIDI binding"))return 0;
  if(!check(voices.bindAll(state,p,n),"voice binding"))return 0;if(!check(foreground.bindVerifiedFirmware(state,p,n),"foreground binding"))return 0;
  if(!check(ui.bindAll(state,p,n),"UI binding"))return 0;if(!check(directUi.bindAll(state,p,n),"direct UI binding"))return 0;if(!check(entry.bindVerifiedFirmware(state,p,n),"Main entry binding"))return 0;
  if(!check(keys.bindAll(state,p,n),"key binding"))return 0;if(!check(arp.bindAll(state,p,n),"arpeggiator binding"))return 0;if(!check(periodic.bindAll(state,p,n),"periodic binding"))return 0;if(!check(deferredUi.bindVerifiedFirmware(state,p,n),"deferred UI binding"))return 0;if(!check(deferredMenu.bindVerifiedFirmware(state,p,n),"deferred menu binding"))return 0;if(!check(deferredNotice.bindVerifiedFirmware(state,p,n),"deferred notice binding"))return 0;if(!check(deferredParameter.bindVerifiedFirmware(state,p,n),"deferred parameter binding"))return 0;if(!check(realtime.bindVerifiedFirmware(state,p,n),"system MIDI binding"))return 0;if(!check(script.bindAll(state,p,n),"script binding"))return 0;if(!check(scriptPeriodic.bindVerifiedFirmware(state,p,n),"script periodic binding"))return 0;
  if(!check(startupServices.bindAllMirrored(state,p,n),"startup mirrored service binding"))return 0;
  if(!check(state.protectImmutableVerified(0x3ae4,0x3ae8,p+0x3ae4)&&state.protectImmutableVerified(0x403ae4,0x403ae8,p+0x3ae4),"entry metadata"))return 0;
  if(!check(startup.completeService(G::Service::BindOverlay,state,irq),"overlay completion"))return false;bound=true;return true;
 }
 bool startDisplay(ControllerStartupDisplayRuntime::Request request,DirectParent parent){using S=ControllerStartupDisplayRuntime::Service;using K=ControllerUiServices::DirectKind;K kind;if(request.service==S::EditMenu)kind=K::WriteDisplay;else if(request.service==S::CurrentControl)kind=K::Relative;else if(request.service==S::OtherMode)kind=K::MenuDisplay;else return false;if(!directUi.startDirect(kind,0,request.input))return false;directParent=parent;return true;}
 unsigned timedDirectUi(){if(!check(directUi.routeDirect(state,irq,dsp,voices,startup.panel(),startup.lcd(),numeric)==ControllerUiServices::Route::Ready,"direct UI child remains open"))return 0;auto r=directUi.advanceDirect(state,irq,std::numeric_limits<unsigned>::max());if(r.returned){auto parent=directParent;directParent=DirectParent::None;switch(parent){case DirectParent::Midi:if(!check(midi.completeExternal(directMidiRequest,voices),"MIDI display completion"))return 0;break;case DirectParent::Startup:if(!check(startupServices.completeDisplayService(startup,state,irq,directStartupRequest.service),"startup display completion"))return 0;break;case DirectParent::Script:if(!check(script.completeExternal(directScriptRequest,scriptPeriodic,midi,voices),"script display completion"))return 0;break;case DirectParent::Ui:if(!check(ui.completeExternal(foreground,directUiRequest),"UI display completion"))return 0;break;case DirectParent::Callback:if(!check(foreground.completeCallback(callback),"direct callback completion"))return 0;callback=ControllerCallback::Unknown;break;case DirectParent::DeferredUi:if(!check(deferredUi.completeExternal(directStartupRequest.service),"deferred display completion"))return 0;if(!deferredUi.active()){if(!check(foreground.completeCallback(callback),"deferred display tail completion"))return 0;callback=ControllerCallback::Unknown;}break;default:return fail("direct UI continuation");}}return r.clocks;}
 unsigned timedStartup(){
  constexpr auto budget=std::numeric_limits<unsigned>::max();
  if(directUi.active())return timedDirectUi();
  if(rootStartup!=StartupChild::None){unsigned used=0;bool done=false;if(rootStartup==StartupChild::DspBootstrap){auto r=dspStartup.advanceBefore(state,irq,dsp,budget);used=r.clocks;done=r.returned;}else{auto r=dspGlobals.advanceBefore(state,irq,dsp,budget);used=r.clocks;done=r.returned;}if(done){if(!check(startup.completeService(G::Service::Startup,state,irq,0,rootStartup),"DSP startup completion"))return 0;rootStartup=StartupChild::None;}return used;}
  if(firmwareUpdate.active()){auto r=firmwareUpdate.advanceBefore(state,irq,startup.lcd(),numeric,budget);if(r.returned)if(!check(startup.completeService(G::Service::FirmwareUpdate,state,irq),"firmware publication completion"))return 0;return r.clocks;}
  auto q=startup.request();if(q.service==G::Service::BindOverlay){bindOverlay();return 0;}
  if(q.service==G::Service::FirmwareUpdate){if(!check(firmwareUpdate.start(),"firmware publisher admission"))return 0;return 0;}
  auto route=startupServices.route(startup,state,irq,dsp,midi,voices,numeric);
  if(!check(route!=SH::Route::Invalid,"startup service route"))return 0;
  if(route==SH::Route::External&&startupServices.active()){auto request=startupServices.displayRequest();if(startDisplay(request,DirectParent::Startup)){directStartupRequest=request;return 0;}}
  if(route==SH::Route::External&&q.service==G::Service::Startup){if(q.startup==StartupChild::DspBootstrap){if(!check(dspStartup.start(),"DSP startup admission"))return 0;rootStartup=q.startup;return 0;}if(q.startup==StartupChild::DspGlobals){if(!check(dspGlobals.start(),"DSP globals admission"))return 0;rootStartup=q.startup;return 0;}return fail("startup service remains open");}
  if(startupServices.active())return startupServices.advanceBefore(startup,state,irq,dsp,midi,voices,numeric,budget).clocks;
  auto r=startup.advanceBefore(state,irq,budget);return r.clocks;
 }
 unsigned uiForegroundPhase(){
  constexpr auto budget=std::numeric_limits<unsigned>::max();foregroundOwner_=ForegroundOwner::Unknown;
  auto route=ui.routeForeground(foreground,state,irq,dsp,voices,startup.panel(),startup.lcd(),numeric);if(route==ControllerUiServices::Route::External){auto request=ui.externalRequest();if(request.owner==ControllerUiServices::Owner::KeyDisplay&&startDisplay(request.keyDisplay,DirectParent::Ui)){directUiRequest=request;return 0;}if(request.owner==ControllerUiServices::Owner::KeyRange&&request.keyRange.service==ControllerUiKeyRangeRuntime::Service::Script){if(!check(script.start(),"script admission"))return 0;scriptUiParent=request;return 0;}}if(!check(route==ControllerUiServices::Route::Ready,"UI request remains open"))return 0;auto r=ui.advanceBefore(foreground,state,irq,budget);if(r.clocks&&ui.active())foregroundOwner_=ForegroundOwner::Ui;return r.clocks;
 }
 unsigned midiForegroundPhase(const F::Request*q){
  constexpr auto budget=std::numeric_limits<unsigned>::max();foregroundOwner_=ForegroundOwner::Unknown;
  if(!midi.active()&&!check(q!=nullptr,"missing MIDI admission"))return 0;
  if(!midi.active()&&(q->handler==F::MidiHandler::Ignore||q->handler==F::MidiHandler::Realtime)){if(!check(realtime.start(q->handler==F::MidiHandler::Ignore),"system MIDI admission"))return 0;realtimeParent=q->handler;return 0;}auto route=midi.routeForeground(foreground,state,irq,dsp,voices,startup.lcd(),numeric,startup.panel());if(route==ControllerMidiServices::Route::External){auto request=midi.externalRequest(voices);if(request.owner==ControllerMidiServices::Owner::Display&&startDisplay(request.display,DirectParent::Midi)){directMidiRequest=request;return 0;}}if(!check(route==ControllerMidiServices::Route::Ready,"MIDI child remains open"))return 0;auto r=midi.advanceBefore(foreground,state,irq,dsp,voices,budget);if(r.clocks&&midi.active())foregroundOwner_=ForegroundOwner::Midi;return r.clocks;
 }
 unsigned foregroundPhase(){
  constexpr auto budget=std::numeric_limits<unsigned>::max();
  if(foregroundOwner_==ForegroundOwner::Ui||foregroundOwner_==ForegroundOwner::Midi){
#if defined(K_RESEARCH_NATIVE_TRACE)
   if(!check(entryDone&&!deferredNotice.active()&&!deferredParameter.active()&&!deferredMenu.active()&&!directUi.active()&&!deferredUi.active()&&!script.active()&&!keys.active()&&!arp.active()&&!periodic.active()&&!realtime.active(),"foreground selected owner priority"))return 0;
   const auto selectedRequest=foreground.request();
   if(foregroundOwner_==ForegroundOwner::Ui){if(!check(ui.active()&&selectedRequest.kind==F::RequestKind::MainChild&&selectedRequest.slot>=5&&selectedRequest.slot<=7,"foreground UI ownership"))return 0;}
   else if(!check(midi.active()&&!ui.active()&&selectedRequest.kind==F::RequestKind::MidiHandler,"foreground MIDI ownership"))return 0;
#endif
   return foregroundOwner_==ForegroundOwner::Ui?uiForegroundPhase():midiForegroundPhase(nullptr);
  }
  if(foregroundOwner_==ForegroundOwner::Core){
#if defined(K_RESEARCH_NATIVE_TRACE)
   if(!check(entryDone&&!deferredNotice.active()&&!deferredParameter.active()&&!deferredMenu.active()&&!directUi.active()&&!deferredUi.active()&&!script.active()&&!keys.active()&&!arp.active()&&!periodic.active()&&!realtime.active()&&!ui.active()&&!midi.active(),"foreground core ownership"))return 0;
   if(voices.foregroundGlobalActive()&&!check(foreground.request().kind==F::RequestKind::VoiceGlobal,"foreground global ownership"))return 0;
#endif
   if(voices.foregroundGlobalActive()){
    if(!check(voices.routeForeground(foreground,state,dsp,nullptr)==ControllerVoiceServices::Route::Ready,"voice child remains open"))return 0;
    return voices.advanceForeground(foreground,state,irq,dsp,budget).clocks;
   }
   if(!voices.globalActive()){
#if defined(K_RESEARCH_NATIVE_TRACE)
    if(!check(voices.boundTo(state)&&dsp.boundTo(state),"foreground fixed bindings"))return 0;
    const auto probeCycles=state.cycles,probePending=state.pendingIrqs,probeHigh=state.pendingIrqsHigh;const auto probeRead=state.lastRead,probeWrite=state.lastWrite;const auto probeMask=irq.mask();
#endif
    const auto r=state.interruptMode2||state.pendingIrqsHigh?foreground.advanceBefore(state,irq,budget):NativeForegroundAccess::advance(foreground,state,irq,budget);
    if(r.clocks)return r.clocks;
    // Await guards and the outer unsupported-mode guard precede guest work.
    // Other zero results may have changed state and must not be replayed.
    switch(r.blocked){
     case F::Block::UnsupportedMode:case F::Block::AwaitMainChild:case F::Block::AwaitCallback:case F::Block::AwaitPumpChild:case F::Block::AwaitNoteChild:case F::Block::AwaitExpressionChild:case F::Block::AwaitRefreshChild:case F::Block::AwaitVoiceGlobal:
#if defined(K_RESEARCH_NATIVE_TRACE)
      if(!check(state.cycles==probeCycles&&state.pendingIrqs==probePending&&state.pendingIrqsHigh==probeHigh&&state.lastRead==probeRead&&state.lastWrite==probeWrite&&irq.mask()==probeMask,"foreground guard changed state"))return 0;
#endif
      break;
     default:return 0;
    }
   }else if(!foreground.hasPendingRequest()){
    if(!check(voices.routeForeground(foreground,state,dsp,nullptr)==ControllerVoiceServices::Route::Ready,"voice child remains open"))return 0;
    return voices.advanceForeground(foreground,state,irq,dsp,budget).clocks;
   }
   foregroundOwner_=ForegroundOwner::Unknown;
  }
  return foregroundSlowPhase();
 }
 // Keep canonical admission and its request payload off the selected-owner path.
 unsigned foregroundSlowPhase(){
  constexpr auto budget=std::numeric_limits<unsigned>::max();
  if(!entryDone){if(!entryCall){if(state.pendingIrqs)(void)state.read8(0xfffff2);state.clockInternal(10);state.lastRead=state.lastWrite=-1;entryCall=true;if(!check(entry.start(),"Main entry admission"))return 0;ready=board.readMemory(0xcd82)>=1&&board.readMemory(0xcd82)<=4&&board.readMemory(0xcd83)<100;for(unsigned i=0;i<128&&ready;++i)ready=board.readMemory(0xba5c+i)<128;return 10+(state.pendingIrqs?1:0);}auto r=entry.advanceBefore(state,irq,budget);if(r.returned){entryDone=true;if(!check(foreground.start(),"foreground admission"))return 0;}return r.clocks;}
  if(deferredNotice.active()){auto r=deferredNotice.advanceBefore(state,irq,startup.lcd(),budget);if(r.returned){if(!check(foreground.completeCallback(callback),"deferred notice completion"))return 0;callback=ControllerCallback::Unknown;}return r.clocks;}
  if(deferredParameter.active()){if(!check(deferredParameter.route(state,irq,dsp,voices,directUi,startup.panel(),startup.lcd(),numeric)==ControllerDeferredParameterRuntime::Route::Ready,"deferred parameter child remains open"))return 0;auto r=deferredParameter.advanceBefore(state,irq,directUi,budget);if(r.returned){if(!check(foreground.completeCallback(callback),"deferred parameter completion"))return 0;callback=ControllerCallback::Unknown;}return r.clocks;}
  if(deferredMenu.active()){if(!check(deferredMenu.route(state,irq,dsp,voices,directUi,startup.panel(),startup.lcd(),numeric)==ControllerDeferredMenuRuntime::Route::Ready,"deferred menu child remains open"))return 0;auto r=deferredMenu.advanceBefore(state,irq,directUi,budget);if(r.returned){if(!check(foreground.completeCallback(callback),"deferred menu completion"))return 0;callback=ControllerCallback::Unknown;}return r.clocks;}
  if(directUi.active())return timedDirectUi();
  if(deferredUi.active()){auto request=deferredUi.request();if(request.service!=ControllerStartupDisplayRuntime::Service::None){if(!check(startDisplay(request,DirectParent::DeferredUi),"deferred display admission"))return 0;directStartupRequest=request;return 0;}auto r=deferredUi.advanceBefore(state,irq,startup.panel(),numeric,startup.lcd(),budget);if(r.returned){if(!check(foreground.completeCallback(callback),"deferred UI completion"))return 0;callback=ControllerCallback::Unknown;}return r.clocks;}
  if(script.active()){auto route=script.route(state,irq,dsp,arp,voices,scriptPeriodic,midi,startup.lcd(),numeric,startup.panel());if(route==ControllerScriptPlaybackServices::Route::External){auto request=script.request(scriptPeriodic,midi,voices);if(startDisplay(request.display,DirectParent::Script)||(request.midi.owner==ControllerMidiServices::Owner::Display&&startDisplay(request.midi.display,DirectParent::Script))){directScriptRequest=request;return 0;}if(request.periodic.service==ControllerPeriodicUiRuntime::Service::Encoder){using E=ControllerUiEncoderRouteRuntime::Service;using D=ControllerUiServices::DirectKind;auto q=request.periodic.encoder;bool supported=true;D kind=D::Relative;switch(q.service){case E::Edit:kind=D::ProgramEdit;break;case E::Display:kind=D::WriteDisplay;break;case E::ParameterControl:kind=D::Relative;break;case E::ParameterMenu:kind=D::MenuEdit;break;default:supported=false;break;}if(supported&&directUi.startDirect(kind,0,q.delta)){directParent=DirectParent::Script;directScriptRequest=request;return 0;}}}if(!check(route==ControllerScriptPlaybackServices::Route::Ready,"script child remains open"))return 0;auto r=script.advanceBefore(state,irq,dsp,arp,voices,scriptPeriodic,midi,budget);if(r.returned)if(!check(ui.completeExternal(foreground,scriptUiParent),"script UI completion"))return 0;return r.clocks;}
  if(keys.active()){if(!check(keys.route(state)==ControllerKeyServices::Route::Ready,"key child route"))return 0;auto r=keys.advanceBefore(state,irq,dsp,budget);if(r.returned)if(!check(foreground.completeMain(keyParent),"key completion"))return 0;return r.clocks;}
  if(arp.active()){if(!check(arp.route(state,dsp)==ControllerArpServices::Route::Ready,"arpeggiator route"))return 0;auto r=arp.advanceBefore(state,irq,budget);if(r.returned)if(!check(foreground.completeMain(M::Child::Arpeggiator),"arpeggiator completion"))return 0;return r.clocks;}
  if(periodic.active()){auto r=periodic.advanceBefore(state,irq,startup.lcd(),numeric,dsp,budget);if(r.returned){if(!check(foreground.completeCallback(callback),"periodic callback completion"))return 0;callback=ControllerCallback::Unknown;}return r.clocks;}
  if(realtime.active()){auto r=realtime.advanceBefore(state,irq,dsp,budget);if(r.returned){if(!check(foreground.completeMidiHandler(realtimeParent),"system MIDI completion"))return 0;realtimeParent=F::MidiHandler::Unknown;}return r.clocks;}
  auto q=foreground.request();
  if(ui.active()||(q.kind==F::RequestKind::MainChild&&q.slot>=5&&q.slot<=7))return uiForegroundPhase();
  if(midi.active()||q.kind==F::RequestKind::MidiHandler)return midiForegroundPhase(&q);
  if(q.kind==F::RequestKind::MainChild){if(q.slot<5){if(!check(foreground.beginDeferred(),"deferred admission"))return 0;return 0;}switch(q.child){case M::Child::MidiInput:if(!check(foreground.beginMidiPump(),"MIDI pump admission"))return 0;return 0;case M::Child::LocalKeys:case M::Child::Playback:keyParent=q.child;if(!check(q.child==M::Child::LocalKeys?keys.startKey():keys.startPlayback(),"key admission"))return 0;return 0;case M::Child::Arpeggiator:if(!check(arp.start(),"arpeggiator admission"))return 0;return 0;default:break;}}
  if(q.kind==F::RequestKind::Callback){if(q.callback==ControllerCallback::Callback2){if(!check(deferredNotice.start(),"deferred notice admission"))return 0;callback=q.callback;return 0;}if(q.callback==ControllerCallback::Callback3||q.callback==ControllerCallback::Callback4){if(!check(deferredParameter.start(q.callback==ControllerCallback::Callback3?ControllerDeferredParameterRuntime::Kind::Decrease:ControllerDeferredParameterRuntime::Kind::Increase),"deferred parameter admission"))return 0;callback=q.callback;return 0;}if(q.callback==ControllerCallback::Callback0||q.callback==ControllerCallback::Callback1){if(!check(deferredMenu.start(q.callback==ControllerCallback::Callback0?ControllerDeferredMenuRuntime::Kind::Previous:ControllerDeferredMenuRuntime::Kind::Next),"deferred menu admission"))return 0;callback=q.callback;return 0;}if(q.callback==ControllerCallback::Callback5||q.callback==ControllerCallback::Callback9||q.callback==ControllerCallback::Callback10){auto kind=q.callback==ControllerCallback::Callback5?ControllerDeferredUiRuntime::Kind::ClearNotice:q.callback==ControllerCallback::Callback9?ControllerDeferredUiRuntime::Kind::Noop:ControllerDeferredUiRuntime::Kind::ClearEdit;if(!check(deferredUi.start(kind),"deferred UI admission"))return 0;callback=q.callback;return 0;}if(q.callback==ControllerCallback::Callback7||q.callback==ControllerCallback::Callback8){if(!check(directUi.startDirect(q.callback==ControllerCallback::Callback7?ControllerUiServices::DirectKind::ProgramRepeatFirst:ControllerUiServices::DirectKind::ProgramRepeatAgain,0,0),"program repeat callback admission"))return 0;callback=q.callback;directParent=DirectParent::Callback;return 0;}if(q.callback==ControllerCallback::Callback6){if(!check(periodic.start(),"periodic callback admission"))return 0;callback=q.callback;return 0;}return fail("callback remains open");}
  if(!check(voices.routeForeground(foreground,state,dsp,&q)==ControllerVoiceServices::Route::Ready,"voice child remains open"))return 0;
  auto r=voices.advanceForeground(foreground,state,irq,dsp,budget);foregroundOwner_=ForegroundOwner::Core;return r.clocks;
 }
 bool validInitialPatch()const{auto bank=board.readMemory(0xcd82);bool valid=bank>=1&&bank<=4&&board.readMemory(0xcd83)<100;for(unsigned i=0;i<128&&valid;++i)valid=board.readMemory(0xba5c+i)<128;return valid;}
 unsigned irqPhase(){
  constexpr auto budget=std::numeric_limits<unsigned>::max();
  auto r=irq.advanceBefore(state,budget);unsigned used=r.clocks;if(r.entered){++stats.nativeIrqEntries;auto body=irq.advanceBefore(state,budget);if(!check(body.clocks!=0,"first IRQ body interval"))return 0;used+=body.clocks;}
  if(!check(used!=0,"IRQ phase"))return 0;++stats.nativeIrqSteps;return used;
 }
 unsigned startupPhase(){
  if(startup.phase()==G::Phase::MainScratch)enteredMain=true;
  if(startup.complete()&&!entryCall)ready=validInitialPatch();
  if(irq.active()||(state.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return irqPhase();
  if(!startup.complete()){auto n=timedStartup();if(n)++stats.nativeStartupSteps;return n;}
  auto n=foregroundPhase();if(n)++stats.nativeForegroundSteps;return n;
 }
 unsigned phase(){
  if(!entryDone)return startupPhase();
  if(irq.active()||(state.pendingIrqs&&irq.mask()==ControllerInterruptRuntime::Mask::Enabled))return irqPhase();
  auto n=foregroundPhase();if(n)++stats.nativeForegroundSteps;return n;
 }
 uint64_t exec(uint32_t budget){auto begin=state.cycles;if(fault)return 0;while(state.cycles-begin<budget){auto before=state.cycles;unsigned used=0;for(unsigned i=0;i<64&&!used&&!fault;++i)used=phase();if(fault)return state.cycles-begin;if(!check(used&&state.cycles-before==used,"controller progress"))return 0;board.tickAfterPhase();++stats.instructions;
 }
#if defined(K_RESEARCH_NATIVE_TRACE)
  kNativeControllerSlice(begin,state.cycles,state.pendingIrqs,state.pendingIrqsHigh);
#endif
 return state.cycles-begin;}
};
NativeController::NativeController(const std::vector<uint8_t>&image,ControllerHostCallbacks callbacks,const std::vector<uint8_t>&flash):impl_(std::make_unique<Impl>(image,std::move(callbacks),flash)){}
NativeController::~NativeController()=default;
void NativeController::reset(){auto next=std::make_unique<Impl>(impl_->image,impl_->host,impl_->storage);impl_.swap(next);}
uint64_t NativeController::exec(uint32_t n){return impl_->exec(n);}
bool NativeController::enqueueMidi(const uint8_t*p,size_t n){return impl_->board.enqueueMidi(p,n);}bool NativeController::popMidiOut(uint8_t&v){return impl_->board.popMidiOut(v);}
bool NativeController::bootReady()const{return impl_->ready;}
ControllerDebugState NativeController::debugState()const{auto s=impl_->stats;s.cycles=impl_->state.cycles;s.ramOverlay=impl_->board.ramOverlay();s.mainEntered=impl_->enteredMain;s.hostReads=impl_->board.hostReadCount();s.hostWrites=impl_->board.hostWriteCount();s.midiReceived=impl_->board.midiReceivedCount();s.initializedFromFirmware=impl_->board.initializedFromFirmware();s.currentBank=readMemory(0xcd82);s.currentProgram=readMemory(0xcd83);auto&lcd=impl_->board.lcdText();s.lcd.assign(lcd.begin(),lcd.end());return s;}
uint8_t NativeController::readMemory(uint32_t a)const{return impl_->board.readMemory(a);}
std::vector<uint8_t>NativeController::saveFlash()const{return impl_->board.flashData();}void NativeController::copyFlash(std::vector<uint8_t>&out)const{out.assign(impl_->board.flashData().begin(),impl_->board.flashData().end());}uint64_t NativeController::flashRevision()const{return impl_->board.storageRevision();}
bool NativeController::hasProgram(uint8_t bank,uint8_t program)const{if(bank<1||bank>4||program>=100)return false;auto offset=0x26400+((size_t(bank)-1)*100+program)*128;for(unsigned i=0;i<128;++i)if(impl_->board.flashData()[offset+i]>=128)return false;return true;}
const uint8_t*NativeController::diagnosticMemory(){return impl_->state.memory;}uint16_t NativeController::diagnosticRead16(uint32_t a){return impl_->state.read16(a);}void NativeController::diagnosticWrite8(uint32_t a,uint8_t v){impl_->state.write8(a,v);}void NativeController::diagnosticWrite16(uint32_t a,uint16_t v){impl_->state.write16(a,v);}void NativeController::diagnosticInterrupt(int v){impl_->state.requestInterrupt(v);}
    void NativeController::copyCurrentPatch(uint8_t* p) const
    {
        for(unsigned i=0;i<128;++i)p[i]=readMemory(0xba5c+i);
        // Local panel edits operate on decoded mirrors. Serialize these only
        // into the caller's snapshot, following original 4F48/525C; no guest
        // memory writes or calls. Preserve bits outside each proven field.
        struct Mirror {uint8_t packed,decoded,mask,shift,bias;};
        static constexpr Mirror fields[]{
            {0,0x81,7,0,0},{0,0x80,0x18,3,0},{0,0x82,0x60,5,0},
            {5,0x83,3,0,0},{5,0x84,0x0c,2,0},{5,0x85,0x30,4,0},{5,0x86,0x40,6,0},
            {6,0x87,3,0,63},{6,0x88,0x0c,2,63},{6,0x89,0x30,4,63},{6,0x8a,0x40,6,0},
            {0x4e,0x9b,1,0,0},{0x4e,0x9c,2,1,0},{0x4e,0x8b,0x0c,2,0},{0x4e,0x8c,0x30,4,0},
            {0x4f,0xa0,1,0,0},{0x4f,0xa1,2,1,0},{0x4f,0xa2,4,2,0},
            {0x4f,0xa3,8,3,0},{0x4f,0xa4,0x10,4,0},{0x4f,0xa5,0x20,5,0},
            {0x50,0x9d,1,0,0},{0x50,0x9e,2,1,0},{0x50,0x9f,4,2,0},{0x50,0x8d,0x78,3,0},
            {0x58,0x8e,3,0,0},{0x58,0x8f,4,2,0},{0x58,0xa6,8,3,0},
            {0x58,0xa7,0x10,4,0},{0x58,0x90,0x60,5,0},
            {0x79,0x91,7,0,0},{0x79,0x92,0x18,3,0},
            {0x7a,0x93,3,0,0},{0x7a,0x94,0x0c,2,0},{0x7a,0x95,0x30,4,0},
            {0x7b,0x96,7,0,0},{0x7b,0x98,8,3,0},{0x7b,0x97,0x10,4,0},
            {0x7b,0x99,0x20,5,0},{0x7b,0x9a,0x40,6,0},
            {0x7f,0xa8,3,0,0},{0x7f,0xa9,0x0c,2,0},{0x7f,0xaa,0x30,4,0},{0x7f,0xab,0x40,6,0},
            {0x7e,0xac,7,0,0},{0x7e,0xad,0x78,3,0}
        };
        for(const auto& f:fields)
        {
            const auto value=static_cast<uint8_t>(readMemory(0xba5c+f.decoded)-f.bias);
            p[f.packed]=(p[f.packed]&~f.mask)|((value<<f.shift)&f.mask);
        }
    }
}

namespace kstation {const char*NativeController::faultMessage()const noexcept{return impl_->fault;}}

namespace kstation {unsigned NativeController::immutableBindingCount()const noexcept{return impl_->state.immutableBindingCount();}}
