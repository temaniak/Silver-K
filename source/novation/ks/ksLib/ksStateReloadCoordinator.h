#pragma once
#include "ksStateMultiLoadTask.h"
#include "ksStatePatchLoadTask.h"
#include "ksStateStorageTask.h"
#include "ksStateModeTask.h"
#include "ksStateGlobalsTask.h"
#include "ksStateParameterExpandTask.h"
#include "ksStatePanelModesTask.h"
#include "ksPanelAdcScanTask.h"
#include "ksPartEnableTask.h"
#include "ksPartControlTasks.h"
#include "ksKeyResetTask.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Four-part reload coordinator. Effects are typed suspension requests consumed
// by the closed StateReloadService, never compatibility calls or code tokens.
class StateReloadCoordinator final {
public:
 enum class Component { Multi,Storage,Mode,Patch,Invalidate,AdcReset,Expand,Semitone,Enable,Lookup,Keys,Effects };
 enum class Request { None,Voices,Signal,Panel };
 struct Effect { bool read=false,write=false; };
 Component component()const{return component_;}
 Request request()const{return request_;}
 bool complete()const{return multi_.complete();}
 uint32_t components()const{return components_;}
 const Effect& effect()const{return effect_;}
 const StateMultiLoadTask& multi()const{return multi_;}
 uint32_t expandedCurrentBase()const{return expandSource_;}
 const StatePatchLoadTask* patch()const{return patch_?&*patch_:nullptr;}
 bool resumeEffects(){
  if(component_!=Component::Effects||request_==Request::None)return false;
  const auto r=request_;request_=Request::None;
  if(r==Request::Panel){multi_.resumeChild();select(Component::Multi);}
  else {patch_->resumeChild();select(Component::Patch);}
  return true;
 }
 unsigned nextClocks(int previousWrite=-1)const{
  if(complete())return 0;
  switch(component_){
  case Component::Multi:return multi_.nextClocks(byteClock(multiWrite(),previousWrite));
  case Component::Storage:return storage_->nextClocks();
  case Component::Mode:return mode_->nextClocks();
  case Component::Patch:return patch_->nextClocks(byteClock(patchWrite(),previousWrite));
  case Component::Invalidate:return invalid_->nextClocks(byteClock(invalidBase_+invalid_->byteOffset(),previousWrite));
  case Component::AdcReset:return adc_->nextClocks();
  case Component::Expand:return expand_->nextClocks(byteClock(expandTarget_+expand_->targetOffset(),previousWrite));
  case Component::Semitone:return semitone_->nextClocks(byteClock(semitone_->stage()==StatePanelModesTask::Stage::WriteOldSemitone?0x1678e:0x1678d,previousWrite));
  case Component::Enable:return enable_->nextClocks([&](PartEnableTask::Write w){return byteClock(enableWrite(w),previousWrite);});
  case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();
  case Component::Keys:return keys_->complete()?17:keys_->nextClocks(keys_->publication()?byteClock(keyBase_+keys_->publicationOffset(),previousWrite):3);
  case Component::Effects:return 0;
  }return 0;
 }
 template<class Reader,class Writer>
 unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;
  effect_={};
  const auto read16=[&](uint32_t a){return uint16_t((uint16_t(read(a&0xffffff,cost))<<8)|uint8_t(read((a+1)&0xffffff,cost)));};
  const auto read32=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read((a+n)&0xffffff,cost-3*((3-n)/2)));return v;};
  const auto write16=[&](uint32_t a,uint16_t v){write(a&0xffffff,uint8_t(v>>8),cost);write((a+1)&0xffffff,uint8_t(v),cost);};
  const auto write32=[&](uint32_t a,uint32_t v){for(unsigned n=0;n<4;++n)write((a+n)&0xffffff,uint8_t(v>>(24-8*n)),cost-3*((3-n)/2));};
  switch(component_){
  case Component::Multi:{using T=StateMultiLoadTask;using S=T::Stage;const auto stage=multi_.stage();
   multi_.advanceBefore(cost,[&](T::Read r)->uint32_t{
    const uint32_t a=r==T::Read::Selection?source_+189+multi_.selection():r==T::Read::GlobalField?source_+multi_.sourceGlobalOffset():r==T::Read::PartBank?source_+256*multi_.part()+187:r==T::Read::PartProgram?source_+256*multi_.part()+188:r==T::Read::VoiceMode?0x169fc+227:0x167b8+multi_.selection();
    return read(a&0xffffff,cost);
   },[&](T::Write r,uint32_t v){if(r==T::Write::ProgramNumber)write16(0x167b0+2*multi_.part(),uint16_t(v));else write(multiWrite()&0xffffff,uint8_t(v),cost);},byteClock(multiWrite(),previousWrite));
   effect_.write=stage==S::RetainSource||stage==S::RetainProgramTable||stage==S::RetainPart||stage==S::DispatchLoad;
   effect_.read=stage==S::RestorePart||stage==S::RestoreProgramTable||stage==S::RestoreSource||stage==S::ReturnResolved||stage==S::ReturnLoaded;
   if(multi_.awaitingChild()){
    effect_.write=true;
    switch(multi_.child()){
    case T::Child::Source:storage_.emplace(StateStorageTask::Kind::MultiAddress);select(Component::Storage);break;
    case T::Child::Mode:case T::Child::RestoreMode:mode_.emplace();select(Component::Mode);break;
    case T::Child::Part:patchSource_=source_+256*multi_.part();patch_.emplace(StatePatchLoadTask::Entry::Provided);select(Component::Patch);break;
    case T::Child::VoiceMode:semitoneOwner_=Component::Multi;semitone_.emplace(StatePanelModesTask::Kind::Semitone,uint16_t(multi_.voiceMode())<<8);select(Component::Semitone);break;
    case T::Child::Panel:request_=Request::Panel;select(Component::Effects);break;
    }
   }break;}
  case Component::Storage:{using T=StateStorageTask;using S=T::Stage;const auto stage=storage_->stage();
   storage_->advanceBefore(cost,[&](T::Read r){return read(r==T::Read::Bank?0x167ab:r==T::Read::Mode?0x167ba:r==T::Read::Dirty?0x1679a:0x17526,cost);},[&](T::Write r,uint32_t v){write32(r==T::Write::CurrentBase?0x167bc:0x167c0,v);});
   if(storage_->complete()){
    effect_.read=true;
    if(storage_->kind()==T::Kind::MultiAddress){source_=storage_->address();multi_.resumeChild();select(Component::Multi);}
    else {mode_->resumePart();select(Component::Mode);}
    storage_.reset();
   }else if(stage==S::BindPartChild)throw std::logic_error("Reload storage kind unexpectedly requested a child");
   break;}
  case Component::Mode:{using T=StateModeTask;using S=T::Stage;const auto stage=mode_->stage();
   mode_->advanceBefore(cost,[&](T::Read r)->uint32_t{
    if(r==T::Read::CurrentProgram||r==T::Read::SingleSlot||r==T::Read::SlotProgram)return read16(r==T::Read::CurrentProgram?0x167ac:r==T::Read::SingleSlot?0x167ae:0x167ae + mode_->slotOffset());
    return read(r==T::Read::Mode?0x167ba:r==T::Read::PreviousMode?0x167bb:r==T::Read::PartMask?0x167d0:r==T::Read::AuxMask?0x167d1:r==T::Read::Active?0x16798:0x167d4,cost);
   },[&](T::Write r,uint32_t v){
    switch(r){case T::Write::CurrentBase:write32(0x167bc,v);return;case T::Write::LiveBase:write32(0x167c0,v);return;case T::Write::SingleSlot:write16(0x167ae,uint16_t(v));return;case T::Write::CurrentProgram:write16(0x167ac,uint16_t(v));return;case T::Write::SlotProgram:write16(0x167ae + mode_->slotOffset(),uint16_t(v));return;default:break;}
    write(r==T::Write::Active?0x16798:r==T::Write::PartMask?0x167d2:r==T::Write::AuxMask?0x167d3:r==T::Write::PreviousMode?0x167bb:0x167d4,uint8_t(v),cost);
   });
   effect_.write=stage==S::RetainAddress||stage==S::RetainSlot||stage==S::RetainMode||stage==S::BindPart;
   effect_.read=stage==S::RestoreMode||stage==S::RestoreSlot||stage==S::RestoreAddress||stage==S::ReturnMode;
   if(mode_->awaitingPart()){storage_.emplace(StateStorageTask::Kind::BindPart,mode_->part());select(Component::Storage);}
   else if(mode_->complete()){mode_.reset();multi_.resumeChild();select(Component::Multi);}
   break;}
  case Component::Patch:{using T=StatePatchLoadTask;using S=T::Stage;const auto stage=patch_->stage();
   patch_->advanceBefore(cost,[&](T::Read r)->uint32_t{
    if(r==T::Read::Target)return patchTarget_=checkedData(read32(0x167bc),209);
    if(r==T::Read::Byte)return read((patchSource_+patch_->byteOffset())&0xffffff,cost-2);
    return read(0x16798,cost);
   },[&](T::Write,uint8_t v){write(patchWrite()&0xffffff,v,cost);},byteClock(patchWrite(),previousWrite));
   effect_.read=stage==S::ReturnLoad;
   if(patch_->awaitingChild()){
    effect_.write=true;
    switch(patch_->child()){
    case T::Child::Source:throw std::logic_error("Provided reload patch unexpectedly requested a source");
    case T::Child::InvalidateLive:invalid_.emplace(StateGlobalsTask::Kind::InvalidateLive);select(Component::Invalidate);break;
    case T::Child::ExpandFields:expand_.emplace(StateParameterExpandTask::Entry::Refresh);select(Component::Expand);break;
    case T::Child::RefreshPart:enablePart_=patch_->part();enable_.emplace();select(Component::Enable);break;
    case T::Child::RefreshVoices:request_=Request::Voices;select(Component::Effects);break;
    case T::Child::RefreshSerial:keys_.emplace(KeyResetTask::Kind::Held);select(Component::Keys);break;
    case T::Child::RefreshPerformance:request_=Request::Signal;select(Component::Effects);break;
    case T::Child::Context:lookupOwner_=Component::Patch;lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);break;
    }
   }else if(patch_->complete()){patch_.reset();multi_.resumeChild();select(Component::Multi);}
   break;}
  case Component::Invalidate:{using T=StateGlobalsTask;
   invalid_->advanceBefore(cost,[&](T::Read){return invalidBase_=checkedData(read32(0x167c0),255);},[&](T::Write,uint8_t v){write((invalidBase_+invalid_->byteOffset())&0xffffff,v,cost);},byteClock(invalidBase_+invalid_->byteOffset(),previousWrite));
   if(invalid_->awaitingInvalid()){effect_.write=true;adc_.emplace(PanelAdcScanTask::Kind::ResetLast);select(Component::AdcReset);}
   else if(invalid_->complete()){effect_.read=true;invalid_.reset();patch_->resumeChild();select(Component::Patch);}
   break;}
  case Component::AdcReset:{using T=PanelAdcScanTask;adc_->advanceBefore(cost,[](T::Read){return 0u;},[&](T::Write,uint8_t v){write(0x156e4,v,cost);});
   if(adc_->complete()){effect_.read=true;adc_.reset();invalid_->resumeInvalid();select(Component::Invalidate);}break;}
  case Component::Expand:{using T=StateParameterExpandTask;using S=T::Stage;const auto stage=expand_->stage();
   expand_->advanceBefore(cost,[&](T::Read r)->uint32_t{if(r==T::Read::CurrentBase)return expandSource_=checkedData(read32(0x167bc),228);if(r==T::Read::LiveBase)return expandTarget_=checkedData(read32(0x167c0),228);if(r==T::Read::Mode)return read(0x167ba,cost);return read((expandSource_+expand_->sourceOffset())&0xffffff,cost);},[&](unsigned offset,uint8_t v){write((expandTarget_+offset)&0xffffff,v,cost);},byteClock(expandTarget_+expand_->targetOffset(),previousWrite));
   if(stage==S::CaptureCurrent)expandTarget_=expandSource_;
   effect_.write=stage==S::DispatchFirst||stage==S::RetainRequest||stage==S::DispatchSigned;
   effect_.read=stage==S::RestoreRequest||stage==S::ReturnExpansion;
   if(expand_->awaitingSigned()){semitoneOwner_=Component::Expand;semitone_.emplace(StatePanelModesTask::Kind::Semitone,uint16_t(expand_->signedMode())<<8);select(Component::Semitone);}
   else if(expand_->complete()){expand_.reset();patch_->resumeChild();select(Component::Patch);}
   break;}
  case Component::Semitone:{using T=StatePanelModesTask;semitone_->advanceBefore(cost,[&](T::Read r,uint32_t){return read(r==T::Read::Type?0x158f2:0x1678d,cost);},[&](T::Write r,uint32_t,uint32_t v){write(r==T::Write::PreviousSemitone?0x1678e:0x1678d,uint8_t(v),cost);},byteClock(semitone_->stage()==T::Stage::WriteOldSemitone?0x1678e:0x1678d,previousWrite));
   if(semitone_->complete()){effect_.read=true;semitone_.reset();if(semitoneOwner_==Component::Expand){expand_->resumeSigned();select(Component::Expand);}else {multi_.resumeChild();select(Component::Multi);}}break;}
  case Component::Enable:{using T=PartEnableTask;const auto stage=enable_->stage();enable_->advanceBefore(cost,[&](T::Read r){return r==T::Read::Input?enablePart_:uint8_t(read(0x174ae + 20*enable_->part()+18,cost));},[&](T::Write r,uint8_t v){write(enableWrite(r)&0xffffff,v,cost);},[&](T::Write r){return byteClock(enableWrite(r),previousWrite);});
   if(enable_->awaiting()){effect_.write=true;lookupOwner_=Component::Enable;lookup_.emplace(PartLookupTask::Layout::Controls);select(Component::Lookup);}
   else if(stage==T::Stage::Finish){effect_.read=true;enable_.reset();patch_->resumeChild();select(Component::Patch);}break;}
  case Component::Lookup:{using T=PartLookupTask;
   if(lookup_->stage()==T::Stage::Complete){effect_.read=true;const auto part=lookup_->index();lookup_.reset();
    if(lookupOwner_==Component::Enable){enable_->resumeChild(part);select(Component::Enable);}
    else if(lookupOwner_==Component::Keys){keys_->resumePart(part);keyBase_=0x160d2+334*part;select(Component::Keys);}
    else {patchContext_=0x160d2+334*part;patch_->resumeChild();select(Component::Patch);}
   }else lookup_->advanceBefore(cost,[&](T::Layout){return read(0x17526,cost);});
   break;}
  case Component::Keys:{using T=KeyResetTask;using S=T::Stage;
   if(keys_->complete()){effect_.read=true;keys_.reset();patch_->resumeChild();select(Component::Patch);break;}
   const auto stage=keys_->stage();keys_->advanceBefore(cost,[&](unsigned offset,uint8_t v){write((keyBase_+offset)&0xffffff,v,cost-(stage==S::ClearByte?2:0));},[&](unsigned offset){return byteClock(keyBase_+offset,previousWrite);});
   effect_.write=stage==S::RetainPart||stage==S::RetainBitmap||stage==S::RetainCount||stage==S::DispatchPart;
   effect_.read=stage==S::ReleaseCount||stage==S::ReleaseBitmap||stage==S::ReleasePart;
   if(keys_->awaitingPart()){lookupOwner_=Component::Keys;lookup_.emplace(PartLookupTask::Layout::Parameters);select(Component::Lookup);}break;}
  case Component::Effects:break;
  }
  return cost;
 }
private:
 static uint32_t checkedData(uint32_t raw,unsigned count){
  const auto a=raw&0xffffff;
  const bool supported=count&&((a>=0xb8e8&&a<0x200000&&count<=0x200000-a)||(a>=0xa0b8e8&&a<0xc00000&&count<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&count<=0xfff000-a));
  if(!supported)throw std::logic_error("Unsupported evolving reload data span");
  return raw;
 }
 static unsigned byteClock(uint32_t a,int previous){a&=0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1)?0u:3u;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 uint32_t multiWrite()const{using S=StateMultiLoadTask::Stage;switch(multi_.stage()){
 case S::WriteSelection:return 0x167b8+multi_.selection();case S::WriteGlobalField:return 0x168fc+multi_.targetGlobalOffset();case S::WriteRestoredSelection:return 0x167d0+multi_.selection();case S::WriteVoiceMode:return 0x168fc+227;case S::WriteActiveMask:return 0x167d0;case S::WritePreviousMode:return 0x167bb;case S::WritePreviousPart:return 0x167d4;case S::WriteDirtyState:return 0x16783;case S::WriteGateState:return 0x167a3;case S::WriteBankState:return 0x1679e;default:return 0x167a0;}}
 uint32_t patchWrite()const{return patch_->stage()==StatePatchLoadTask::Stage::WritePending?patchContext_+310:patchTarget_+patch_->byteOffset();}
 uint32_t enableWrite(PartEnableTask::Write w)const{return w==PartEnableTask::Write::Part?0x17526:0x174ae + 20*enable_->part()+(w==PartEnableTask::Write::Enable?17:18);}
 StateMultiLoadTask multi_{StateMultiLoadTask::Entry::Selected};
 std::optional<StateStorageTask>storage_;std::optional<StateModeTask>mode_;std::optional<StatePatchLoadTask>patch_;
 std::optional<StateGlobalsTask>invalid_;std::optional<PanelAdcScanTask>adc_;std::optional<StateParameterExpandTask>expand_;
 std::optional<StatePanelModesTask>semitone_;std::optional<PartEnableTask>enable_;std::optional<PartLookupTask>lookup_;std::optional<KeyResetTask>keys_;
 Component component_=Component::Multi,semitoneOwner_=Component::Multi,lookupOwner_=Component::Patch;Request request_=Request::None;Effect effect_{};uint32_t components_=1;
 uint32_t source_=0,patchSource_=0,patchTarget_=0,patchContext_=0,invalidBase_=0,expandSource_=0,expandTarget_=0,keyBase_=0;uint8_t enablePart_=0;
};
}
