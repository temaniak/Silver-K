#pragma once
#include "ksFlashPageTask.h"
#include <optional>
namespace ks {
// A page transaction is an indivisible typed graph, including both live
// comparisons. The caller applies the transient write priority to its scheduler.
class StateSavePageService final {
public:
 enum class Kind {Program,Compare};
 enum class Component {Program,Compare,Verify};
 enum class Decision {PageBytes,FlashRange,Protection,SkipProtected,Elapsed,Timeout,Ready};
 struct Input{uint32_t source=0,target=0;};
 explicit StateSavePageService(Kind k,Input i):kind_(k),source_(i.source),target_(i.target){if(k==Kind::Program)program_.emplace(i.source,i.target);else compare_.emplace(i.source,i.target);}
 Kind kind()const{return kind_;}Component component()const{return compare_?(verify_?Component::Verify:Component::Compare):Component::Program;}
 uint32_t components()const{return components_;}bool complete()const{return complete_;}uint32_t source()const{return source_;}uint32_t target()const{return target_;}bool writeWindow()const{return writeWindow_;}
 Decision decision()const{return decision_;}uint32_t left()const{return left_;}uint32_t right()const{return right_;}
 const FlashPageProgramTask*programTask()const{return !compare_&&program_?&*program_:nullptr;}const FlashPageCompareTask*compareTask()const{return compare_?&*compare_:nullptr;}
 bool endsReadTransfer()const{if(compare_){using S=FlashPageCompareTask::Stage;switch(compare_->stage()){case S::DiscardSource:case S::DiscardTarget:case S::RestoreSource:case S::RestoreTarget:case S::ReleaseContext:case S::Return:return true;default:return false;}}if(program_){using S=FlashPageProgramTask::Stage;switch(program_->stage()){case S::ReleaseClock:case S::ReleaseByte:case S::ReleaseCommand:case S::ReleaseContext:case S::Return:case S::LeaveWriteWindow:return true;default:return false;}}return false;}
 bool endsWriteTransfer()const{if(compare_){using S=FlashPageCompareTask::Stage;return compare_->stage()==S::AdmitContext||compare_->stage()==S::AdmitTarget||compare_->stage()==S::AdmitSource;}if(program_){using S=FlashPageProgramTask::Stage;switch(program_->stage()){case S::AdmitContext:case S::AdmitCommand:case S::AdmitByte:case S::AdmitClock:case S::ComparePage:case S::VerifyPage:case S::EnterWriteWindow:return true;default:return false;}}return false;}
 unsigned nextClocks(int previousWrite=-1)const{return complete_?0:compare_?compare_->nextClocks():program_->nextClocks(byteCost(previousWrite));}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;components_|=1u<<unsigned(component());
  auto word=[&](uint32_t a){return uint16_t((unsigned(uint8_t(read(a,cost-3)))<<8)|uint8_t(read(a+1,cost)));};
  auto longword=[&](uint32_t a){return(uint32_t(uint8_t(read(a,cost-9)))<<24)|(uint32_t(uint8_t(read(a+1,cost-6)))<<16)|(unsigned(uint8_t(read(a+2,cost-3)))<<8)|uint8_t(read(a+3,cost));};
  if(compare_){auto&t=*compare_;t.advanceBefore(cost,[&](bool target,unsigned position){return uint8_t(read(((target?(t.target()&0xffff00):t.source())+position)&0xffffff,cost-2));});
   if(t.stage()==FlashPageCompareTask::Stage::AdmitDifference){decision_=Decision::PageBytes;left_=t.targetByte();right_=t.sourceByte();}
   if(t.complete()){source_=t.source();target_=t.target();compare_.reset();if(program_)program_->resumeCompare(source_,target_);else complete_=true;}
  }else{auto&t=*program_;using S=FlashPageProgramTask::Stage;const auto stage=t.stage();
   if(stage==S::Offset){decision_=Decision::FlashRange;left_=t.target();right_=0x800000;}
   if(stage==S::CompareProtected){decision_=Decision::Protection;left_=t.target()-0x800000u;right_=0x4000;}
   if(stage==S::SkipTarget){decision_=Decision::SkipProtected;left_=t.target();right_=256;}
   if(stage==S::Elapsed){decision_=Decision::Elapsed;left_=uint8_t(t.currentTick());right_=uint8_t(t.startTick());}
   if(stage==S::CompareTimeout){decision_=Decision::Timeout;left_=t.elapsed();right_=15;}
   if(stage==S::CompareReady){decision_=Decision::Ready;left_=t.byte();right_=t.ready();}
   if(stage==S::RaiseWritePriority)writeWindow_=true;if(stage==S::LeaveWriteWindow)writeWindow_=false;
   t.advanceBefore(cost,[&](FlashPageProgramTask::Read what,unsigned position)->uint32_t{using R=FlashPageProgramTask::Read;switch(what){case R::Pending:return word(0xffed6a);case R::Source:return read((t.source()+position)&0xffffff,cost-2);case R::StartTick:case R::CurrentTick:return longword(0xffec10);case R::Ready:return read((t.target()+position)&0xffffff,cost);}return 0;},[&](FlashPageProgramTask::Write what,uint16_t value,unsigned position){using W=FlashPageProgramTask::Write;switch(what){case W::Pending:write(0xffed6a,uint8_t(value>>8),cost-3);write(0xffed6b,uint8_t(value),cost);break;case W::FirstCommand:case W::ProgramCommand:write(t.bank()+0x5555,uint8_t(value),cost);break;case W::SecondCommand:write(t.bank()+0x2aaa,uint8_t(value),cost);break;case W::Byte:write((t.target()+position)&0xffffff,uint8_t(value),cost);break;}},byteCost(previousWrite));
   if(t.awaitingCompare()){verify_=stage==S::VerifyPage;compare_.emplace(t.source(),t.target());}else if(t.complete()){source_=t.source();target_=t.target();complete_=true;}
  }
  return cost;
 }
private:
 unsigned byteCost(int previous)const{const auto&t=*program_;if(t.stage()!=FlashPageProgramTask::Stage::WriteTarget)return 3;const auto address=(t.target()+t.position())&0xffffff;return address<0x400000&&!(previous&1)&&address==(uint32_t(previous)|1u)?0:3;}
 Kind kind_;Decision decision_=Decision::PageBytes;uint32_t left_=0,right_=0,source_=0,target_=0,components_=0;bool complete_=false,verify_=false,writeWindow_=false;std::optional<FlashPageProgramTask>program_;std::optional<FlashPageCompareTask>compare_;
};
}

#pragma once
#include "ksStateStoreUiTask.h"
#include "ksStatePresetSaveTask.h"
#include "ksStatePresetFormatTask.h"
#include "ksStateStoreServicesTask.h"
#include "ksStateFlashCommitTask.h"
#include "ksFlashPageTask.h"
#include "ksStateMultiSaveTask.h"
#include "ksStateGlobalSaveTask.h"
#include "ksStateProgramTask.h"
#include "ksStateStoreService.h"
#include "ksStateDisplayService.h"
#include "ksPanelInitializeTask.h"
#include "ksStateBankNavigationTask.h"
#include "ksScheduledTimerTask.h"
#include "ksTimerServiceTask.h"
#include "ksStateReloadService.h"
#include <array>
#include <optional>
#include <variant>
#include <stdexcept>
namespace ks {
// The complete commit transaction retains typed continuations and data
// resources. Flash transfers and their write window remain individually
// observable; no guest execution frame is present in this graph.
class StateSaveService final {
public:
 enum class Kind {CommitMulti,CommitSingle,PresetSave,FormatDirect,FormatPart,Offset,Check,Message,FlashCommit,GlobalSave,MultiCurrent,MultiProvided,FlashProgram,FlashCompare};
 enum class LastDecision {Entry,Page,ProgramBank,ProgramAlias,FormatIndex,LcdReply,SourceAdvance};
 struct Summary {
  bool packed=false,named=false,formatted=false,formatCopied=false,preset=false,alias=false,multi=false,multiAccepted=false,global=false,notice=false,checked=false,offset=false,page=false,reloaded=false;
  uint32_t packingBase=0,packingOriginal=0,packingField=0,nameStorage=0,formatSourceAfter=0,pageSource=0,pageTarget=0,multiSource=0,multiTarget=0,multiSourceBeforeAdvance=0;
  uint32_t reloadCurrent=0,reloadResolvedBase=0,reloadSelectorReference=0,reloadExpandedBase=0;uint16_t reloadBounds=0,commitPending=0;
  uint16_t packingProgram=0,presetProgram=0,formatProgram=0,multiProgram=0,formatIndex=0;uint8_t packingValue=0,packingContributor=0,formatLastByte=0,writeFlag=0,checkFlag=0,marker=0;
  StateDisplayService::LcdResult lcd{};StateSavePageService::Decision pageDecision=StateSavePageService::Decision::PageBytes;uint32_t pageLeft=0,pageRight=0;
  LastDecision decision=LastDecision::Entry;
 };
 const Summary& summary()const{return summary_;}
 enum class Component {Ui,Preset,Format,Index,Offset,Commit,Program,Compare,Multi,Global,Preparation,Display,Timer,Clock,Reset,Resolve,Reload};
 struct Input {uint32_t source=0,program=0,target=0;ScheduledTimerTask::ActionHandle noticeAction{},saveAction{};};
 struct Effect {bool read=false,write=false,enterWrite=false,leaveWrite=false;};
 explicit StateSaveService(Kind k):StateSaveService(k,Input{}){}
 StateSaveService(Kind k,Input in):kind_(k),input_(in){switch(k){
 case Kind::CommitMulti:push(StateStoreUiTask(StateStoreUiTask::Kind::CommitMulti));break;
 case Kind::CommitSingle:push(StateStoreUiTask(StateStoreUiTask::Kind::CommitSingle));break;
 case Kind::PresetSave:push(StatePresetSaveTask{});break;
 case Kind::FormatDirect:push(StatePresetFormatTask(StatePresetFormatTask::Entry::Direct),in);break;
 case Kind::FormatPart:push(StatePresetFormatTask(StatePresetFormatTask::Entry::PartAware),in);break;
 case Kind::Offset:push(StateStoreServicesTask(StateStoreServicesTask::Kind::Offset),in);break;
 case Kind::Check:push(StateStoreServicesTask(StateStoreServicesTask::Kind::Check),in);break;
 case Kind::Message:push(StateStoreServicesTask(StateStoreServicesTask::Kind::Message),in);break;
 case Kind::FlashCommit:push(StateFlashCommitTask{},in);break;
 case Kind::GlobalSave:push(StateGlobalSaveTask{},Input{0x169e6,0,0x81ff00,in.noticeAction,in.saveAction});break;
 case Kind::MultiCurrent:push(StateMultiSaveTask(StateMultiSaveTask::Entry::Current));break;
 case Kind::MultiProvided:push(StateMultiSaveTask(StateMultiSaveTask::Entry::Provided),in);break;
 case Kind::FlashProgram:push(StateSavePageService(StateSavePageService::Kind::Program,{in.source,in.target}),in);break;
 case Kind::FlashCompare:push(StateSavePageService(StateSavePageService::Kind::Compare,{in.source,in.target}),in);break;
 }}
 Kind kind()const{return kind_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return !depth_;}unsigned depth()const{return depth_;}
 const Effect& effect()const{return effect_;}unsigned writePriority()const{return writeWindow_?6:0;}
 uint32_t source()const{return result_.source;}uint32_t target()const{return result_.target;}uint16_t program()const{return program_;}uint16_t index()const{return index_;}uint32_t current()const{return current_;}uint32_t tick()const{return tick_;}uint8_t value()const{return value_;}uint8_t mode()const{return mode_;}bool allowed()const{return allowed_;}
 template<class T>const T* task()const{if(!depth_)return nullptr;if constexpr(std::is_same_v<T,FlashPageProgramTask>||std::is_same_v<T,FlashPageCompareTask>){const auto*p=std::get_if<StateSavePageService>(&frames_[depth_-1]->task);if(!p)return nullptr;if constexpr(std::is_same_v<T,FlashPageProgramTask>)return p->programTask();else return p->compareTask();}else return std::get_if<T>(&frames_[depth_-1]->task);}
 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}
 unsigned nextClocks(int previousWrite=-1)const{
  if(!depth_)return 0;const auto&f=*frames_[depth_-1];
  return std::visit([&](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,StateStoreService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,StateReloadService>)return t.nextClocks(previousWrite);
   else if constexpr(std::is_same_v<T,StateStoreUiTask>)return t.nextClocks(byteClocks(uiTarget(t),previousWrite));
   else if constexpr(std::is_same_v<T,StatePresetSaveTask>)return t.nextClocks(byteClocks(0x167ad,previousWrite));
   else if constexpr(std::is_same_v<T,StatePresetFormatTask>)return t.nextClocks(byteClocks(0x167fc + t.storeOffset(),previousWrite),wordClocks(f.input.program));
   else if constexpr(std::is_same_v<T,StateMultiSaveTask>)return t.nextClocks(byteClocks(0x167fc + t.byteOffset(),previousWrite),wordClocks(f.input.program));
   else if constexpr(std::is_same_v<T,StateGlobalSaveTask>)return t.nextClocks(byteClocks(globalTarget(t),previousWrite));
   else if constexpr(std::is_same_v<T,StateSavePageService>)return t.nextClocks(previousWrite);
   else if constexpr(std::is_same_v<T,StateBankNavigationTask>)return t.nextClocks(byteClocks(t.stage()==T::Stage::WritePartMarker?0x167d6:0x16791,previousWrite));
   else return t.nextClocks();
  },f.task);
 }
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolve,int previousWrite=-1){
  const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;effect_={};auto&f=*frames_[depth_-1];
  auto byte=[&](uint32_t a,unsigned end=0){return uint8_t(read(a&0xffffff,end?end:cost));};
  auto word=[&](uint32_t a){const auto gap=byteClocks(a+1,history(a));return uint16_t((unsigned(byte(a,cost-gap))<<8)|byte(a+1));};
  auto longword=[&](uint32_t a){const bool paired=(a&0xffffff)<0x400000;return(uint32_t(byte(a,cost-(paired?3:9)))<<24)|(uint32_t(byte(a+1,cost-(paired?3:6)))<<16)|(unsigned(byte(a+2,cost-(paired?0:3)))<<8)|byte(a+3);};
  auto putWord=[&](uint32_t a,uint16_t v){const auto gap=byteClocks(a+1,history(a));write(a,uint8_t(v>>8),cost-gap);write(a+1,uint8_t(v),cost);};
  auto putLong=[&](uint32_t a,uint32_t v){write(a,uint8_t(v>>24),cost-3);write(a+1,uint8_t(v>>16),cost-3);write(a+2,uint8_t(v>>8),cost);write(a+3,uint8_t(v),cost);};
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
   if constexpr(std::is_same_v<T,StateSavePageService>){
    effect_.read=t.endsReadTransfer();effect_.write=t.endsWriteTransfer();t.advanceBefore(cost,read,write,previousWrite);writeWindow_=t.writeWindow();
    if(t.complete()){summary_.page=true;summary_.pageSource=t.source();summary_.pageTarget=t.target();summary_.pageDecision=t.decision();summary_.pageLeft=t.left();summary_.pageRight=t.right();summary_.decision=LastDecision::Page;finish(Input{t.source(),0,t.target()});}
   }else if constexpr(std::is_same_v<T,StateFlashCommitTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::DispatchProgram||stage==S::RetainSource||stage==S::RetainTarget||stage==S::DispatchIdle;effect_.read=stage==S::RestoreSource||stage==S::RestoreTarget||stage==S::ReturnCommit;
    t.advanceBefore(cost,[&]{return summary_.commitPending=word(0xffed6a);},[&](uint16_t v){putWord(0xffed6a,v);});
    if(t.awaitingChild()){if(t.child()==T::Child::Program)push(StateSavePageService(StateSavePageService::Kind::Program,{f.input.source,f.input.target}),f.input);else push(PanelInitializeTask(PanelInitializeTask::Kind::Wake));}else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,StateStoreServicesTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::RetainSource||stage==S::RetainTarget||stage==S::DispatchMessage||stage==S::DispatchCheckMessage;effect_.read=stage==S::RestoreSource||stage==S::RestoreTarget||stage==S::ReturnOffset||stage==S::ReturnCheck;
    if(stage==S::SetFlagBase)summary_.offset=true;if(stage==S::AdvanceSource)f.input.source+=256;if(stage==S::AdvanceTarget)f.input.target+=256;
    t.advanceBefore(cost,[&]{value_=byte(0x169eb);summary_.writeFlag=value_;if(stage==S::ReadCheckFlag){summary_.checked=true;summary_.checkFlag=value_;}allowed_=value_!=0;return value_;});
    if(t.awaitingMessage())push(StateStoreServicesTask(T::Kind::Message));
    else if(t.awaitingTail()){if(t.service()==T::Service::Program)push(StateFlashCommitTask{},f.input);else push(StateBankNavigationTask(StateBankNavigationTask::Kind::Notice),Input{0x14ea6});}
    else if(t.complete()){if(stage==S::ReturnOffset){summary_.pageSource=f.input.source;summary_.pageTarget=f.input.target;}finish(f.input);}
   }else if constexpr(std::is_same_v<T,StatePresetFormatTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::GetDirectIndex||stage==S::GetPartIndex;effect_.read=stage==S::ReturnDirectRejected||stage==S::ReturnPartRejected;
    t.advanceBefore(cost,[&](typename T::Read what)->uint32_t{switch(what){case T::Read::Program:summary_.formatted=true;summary_.formatSourceAfter=0;return summary_.formatProgram=program_=word(f.input.program);case T::Read::Provided:{const bool increment=stage!=S::ReadMiddle&&stage!=S::ReadSharedExtra;summary_.formatSourceAfter=f.input.source+t.byteOffset()+(increment?1:0);summary_.formatCopied=true;return summary_.formatLastByte=byte(f.input.source+t.byteOffset(),increment?cost-2:cost);}case T::Read::Existing:summary_.formatCopied=true;return summary_.formatLastByte=byte(t.pageAddress()+t.byteOffset(),cost-2);case T::Read::Flags:return byte(0x167fc);case T::Read::Mode:return byte(0x1687a);}return 0;},[&](unsigned off,uint8_t v){if(stage==S::WriteZero||stage==S::WritePartZero)summary_.formatLastByte=0;if(stage==S::WriteSharedExtra)summary_.formatLastByte=v;write(0x167fc + off,v,cost);},byteClocks(0x167fc + t.storeOffset(),previousWrite),wordClocks(f.input.program));
    if(stage==S::CompareDirectIndex||stage==S::ComparePartIndex){summary_.decision=LastDecision::FormatIndex;summary_.formatIndex=index_;}
    if(t.awaitingIndex())push(StateProgramTask(StateProgramTask::Kind::ReadIndex),Input{0,f.input.program});
    else if(t.awaitingStore())push(StateStoreServicesTask(StateStoreServicesTask::Kind::Offset),Input{0x167fc,0,t.pageAddress()});
    else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,StateProgramTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::RetainBank||stage==S::RetainExtra;effect_.read=stage==S::RestoreBank||stage==S::RestoreExtra||stage==S::ReturnIndex;
    t.advanceBefore(cost,[&](typename T::Read what){return byte(f.input.program+(what==T::Read::Program?1:0),stage==S::ReadBank?cost-2:cost);},[](typename T::Write,uint8_t){});
    if(t.complete()){index_=t.index();finish(f.input);}
   }else if constexpr(std::is_same_v<T,StatePresetSaveTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::PackFields||stage==S::StorePage||stage==S::StoreAlias||stage==S::RetainProgram||stage==S::ScheduleTimer;effect_.read=stage==S::RestoreProgram||stage==S::ReturnSave;
    t.advanceBefore(cost,[&](typename T::Read what)->uint32_t{if(what==T::Read::Program){summary_.preset=true;return summary_.presetProgram=program_=word(0x167ac);}current_=longword(0x167bc);if(!dataSpan(current_,228))throw std::logic_error("Unsupported evolving preset save resource");return current_;},[&](typename T::Write what,uint16_t v){if(what==T::Write::Alias)putWord(0x167ac,v);else write(0x167ad,uint8_t(v),cost);},byteClocks(0x167ad,previousWrite));
    if(stage==S::StoreAlias)summary_.alias=true;if(stage==S::CompareBank)summary_.decision=LastDecision::ProgramBank;if(stage==S::CompareAlias)summary_.decision=LastDecision::ProgramAlias;
    if(t.awaitingChild()){switch(t.child()){case T::Child::Pack:push(StateStoreService(StateStoreService::Kind::PackAll));break;case T::Child::Store:case T::Child::Alias:push(StatePresetFormatTask(StatePresetFormatTask::Entry::PartAware),Input{current_,0x167ac});break;case T::Child::Timer:startTimer(2,250,input_.saveAction);break;}}
    else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,StateMultiSaveTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::SerializeCurrent||stage==S::StoreCurrent||stage==S::RetainSource||stage==S::RetainPage||stage==S::ProgramPage;effect_.read=stage==S::RestoreSource||stage==S::RestorePage||stage==S::ReturnMulti||stage==S::ReturnCurrent;
    t.advanceBefore(cost,[&](typename T::Read what)->uint32_t{switch(what){case T::Read::Enabled:return summary_.writeFlag=byte(0x169eb);case T::Read::Program:summary_.multi=true;summary_.multiSource=f.input.source;return summary_.multiProgram=program_=word(f.input.program);case T::Read::Byte:return byte(f.input.source+t.pageNumber()*256+t.byteOffset(),cost-2);}return 0;},[&](unsigned off,uint8_t v){write(0x167fc + off,v,cost);},byteClocks(0x167fc + t.byteOffset(),previousWrite),wordClocks(f.input.program));
    if(stage==S::CaptureTarget){f.input.target=t.pageAddress();summary_.multiAccepted=true;summary_.multiTarget=f.input.target;}if(stage==S::AdvancePageSource){summary_.multiSourceBeforeAdvance=f.input.source+t.pageNumber()*256;summary_.decision=LastDecision::SourceAdvance;}
    if(t.awaitingMessage())push(StateStoreServicesTask(StateStoreServicesTask::Kind::Message));
    else if(t.awaitingChild()){switch(t.child()){case T::Child::Serialize:push(StateStoreService(StateStoreService::Kind::MultiSerialize));break;case T::Child::Store:push(StateMultiSaveTask(T::Entry::Provided),Input{0x169fc,0x167aa});break;case T::Child::Program:push(StateSavePageService(StateSavePageService::Kind::Program,{0x167fc,f.input.target}),Input{0x167fc,0,f.input.target});break;}}
    else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,StateGlobalSaveTask>){using S=typename T::Stage;const auto stage=t.stage();summary_.global=true;
    t.advanceBefore(cost,[&](typename T::Read what)->uint32_t{switch(what){case T::Read::Enabled:return summary_.writeFlag=byte(0x169eb);case T::Read::Part:return byte(0x167ba);case T::Read::Single:return word(0x167ae);case T::Read::Multi:return word(0x167aa);case T::Read::Byte:return byte(0x169e6+t.byteOffset(),cost-2);}return 0;},[&](typename T::Write,uint8_t v){write(globalTarget(t),v,cost);},byteClocks(globalTarget(t),previousWrite));
    if(t.awaitingProgram())push(StateFlashCommitTask{},Input{0x167fc,0,0x81ff00});
   }else if constexpr(std::is_same_v<T,StateStoreService>){
    if(const auto*p=t.multiTask()){using S=StateMultiSerializeTask::Stage;switch(p->stage()){case S::RetainOriginal:case S::RetainMulti:case S::RetainProgram:case S::RetainPart:case S::RetainNameTarget:case S::DispatchPart:case S::DispatchPacking:effect_.write=true;break;case S::RestoreNameTarget:case S::RestorePart:case S::RestoreProgram:case S::RestoreMulti:case S::RestoreOriginal:case S::ReturnMulti:effect_.read=true;break;default:break;}}
    if(const auto*p=t.packTask()){using S=StatePackRefreshTask::Stage;switch(p->stage()){case S::DispatchEntry:case S::RetainGroup:case S::DispatchRecipe:effect_.write=true;break;case S::RestoreGroup:case S::ReturnEntry:case S::ReturnAll:effect_.read=true;break;default:break;}}
    if(const auto*p=t.recipeTask())effect_.read=p->stage()==StatePackFieldsTask::Stage::ReturnPacked;
    if(const auto*p=t.partTask())effect_.read=p->stage()==PartLookupTask::Stage::Complete;
    if(const auto*p=t.nameTask())effect_.read=p->stage()==StatePatchNameTask::Stage::ReturnRead||p->stage()==StatePatchNameTask::Stage::ReturnWrite;
    t.advanceBefore(cost,read,write,resolve,previousWrite);if(t.complete()){if(t.kind()<=StateStoreService::Kind::ProgramMode){summary_.packed=true;summary_.packingBase=t.base();summary_.packingOriginal=t.original();summary_.packingProgram=t.lastProgram();summary_.packingField=t.field();summary_.packingValue=t.value();summary_.packingContributor=t.contributor();}else{summary_.named=true;summary_.nameStorage=t.storage();}finish(f.input);}
   }else if constexpr(std::is_same_v<T,StateDisplayService>){
    effect_.read=t.endsReadTransfer();effect_.write=t.endsWriteTransfer();t.advanceBefore(cost,read,write,previousWrite);if(t.complete()){if(t.kind()==StateDisplayService::Kind::Text&&t.lcdResult().observed){summary_.lcd=t.lcdResult();summary_.decision=LastDecision::LcdReply;}finish(f.input);}
   }else if constexpr(std::is_same_v<T,StateReloadService>){
    summary_.reloaded=true;t.advanceBefore(cost,read,write,resolve,previousWrite);effect_.read=t.effect().read;effect_.write=t.effect().write;if(t.complete()){if(const auto*p=t.panel()){summary_.reloadCurrent=p->current();summary_.reloadResolvedBase=p->lastResolvedBase();summary_.reloadSelectorReference=p->lastBitSelectorReference();summary_.reloadBounds=p->bounds();}summary_.reloadExpandedBase=t.coordinator().expandedCurrentBase();finish(f.input);}
   }else if constexpr(std::is_same_v<T,PanelInitializeTask>){using S=typename T::Stage;const auto stage=t.stage();effect_.write=stage==S::Hold||stage==S::DispatchWake;effect_.read=stage==S::Restore||stage==S::Finish;
    t.advanceBefore(cost,[&](typename T::Write,uint32_t v){write(0xffec23,uint8_t(v),cost);});
    if(t.child()==T::Child::Wake){StateDisplayService::Input di;di.character=14;push(StateDisplayService(StateDisplayService::Kind::Command,di));}else if(t.stage()==S::Complete)finish(f.input);
   }else if constexpr(std::is_same_v<T,ScheduledTimerTask>){using S=typename T::Stage;using F=typename T::Field;const auto stage=t.stage();effect_.write=stage==S::DispatchClock;effect_.read=stage==S::Finish;
    const auto base=0x16758+t.slot()*8;
    t.advanceBefore(cost,[&](F what,bool)->uint32_t{return what==F::Action?f.input.target:f.input.program;},[&](F what,uint32_t v){if(what==F::Action)putLong(base+4,v);else putWord(base+(what==F::Started?0:2),uint16_t(v));});
    if(t.awaitingClock())push(ControllerClockReadTask{});else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,ControllerClockReadTask>){effect_.read=t.stage()==T::Stage::Return;t.advanceBefore(cost,[&]{return tick_=longword(0xffec10);});if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,StateBankNavigationTask>){using S=typename T::Stage;const auto stage=t.stage();effect_.write=stage==S::ShowNotice||stage==S::ScheduleNotice;effect_.read=stage==S::ReturnNotice||stage==S::ReturnPartMarker;
    t.advanceBefore(cost,[&](typename T::Read what,uint32_t)->uint32_t{return byte(what==T::Read::Part?0x16799:0x101a4+uint8_t(value_*2));},[&](typename T::Write what,uint32_t,uint8_t v){write(what==T::Write::Marker?0x167d6:0x16791,v,cost);},byteClocks(stage==S::WritePartMarker?0x167d6:0x16791,previousWrite));
    if(stage==S::ShowNotice)summary_.notice=true;if(stage==S::ReadPartMarker)summary_.marker=uint8_t(t.value());if(stage==S::ReadPartIndex)value_=uint8_t(t.value());
    if(t.awaitingChild()){if(t.resumeStage()==S::BindNoticeFlag){StateDisplayService::Input di;di.source=f.input.source;push(StateDisplayService(StateDisplayService::Kind::Text,di));}else startTimer(1,750,input_.noticeAction);}
    else if(t.complete())finish(f.input);
   }else if constexpr(std::is_same_v<T,ResetTask>){
    t.advance();if(t.phase==1)value_=0;if(t.phase==2)write(0x16784,0,cost);if(t.phase==3)write(0x16792,0,cost);if(t.phase==4){effect_.read=true;finish(f.input);}
   }else if constexpr(std::is_same_v<T,StateStoreUiTask>){using S=typename T::Stage;const auto stage=t.stage();
    effect_.write=stage==S::SerializeMulti||stage==S::SaveMulti||stage==S::DisplayMulti||stage==S::PrepareSingle||stage==S::SaveSingle||stage==S::ResetSinglePanel||stage==S::ReloadMulti||stage==S::RefreshMultiName;
    if(stage==S::SelectMultiMessage)f.input.source=0x14e93;if(stage==S::SelectSingleMessage)f.input.source=0x14e21;if(stage==S::SelectReloadMessage)f.input.source=0x14e46;
    t.advanceBefore(cost,[&](typename T::Read what)->uint32_t{if(what==T::Read::Current){current_=longword(0x167bc);if(!dataSpan(current_,228))throw std::logic_error("Unsupported evolving UI save resource");return current_;}if(what==T::Read::Mode)return mode_=byte(0x167ba);return byte(0x167da);},[&](typename T::Write,uint8_t v){write(uiTarget(t),v,cost);},byteClocks(uiTarget(t),previousWrite));
    if(t.awaitingChild()){switch(t.child()){
     case T::Child::SerializeMulti:push(StateStoreService(StateStoreService::Kind::WriteMultiName));break;
     case T::Child::PrepareSingle:push(StateStoreService(StateStoreService::Kind::WritePatchName));break;
     case T::Child::SaveMulti:push(StateMultiSaveTask(StateMultiSaveTask::Entry::Current));break;
     case T::Child::SaveSingle:push(StatePresetSaveTask{});break;
     case T::Child::DisplayMulti:push(StateBankNavigationTask(StateBankNavigationTask::Kind::Notice),f.input);break;
     case T::Child::ResetPanel:push(ResetTask{});break;
     case T::Child::ReloadMulti:push(StateReloadService{});break;
     case T::Child::RefreshName:push(StateBankNavigationTask(StateBankNavigationTask::Kind::Resolve));break;
     default:throw std::logic_error("Unsupported save UI child");}}
    else if(t.awaitingTail()){if(t.tail()==T::Tail::ResetPanel)push(ResetTask{});else if(t.tail()==T::Tail::Display)push(StateBankNavigationTask(StateBankNavigationTask::Kind::Notice),f.input);else throw std::logic_error("Unexpected save UI tail");}
   }
  },f.task);
  selectCurrent();return cost;
 }
private:
 struct ResetTask {unsigned phase=0;unsigned nextClocks()const{return phase==0?3:phase==3?17:12;}void advance(){++phase;}};
 using Task=std::variant<StateStoreUiTask,StatePresetSaveTask,StatePresetFormatTask,StateProgramTask,StateStoreServicesTask,StateFlashCommitTask,StateSavePageService,StateMultiSaveTask,StateGlobalSaveTask,StateStoreService,StateDisplayService,PanelInitializeTask,ScheduledTimerTask,ControllerClockReadTask,StateBankNavigationTask,ResetTask,StateReloadService>;
 struct Frame {template<class T>Frame(T t,Input i):task(std::move(t)),input(i){}Task task;Input input;};
 template<class T>void push(T task){push(std::move(task),Input{});}template<class T>void push(T task,Input in){if(depth_==frames_.size())throw std::logic_error("Native save continuation capacity exceeded");frames_[depth_++].emplace(std::move(task),in);selectCurrent();}
 void startTimer(unsigned slot,uint16_t delay,ScheduledTimerTask::ActionHandle action){Input in;in.program=delay;in.target=action.value;push(ScheduledTimerTask(ScheduledTimerTask::Kind::Arm,slot),in);}
 void finish(Input result){frames_[--depth_].reset();result_=result;if(!depth_)return;auto&f=*frames_[depth_-1];std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
  if constexpr(std::is_same_v<T,StateFlashCommitTask>){if(t.child()==T::Child::Program){f.input.source=result.source;f.input.target=result.target;}t.resumeChild();}
  else if constexpr(std::is_same_v<T,StateStoreServicesTask>){if(t.awaitingMessage())t.resumeMessage();else{t.retireTail();finish(result);}}
  else if constexpr(std::is_same_v<T,StatePresetFormatTask>){if(t.awaitingIndex())t.resumeIndex(index_);else{t.retireStore();finish(result);}}
  else if constexpr(std::is_same_v<T,StateMultiSaveTask>){if(t.awaitingMessage()){t.retireMessage();finish(result);}else{if(t.child()==T::Child::Program)f.input.target=result.target;t.resumeChild();}}
  else if constexpr(std::is_same_v<T,StateGlobalSaveTask>){t.retireProgram();finish(result);}
  else if constexpr(std::is_same_v<T,StatePresetSaveTask>)t.resumeChild();
  else if constexpr(std::is_same_v<T,PanelInitializeTask>)t.resumeChild();
  else if constexpr(std::is_same_v<T,ScheduledTimerTask>)t.resumeClock(uint16_t(tick_));
  else if constexpr(std::is_same_v<T,StateBankNavigationTask>)t.resumeChild(0);
  else if constexpr(std::is_same_v<T,StateStoreUiTask>){if(t.awaitingTail()){t.retireTail();finish(result);}else t.resumeChild();}
 },f.task);}
 void selectCurrent(){if(!depth_)return;const auto&f=*frames_[depth_-1];component_=std::visit([](const auto&t)->Component{using T=std::decay_t<decltype(t)>;
  if constexpr(std::is_same_v<T,StateStoreUiTask>)return Component::Ui;else if constexpr(std::is_same_v<T,StatePresetSaveTask>)return Component::Preset;else if constexpr(std::is_same_v<T,StatePresetFormatTask>)return Component::Format;else if constexpr(std::is_same_v<T,StateProgramTask>)return Component::Index;else if constexpr(std::is_same_v<T,StateStoreServicesTask>)return Component::Offset;else if constexpr(std::is_same_v<T,StateFlashCommitTask>)return Component::Commit;else if constexpr(std::is_same_v<T,StateSavePageService>)return t.component()==StateSavePageService::Component::Program?Component::Program:Component::Compare;else if constexpr(std::is_same_v<T,StateMultiSaveTask>)return Component::Multi;else if constexpr(std::is_same_v<T,StateGlobalSaveTask>)return Component::Global;else if constexpr(std::is_same_v<T,StateStoreService>)return Component::Preparation;else if constexpr(std::is_same_v<T,StateDisplayService>||std::is_same_v<T,PanelInitializeTask>)return Component::Display;else if constexpr(std::is_same_v<T,ScheduledTimerTask>)return Component::Timer;else if constexpr(std::is_same_v<T,ControllerClockReadTask>)return Component::Clock;else if constexpr(std::is_same_v<T,ResetTask>)return Component::Reset;else if constexpr(std::is_same_v<T,StateReloadService>)return Component::Reload;else return t.stage()>=StateBankNavigationTask::Stage::ShowNotice?Component::Display:Component::Resolve;
 },f.task);components_|=1u<<unsigned(component_);}
 static int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
 static unsigned byteClocks(uint32_t a,int prev){a&=0xffffff;if(a<0x400000)return !(prev&1)&&a==(uint32_t(prev)|1u)?0:3;if(a>=0xfffd10&&a<0xffffa0)return !(prev&1)&&a==(uint32_t(prev)|1u)?0:1;return a>=0xffffa0?1:3;}
 static unsigned wordClocks(uint32_t a){return byteClocks(a,-1)+byteClocks(a+1,history(a));}
 static unsigned programByteCost(const FlashPageProgramTask&t,int prev){using S=FlashPageProgramTask::Stage;return t.stage()==S::WriteTarget?byteClocks(t.target()+t.position(),prev):3;}
 static uint32_t uiTarget(const StateStoreUiTask&t){using S=StateStoreUiTask::Stage;return t.stage()==S::WriteCategory?t.current()+176:t.stage()==S::WriteMultiDirty||t.stage()==S::WriteProgramDirty?0x16799:0x16790;}
 static uint32_t globalTarget(const StateGlobalSaveTask&t){using S=StateGlobalSaveTask::Stage;switch(t.stage()){case S::WritePart:return 0x169ea;case S::WriteSingleBank:return 0x169e8;case S::WriteSingleProgram:return 0x169e9;case S::WriteMultiBank:return 0x169e6;case S::WriteMultiProgram:return 0x169e7;default:return 0x167fc+t.byteOffset();}}
 Kind kind_;Summary summary_{};Input input_,result_{};std::array<std::optional<Frame>,12>frames_{};unsigned depth_=0;Component component_=Component::Ui;uint32_t components_=0,current_=0,tick_=0;uint16_t program_=0,index_=0;uint8_t value_=0,mode_=0;bool allowed_=false,writeWindow_=false;Effect effect_{};
};
}
