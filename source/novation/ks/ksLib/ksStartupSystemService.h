#pragma once
#include "ksSystemServicesTask.h"
#include "ksStartupHardwareService.h"
#include "ksStartupPanelDetectService.h"
#include "ksStartupLcdActionService.h"
#include "ksStartupSerialService.h"
#include "ksStartupDspService.h"
#include "ksStartupTimingService.h"
#include "ksStartupBannerService.h"
#include "ksPanelScanService.h"
#include "ksPanelSettingsService.h"
#include "ksPanelControlsService.h"
#include "ksStateFactoryDspService.h"
#include "ksStateResetService.h"
#include <variant>
namespace ks {
// Small initialization policies that recurse only into owned LCD/key services.
class StartupSystemLeafService final {
public:
 using Kind=SystemServicesTask::Kind;struct Effect{bool read=false,write=false;};
 explicit StartupSystemLeafService(Kind k):policy_(k){if(k!=Kind::KeyMap&&k!=Kind::Keyboard&&k!=Kind::Glyphs)throw std::logic_error("Startup system leaf kind");}
 bool complete()const{return policy_.complete();}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return lcd_?lcd_->nextClocks(previous):reset_?reset_->nextClocks():policy_.nextClocks(StartupHardwareService::byteClocks(writeTarget(policy_.stage()),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(lcd_){lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};if(lcd_->complete()){lcd_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup glyph action return");}return cost;}
  if(reset_){reset_->advanceBefore(cost,read,write);if(reset_->complete()){reset_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup keyboard reset return");effect_.read=true;}return cost;}
  using T=SystemServicesTask;using S=T::Stage;const auto stage=policy_.stage();policy_.advanceBefore(cost,[&](T::Read q,unsigned){if(q!=T::Read::Hardware)throw std::logic_error("Unexpected startup system leaf observation");return read(0xffec14,cost);},[&](T::Write q,uint16_t value){const auto at=q==T::Write::KeyOne?0xb93bu:q==T::Write::KeyTwo?0xb93au:q==T::Write::KeyFour?0xb938u:0xffff6du;write(at,uint8_t(value),cost-(q==T::Write::KeyboardPort?0u:2u));},StartupHardwareService::byteClocks(writeTarget(stage),previous));effect_.read=true;
  if(policy_.awaitingChild()){effect_.write=true;switch(stage){case S::ResetKeyQueue:reset_.emplace(StateResetService::Kind::Keyboard);break;case S::SetGlyphCommand:lcd_.emplace(StartupLcdActionService::Kind::CursorSecond,0);break;case S::WriteGlyphRow:lcd_.emplace(StartupLcdActionService::Kind::DirectData,uint8_t(policy_.mask()));break;case S::WriteEmptyRow:lcd_.emplace(StartupLcdActionService::Kind::DirectData,0);break;case S::WriteFullRow:lcd_.emplace(StartupLcdActionService::Kind::DirectData,31);break;case S::WriteLastRow:lcd_.emplace(StartupLcdActionService::Kind::DirectData,16);break;default:throw std::logic_error("Unowned startup system leaf child");}}return cost;
 }
private:
 static uint32_t writeTarget(SystemServicesTask::Stage s){using S=SystemServicesTask::Stage;return s==S::WriteKeyOne?0xb93bu:s==S::WriteKeyTwo?0xb93au:s==S::WriteKeyFour?0xb938u:0xffff6du;}
 SystemServicesTask policy_;std::optional<StartupLcdActionService>lcd_;std::optional<StateResetService>reset_;Effect effect_{};
};
// Physical board initialization is a fixed typed child sequence. The legacy
// policy names are retained as domain labels; actual DSP boot/reset/table work
// is owned by the corresponding real services rather than name-based guesses.
class StartupSystemService final {
public:
 struct Effect{bool read=false,write=false;};
 explicit StartupSystemService(unsigned initialMask=7):mask_(initialMask){if(mask_>7)throw std::logic_error("Startup system mask");}
 bool complete()const{return policy_.complete()&&std::holds_alternative<std::monostate>(child_);}const Effect&effect()const{return effect_;}
 unsigned priority()const{return mask_|std::visit([](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return 0;else if constexpr(std::is_same_v<S,PanelScanService>||std::is_same_v<S,PanelControlsService>)return 0;else if constexpr(std::is_same_v<S,PanelSettingsService>)return s.writePriority()?7u:0u;else if constexpr(std::is_same_v<S,StateFactoryDspService>)return s.critical()?7u:0u;else return s.priority();},child_);}
 const SystemServicesTask&policy()const{return policy_;}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;if(std::holds_alternative<std::monostate>(child_))return policy_.nextClocks();return std::visit([&](const auto&s)->unsigned{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return 0;else if constexpr(std::is_same_v<S,PanelScanService>||std::is_same_v<S,PanelControlsService>)return s.nextClocks();else return s.nextClocks(previous);},child_);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(!std::holds_alternative<std::monostate>(child_)){bool done=false;std::visit([&](auto&s){using S=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<S,std::monostate>){if constexpr(std::is_same_v<S,PanelScanService>||std::is_same_v<S,PanelControlsService>)s.advanceBefore(cost,read,write);else if constexpr(std::is_same_v<S,PanelSettingsService>)s.advanceBefore(cost,read,write,0,previous);else s.advanceBefore(cost,read,write,previous);if constexpr(std::is_same_v<S,PanelScanService>||std::is_same_v<S,PanelControlsService>)effect_={s.complete(),false};else effect_={s.effect().read,s.effect().write};done=s.complete();}},child_);if(done){child_.emplace<std::monostate>();if(!policy_.resumeChild())throw std::logic_error("Startup system child return");effect_.read=true;}return cost;}
  using T=SystemServicesTask;using S=T::Stage;const auto stage=policy_.stage();policy_.advanceBefore(cost,[](auto,auto){throw std::logic_error("Unexpected initialization policy read");return 0u;},[](auto,auto){throw std::logic_error("Unexpected initialization policy write");});effect_.read=true;
  if(stage==S::EnableInterrupts)mask_=0;
  if(policy_.awaitingChild()){effect_.write=true;using H=HardwareSetupTask::Kind;switch(stage){
   case S::ResetHardware:child_.emplace<StartupPanelDetectService>();break;case S::PatchKeyMap:child_.emplace<StartupSystemLeafService>(T::Kind::KeyMap);break;case S::InitializeDisplay:child_.emplace<StartupLcdActionService>(StartupLcdActionService::Kind::Initialize);break;case S::InitializeButtons:child_.emplace<PanelScanService>(PanelScanService::Kind::Initialize);break;
   case S::InitializeUart:child_.emplace<StartupSerialService>(StartupSerialService::Kind::Configure);break;case S::InitializeMidi:child_.emplace<StartupSerialService>(StartupSerialService::Kind::Start);break;case S::InitializeKeyboard:child_.emplace<StartupSystemLeafService>(T::Kind::Keyboard);break;case S::InitializeCalibration:child_.emplace<PanelSettingsService>(PanelSettingsService::Kind::Reset);break;
   case S::InitializeDspLink:child_.emplace<StartupHardwareService>(H::Dram);break;case S::InitializeDspWait:child_.emplace<StartupHardwareService>(H::Clock);break;case S::InitializePanelLevel:child_.emplace<PanelSettingsService>(PanelSettingsService::Kind::AnalogInitialize);break;case S::InitializeControlMaps:child_.emplace<PanelControlsService>(PanelControlsService::Kind::PedalReset);break;case S::InitializeAdc:child_.emplace<PanelSettingsService>(PanelSettingsService::Kind::Warmup);break;
   case S::InitializeMidiJobs:child_.emplace<StartupDspService>();break;case S::InitializePlayback:child_.emplace<StateFactoryDspService>(StateFactoryDspService::Kind::Reset);break;case S::InitializeClock:child_.emplace<StartupTimingService>();break;case S::DisplayStartup:child_.emplace<StartupBannerService>();break;case S::InitializeGlyphs:child_.emplace<StartupSystemLeafService>(T::Kind::Glyphs);break;default:throw std::logic_error("Unowned startup initialization child");}}
  return cost;
 }
private:SystemServicesTask policy_{SystemServicesTask::Kind::Initialize};unsigned mask_=7;Effect effect_{};
 using Child=std::variant<std::monostate,StartupHardwareService,StartupPanelDetectService,StartupLcdActionService,StartupSerialService,StartupDspService,StartupTimingService,StartupBannerService,StartupSystemLeafService,PanelScanService,PanelSettingsService,PanelControlsService,StateFactoryDspService>;Child child_{};
};
}
