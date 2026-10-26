#pragma once
#include "ksDspStartupTasks.h"
#include "ksDspBootSetupTask.h"
#include "ksDspBootStreamTask.h"
#include "ksDspFaultTask.h"
#include "ksControllerDspService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Actual dual-firmware startup. Source references are authenticated data spans;
// child selection is by typed requests, never instruction addresses.
class StartupDspService final {
public:
 struct Effect{bool read=false,write=false;};
 enum class Component{Policy,Reset,Setup,Stream,Transaction,Fault};
 explicit StartupDspService(uint32_t sourceA=0xbad6,uint32_t sourceB=0xd0c3):sources_{{sourceA,sourceB}}{if(!span(sourceA,3)||!span(sourceB,3))throw std::logic_error("Startup DSP source range");}
 bool complete()const{return complete_;}unsigned priority()const{return 0;}
 const Effect&effect()const{return effect_;}Component component()const{return component_;}
 const DspStartupTask&policy()const{return policy_;}
 unsigned nextClocks(int previous=-1)const{
  if(complete_)return 0;
  switch(component_){case Component::Policy:return policy_.stage()==DspStartupTask::Stage::Complete?17:policy_.nextClocks();case Component::Reset:return reset_->complete()?17:reset_->nextClocks();case Component::Setup:return setup_->complete()?17:setup_->nextBoundaryClocks();case Component::Stream:return stream_->complete()?17:stream_->nextBoundaryClocks();case Component::Fault:return fault_->complete()?17:fault_->nextBoundaryClocks();case Component::Transaction:return transaction_->nextClocks(previous);}return 0;
 }
 static bool span(uint32_t raw,uint64_t n){const auto a=raw&0xffffff;if(raw!=a)return false;if(a<0x200000)return n<=0x200000-a;for(auto range:{std::pair<uint32_t,uint32_t>{0x800000,0x880000},{0xa00000,0xc00000},{0xffdc00,0xfffc00}})if(a>=range.first&&a<range.second)return n<=range.second-a;return false;}
 template<class Read,class Write>unsigned advanceBefore(unsigned budget,Read&&read,Write&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||budget<cost)return 0;
  if(component_==Component::Setup&&setup_->stage()==DspBootSetupTask::Stage::DispatchStream&&!span(source(),3ull*(setup_->streamWords()?unsigned(setup_->streamWords()):65536u)))throw std::logic_error("Startup DSP stream extent");
  effect_={true,false};
  switch(component_){
  case Component::Policy:{using S=DspStartupTask::Stage;const auto stage=policy_.stage();if(stage==S::Complete){complete_=true;break;}if(!policy_.advanceBefore(cost))throw std::logic_error("Startup DSP policy admission");switch(policy_.request()){
   case DspStartupTask::Request::Reset:reset_.emplace();component_=Component::Reset;effect_.write=true;break;
   case DspStartupTask::Request::SetupA:case DspStartupTask::Request::SetupB:port_=policy_.request()==DspStartupTask::Request::SetupA?DspPort::A:DspPort::B;setup_.emplace();component_=Component::Setup;effect_.write=true;break;
   case DspStartupTask::Request::SelectA:transaction_.emplace(ControllerDspService::Kind::Select,ControllerDspService::Input{DspRoutingTask::Kind::WriteRelative,DspPort::A,0,0});transactionTail_=true;component_=Component::Transaction;effect_.write=true;break;default:break;}break;}
  case Component::Reset:{if(reset_->complete()){reset_.reset();resumePolicy();break;}using S=DspResetTask::Stage;const auto stage=reset_->stage();effect_.write=stage==S::Admission||((stage==S::SettleLow||stage==S::SettleHigh)&&(reset_->settling().stage()==DspSettlingTask::Stage::Admission||reset_->settling().stage()==DspSettlingTask::Stage::InnerAdmission));reset_->advanceBefore(cost,[&]{return read(0xffff6f,cost);},[&](uint8_t value){write(0xffff6f,value,cost);});break;}
  case Component::Setup:{if(setup_->complete()){setup_.reset();resumePolicy();break;}using T=DspBootSetupTask;setup_->advanceBefore(cost,[&](unsigned offset){return read(source()+offset,cost);},[&]{return read(portBase()+4,cost);});switch(setup_->request()){
   case T::Request::Stream:stream_.emplace(setup_->streamWords());component_=Component::Stream;effect_.write=true;break;
   case T::Request::Address:address_=T::tableAddress();transaction_.emplace(ControllerDspService::Kind::Address,ControllerDspService::Input{DspRoutingTask::Kind::WriteRelative,port_,address_,0});transactionTail_=false;component_=Component::Transaction;effect_.write=true;break;
   case T::Request::ExponentialWord:case T::Request::ReciprocalWord:transaction_.emplace(ControllerDspService::Kind::Word,ControllerDspService::Input{DspRoutingTask::Kind::WriteRelative,port_,address_,setup_->wordValue()>>8});transactionTail_=false;component_=Component::Transaction;effect_.write=true;break;
   case T::Request::Fault:fault_.emplace();faultFromStream_=false;component_=Component::Fault;effect_.write=true;break;default:break;}break;}
  case Component::Stream:{if(stream_->complete()){stream_.reset();resumeSetup();break;}using T=DspBootStreamTask;stream_->advanceBefore(cost,[&]{return read(portBase()+4,cost);},[&](uint32_t offset){return read(source()+offset,cost-2);},[&](unsigned offset,uint8_t value){write(portBase()+offset,value,cost);});if(stream_->stage()==T::Stage::AwaitFault){fault_.emplace();faultFromStream_=true;component_=Component::Fault;effect_.write=true;}break;}
  case Component::Transaction:{transaction_->advanceBefore(cost,read,write,previous);effect_={transaction_->effect().read,transaction_->effect().write};if(transaction_->complete()){address_=transaction_->cachedAddress();transaction_.reset();if(transactionTail_)resumePolicy();else resumeSetup();}break;}
  case Component::Fault:{using T=DspFaultTask;if(fault_->complete()){fault_.reset();if(faultFromStream_){if(!stream_->resumeFault())throw std::logic_error("Startup stream fault return");component_=Component::Stream;}else resumeSetup();break;}const auto stage=fault_->stage();effect_.write=stage==T::Stage::Admission||stage==T::Stage::LatchAdmission||stage==T::Stage::DelayAdmission;fault_->advanceBefore(cost,[&](T::Read q){return read(q==T::Read::Latch?0xffec22u:0xffff62u,cost-(q==T::Read::Control?1u:0u));},[&](T::Write q,uint8_t value){write(q==T::Write::Latch?0xffec22u:q==T::Write::Data?0xffff61u:0xffff62u,value,cost);});break;}
  }return cost;
 }
private:
 uint32_t source()const{return sources_[port_==DspPort::A?0:1];}uint32_t portBase()const{return port_==DspPort::A?0x400000u:0x600000u;}
 void resumePolicy(){if(!policy_.resumeChild())throw std::logic_error("Startup DSP child return");component_=Component::Policy;}
 void resumeSetup(){if(!setup_||!setup_->resumeChild())throw std::logic_error("Startup DSP setup child return");component_=Component::Setup;}
 DspStartupTask policy_;std::array<uint32_t,2>sources_;DspPort port_=DspPort::A;uint16_t address_=0;bool complete_=false,transactionTail_=false,faultFromStream_=false;Component component_=Component::Policy;Effect effect_{};
 std::optional<DspResetTask>reset_;std::optional<DspBootSetupTask>setup_;std::optional<DspBootStreamTask>stream_;std::optional<ControllerDspService>transaction_;std::optional<DspFaultTask>fault_;
};
}
