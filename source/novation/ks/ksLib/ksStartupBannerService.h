#pragma once
#include "ksStartupBannerTask.h"
#include "ksPanelVersionTask.h"
#include "ksStateDisplayService.h"
#include "ksPanelSettingsLcdService.h"
#include "ksStartupHardwareService.h"
#include <optional>
namespace ks {
class StartupVersionService final {
public:
 enum class Mode{Version,Patch};struct Effect{bool read=false,write=false;};
 StartupVersionService(Mode mode,uint32_t source,uint8_t patch=0,uint16_t presentation=0):policy_(mode==Mode::Version?PanelVersionTask::Kind::Version:PanelVersionTask::Kind::Patch),source_(source),patch_(patch),presentation_(presentation){if(source_>=0x200000)throw std::logic_error("Startup version source range");}
 bool complete()const{return policy_.stage()==PanelVersionTask::Stage::Complete;}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 uint8_t secondary()const{return policy_.secondary();}uint16_t presentation()const{return presentation_;}
 unsigned nextClocks(int previous=-1)const{return lcd_?lcd_->nextClocks(previous):policy_.nextClocks();}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(lcd_){lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};if(lcd_->complete()){presentation_=lcd_->output();lcd_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup version display return");}return cost;}
  using T=PanelVersionTask;const auto stage=policy_.stage();policy_.advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Source:return source_;case T::Read::Primary:return read(source_,cost-2);case T::Read::Secondary:return read(source_+1,cost);case T::Read::Patch:return patch_;}throw std::logic_error("Startup version observation");});effect_.read=true;
  if(stage==T::Stage::ReadPrimary)presentation_=uint16_t(source_+1);
  using L=PanelSettingsLcdService;if(policy_.child()!=T::Child::None){effect_.write=true;auto kind=L::Kind::Character;uint16_t value=0;switch(policy_.child()){
   case T::Child::Major:kind=L::Kind::TwoSpaces;value=policy_.major();break;case T::Child::Dot:case T::Child::PatchDot:value='.';break;case T::Child::Minor:value=uint16_t('0'+policy_.minor());break;case T::Child::PatchValue:kind=L::Kind::Digit;value=policy_.patch();break;default:throw std::logic_error("Unowned startup version child");}
   const auto payload=kind==L::Kind::TwoSpaces?presentation_:uint16_t((presentation_&0xff00u)|uint8_t(value));lcd_.emplace(kind,L::Input{value,payload,0});
  }return cost;
 }
private:PanelVersionTask policy_;uint32_t source_=0;uint8_t patch_=0;uint16_t presentation_=0;std::optional<PanelSettingsLcdService>lcd_;Effect effect_{};
};
class StartupBannerService final {
public:
 struct Effect{bool read=false,write=false;};
 bool complete()const{return policy_.complete();}unsigned priority()const{return 0;}const Effect&effect()const{return effect_;}
 unsigned nextClocks(int previous=-1)const{return text_?text_->nextClocks(previous):version_?version_->nextClocks(previous):policy_.nextClocks(StartupHardwareService::byteClocks(writeTarget(policy_.stage()),previous));}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;effect_={};
  if(text_){effect_={text_->endsReadTransfer(),text_->endsWriteTransfer()};text_->advanceBefore(cost,read,write,previous);if(text_->complete()){presentation_=text_->output();text_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup banner text return");}return cost;}
  if(version_){version_->advanceBefore(cost,read,write,previous);effect_={version_->effect().read,version_->effect().write};if(version_->complete()){presentation_=version_->presentation();if(versionMode_==StartupVersionService::Mode::Version)patch_=version_->secondary();version_.reset();if(!policy_.resumeChild())throw std::logic_error("Startup banner version return");}return cost;}
  using T=StartupBannerTask;using S=T::Stage;const auto stage=policy_.stage();policy_.advanceBefore(cost,[&](T::Read q){return read(q==T::Read::Hardware?0xffec14u:0xffff51u,cost);},[&](T::Write q,uint8_t value){write(q==T::Write::Direction?0xfffeb1u:0xffff62u,value,cost);},StartupHardwareService::byteClocks(writeTarget(stage),previous));effect_.read=true;
  if(policy_.awaitingChild()){effect_.write=true;if(stage==S::ShowBanner){StateDisplayService::Input in;in.source=policy_.hardware()?0x2e2u:0x2d4u;in.character=presentation_;text_.emplace(StateDisplayService::Kind::Text,in);}else if(stage==S::ShowVersion){versionMode_=StartupVersionService::Mode::Version;version_.emplace(versionMode_,0x1b8u,0,presentation_);}else if(stage==S::InitializeBannerNotice){versionMode_=StartupVersionService::Mode::Patch;version_.emplace(versionMode_,0x1b8u,patch_,presentation_);}else throw std::logic_error("Unowned startup banner child");}return cost;
 }
private:
 static uint32_t writeTarget(StartupBannerTask::Stage s){using S=StartupBannerTask::Stage;return s==S::WriteBannerDirection||s==S::WriteIdleDirection?0xfffeb1u:0xffff62u;}
 StartupBannerTask policy_{StartupBannerTask::Kind::Banner};std::optional<StateDisplayService>text_;std::optional<StartupVersionService>version_;StartupVersionService::Mode versionMode_=StartupVersionService::Mode::Version;uint16_t presentation_=0;uint8_t patch_=0;Effect effect_{};
};
}
