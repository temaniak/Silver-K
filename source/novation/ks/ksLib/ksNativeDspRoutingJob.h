#pragma once
#include "ksNativeDspJob.h"
#include "ksDspRoutingTask.h"
#include "ksDspStartupTasks.h"
#include <optional>
namespace ks {
// Domain context shared by DSP routing jobs. These are live protocol mirrors,
// with typed reads/publications supplied by the engine, not processor state.
struct NativeDspContext {NativeDspJob::Port port=NativeDspJob::Port::A;uint16_t primary=0,alternate=0,cached=0xffff;uint8_t voiceB=0,part=0,basePart=0;};
class NativeDspRoutingJob final {
 using T=DspRoutingTask;using K=T::Kind;using R=T::Request;using S=T::Stage;
 enum class LeafRole {None,Address,Read,AddressForWord,WordTail};
public:
 NativeDspRoutingJob(K kind,uint16_t address,uint32_t word,NativeDspContext&context):root_(kind,address),word_(word&0xffffff),context_(context){}
 bool complete()const{return complete_;}uint32_t result()const{return result_;}
 unsigned nextClocks()const{
  if(complete_)return 0;if(fault_)return fault_->nextClocks();if(leaf_)return leaf_->nextClocks();
  if(select_)return select_->stage()==DspSelectTask::Stage::Complete?17:select_->nextClocks();
  const auto&t=current();return t.complete()?17:t.nextClocks();
 }
 // IO is a typed engine adapter exposing read/writeRoute, read/writeDsp and
 // read/writeFault. All callback times are offsets inside the admitted step.
 template<class IO>NativeDspJob::Progress advanceBefore(unsigned available,IO&io){
  const auto cost=nextClocks();if(!cost||cost>available)return {};NativeDspJob::Progress out{cost};
  if(fault_){out=fault_->advanceBefore(cost,[&](DspFaultTask::Read r,unsigned at){return io.readFault(r,at);},[&](DspFaultTask::Write w,uint8_t v,unsigned at){io.writeFault(w,v,at);});if(fault_->complete()){fault_.reset();leaf_->resumeFault();}out.completed=complete_;return out;}
  if(leaf_){out=leaf_->advanceBefore(cost,[&](NativeDspJob::Port p,unsigned off,unsigned at){return io.readDsp(p,off,at);},[&](NativeDspJob::Port p,unsigned off,uint8_t value,unsigned at){io.writeDsp(p,off,value,at);},[&](uint16_t value,unsigned at){context_.cached=value;io.writeRoute(T::Write::CachedAddress,value,at);});context_.port=leaf_->port();
   if(out.faultRequested){fault_.emplace();out.faultRequested=false;return out;}
   if(leaf_->complete()){
    result_=leaf_->result();leaf_.reset();if(role_==LeafRole::AddressForWord){role_=LeafRole::WordTail;leaf_.emplace(NativeDspJob::Word{word_,true,current().address()},context_.port);}
    else if(role_==LeafRole::WordTail){role_=LeafRole::None;finishCurrent();}
    else {role_=LeafRole::None;current().resumeChild();}
   }out.completed=complete_;return out;
  }
  if(select_){if(select_->stage()==DspSelectTask::Stage::Complete){select_.reset();current().resumeChild();return out;}
   const auto stage=select_->stage();if(stage==DspSelectTask::Stage::Select)context_.port=select_->port()==DspPort::A?NativeDspJob::Port::A:NativeDspJob::Port::B;
   select_->advanceBefore(cost,[&](DspPort,uint16_t value){context_.cached=value;io.writeRoute(T::Write::CachedAddress,value,cost);});return out;
  }
  auto&t=current();if(t.complete()){finishCurrent();out.completed=complete_;return out;}
  const auto stage=t.stage();if(stage==S::SelectPort)context_.port=t.kind()==K::WriteB?NativeDspJob::Port::B:NativeDspJob::Port::A;
  t.advanceBefore(cost,[&](T::Read r){return io.readRoute(r,cost);},[&](T::Write w,uint16_t value){if(w==T::Write::PrimaryOffset)context_.primary=value;else if(w==T::Write::AlternateOffset)context_.alternate=value;else context_.cached=value;io.writeRoute(w,value,cost);});
  switch(t.request()){
  case R::None:break;
  case R::PrimeAddress:window_.emplace(K::PrimeAbsolute,t.address());break;
  case R::WriteAddress:window_.emplace(K::WriteAbsolute,t.address());break;
  case R::ReadAddress:role_=LeafRole::Read;leaf_.emplace(NativeDspJob::Address{t.address(),true,DspAddressTask::PortSelection::B},context_.port);break;
  case R::SelectA:case R::SelectB:select_.emplace(t.request()==R::SelectA?DspPort::A:DspPort::B);break;
  case R::Address:role_=LeafRole::Address;leaf_.emplace(NativeDspJob::Address{t.address(),false},context_.port);break;
  case R::AddressThenWord:role_=LeafRole::AddressForWord;leaf_.emplace(NativeDspJob::Address{t.address(),false},context_.port);break;
  }return out;
 }
private:
 T&current(){return window_?*window_:root_;}const T&current()const{return window_?*window_:root_;}
 void finishCurrent(){if(window_){window_.reset();root_.resumeChild();}else complete_=true;}
 T root_;std::optional<T>window_;std::optional<NativeDspJob>leaf_;std::optional<DspSelectTask>select_;std::optional<NativeDspFaultJob>fault_;uint32_t word_,result_=0;NativeDspContext&context_;LeafRole role_=LeafRole::None;bool complete_=false;
};}
