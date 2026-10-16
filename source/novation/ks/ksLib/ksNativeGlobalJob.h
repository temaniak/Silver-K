#pragma once
#include "ksNativeDspRoutingJob.h"
#include "ksNativeParameterJob.h"
#include "ksPartControlTasks.h"
#include "ksSquareRootTask.h"
#include "ksPartRouteTask.h"
#include "ksGlobalSetupTask.h"
#include "ksGlobalScalarTask.h"
#include "ksGlobalTimeTask.h"
#include "ksGlobalShapeTask.h"
#include "ksGlobalRangeTask.h"
#include "ksGlobalMixTask.h"
#include "ksGlobalTableTask.h"
#include "ksGlobalMatrixTask.h"
#include "ksGlobalClampedTimeTask.h"
#include "ksGlobalPairTask.h"
#include "ksGlobalTailScalarTask.h"
#include "ksGlobalFinishScalarTask.h"
#include "ksGlobalFinalTask.h"
#include "ksGlobalEffectTask.h"
#include <optional>
#include <variant>
#include <type_traits>
namespace ks {
// Resource references point at musical patch/control data, never an execution
// image. A full refresh binds its patch once through the live controls lookup.
struct NativeGlobalContext {NativeDspContext dsp;uint32_t patch=0;uint8_t control=0,part=0,mode=0;};
class NativeGlobalJob final {
 using Tasks=std::variant<GlobalSetupTask,GlobalScalarTask,GlobalTimeTask,GlobalShapeTask,GlobalRangeTask,GlobalMixTask,GlobalTableTask,GlobalMatrixTask,GlobalClampedTimeTask,GlobalPairTask,GlobalTailScalarTask,GlobalFinishScalarTask,GlobalFinalTask,GlobalEffectTask>;
 enum class Completion {Task,SetupControl,SetupPart,SetupRoute,MatrixRoot,MatrixModes,ModesControl,ModesWord,EffectCoefficient};
public:
 explicit NativeGlobalJob(NativeGlobalContext&context):context_(context),task_(std::in_place_type<GlobalSetupTask>){}
 bool complete()const{return complete_;}
 template<class IO>unsigned nextClocks(IO&io)const {
  if(complete_)return 0;if(route_)return route_->nextClocks();if(lookup_)return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();if(root_)return root_->nextClocks();if(coefficient_)return coefficient_->nextClocks();
  if(modes_)return modes_->nextClocks([]{return 3u;});
  return std::visit([&](const auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,GlobalSetupTask>)return t.nextClocks([&](GlobalSetupTask::Write w){return io.memoryWriteClocks(setupAddress(t,w));});else if constexpr(std::is_same_v<T,GlobalFinalTask>)return t.nextClocks([&]{return io.memoryWriteClocks(0x17527);});else return t.nextClocks();},task_);
 }
 // Typed data IO: readGlobal8/16/32, writeGlobal8/16, memoryWriteClocks;
 // DSP route/fault IO is the NativeDspRoutingJob contract. Times are offsets
 // inside one admitted boundary. Returns are ordinary typed completions.
 template<class IO>NativeDspJob::Progress advanceBefore(unsigned available,IO&io){
  const auto cost=nextClocks(io);if(!cost||cost>available)return {};NativeDspJob::Progress out{cost};
  if(route_){out=route_->advanceBefore(cost,io);if(route_->complete()){route_.reset();completeChild(0);}out.completed=complete_;return out;}
  if(lookup_){if(lookup_->stage()==PartLookupTask::Stage::Complete){const auto index=lookup_->index();lookup_.reset();completeChild(index);}else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout){return io.readGlobal8(0x17526,cost);});out.completed=complete_;return out;}
  if(root_){root_->advanceBefore(cost,[&]{return std::get<GlobalMatrixTask>(task_).target();});if(root_->stage()==SquareRootTask::Stage::Complete){auto value=root_->result();root_.reset();completeChild(value);}out.completed=complete_;return out;}
  if(coefficient_){out=coefficient_->advanceBefore(cost);if(coefficient_->complete()){auto value=uint32_t(coefficient_->value());coefficient_.reset();completeChild(value);}out.completed=complete_;return out;}
  if(modes_){advanceModes(cost,io);out.completed=complete_;return out;}
  std::visit([&](auto&t){advanceTask(t,cost,io);},task_);
  retireTask();out.completed=complete_;return out;
 }
private:
 static unsigned setupAddress(const GlobalSetupTask&t,GlobalSetupTask::Write w){return w==GlobalSetupTask::Write::Mirror?0x174aeu+20*t.control()+9+t.index():0x160d2u+334*t.part()+313;}
 void route(uint16_t offset,uint32_t word,Completion completion=Completion::Task,DspRoutingTask::Kind kind=DspRoutingTask::Kind::WriteRelative){completion_=completion;route_.emplace(kind,offset,word,context_.dsp);}
 void lookup(PartLookupTask::Layout layout,Completion completion){completion_=completion;lookup_.emplace(layout);}
 void completeChild(uint32_t value){auto completion=completion_;completion_=Completion::Task;
  if(completion==Completion::SetupControl){context_.control=uint8_t(value);std::get<GlobalSetupTask>(task_).resumeChild(uint8_t(value));}
  else if(completion==Completion::SetupPart){context_.part=uint8_t(value);std::get<GlobalSetupTask>(task_).resumeChild(uint8_t(value));}
  else if(completion==Completion::ModesControl){modes_->resumeChild(uint8_t(value));}
  else if(completion==Completion::ModesWord){modes_->resumeChild();}
  else if(completion==Completion::MatrixRoot){std::get<GlobalMatrixTask>(task_).resumeChild(uint8_t(value));}
  else if(completion==Completion::MatrixModes){std::get<GlobalMatrixTask>(task_).resumeChild();}
  else if(completion==Completion::EffectCoefficient){std::get<GlobalEffectTask>(task_).resumeChild(value);}
  else std::visit([](auto&t){using T=std::decay_t<decltype(t)>;if constexpr(!std::is_same_v<T,GlobalTableTask>)t.resumeChild();},task_);
  retireTask();
 }
 bool taskComplete()const{return std::visit([](const auto&t){return t.stage()==std::decay_t<decltype(t)>::Stage::Complete;},task_);}
 void retireTask(){if(!taskComplete())return;if(phase_==31&&!std::get<GlobalFinalTask>(task_).handoff()){complete_=true;return;}++phase_;switch(phase_){
 case 1:case 2:case 3:case 4:case 5:case 6:case 7:task_.emplace<GlobalScalarTask>(GlobalScalarTask::Kind(phase_-1));break;
 case 8:case 12:case 28:task_.emplace<GlobalTimeTask>(phase_==12?GlobalTimeTask::Kind::Second:GlobalTimeTask::Kind::First);break;
 case 9:case 10:case 11:task_.emplace<GlobalShapeTask>(GlobalShapeTask::Kind(phase_-9));break;
 case 13:task_.emplace<GlobalRangeTask>();break;
 case 14:case 15:case 16:case 17:case 18:case 19:task_.emplace<GlobalMixTask>(GlobalMixTask::Kind(phase_-14));break;
 case 20:task_.emplace<GlobalTableTask>();break;case 21:task_.emplace<GlobalMatrixTask>();break;case 22:task_.emplace<GlobalClampedTimeTask>();break;case 23:task_.emplace<GlobalPairTask>();break;
 case 24:case 25:case 26:case 27:task_.emplace<GlobalTailScalarTask>(GlobalTailScalarTask::Kind(phase_-24));break;
 case 29:case 30:task_.emplace<GlobalFinishScalarTask>(GlobalFinishScalarTask::Kind(phase_-29));break;
 case 31:task_.emplace<GlobalFinalTask>();break;case 32:task_.emplace<GlobalEffectTask>();break;default:complete_=true;break;
 }}
 template<class IO>void advanceModes(unsigned cost,IO&io){using T=PartRouteTask;using C=typename T::Child;auto&t=*modes_;t.advanceBefore(cost,[&](T::Read r)->uint8_t{return io.readGlobal8(0x174aeu+20*t.part()+(r==T::Read::Level?9:10),cost);},[](uint8_t){},[]{return 3u;});switch(t.child()){
 case C::Control:lookup(PartLookupTask::Layout::Controls,Completion::ModesControl);break;
 case C::Mode:route(0x831,t.code(),Completion::ModesWord);break;case C::Low:route(0x832,uint32_t(t.low())<<16,Completion::ModesWord);break;case C::High:route(0x833,uint32_t(t.high())<<16,Completion::ModesWord);break;default:break;}
 if(t.stage()==T::Stage::Complete){modes_.reset();completeChild(0);}
 }
 template<class T,class IO>void advanceTask(T&t,unsigned cost,IO&io){
  auto patch8=[&](unsigned field,unsigned at){return io.readGlobal8((context_.patch+field)&0xffffff,at);};
  if constexpr(std::is_same_v<T,GlobalSetupTask>){using R=typename T::Read;using W=typename T::Write;using C=typename T::Child;auto stage=t.stage();t.advanceBefore(cost,[&](R r)->uint32_t{if(r==R::Patch){context_.patch=io.readGlobal32(0x174aeu+20*t.control(),cost);return context_.patch;}return r==R::Mirror?patch8(179+t.index(),cost-2):patch8(209,cost);},[&](W w,uint8_t v){io.writeGlobal8(setupAddress(t,w),v,cost);},[&](W w){return io.memoryWriteClocks(setupAddress(t,w));});(void)stage;switch(t.child()){case C::Control:lookup(PartLookupTask::Layout::Controls,Completion::SetupControl);break;case C::Part:lookup(PartLookupTask::Layout::Parameters,Completion::SetupPart);break;case C::Route:route(0,0,Completion::SetupRoute,DspRoutingTask::Kind::VoiceA);break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalScalarTask>||std::is_same_v<T,GlobalTailScalarTask>||std::is_same_v<T,GlobalFinishScalarTask>){t.advanceBefore(cost,[&]{return patch8(t.field(),cost);});if(t.awaiting())route(t.offset(),t.wireWord());}
  else if constexpr(std::is_same_v<T,GlobalShapeTask>){t.advanceBefore(cost,[&](bool base)->uint32_t{return base?io.readGlobal16(0x15132,cost):patch8(t.field(),cost);});if(t.awaiting())route(t.offset(),t.wireWord());}
  else if constexpr(std::is_same_v<T,GlobalTimeTask>){using R=typename T::Read;using C=typename T::Child;auto stage=t.stage();t.advanceBefore(cost,[&](R r)->uint32_t{switch(r){case R::Aux:return patch8(218,cost);case R::Mode:return patch8(phase_==28?119:t.modeField(),cost);case R::Time:return patch8(phase_==28?118:t.timeField(),cost);case R::Table:return io.readGlobal32(0x159f4+t.index(),cost);case R::Divisor:return io.readGlobal16(0x1512e,cost);}return 0;});if(stage==T::Stage::ReadAux)context_.mode=t.aux();const auto first=phase_==28?0x83cu:t.firstOffset(),second=first+1;switch(t.child()){case C::First:route(first,0);break;case C::Table:route(second,t.table()>>8);break;case C::Time:route(first,t.scaled()>>8);break;case C::Second:route(second,0);break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalRangeTask>){using R=typename T::Read;using C=typename T::Child;t.advanceBefore(cost,[&](R r)->uint32_t{return r==R::Mode?context_.mode:r==R::Base?io.readGlobal16(0x1512e,cost):patch8(r==R::First?115:116,cost);});switch(t.child()){case C::Mode:route(0x819,t.mode());break;case C::First:route(0x81a,t.firstWord()>>8);break;case C::Second:route(0x81b,t.candidate()>>8);break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalMixTask>){t.advanceBefore(cost,[&](bool mode)->uint32_t{return mode?context_.mode:patch8(t.field(),cost);});if(t.awaiting())route(t.offset(),t.wireWord());}
  else if constexpr(std::is_same_v<T,GlobalTableTask>){t.advanceBefore(cost,[&](bool selector)->uint32_t{return selector?patch8(217,cost):io.readGlobal16(0x15282+t.rowOffset()+t.index()*2,cost-2);},[&](uint16_t word){io.writeGlobal16(0x17546+t.index()*2,word,cost);});}
  else if constexpr(std::is_same_v<T,GlobalMatrixTask>){using R=typename T::Read;using W=typename T::Write;using C=typename T::Child;t.advanceBefore(cost,[&](R r)->uint32_t{switch(r){case R::Amount:return patch8(109,cost);case R::Depth:return io.readGlobal8(0x17547,cost);case R::Bias:return io.readGlobal8(0x17546,cost);case R::Seed:return io.readGlobal16(0x17548,cost);case R::Factor:return io.readGlobal16(0x1754a,cost);case R::RecurrenceBias:return io.readGlobal16(0x1754c,cost);case R::Tail:return io.readGlobal16(0x1754e + t.index()*2,cost-2);case R::Base:return io.readGlobal16(0x1512e,cost);case R::Coefficient:return io.readGlobal16(0x1752e + t.index()*2,cost-2);}return 0;},[&](W w,uint16_t word){io.writeGlobal16((w==W::Generated?0x1752e:0x1753a)+t.index()*2,word,cost);});switch(t.child()){case C::First:route(0x822,uint32_t(t.combined())<<8);break;case C::Reference:route(0x823,0x599a00);break;case C::Root:completion_=Completion::MatrixRoot;root_.emplace();break;case C::Gain:route(0x824,t.gainProduct()>>8);break;case C::Coefficient:route(t.offset(),t.quantized()>>8);break;case C::Modes:completion_=Completion::MatrixModes;modes_.emplace(PartRouteTask::Kind::Refresh);break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalClampedTimeTask>){using R=typename T::Read;using C=typename T::Child;t.advanceBefore(cost,[&](R r)->uint32_t{return r==R::Mode?patch8(103,cost):r==R::Time?patch8(102,cost):r==R::Base?io.readGlobal16(0x1512e,cost):io.readGlobal32(0x15a7c+t.index(),cost);});switch(t.child()){case C::First:route(0x834,0);break;case C::Table:route(0x835,t.table()>>8);break;case C::Time:route(0x834,t.quantized()>>8);break;case C::Second:route(0x835,0);break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalPairTask>){using C=typename T::Child;t.advanceBefore(cost,[&](bool selector)->uint32_t{return selector?patch8(106,cost):io.readGlobal32(0x152fa+t.index(),cost);});if(t.child()==C::First)route(0x836,t.first()>>8);else if(t.child()==C::Second)route(0x837,t.second()>>8);}
  else if constexpr(std::is_same_v<T,GlobalFinalTask>){using R=typename T::Read;using W=typename T::Write;using C=typename T::Child;t.advanceBefore(cost,[&](R r)->uint32_t{switch(r){case R::Master:return io.readGlobal8(0x168fc+89,cost);case R::Level:return io.readGlobal8(0x168fc+91,cost);case R::Route:return io.readGlobal8(0x168fc+124,cost);case R::Inverse:return io.readGlobal8(0x168fc+90,cost);case R::PartCount:return io.readGlobal8(0x167ba,cost);case R::Status:return io.readGlobal8(0x168fc+208,cost);case R::OldStatus:return io.readGlobal8(0x17527,cost);case R::Part:return io.readGlobal8(0x17526,cost);}return 0;},[&](W w,uint16_t value){if(w==W::Status)io.writeGlobal8(0x17527,uint8_t(value),cost);else io.writeGlobal16(w==W::ResetA?0x174a8:0x174aa,0,cost);},[&]{return io.memoryWriteClocks(0x17527);});switch(t.child()){case C::Master:route(0x804d,t.masterProduct()>>8);break;case C::Main:route(0x8049,uint32_t(t.main())<<8);break;case C::Muted:route(0x804a,uint32_t(t.muted())<<8);break;case C::Inverse:route(0x804c,uint32_t(t.inverseWord())<<8);break;case C::Status:route(0x803f,0x800000|t.status());break;default:break;}}
  else if constexpr(std::is_same_v<T,GlobalEffectTask>){using R=typename T::Read;using C=typename T::Child;t.advanceBefore(cost,[&](R r){return patch8(r==R::Flags?123:r==R::Amount?248:r==R::Time?247:246,cost);});switch(t.child()){case C::Initial:route(0x8058,t.initialWireWord());break;case C::Coefficient:completion_=Completion::EffectCoefficient;coefficient_.emplace(int16_t(t.input()));break;case C::FirstB:route(5,t.first()>>8);break;case C::SecondB:route(6,t.second()>>8);break;case C::FirstA:route(5,t.first()>>8,Completion::Task,DspRoutingTask::Kind::WriteB);break;case C::SecondA:route(6,t.second()>>8,Completion::Task,DspRoutingTask::Kind::WriteB);break;default:break;}}
 }
 NativeGlobalContext&context_;Tasks task_;unsigned phase_=0;bool complete_=false;Completion completion_=Completion::Task;std::optional<NativeDspRoutingJob>route_;std::optional<PartLookupTask>lookup_;std::optional<SquareRootTask>root_;std::optional<NativeCoefficientJob>coefficient_;std::optional<PartRouteTask>modes_;
};}
