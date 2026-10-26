#pragma once
#include "ksStateInitTask.h"
#include "ksStatePatchNameTask.h"
#include "ksStateProgramBrowseTask.h"
#include "ksStateStorageTask.h"
#include "ksStateGlobalsTask.h"
#include "ksPanelAdcCalibrationTask.h"
#include "ksTimerServiceTask.h"
#include "ksScheduledTimerService.h"
#include "ksNativeMidiInitializationService.h"
#include "ksNativeArpPrimitiveService.h"
#include "ksStateStatusService.h"
#include "ksStateFieldService.h"
#include "ksStatePlaybackModeService.h"
#include "ksStateProgramLoadService.h"
#include "ksStateResetService.h"
#include "ksStateCentralActionsBindings.h"
#include "ksStartupHardwareService.h"
#include <variant>
namespace ks {
// Only musical/UI data consumed by the first Main iteration. Knownness tracks
// genuine domain producers; an arbitrary initial machine image cannot seed it.
struct StartupPlaybackContext final {
 uint16_t presentation=0,presentationMask=0;uint8_t activation=0;bool activationKnown=false;uint32_t expressionSource=0,expressionMask=0;
 void apply(const StateUiDomainTransfer&t){presentation=t.presentation.apply(presentation);presentationMask=uint16_t(presentationMask|t.presentation.mask);if(t.activation.known){activation=t.activation.value;activationKnown=true;}expressionSource=(expressionSource&~t.expression.mask)|(t.expression.value&t.expression.mask);expressionMask|=t.expression.mask;}
 bool known()const{return presentationMask==65535&&activationKnown&&expressionMask==~0u;}
};
// Fixed leaf owners used by initialization. Every kind is a real semantic
// task; dispatch is never by code address, and resource callbacks stay live.
class StartupStateInitLeaf final {
public:
 enum class Kind{PanelDefault,Banks,Pin,Tables,Globals,Clock};
 struct Effect{bool read=false,write=false;};
 explicit StartupStateInitLeaf(Kind k,uint8_t pin=0x10):kind_(k){switch(k){case Kind::PanelDefault:task_.emplace<StatePatchNameTask>(StatePatchNameTask::Entry::Empty);break;case Kind::Banks:task_.emplace<StateProgramBrowseTask>(StateProgramBrowseTask::Kind::Clear);break;case Kind::Pin:task_.emplace<PanelAdcCalibrationTask>(PanelAdcCalibrationTask::Kind::Read,pin);break;case Kind::Tables:task_.emplace<StateStorageTask>(StateStorageTask::Kind::Tables);break;case Kind::Globals:task_.emplace<StateGlobalsTask>(StateGlobalsTask::Kind::BootDefaults);break;case Kind::Clock:task_.emplace<ControllerClockReadTask>();break;}}
 bool complete()const{return std::visit([](const auto&t){return t.complete();},task_);}const Effect&effect()const{return effect_;}
 uint8_t pin()const{return std::get<PanelAdcCalibrationTask>(task_).result();}uint32_t clock()const{return std::get<ControllerClockReadTask>(task_).counter();}
 StateUiDomainTransfer transfer()const{if(!complete())throw std::logic_error("Incomplete startup leaf outcome");StateUiDomainTransfer out;if(kind_==Kind::Pin)out.presentation=stateUiDomainBindings::low(pin());else if(kind_==Kind::Clock)out.presentation=stateUiDomainBindings::word(uint16_t(clock()));return out;}
 unsigned nextClocks(int previous=-1)const{return std::visit([&](const auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,StateProgramBrowseTask>)return t.nextClocks(StartupHardwareService::byteClocks(0x167fb,previous));else if constexpr(std::is_same_v<T,StateGlobalsTask>)return t.nextClocks(StartupHardwareService::byteClocks(0x169e6+t.byteOffset(),previous));else return t.nextClocks();},task_);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={true,false};
 const auto word=[&](uint32_t a){return uint16_t((unsigned(uint8_t(read(a,cost)))<<8)|uint8_t(read(a+1,cost)));};
 std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
  if constexpr(std::is_same_v<T,StatePatchNameTask>)t.advanceBefore(cost,[](auto){throw std::logic_error("Unexpected empty-name read");return 0u;},[](auto,auto){throw std::logic_error("Unexpected empty-name write");});
  else if constexpr(std::is_same_v<T,StateProgramBrowseTask>){using S=typename T::Stage;const auto stage=t.stage();effect_.write=stage==S::SaveClearDigits;t.advanceBefore(cost,[](auto,auto){throw std::logic_error("Unexpected digit-clear read");return 0u;},[&](auto,uint8_t value){write(0x167fb,value,cost);},StartupHardwareService::byteClocks(0x167fb,previous));}
  else if constexpr(std::is_same_v<T,PanelAdcCalibrationTask>){effect_.write=t.stage()==T::Stage::SaveAddress;t.advanceBefore(cost,[&](auto){return word(0x1560a+t.offset());},[](auto,auto){throw std::logic_error("Unexpected pin-read publication");});}
  else if constexpr(std::is_same_v<T,StateStorageTask>)t.advanceBefore(cost,[](auto){throw std::logic_error("Unexpected metadata-table read");return 0u;},[&](typename T::Write q,uint32_t value){const auto a=q==T::Write::FieldTable?0x167c4u:q==T::Write::LimitTable?0x167c8u:0x167ccu;for(unsigned n=0;n<4;++n)write(a+n,uint8_t(value>>(8*(3-n))),cost-3*((3-n+(a+n)%2)/2));});
  else if constexpr(std::is_same_v<T,StateGlobalsTask>)t.advanceBefore(cost,[&](auto){return read(0x81ff00+t.byteOffset(),cost-2);},[&](auto,uint8_t value){write(0x169e6+t.byteOffset(),value,cost);},StartupHardwareService::byteClocks(0x169e6+t.byteOffset(),previous));
  else if constexpr(std::is_same_v<T,ControllerClockReadTask>)t.advanceBefore(cost,[&]{uint32_t value=0;for(unsigned n=0;n<4;++n)value=(value<<8)|uint8_t(read(0xffec10+n,cost-3*(3-n)));return value;});
 },task_);return cost;}
private:Kind kind_;std::variant<StatePatchNameTask,StateProgramBrowseTask,PanelAdcCalibrationTask,StateStorageTask,StateGlobalsTask,ControllerClockReadTask>task_{std::in_place_type<StatePatchNameTask>,StatePatchNameTask::Entry::Empty};Effect effect_{};
};
// Complete pre-Main initialization owns every ordinary child, including the
// final reset after readiness publication. No caller frame or ABI sink exists.
class StartupStateInitService final {
public:
 struct Effect{bool read=false,write=false;};
 explicit StartupStateInitService(StartupPlaybackContext input={}):context_(input){}
 bool ready()const{return ready_;}bool firstMainAvailable()const{return complete()&&context_.known();}
 bool complete()const{return policy_.complete()&&std::holds_alternative<std::monostate>(child_);}const StartupPlaybackContext&result()const{return context_;}const Effect&effect()const{return effect_;}
 const StateInitTask&policy()const{return policy_;}
 unsigned priority()const{return std::visit([](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>||std::is_same_v<S,StartupStateInitLeaf>||std::is_same_v<S,StateResetService>)return 0;else if constexpr(std::is_same_v<S,NativeMidiInitializationService>)return s.priority();else return s.critical()?7u:0u;},child_);}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(!std::holds_alternative<std::monostate>(child_))return std::visit([&](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return 0;else if constexpr(std::is_same_v<S,StateResetService>)return s.nextClocks();else return s.nextClocks(previous);},child_);return policy_.nextClocks(policy_.publication()?StartupHardwareService::byteClocks(target(policy_.publicationField(),policy_.position()),previous):3u);}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
  if(!std::holds_alternative<std::monostate>(child_)){bool done=false;StateUiDomainTransfer transfer;
   std::visit([&](auto&s){using S=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<S,std::monostate>){
    if constexpr(std::is_same_v<S,StateResetService>)s.advanceBefore(cost,read,write);
    else if constexpr((std::is_same_v<S,StateProgramLoadService>||std::is_same_v<S,StateReloadService>))s.advanceBefore(cost,read,write,[&](uint8_t category,uint32_t raw){return resolve.schema(category,raw);},previous);
    else if constexpr(std::is_same_v<S,ScheduledTimerService>||std::is_same_v<S,StateFieldService>||std::is_same_v<S,StatePlaybackModeService>)s.advanceBefore(cost,read,write,resolve,previous);
    else s.advanceBefore(cost,read,write,previous);
    if constexpr(!std::is_same_v<S,StateResetService>)effect_={s.effect().read,s.effect().write};
    done=s.complete();if(done){
     if constexpr(std::is_same_v<S,StartupStateInitLeaf>){transfer=s.transfer();if(policy_.child()==StateInitTask::Child::Pin)pin_=s.pin();else if(policy_.child()==StateInitTask::Child::Clock)clock_=s.clock();}
     else if constexpr(std::is_same_v<S,NativeArpPrimitiveService>)transfer.presentation=stateUiDomainBindings::word(0);
     else if constexpr(std::is_same_v<S,NativeMidiInitializationService>){if(const auto*t=s.controlsTask()){transfer.activation={true,t->byte()};}}
     else if constexpr(std::is_same_v<S,StateFieldService>)transfer=policy_.child()==StateInitTask::Child::Display?stateUiRefreshDomainTransfer(s):stateFieldDomainTransfer(s);
     else if constexpr(std::is_same_v<S,StateProgramLoadService>)transfer=stateUiDomainBindings::program(s);
     else if constexpr(std::is_same_v<S,StateReloadService>){if(!s.panel())throw std::logic_error("Missing startup Multi panel result");transfer.expression=fieldExpressionBindings::full(0x60010e);transfer.activation={true,uint8_t(s.panel()->bounds()>>8)};transfer.presentation=stateUiDomainBindings::word(uint16_t((s.panel()->panelTask().value()<<8)|21u));}
     else if constexpr(std::is_same_v<S,StatePlaybackModeService>)transfer=centralActionsDomainBindings::modeNode(s,0);
    }
   }},child_);
   if(done){context_.apply(transfer);child_.emplace<std::monostate>();effect_.read=true;if(!policy_.resumeChild())throw std::logic_error("Startup initialization child return");updateReady();}return cost;
  }
  using T=StateInitTask;using Q=T::Read;const auto stage=policy_.stage();
  policy_.advanceBefore(cost,[&](Q q)->uint32_t{switch(q){case Q::HardwarePin:return pin_;case Q::Clock0High:return read(0x169e6,cost);case Q::Clock0Low:return read(0x169e7,cost);case Q::Clock1High:return read(0x169e8,cost);case Q::Clock1Low:return read(0x169e9,cost);case Q::ActiveMask:return read(0x169b9,cost);case Q::ReceiveMask:return read(0x169ba,cost);case Q::PreferredMode:return read(0x169ea,cost);case Q::SelectedMode:return read(0x167ba,cost);case Q::CurrentClock:return uint16_t(clock_);}throw std::logic_error("Startup initialization observation");},[&](T::Write q,unsigned n,uint32_t value){const auto a=target(q,n);const unsigned width=q==T::Write::ActiveGlobalPatch?4:q==T::Write::Clock0||q==T::Write::Clock1||q==T::Write::LastClock?2:1;if(q==T::Write::ActiveGlobalPatch)value=0x168fc;for(unsigned b=0;b<width;++b)write(a+b,uint8_t(value>>(8*(width-b-1))),cost-3*((width-b-1+(a+b)%2)/2));},[&](T::Write q,unsigned n){return StartupHardwareService::byteClocks(target(q,n),previous);});
  if(stage==T::Stage::SetPinRequest){StateUiDomainTransfer t;t.presentation=stateUiDomainBindings::low(0x10);context_.apply(t);}else if(stage==T::Stage::ShiftPin){StateUiDomainTransfer t;t.presentation=stateUiDomainBindings::low(policy_.byte());context_.apply(t);}else if(stage==T::Stage::CapturePin){StateUiDomainTransfer t;t.presentation=stateUiDomainBindings::word(uint16_t(unsigned(policy_.byte())*257u));context_.apply(t);}
  if(policy_.awaiting()){effect_.write=true;startChild();}updateReady();return cost;
 }
private:
 static uint32_t target(StateInitTask::Write q,unsigned n){using W=StateInitTask::Write;switch(q){case W::ResetCell:return 0x16782+n;case W::Default:return 0x167a4;case W::ActiveGlobalPatch:return 0x167bc;case W::Patch0:return 0x169e1;case W::Patch1:return 0x169e2;case W::Patch2:return 0x169e3;case W::PreviewGate:return 0x167d5;case W::PatchBias:return 0x169e0;case W::Clock0:return 0x167aa;case W::Clock1:return 0x167ae;case W::ActiveMask:return 0x167d0;case W::ReceiveMask:return 0x167d1;case W::Sentinel0:return 0x167bb;case W::Sentinel1:return 0x167d4;case W::SelectedMode:return 0x167ba;case W::LastClock:return 0x16780;}throw std::logic_error("Startup initialization publication");}
 void updateReady(){if(policy_.stage()==StateInitTask::Stage::DispatchHardware)ready_=true;}
 void startChild(){using C=StateInitTask::Child;using L=StartupStateInitLeaf;const auto child=policy_.child();switch(child){
 case C::PanelDefault:child_.emplace<L>(L::Kind::PanelDefault);break;case C::Timer0:case C::Timer1:case C::Timer2:case C::Timer3:case C::Timer4:child_.emplace<ScheduledTimerService>(ScheduledTimerService::Kind::Cancel,unsigned(child)-unsigned(C::Timer0));break;
 case C::Banks:child_.emplace<L>(L::Kind::Banks);break;case C::Notes:child_.emplace<NativeArpPrimitiveService>(NativeArpPrimitiveService::Kind::NullResult,NativeArpPrimitiveService::Input{});break;case C::Midi:child_.emplace<NativeMidiInitializationService>(NativeMidiInitializationService::Kind::Initialize);break;case C::PanelTimers:child_.emplace<StateStatusService>(StateStatusService::Kind::Schedule,StateStatusService::Input{context_.presentation});break;
 case C::Pin:child_.emplace<L>(L::Kind::Pin,0x10);break;case C::PanelValue:{StateFieldService::Input in;in.event=context_.presentation;child_.emplace<StateFieldService>(StateFieldService::Kind::Velocity,in);break;}
 case C::PanelState:child_.emplace<L>(L::Kind::Tables);break;case C::GlobalBank:child_.emplace<L>(L::Kind::Globals);break;case C::SingleMode:case C::MultiMode:case C::PreferredMode:child_.emplace<StatePlaybackModeService>(StatePlaybackModeService::Kind::Routing,StatePlaybackModeService::Input{context_.presentation});break;
 case C::Single:case C::PreferredSingle:child_.emplace<StateProgramLoadService>(StateProgramLoadService::Kind::LoadSingle,StateProgramLoadService::Input{context_.presentation});break;case C::Multi:case C::PreferredMulti:child_.emplace<StateReloadService>();break;
 case C::Configuration:child_.emplace<StatePlaybackModeService>(StatePlaybackModeService::Kind::ResolveEditPart,StatePlaybackModeService::Input{context_.presentation});break;case C::Display:child_.emplace<StateFieldService>(StateUiRefreshService::Kind::Refresh,StateUiRefreshService::Input{0,context_.presentation,context_.presentation});break;case C::Clock:child_.emplace<L>(L::Kind::Clock);break;case C::Hardware:child_.emplace<StateResetService>();break;}}
 StateInitTask policy_;StartupPlaybackContext context_;Effect effect_{};uint8_t pin_=0;uint32_t clock_=0;bool ready_=false;
 using Child=std::variant<std::monostate,StartupStateInitLeaf,ScheduledTimerService,NativeArpPrimitiveService,NativeMidiInitializationService,StateStatusService,StateFieldService,StatePlaybackModeService,StateProgramLoadService,StateReloadService,StateResetService>;Child child_{};
};
}
