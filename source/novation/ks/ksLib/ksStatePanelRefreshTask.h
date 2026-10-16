#pragma once
#include <array>
#include <cstdint>
namespace ks {class StatePanelRefreshTask final {public:enum class Stage {SetCommonKey,SetCommonCount,RetainCount,RefreshCommon,AdvanceCommon,RestoreCount,DecrementCount,AdmitCommon,SetFirstSpecial,RefreshFirstSpecial,SetSecondSpecial,RefreshSecondSpecial,ReadCurrent,ReadOscillatorMode,SetOscillatorKey,RefreshOscillator,ReadLfoMode,ShiftLfo,MaskLfo,SetLfoKey,RefreshLfo,ReadFilterMode,ShiftFilterFirst,ShiftFilterSecond,MaskFilter,SetFilterKey,RefreshFilter,ReadMaskFirst,SetMaskFirstKey,RefreshMaskFirst,ReadMaskSecond,SetMaskSecondKey,RefreshMaskSecond,ReadPartMode,SetPartKey,RefreshPart,ReadPageMode,SetPageKey,RefreshPage,ReadEditMode,SetEditKey,RefreshEdit,ReadGlobalMode,SetGlobalKey,RefreshGlobal,ReadAlternateMode,SetAlternateKey,RefreshAlternate,ReadMenuMode,SetMenuKey,RefreshMenu,Return,AwaitChild,Complete};enum class Read {Current,OscillatorMode,LfoMode,FilterMode,MaskFirst,MaskSecond,PartMode,PageMode,EditMode,GlobalMode,AlternateMode,MenuMode};enum class Child {Common,Special,Indicator};Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingChild()const{return phase_==Stage::AwaitChild;}Child child()const{return child_;}uint8_t key()const{return key_;}uint8_t value()const{return value_;}bool resumeChild(){if(!awaitingChild())return false;phase_=resume_;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,52> costs{{6,3,11,20,3,11,3,3,6,20,6,20,18,15,3,20,15,3,3,3,20,15,3,3,3,3,20,12,3,20,12,3,20,12,3,20,12,3,20,12,3,20,12,3,20,12,3,20,12,3,20,17}};return unsigned(phase_)<costs.size()?costs[unsigned(phase_)]:0;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto c=nextClocks()){if(c>available-used)break;switch(phase_){case Stage::SetCommonKey:key_=38;next();break;
case Stage::RefreshCommon:child_=Child::Common;resume_=Stage::AdvanceCommon;phase_=Stage::AwaitChild;break;
case Stage::AdvanceCommon:++key_;++index_;next();break;
case Stage::AdmitCommon:phase_=index_<9?Stage::RetainCount:Stage::SetFirstSpecial;break;
case Stage::SetFirstSpecial:key_=1;next();break;
case Stage::RefreshFirstSpecial:child_=Child::Special;resume_=Stage::SetSecondSpecial;phase_=Stage::AwaitChild;break;
case Stage::SetSecondSpecial:key_=14;next();break;
case Stage::RefreshSecondSpecial:child_=Child::Special;resume_=Stage::ReadCurrent;phase_=Stage::AwaitChild;break;
case Stage::ReadCurrent:(void)read(Read::Current);next();break;
case Stage::ReadOscillatorMode:value_=uint8_t(read(Read::OscillatorMode));next();break;
case Stage::SetOscillatorKey:key_=2;next();break;
case Stage::RefreshOscillator:child_=Child::Indicator;resume_=Stage::ReadLfoMode;phase_=Stage::AwaitChild;break;
case Stage::ReadLfoMode:value_=uint8_t(read(Read::LfoMode));next();break;
case Stage::ShiftLfo:value_>>=2;next();break;
case Stage::MaskLfo:value_&=1;next();break;
case Stage::SetLfoKey:key_=6;next();break;
case Stage::RefreshLfo:child_=Child::Indicator;resume_=Stage::ReadFilterMode;phase_=Stage::AwaitChild;break;
case Stage::ReadFilterMode:value_=uint8_t(read(Read::FilterMode));next();break;
case Stage::ShiftFilterFirst:value_>>=2;next();break;
case Stage::ShiftFilterSecond:value_>>=2;next();break;
case Stage::MaskFilter:value_&=1;next();break;
case Stage::SetFilterKey:key_=11;next();break;
case Stage::RefreshFilter:child_=Child::Indicator;resume_=Stage::ReadMaskFirst;phase_=Stage::AwaitChild;break;
case Stage::ReadMaskFirst:value_=uint8_t(read(Read::MaskFirst));next();break;
case Stage::SetMaskFirstKey:key_=12;next();break;
case Stage::RefreshMaskFirst:child_=Child::Indicator;resume_=Stage::ReadMaskSecond;phase_=Stage::AwaitChild;break;
case Stage::ReadMaskSecond:value_=uint8_t(read(Read::MaskSecond));next();break;
case Stage::SetMaskSecondKey:key_=13;next();break;
case Stage::RefreshMaskSecond:child_=Child::Indicator;resume_=Stage::ReadPartMode;phase_=Stage::AwaitChild;break;
case Stage::ReadPartMode:value_=uint8_t(read(Read::PartMode));next();break;
case Stage::SetPartKey:key_=17;next();break;
case Stage::RefreshPart:child_=Child::Indicator;resume_=Stage::ReadPageMode;phase_=Stage::AwaitChild;break;
case Stage::ReadPageMode:value_=uint8_t(read(Read::PageMode));next();break;
case Stage::SetPageKey:key_=16;next();break;
case Stage::RefreshPage:child_=Child::Indicator;resume_=Stage::ReadEditMode;phase_=Stage::AwaitChild;break;
case Stage::ReadEditMode:value_=uint8_t(read(Read::EditMode));next();break;
case Stage::SetEditKey:key_=3;next();break;
case Stage::RefreshEdit:child_=Child::Indicator;resume_=Stage::ReadGlobalMode;phase_=Stage::AwaitChild;break;
case Stage::ReadGlobalMode:value_=uint8_t(read(Read::GlobalMode));next();break;
case Stage::SetGlobalKey:key_=10;next();break;
case Stage::RefreshGlobal:child_=Child::Indicator;resume_=Stage::ReadAlternateMode;phase_=Stage::AwaitChild;break;
case Stage::ReadAlternateMode:value_=uint8_t(read(Read::AlternateMode));next();break;
case Stage::SetAlternateKey:key_=20;next();break;
case Stage::RefreshAlternate:child_=Child::Indicator;resume_=Stage::ReadMenuMode;phase_=Stage::AwaitChild;break;
case Stage::ReadMenuMode:value_=uint8_t(read(Read::MenuMode));next();break;
case Stage::SetMenuKey:key_=21;next();break;
case Stage::RefreshMenu:child_=Child::Indicator;resume_=Stage::Return;phase_=Stage::AwaitChild;break;
case Stage::Return:phase_=Stage::Complete;break; default:next();break;}used+=c;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_=Stage::SetCommonKey,resume_=Stage::Complete;Child child_=Child::Common;unsigned index_=0;uint8_t key_=0,value_=0;};}
