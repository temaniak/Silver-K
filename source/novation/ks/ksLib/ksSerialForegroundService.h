#pragma once
#include "ksSerialQueueTask.h"
#include "ksSerialReceiveTask.h"
#include "ksSerialTransmitTask.h"
#include <optional>

namespace ks {
// Complete foreground UART graphs. Blocking receive retries a complete queue
// pop against live resources; transmit waits for live space without restarting.
class SerialForegroundService final {
public:
 enum class Kind { ResetRx, PopRx, Receive, ResetTx, Transmit };
 enum class Component { Queue, Receive, Transmit };
 explicit SerialForegroundService(Kind kind,uint8_t input=0):kind_(kind),input_(input){
  if(kind==Kind::Transmit){transmit_.emplace();component_=Component::Transmit;}
  else if(kind==Kind::Receive){receive_.emplace();component_=Component::Receive;}
  else queue_.emplace(queueKind(kind));
 }
 Kind kind()const{return kind_;}Component component()const{return component_;}
 bool complete()const{return complete_;}bool critical()const{return critical_;}
 uint8_t status()const{return status_;}uint8_t data()const{return data_;}
 uint16_t result()const{return uint16_t((unsigned(status_)<<8)|data_);}
 uint16_t producer()const{return producer_;}uint16_t consumer()const{return consumer_;}
 uint16_t nextProducer()const{return uint16_t((producer_+1)&255);}
 bool atQueue(SerialQueueTask::Stage stage)const{return component_==Component::Queue&&queue_&&queue_->stage()==stage;}
 bool atReceive(SerialReceiveTask::Stage stage)const{return component_==Component::Receive&&receive_&&receive_->stage()==stage;}
 bool atTransmit(SerialTransmitTask::Stage stage)const{return component_==Component::Transmit&&transmit_&&transmit_->stage()==stage;}
 unsigned nextClocks()const{
  if(complete_)return 0;
  if(component_==Component::Queue)return queue_->nextClocks([]{return 3u;});
  if(component_==Component::Receive)return receive_->nextClocks();
  return transmit_->nextClocks([](SerialTransmitTask::Write what){return what==SerialTransmitTask::Write::Enable?1u:3u;});
 }
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write){
  const auto cost=nextClocks();if(!cost||cost>available)return 0;
  const auto readWord=[&](uint32_t a){const auto high=uint8_t(read(a,cost-3));return uint16_t((unsigned(high)<<8)|uint8_t(read(a+1,cost)));};
  const auto writeWord=[&](uint32_t a,uint16_t value){write(a,uint8_t(value>>8),cost-3);write(a+1,uint8_t(value),cost);};
  if(component_==Component::Queue){
   using T=SerialQueueTask;const auto stage=queue_->stage();const bool tx=queue_->kind()==T::Kind::ResetTx;
   queue_->advanceBefore(cost,[&](T::Read field)->uint16_t{
    switch(field){case T::Read::Status:return read(0xffed68,cost);
    case T::Read::Consumer:return readWord(tx?0xffed6e:0xffec66);
    case T::Read::Producer:return readWord(tx?0xffed6c:0xffec64);
    case T::Read::Data:return read(queue_->dataAddress(),cost);}return 0;
   },[&](T::Write field,uint16_t value){
    if(field==T::Write::Status)write(0xffed68,uint8_t(value),cost);
    else writeWord(field==T::Write::Producer?(tx?0xffed6c:0xffec64):(tx?0xffed6e:0xffec66),value);
   },[]{return 3u;});
   if(stage==T::Stage::MaskCritical)critical_=true;
   if(stage==T::Stage::RestoreCritical)critical_=false;
   if(queue_->stage()==T::Stage::Complete){
    status_=queue_->status();data_=queue_->data();consumer_=queue_->consumer();producer_=queue_->producer();queue_.reset();
    if(kind_==Kind::Receive){receive_->resumeChild(status_,data_);component_=Component::Receive;}
    else complete_=true;
   }
  }else if(component_==Component::Receive){
   receive_->advanceBefore(cost);
   if(receive_->awaiting()){queue_.emplace(SerialQueueTask::Kind::PopRx);component_=Component::Queue;}
   else if(receive_->stage()==SerialReceiveTask::Stage::Complete){status_=receive_->status();data_=receive_->data();complete_=true;}
  }else{
   using T=SerialTransmitTask;const auto stage=transmit_->stage();
   transmit_->advanceBefore(cost,[&](T::Read field)->uint16_t{return field==T::Read::Input?input_:readWord(field==T::Read::Producer?0xffed6c:0xffed6e);},
    [&](T::Write field,uint16_t value){if(field==T::Write::Producer)writeWord(0xffed6c,value);else write(field==T::Write::Data?transmit_->dataAddress():0xffff8a,uint8_t(value),cost);},
    [](T::Write field){return field==T::Write::Enable?1u:3u;});
   if(stage==T::Stage::MaskCritical)critical_=true;
   if(stage==T::Stage::RestoreCritical)critical_=false;
   producer_=transmit_->producer();consumer_=transmit_->consumer();
   if(transmit_->stage()==T::Stage::Complete){data_=input_;complete_=true;}
  }
  return cost;
 }
private:
 static SerialQueueTask::Kind queueKind(Kind kind){return kind==Kind::ResetRx?SerialQueueTask::Kind::ResetRx:kind==Kind::ResetTx?SerialQueueTask::Kind::ResetTx:SerialQueueTask::Kind::PopRx;}
 Kind kind_;Component component_=Component::Queue;uint8_t input_=0,status_=0,data_=0;uint16_t producer_=0,consumer_=0;bool complete_=false,critical_=false;
 std::optional<SerialQueueTask>queue_;std::optional<SerialReceiveTask>receive_;std::optional<SerialTransmitTask>transmit_;
};
}
