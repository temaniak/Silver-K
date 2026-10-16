#pragma once
#include "ksNativeDspJob.h"
#include "ksParameterScale.h"
#include "ksParameterModulationTask.h"
#include "ksParameterHeaderTask.h"
#include "ksParameterCurveTask.h"
#include "ksParameterSignedSquareTask.h"
#include "ksParameterScalarTask.h"
#include "ksParameterTimeTask.h"
#include <optional>
namespace ks {
class NativeCoefficientJob final {public:
 explicit NativeCoefficientJob(int16_t input):task_(input){}
 bool complete()const{return complete_;}int32_t value()const{return task_.result().coefficient;}
 unsigned nextClocks()const{return complete_?0:task_.complete()?17:task_.nextBoundaryClocks();}
 NativeDspJob::Progress advanceBefore(unsigned available){auto cost=nextClocks();if(!cost||cost>available)return {};if(task_.complete()){complete_=true;return {cost,true,false};}task_.advanceBefore(cost);return {cost,false,false};}
private:ParameterScaleTask task_;bool complete_=false;
};
// One logical parameter-calculation block. Its final word call returns into
// the next block in the voice update sequence; no extra return interval is
// inserted at the block boundary.
class NativeParameterJob final {public:
 enum class HeaderRead {CurrentPart,BasePart};
 struct Modulation {unsigned output;};
 struct Header {ParameterHeaderTask::Kind kind;};
 enum class TimeBase {Rise,Decay,Auxiliary};
 struct Curve {uint16_t field;};
 struct SignedSquare {uint16_t field;};
 struct Scalar {ParameterScalarTask::Kind kind;uint16_t field;};
 struct Time {uint16_t field;TimeBase base;uint8_t multiplier,threshold;bool optional;};
 NativeParameterJob(Scalar request,NativeDspJob::Port port):task_(std::in_place_type<ParameterScalarTask>,request.kind),field_(request.field),base_(TimeBase::Rise),port_(port){}
 NativeParameterJob(Time request,NativeDspJob::Port port):task_(std::in_place_type<ParameterTimeTask>,request.multiplier,request.threshold,request.optional),field_(request.field),base_(request.base),port_(port){}
 NativeParameterJob(Curve request,NativeDspJob::Port port):task_(std::in_place_type<ParameterCurveTask>),field_(request.field),base_(TimeBase::Rise),port_(port){}
 NativeParameterJob(SignedSquare request,NativeDspJob::Port port):task_(std::in_place_type<ParameterSignedSquareTask>),field_(request.field),base_(TimeBase::Rise),port_(port){}
 NativeParameterJob(Header request,NativeDspJob::Port port):task_(std::in_place_type<ParameterHeaderTask>,request.kind),field_(request.kind==ParameterHeaderTask::Kind::Byte?148:6),base_(TimeBase::Rise),port_(port){}
 NativeParameterJob(Modulation request,NativeDspJob::Port port):task_(std::in_place_type<ParameterModulationTask>,request.output),field_(0),base_(TimeBase::Rise),port_(port){}
 bool complete()const{return complete_;}
 unsigned nextClocks()const{if(complete_)return 0;if(fault_)return fault_->nextClocks();if(word_)return word_->nextClocks();if(coefficient_)return coefficient_->nextClocks();return std::visit([](const auto&t){return t.nextClocks();},task_);}
 template<class IO>NativeDspJob::Progress advanceBefore(unsigned available,IO&io){
  const auto cost=nextClocks();if(!cost||cost>available)return {};NativeDspJob::Progress out{cost};
  if(fault_){out=fault_->advanceBefore(cost,[&](DspFaultTask::Read r,unsigned at){return io.readFault(r,at);},[&](DspFaultTask::Write w,uint8_t v,unsigned at){io.writeFault(w,v,at);});if(fault_->complete()){fault_.reset();word_->resumeFault();}out.completed=false;return out;}
  if(word_){out=word_->advanceBefore(cost,[&](NativeDspJob::Port p,unsigned off,unsigned at){return io.readDsp(p,off,at);},[&](NativeDspJob::Port p,unsigned off,uint8_t value,unsigned at){io.writeDsp(p,off,value,at);},[](uint16_t,unsigned){});if(out.faultRequested){fault_.emplace();out.faultRequested=false;}
   else if(word_->complete()){word_.reset();std::visit([](auto&t){t.resumeWord();},task_);complete_=std::visit([](const auto&t){return t.complete();},task_);}out.completed=complete_;return out;
  }
  if(coefficient_){out=coefficient_->advanceBefore(cost);if(coefficient_->complete()){auto value=uint32_t(coefficient_->value());coefficient_.reset();std::get<ParameterTimeTask>(task_).resumeCoefficient(value);}out.completed=false;return out;}
  std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,ParameterScalarTask>||std::is_same_v<T,ParameterSignedSquareTask>)t.advanceBefore(cost,[&]{return io.readParameter(field_,cost);});else if constexpr(std::is_same_v<T,ParameterModulationTask>){t.advanceBefore(cost,[&](ParameterModulationTask::Read what)->uint32_t{using R=ParameterModulationTask::Read;switch(what){case R::Bias:return io.readParameter(t.biasOffset(),cost);case R::Depth:return io.readParameter(t.depthOffset(),cost);case R::Modulation:return io.readModulation(t.modulationOffset(),cost);case R::Curve:return io.readCurve(uint8_t(t.curveIndex()),cost);}return 0;});}else if constexpr(std::is_same_v<T,ParameterHeaderTask>){t.advanceBefore(cost,[&](unsigned what)->uint32_t{return what<2?io.readHeader(HeaderRead(what),cost):io.readParameter(field_,cost);});}else if constexpr(std::is_same_v<T,ParameterCurveTask>){t.advanceBefore(cost,[&](bool curve)->uint32_t{return curve?io.readCurve(t.index(),cost):io.readParameter(field_,cost);});}else{t.advanceBefore(cost,[&](bool base)->uint32_t{return base?io.readTimeBase(base_,cost):io.readParameter(field_,cost);});if(t.awaitingCoefficient())coefficient_.emplace(int16_t(t.scaleInput()));}
   if(t.awaitingWord())word_.emplace(NativeDspJob::Word{t.dspWord()},port_);
  },task_);return out;
 }
private:std::variant<ParameterScalarTask,ParameterTimeTask,ParameterCurveTask,ParameterSignedSquareTask,ParameterHeaderTask,ParameterModulationTask> task_;uint16_t field_;TimeBase base_;NativeDspJob::Port port_;std::optional<NativeDspJob>word_;std::optional<NativeDspFaultJob>fault_;std::optional<NativeCoefficientJob>coefficient_;bool complete_=false;
};}
