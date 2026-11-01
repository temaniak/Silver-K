#pragma once
#include <cstdint>
namespace ks {
// Byte ring used by internal MIDI. A full producer still writes its byte, sets
// overflow=2 and leaves the producer unchanged; the consumer advances first.
class InternalMidiQueueTask final {
public:
 enum class Kind { Reset, Pop, Push };
 enum class Stage { SaveReset, ClearReset, ResetProducer, ResetConsumer, ResetOverflow, RestoreReset, ReturnReset, ClearResult, SavePop, SavePopAddress, ReadConsumer, ReadProducer, CompareEmpty, SelectEmpty, SelectPopBuffer, NormalizeConsumer, LocatePop, AdvanceConsumer, WrapConsumer, PublishConsumer, ReadByte, SetAvailable, RestorePopAddress, RestorePop, ReturnPop, SavePush, SavePushAddress, ReadPushProducer, NormalizeProducer, SelectPushBuffer, LocatePush, WriteByte, AdvanceProducer, WrapProducer, ReadPushConsumer, CompareFull, SelectFull, PublishProducer, FinishPublish, MarkOverflow, WriteOverflow, RestorePushAddress, RestorePush, ReturnPush, Complete };
 enum class Read { Consumer, Producer, Byte };enum class Write { Consumer, Producer, Byte, Overflow };
 explicit InternalMidiQueueTask(Kind kind,uint8_t value=0):stage_(kind==Kind::Reset?Stage::SaveReset:kind==Kind::Pop?Stage::ClearResult:Stage::SavePush),value_(value){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
 uint16_t consumer()const{return consumer_;}uint16_t producer()const{return producer_;}uint16_t index()const{return index_;}
 uint8_t value()const{return value_;}bool available()const{return available_;}bool full()const{return full_;}
 unsigned nextClocks(unsigned byteWrite=3,unsigned flagWrite=3)const{switch(stage_){case Stage::WrapConsumer:case Stage::ReadByte:case Stage::WrapProducer:return 6; case Stage::SelectPopBuffer:case Stage::SelectPushBuffer:return 9; case Stage::SaveReset:case Stage::RestoreReset:return 11; case Stage::ResetProducer:case Stage::ResetConsumer:case Stage::ResetOverflow:case Stage::ReadConsumer:case Stage::ReadProducer:case Stage::PublishConsumer:case Stage::ReadPushProducer:case Stage::ReadPushConsumer:case Stage::PublishProducer:return 12; case Stage::ReturnReset:case Stage::ReturnPop:case Stage::ReturnPush:return 17; case Stage::SavePop:case Stage::SavePopAddress:case Stage::RestorePopAddress:case Stage::RestorePop:case Stage::SavePush:case Stage::SavePushAddress:case Stage::RestorePushAddress:case Stage::RestorePush:return 20; case Stage::WriteByte:return 3+byteWrite;case Stage::WriteOverflow:return 9+flagWrite;case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned byteWrite=3,unsigned flagWrite=3){
  const auto cost=nextClocks(byteWrite,flagWrite);if(!cost||available<cost)return 0;
  switch(stage_){
  case Stage::ResetProducer:write(Write::Producer,0);next();break;
  case Stage::ResetConsumer:write(Write::Consumer,0);next();break;
  case Stage::ResetOverflow:write(Write::Overflow,0);next();break;
  case Stage::ReadConsumer:case Stage::ReadPushConsumer:consumer_=uint16_t(read(Read::Consumer));next();break;
  case Stage::ReadProducer:case Stage::ReadPushProducer:producer_=uint16_t(read(Read::Producer));next();break;
  case Stage::SelectEmpty:stage_=consumer_==producer_?Stage::RestorePopAddress:Stage::SelectPopBuffer;break;
  case Stage::LocatePop:index_=consumer_;next();break;
  case Stage::AdvanceConsumer:consumer_=uint16_t(consumer_+1);next();break;
  case Stage::WrapConsumer:consumer_&=255;next();break;
  case Stage::PublishConsumer:write(Write::Consumer,consumer_);next();break;
  case Stage::ReadByte:value_=uint8_t(read(Read::Byte));next();break;
  case Stage::SetAvailable:available_=true;next();break;
  case Stage::LocatePush:index_=producer_;next();break;
  case Stage::WriteByte:write(Write::Byte,value_);next();break;
  case Stage::AdvanceProducer:producer_=uint16_t(producer_+1);next();break;
  case Stage::WrapProducer:producer_&=255;next();break;
  case Stage::SelectFull:full_=consumer_==producer_;stage_=full_?Stage::MarkOverflow:Stage::PublishProducer;break;
  case Stage::PublishProducer:write(Write::Producer,producer_);next();break;
  case Stage::FinishPublish:stage_=Stage::RestorePushAddress;break;
  case Stage::WriteOverflow:write(Write::Overflow,2);next();break;
  case Stage::ReturnReset:case Stage::ReturnPop:case Stage::ReturnPush:stage_=Stage::Complete;break;
  default:next();break;
  }return cost;
 }
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_;uint8_t value_;uint16_t consumer_=0,producer_=0,index_=0;bool available_=false,full_=false;
};
}
