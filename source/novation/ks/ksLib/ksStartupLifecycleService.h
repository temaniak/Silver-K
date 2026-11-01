#pragma once
#include "ksResetImageTask.h"
#include "ksFirmwareLayoutCheckTask.h"
#include "ksStartupHardwareService.h"
#include "ksStartupPanelResetService.h"
#include "ksStartupLoaderProbeService.h"
#include "ksStartupSystemService.h"
#include "ksStartupStateInitService.h"
#include <variant>
namespace ks {
// The whole ordinary cold/warm/overlay startup owns its actual children and
// pre-Main initialization. Every suspended edge is an enum/owned value, and
// completion is a semantic transition rather than an executable continuation.
class StartupLifecycleService final {
public:
 using Entry=ResetImageTask::Entry;
 enum class Phase{ResetImage,System,StateInit,Complete};
 enum class Outcome{Running,Playback,FirmwareUpdateRequested,MissingDomain};
 struct Effect{bool read=false,write=false;};
 explicit StartupLifecycleService(Entry entry=Entry::Cold,unsigned initialMask=0):reset_(entry),mask_(initialMask){if(mask_>7)throw std::logic_error("Startup initial interrupt mask");}
 Phase phase()const{return phase_;}Outcome outcome()const{return outcome_;}bool mainEntered()const{return mainEntered_;}bool ready()const{return ready_;}
 bool firstMainAvailable()const{return outcome_==Outcome::Playback;}bool complete()const{return outcome_!=Outcome::Running;}bool errorHold()const{return reset_.stage()==ResetImageTask::Stage::HoldError;}
 const StartupPlaybackContext&result()const{return result_;}const Effect&effect()const{return effect_;}const ResetImageTask&resetPolicy()const{return reset_;}
 unsigned priority()const{return (phase_==Phase::ResetImage?mask_:0u)|std::visit([](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>||std::is_same_v<S,FirmwareLayoutCheckTask>)return 0;else if constexpr(std::is_same_v<S,StateDisplayService>)return 0;else return s.priority();},child_);}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(std::holds_alternative<std::monostate>(child_))return reset_.nextClocks();return std::visit([&](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return 0;else if constexpr(std::is_same_v<S,FirmwareLayoutCheckTask>)return s.nextClocks();else return s.nextClocks(previous);},child_);}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;
  using T=ResetImageTask;using S=T::Stage;if(phase_==Phase::ResetImage&&reset_.stage()==S::ReadImageWord&&reset_.imageSize()>0x200000u-260u)throw std::logic_error("Startup image extent exceeds physical SRAM");
  effect_={};
  if(!std::holds_alternative<std::monostate>(child_)){bool done=false;bool requestedUpdate=false;bool systemDone=false;bool initDone=false;
   std::visit([&](auto&s){using C=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<C,std::monostate>){
    if constexpr(std::is_same_v<C,FirmwareLayoutCheckTask>){s.advanceBefore(cost);effect_.read=true;}
    else if constexpr(std::is_same_v<C,StateDisplayService>){effect_={s.endsReadTransfer(),s.endsWriteTransfer()};s.advanceBefore(cost,read,write,previous);}
    else if constexpr(std::is_same_v<C,StartupStateInitService>){s.advanceBefore(cost,read,write,resolve,previous);effect_={s.effect().read,s.effect().write};ready_=s.ready();}
    else{s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};}
    if constexpr(std::is_same_v<C,FirmwareLayoutCheckTask>)done=s.stage()==C::Stage::Complete;else done=s.complete();if(done){if constexpr(std::is_same_v<C,StartupLoaderProbeService>)requestedUpdate=s.outcome()==C::Outcome::FirmwareUpdateRequested;else if constexpr(std::is_same_v<C,StartupSystemService>)systemDone=true;else if constexpr(std::is_same_v<C,StartupStateInitService>){result_=s.result();initDone=true;}}
   }},child_);
   if(done){child_.emplace<std::monostate>();effect_.read=true;if(requestedUpdate){outcome_=Outcome::FirmwareUpdateRequested;return cost;}if(initDone){phase_=Phase::Complete;outcome_=result_.known()?Outcome::Playback:Outcome::MissingDomain;}else if(systemDone){phase_=Phase::StateInit;mask_=0;mainEntered_=true;child_.emplace<StartupStateInitService>();}else if(!reset_.resumeChild())throw std::logic_error("Startup reset child ownership");}return cost;
  }
  const auto stage=reset_.stage();
  const auto wide=[&](uint32_t at,unsigned width,unsigned back){uint32_t value=0;for(unsigned n=0;n<width;++n)value=(value<<8)|uint8_t(read(at+n,cost-back-3*(width-n-1)));return value;};
  const auto put=[&](uint32_t at,uint32_t value,unsigned width){for(unsigned n=0;n<width;++n)write(at+n,uint8_t(value>>(8*(width-n-1))),cost-3*(width-n-1));};
  reset_.advanceBefore(cost,[&](S q,unsigned n)->uint32_t{switch(q){case S::ReadProbeMarker:case S::ReadWarmMarker:return read(0xffec33,cost);case S::ReadBootByte:return read(0x800000+n,cost-2);case S::ReadSignature:return read(0x190+n,cost-2);case S::ReadStoredSignature:return read(0x804190+n,cost-2);case S::ReadImageSize:return wide(0x804184,4,0);case S::ReadImageWord:return wide(0x804000+n,2,2);case S::ReadExpectedChecksum:return wide(0x804000+n,4,2);case S::ReadOverlayByte:return read(0x422+n,cost-2);case S::SwitchOverlay:return read(0xffff6f,cost-1);default:throw std::logic_error("Startup reset observation");}},[&](S q,unsigned n,uint32_t value){switch(q){case S::ClearWord:put(0xffec00+2*n,value,2);break;case S::PublishMarker:put(0xffec33,value,1);break;case S::WriteBootByte:put(0xa00000+n,value,1);break;case S::WriteImageWord:case S::WriteTailWord:put(0xa00000+n,value,2);break;case S::WriteChecksum:put(0xa00000+n,value,4);break;case S::WriteOverlayByte:put(0xffec00+n,value,1);break;case S::SwitchOverlay:put(0xffff6f,value,1);break;case S::PublishOverlayMode:put(0xfffed1,value,1);break;default:throw std::logic_error("Startup reset publication");}});effect_.read=true;
  if(stage==S::MaskForOverlay)mask_=7;
  if(reset_.stage()==S::BindUpdateMessage){outcome_=Outcome::FirmwareUpdateRequested;return cost;}
  if(reset_.playback()){phase_=Phase::System;child_.emplace<StartupSystemService>(mask_);return cost;}
  if(reset_.awaiting()){effect_.write=true;using H=HardwareSetupTask::Kind;switch(reset_.child()){
   case T::Child::Pins:child_.emplace<StartupHardwareService>(H::Pins);break;case T::Child::Panel:child_.emplace<StartupPanelResetService>();break;case T::Child::Runtime:child_.emplace<StartupHardwareService>(H::WaitStates);break;case T::Child::RuntimeA:child_.emplace<StartupHardwareService>(H::Dram);break;case T::Child::RuntimeB:child_.emplace<StartupHardwareService>(H::Clock);break;case T::Child::Display:child_.emplace<StartupLcdActionService>(StartupLcdActionService::Kind::Initialize);break;case T::Child::ImageBounds:child_.emplace<FirmwareLayoutCheckTask>();break;case T::Child::Probe:child_.emplace<StartupLoaderProbeService>();break;
   case T::Child::ErrorMessage:{StateDisplayService::Input in;in.source=0x2c5;child_.emplace<StateDisplayService>(StateDisplayService::Kind::Text,in);break;}
   case T::Child::UpdateMessage:case T::Child::FirmwareUpdate:case T::Child::DoneMessage:outcome_=Outcome::FirmwareUpdateRequested;break;default:throw std::logic_error("Unowned startup reset child");}}
  return cost;
 }
private:
 ResetImageTask reset_;unsigned mask_=0;Phase phase_=Phase::ResetImage;Outcome outcome_=Outcome::Running;bool mainEntered_=false,ready_=false;StartupPlaybackContext result_;Effect effect_{};
 using Child=std::variant<std::monostate,StartupHardwareService,StartupPanelResetService,StartupLcdActionService,FirmwareLayoutCheckTask,StartupLoaderProbeService,StateDisplayService,StartupSystemService,StartupStateInitService>;Child child_{};
};
}
