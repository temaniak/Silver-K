#pragma once
#include "ksDspRoutingTask.h"
#include "ksDspStartupTasks.h"
#include "ksDspAddressTask.h"
#include "ksDspWordTask.h"
#include "ksDspFaultTask.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// A closed command transaction. Addresses, selection and observations describe
// the bus protocol; nested return ownership is a bounded semantic continuation.
class ControllerDspService final {
public:
 enum class Kind{Routing,Address,Read,Word,IncrementWord,Select};
 struct Input{DspRoutingTask::Kind routing=DspRoutingTask::Kind::WriteRelative;DspPort port=DspPort::A;uint16_t address=0;uint32_t word=0;};
 struct Effect{bool read=false,write=false;};
 struct Summary{Kind kind=Kind::Address;Input input{};DspPort port=DspPort::A;uint16_t address=0,cachedAddress=0,remaining=0;uint32_t word=0,reply=0;bool retried=false,clearedRetry=false;uint64_t faults=0;uint8_t status=0,data=0;bool offsetAdded=false,cacheCompared=false,voiceDifference=false,selectedPort=false,wordTransmitted=false,addressRequested=false;uint16_t offsetBase=0,offsetValue=0,comparedAddress=0,comparedCached=0;uint8_t voice=0,baseVoice=0;uint32_t alternateProduct=0;};
 ControllerDspService(Kind kind,Input input):summary_{kind,input,input.port,input.address,input.address,0,input.word&0xffffffu,0,false,false,0,0,0}{
  switch(kind){case Kind::Routing:pushRoute(input.routing,input.address);break;case Kind::Address:address_.emplace(input.address);break;case Kind::Read:address_.emplace(input.address,true,input.port==DspPort::A?DspAddressTask::PortSelection::A:DspAddressTask::PortSelection::B);break;case Kind::Word:case Kind::IncrementWord:word_.emplace(input.word,kind==Kind::IncrementWord,input.address);break;case Kind::Select:select_.emplace(input.port);break;}
 }
 bool complete()const{return complete_;}bool critical()const{return false;}Kind kind()const{return summary_.kind;}const Summary&summary()const{return summary_;}const Effect&effect()const{return effect_;}
 DspPort port()const{return summary_.port;}uint16_t address()const{return summary_.address;}uint16_t cachedAddress()const{return summary_.cachedAddress;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,DspRoutingTask>)return routeCount_?&*routes_[routeCount_-1]:lastRoute_?&*lastRoute_:nullptr;else if constexpr(std::is_same_v<T,DspAddressTask>)return address_?&*address_:lastAddress_?&*lastAddress_:nullptr;else if constexpr(std::is_same_v<T,DspWordTask>)return word_?&*word_:lastWord_?&*lastWord_:nullptr;else if constexpr(std::is_same_v<T,DspSelectTask>)return select_?&*select_:lastSelect_?&*lastSelect_:nullptr;else if constexpr(std::is_same_v<T,DspFaultTask>)return fault_?&*fault_:lastFault_?&*lastFault_:nullptr;else return nullptr;}
 unsigned nextClocks(int previousWrite=-1)const{(void)previousWrite;if(complete_)return 0;if(fault_)return fault_->complete()?17:fault_->nextBoundaryClocks();if(address_)return address_->complete()?17:address_->nextBoundaryClocks();if(word_)return word_->complete()?17:word_->nextBoundaryClocks();if(select_)return select_->stage()==DspSelectTask::Stage::Complete?17:select_->nextClocks();if(routeCount_){const auto&t=*routes_[routeCount_-1];return t.stage()==DspRoutingTask::Stage::Complete?17:t.nextClocks();}throw std::logic_error("Missing controller DSP transaction");}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previousWrite=-1){const auto cost=nextClocks(previousWrite);if(!cost||available<cost)return 0;effect_={};
  const auto port=[&]{return summary_.port==DspPort::A?0x400000u:0x600000u;};
  const auto read16=[&](uint32_t a){return uint16_t((unsigned(read(a,cost))<<8)|read(a+1,cost));};
  const auto write16=[&](uint32_t a,uint16_t v){write(a,uint8_t(v>>8),cost);write(a+1,uint8_t(v),cost);};
  if(fault_){using T=DspFaultTask;if(fault_->complete()){lastFault_=fault_;fault_.reset();effect_.read=true;if(address_)address_->resumeFault();else word_->resumeFault();}else{const auto s=fault_->stage();effect_.write=s==T::Stage::Admission||s==T::Stage::LatchAdmission||s==T::Stage::DelayAdmission;effect_.read=s==T::Stage::DelayRelease||s==T::Stage::LatchRelease||s==T::Stage::CompletionDelay;fault_->advanceBefore(cost,[&](T::Read r){return read(r==T::Read::Latch?0xffec22:0xffff62,cost-(r==T::Read::Control?1:0));},[&](T::Write w,uint8_t v){write(w==T::Write::Latch?0xffec22:w==T::Write::Data?0xffff61:0xffff62,v,cost);});}return cost;}
  if(address_){using T=DspAddressTask;if(address_->complete()){summary_.reply=address_->result();lastAddress_=address_;address_.reset();effect_.read=true;if(addressForWord_){addressForWord_=false;word_.emplace(summary_.word,true,summary_.address);}else resumeRoute();}else{const auto s=address_->stage();summary_.addressRequested=true;if(s==T::Stage::SelectPort){summary_.selectedPort=true;summary_.port=address_->portSelection()==T::PortSelection::A?DspPort::A:DspPort::B;}if(s==T::Stage::DispatchFault)effect_.write=true;if(s==T::Stage::ResetRetries){summary_.retried=true;summary_.remaining=0;}if(s==T::Stage::DecrementRetries)summary_.remaining=uint16_t(address_->retries()-1);address_->advanceBefore(cost,[&](unsigned off){const auto v=uint8_t(read(port()+off,cost));if(off==4)summary_.status=v;else summary_.data=v;return v;},[&](unsigned off,uint8_t v){write(port()+off,v,cost);});if(address_->stage()==T::Stage::AwaitFault){fault_.emplace();++summary_.faults;}}return cost;}
  if(word_){using T=DspWordTask;if(word_->complete()){lastWord_=word_;word_.reset();effect_.read=true;if(routeCount_)finishRoute();else complete_=true;}else{const auto s=word_->stage();summary_.wordTransmitted=true;if(s==T::Stage::DispatchFault)effect_.write=true;if(s==T::Stage::ResetRetries){summary_.retried=true;summary_.remaining=0;}if(s==T::Stage::DecrementRetries)summary_.remaining=uint16_t(word_->retries()-1);word_->advanceBefore(cost,[&]{return summary_.status=uint8_t(read(port()+4,cost));},[&](unsigned off,uint8_t v){write(port()+off,v,cost);},[&](uint16_t v){summary_.address=v;summary_.cachedAddress=v;write16(0x159f2,v);});if(word_->stage()==T::Stage::AwaitFault){fault_.emplace();++summary_.faults;}}return cost;}
  if(select_){if(select_->stage()==DspSelectTask::Stage::Complete){lastSelect_=select_;select_.reset();effect_.read=true;resumeRoute();}else{if(select_->stage()==DspSelectTask::Stage::Select){summary_.selectedPort=true;summary_.port=select_->port();}select_->advanceBefore(cost,[&](DspPort,uint16_t v){summary_.cachedAddress=v;write16(0x159f2,v);});}return cost;}
  using T=DspRoutingTask;auto&t=*routes_[routeCount_-1];if(t.stage()==T::Stage::Complete){effect_.read=true;finishRoute();return cost;}const auto s=t.stage();if(s==T::Stage::DispatchWindow||s==T::Stage::DispatchSelect||s==T::Stage::DispatchAddress||s==T::Stage::DispatchWrite)effect_.write=true;
  if(s==T::Stage::SelectPort){summary_.selectedPort=true;summary_.port=t.kind()==T::Kind::WriteB?DspPort::B:DspPort::A;}
  if(s==T::Stage::PrimaryProduct||s==T::Stage::AlternateProduct){summary_.clearedRetry=true;summary_.retried=false;summary_.remaining=0;}
  t.advanceBefore(cost,[&](T::Read r)->uint16_t{switch(r){case T::Read::PrimaryOffset:return read16(0x174a8);case T::Read::AlternateOffset:return read16(0x174aa);case T::Read::CachedAddress:return summary_.cachedAddress=read16(0x159f2);case T::Read::VoiceB:return read(0x174a7,cost);case T::Read::VoiceA:return read(0x17526,cost);case T::Read::VoiceBase:return read(0x167ba,cost);}throw std::logic_error("Unknown DSP routing observation");},[&](T::Write w,uint16_t v){if(w==T::Write::CachedAddress)summary_.cachedAddress=v;write16(w==T::Write::PrimaryOffset?0x174a8:w==T::Write::AlternateOffset?0x174aa:0x159f2,v);});
  if(s==T::Stage::ReadPrimary||s==T::Stage::ReadAlternate)summary_.offsetValue=t.offset();
  if(s==T::Stage::ResolveAddress){summary_.offsetAdded=true;summary_.offsetBase=t.relative();}
  if(s==T::Stage::CompareCached){summary_.cacheCompared=true;summary_.comparedAddress=t.address();summary_.comparedCached=t.cached();}
  if(s==T::Stage::Difference){summary_.voiceDifference=true;summary_.voice=t.voice();summary_.baseVoice=t.baseVoice();}
  if(s==T::Stage::ReadVoice)summary_.voice=t.voice();
  if(s==T::Stage::AlternateProduct)summary_.alternateProduct=t.product();
  const auto request=t.request();const auto target=t.address();summary_.address=target;
  switch(request){case T::Request::None:break;case T::Request::PrimeAddress:pushRoute(T::Kind::PrimeAbsolute,target);break;case T::Request::WriteAddress:pushRoute(T::Kind::WriteAbsolute,target);break;case T::Request::ReadAddress:address_.emplace(target,true,DspAddressTask::PortSelection::B);break;case T::Request::SelectA:case T::Request::SelectB:select_.emplace(request==T::Request::SelectA?DspPort::A:DspPort::B);break;case T::Request::Address:address_.emplace(target);break;case T::Request::AddressThenWord:addressForWord_=true;address_.emplace(target);break;}
  return cost;
 }
private:
 void pushRoute(DspRoutingTask::Kind k,uint16_t a){if(routeCount_==routes_.size())throw std::logic_error("Controller DSP routing capacity");routes_[routeCount_++].emplace(k,a);}
 void resumeRoute(){if(routeCount_)routes_[routeCount_-1]->resumeChild();else complete_=true;}
 void finishRoute(){lastRoute_=routes_[--routeCount_];routes_[routeCount_].reset();resumeRoute();}
 Summary summary_;Effect effect_{};bool complete_=false,addressForWord_=false;std::array<std::optional<DspRoutingTask>,2>routes_{};unsigned routeCount_=0;
 std::optional<DspRoutingTask>lastRoute_;std::optional<DspAddressTask>address_,lastAddress_;std::optional<DspWordTask>word_,lastWord_;std::optional<DspSelectTask>select_,lastSelect_;std::optional<DspFaultTask>fault_,lastFault_;
};
}
