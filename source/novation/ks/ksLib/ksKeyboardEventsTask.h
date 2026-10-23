#pragma once
#include <cstdint>
namespace ks {
// Keyboard contact transitions, original nonlinear velocity curve and live wrapping event queue.
class KeyboardEventsTask final {
public:
 enum class Kind { Transition0, Transition1, Transition2, Transition3, Transition4, Transition5, Transition6, Transition7, Transition8, Transition9, TransitionA, TransitionB, TransitionC, TransitionD, TransitionE, TransitionF, Press, Release, FixedOn, FixedOff, Repeat, ReleaseVelocity, Velocity, Reset, Pop, Append };enum class Stage { Transition0, Transition1, Transition2, Transition3, Transition4, Transition5, Transition6, Transition7, Transition8, Transition9, TransitionA, TransitionB, TransitionC, TransitionD, TransitionE, TransitionF, ReadPressTick, StorePressTick, SetPressed, ReturnPress, ReadReleaseTick, StoreReleaseTick, ClearPressed, ReturnRelease, ReadFixedOnKey, BindFixedOnVelocity, AppendFixedOn, ReadFixedOffKey, BindFixedOffVelocity, AppendFixedOff, ReadRepeatFlags, SelectRepeatPressed, ReadRepeatTick, ReadRepeatStart, ElapsedRepeat, BiasRepeat, SelectEarlyRepeat, IncrementRepeat, CalculateRepeat, MarkRepeat, ReadRepeatKey, AppendRepeat, ReturnRepeat, ReadReleaseFlags, SelectReleasePressed, ReadVelocityTick, ReadVelocityStart, ElapsedVelocity, CalculateRelease, ReadReleaseKey, AppendRelease, ReturnReleaseVelocity, SaveVelocityValue, SaveVelocityIndex, RetainDelay, BindSlowPeriod, BindSlowBias, ReadVelocityFlags, SelectVelocityMode, BindFastPeriod, BindFastBias, SubtractDelayBias, SelectDelayFloor, ClearDelayFloor, ClearNumeratorLow, SquareDelay, BindDenominatorBias, NormalizeDenominatorBias, AddDenominatorBias, HalfNumerator, TestDenominatorHigh, SelectDenominatorShift, ShiftDenominator, ShiftNumerator, RepeatDenominator, DivideVelocity, IncrementVelocity, SelectVelocityLimit, ClampVelocity, RestoreVelocityIndex, RestoreVelocityValue, ReturnVelocity, SaveResetEvent, ClearResetEvent, WriteResetProducer, WriteResetConsumer, RestoreResetEvent, ReturnReset, SavePopValue, ReadPopConsumer, ReadPopProducer, ComparePopIndices, SelectPopEmpty, ClearPopEvent, RestoreEmptyPopValue, ReturnEmptyPop, SavePopIndex, BindPopBase, NormalizePopIndex, LocatePopEvent, IncrementPopConsumer, WrapPopConsumer, WritePopConsumer, ReadPopEvent, RestorePopIndex, RestorePopValue, ReturnPop, SaveAppendIndex, SaveAppendValue, ReadAppendProducer, NormalizeAppendIndex, BindAppendBase, LocateAppendEvent, WriteAppendEvent, IncrementAppendProducer, WrapAppendProducer, WriteAppendProducer, RestoreAppendValue, RestoreAppendIndex, ReturnAppend, AwaitChild, Complete };
 enum class Read { Tick, Start, Flags, Key, Consumer, Producer, Event };enum class Write { Start, Flags, Consumer, Producer, Event };
 uint32_t record()const{return record_;}uint8_t flags()const{return flags_;}uint8_t key()const{return key_;}uint16_t tick()const{return tick_;}uint16_t start()const{return start_;}uint16_t consumer()const{return consumer_;}uint16_t producer()const{return producer_;}uint16_t message()const{return message_;}uint16_t delay()const{return delay_;}uint8_t velocity()const{return velocity_;}
 explicit KeyboardEventsTask(Kind kind,uint32_t record=0,uint16_t input=0){record_=record;delay_=input;message_=input;switch(kind){case Kind::Transition0:stage_=Stage::Transition0;break;case Kind::Transition1:stage_=Stage::Transition1;break;case Kind::Transition2:stage_=Stage::Transition2;break;case Kind::Transition3:stage_=Stage::Transition3;break;case Kind::Transition4:stage_=Stage::Transition4;break;case Kind::Transition5:stage_=Stage::Transition5;break;case Kind::Transition6:stage_=Stage::Transition6;break;case Kind::Transition7:stage_=Stage::Transition7;break;case Kind::Transition8:stage_=Stage::Transition8;break;case Kind::Transition9:stage_=Stage::Transition9;break;case Kind::TransitionA:stage_=Stage::TransitionA;break;case Kind::TransitionB:stage_=Stage::TransitionB;break;case Kind::TransitionC:stage_=Stage::TransitionC;break;case Kind::TransitionD:stage_=Stage::TransitionD;break;case Kind::TransitionE:stage_=Stage::TransitionE;break;case Kind::TransitionF:stage_=Stage::TransitionF;break;case Kind::Press:stage_=Stage::ReadPressTick;break;case Kind::Release:stage_=Stage::ReadReleaseTick;break;case Kind::FixedOn:stage_=Stage::ReadFixedOnKey;break;case Kind::FixedOff:stage_=Stage::ReadFixedOffKey;break;case Kind::Repeat:stage_=Stage::ReadRepeatFlags;break;case Kind::ReleaseVelocity:stage_=Stage::ReadReleaseFlags;break;case Kind::Velocity:stage_=Stage::SaveVelocityValue;break;case Kind::Reset:stage_=Stage::SaveResetEvent;break;case Kind::Pop:stage_=Stage::SavePopValue;break;case Kind::Append:stage_=Stage::SaveAppendIndex;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t velocity){if(!awaitingChild())return false;if(resume_==Stage::MarkRepeat||resume_==Stage::ReadReleaseKey)velocity_=velocity;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::Transition0:case Stage::Transition2:case Stage::Transition5:case Stage::Transition6:case Stage::Transition8:case Stage::Transition9:case Stage::TransitionA:case Stage::TransitionB:case Stage::TransitionE:case Stage::TransitionF:case Stage::ReturnPress:case Stage::ReturnRelease:case Stage::ReturnRepeat:case Stage::ReturnReleaseVelocity:case Stage::ReturnVelocity:case Stage::ReturnReset:case Stage::ReturnEmptyPop:case Stage::ReturnPop:case Stage::ReturnAppend:return 17; case Stage::ReadPressTick:case Stage::ReadReleaseTick:case Stage::ReadRepeatTick:case Stage::CalculateRepeat:case Stage::ReadVelocityTick:case Stage::CalculateRelease:return 15; case Stage::StorePressTick:case Stage::StoreReleaseTick:case Stage::ReadRepeatStart:case Stage::ReadVelocityStart:return 6+store; case Stage::SetPressed:case Stage::ClearPressed:return 6+3+store; case Stage::ReadFixedOnKey:case Stage::ReadFixedOffKey:case Stage::ReadRepeatFlags:case Stage::ReadRepeatKey:case Stage::ReadReleaseFlags:case Stage::ReadReleaseKey:case Stage::ReadVelocityFlags:case Stage::BindPopBase:case Stage::BindAppendBase:return 9; case Stage::AppendFixedOn:case Stage::AppendFixedOff:case Stage::AppendRepeat:case Stage::AppendRelease:return 8; case Stage::BiasRepeat:case Stage::BindSlowPeriod:case Stage::BindSlowBias:case Stage::BindFastPeriod:case Stage::BindFastBias:case Stage::WrapPopConsumer:case Stage::WrapAppendProducer:return 6; case Stage::SaveVelocityValue:case Stage::SaveVelocityIndex:case Stage::RestoreVelocityIndex:case Stage::RestoreVelocityValue:case Stage::SavePopValue:case Stage::RestoreEmptyPopValue:case Stage::SavePopIndex:case Stage::RestorePopIndex:case Stage::RestorePopValue:case Stage::SaveAppendIndex:case Stage::SaveAppendValue:case Stage::RestoreAppendValue:case Stage::RestoreAppendIndex:return 20; case Stage::SquareDelay:case Stage::DivideVelocity:return 23; case Stage::SaveResetEvent:case Stage::RestoreResetEvent:return 11; case Stage::WriteResetProducer:case Stage::WriteResetConsumer:case Stage::ReadPopConsumer:case Stage::ReadPopProducer:case Stage::WritePopConsumer:case Stage::ReadAppendProducer:case Stage::WriteAppendProducer:return 12; case Stage::ReadPopEvent:case Stage::WriteAppendEvent:return 3+store; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::Transition1:stage_=Stage::ReadRepeatFlags;break;
case Stage::Transition3:stage_=Stage::ReadFixedOffKey;break;
case Stage::Transition4:stage_=Stage::ReadPressTick;break;
case Stage::Transition7:stage_=Stage::ReadReleaseTick;break;
case Stage::TransitionC:stage_=Stage::ReadFixedOnKey;break;
case Stage::TransitionD:stage_=Stage::ReadReleaseFlags;break;
case Stage::ReadPressTick:tick_=uint16_t(read(Read::Tick));next();break;
case Stage::StorePressTick:write(Write::Start,tick_);next();break;
case Stage::SetPressed:flags_=uint8_t(read(Read::Flags))|2;write(Write::Flags,flags_);next();break;
case Stage::ReadReleaseTick:tick_=uint16_t(read(Read::Tick));next();break;
case Stage::StoreReleaseTick:write(Write::Start,tick_);next();break;
case Stage::ClearPressed:flags_=uint8_t(read(Read::Flags))&uint8_t(~2);write(Write::Flags,flags_);next();break;
case Stage::ReadFixedOnKey:key_=uint8_t(read(Read::Key));next();break;
case Stage::BindFixedOnVelocity:message_=uint16_t(key_|0x7f00);next();break;
case Stage::AppendFixedOn:stage_=Stage::SaveAppendIndex;break;
case Stage::ReadFixedOffKey:key_=uint8_t(read(Read::Key));next();break;
case Stage::BindFixedOffVelocity:message_=uint16_t(key_|0xff00);next();break;
case Stage::AppendFixedOff:stage_=Stage::SaveAppendIndex;break;
case Stage::ReadRepeatFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectRepeatPressed:stage_=flags_&2?Stage::ReturnRepeat:Stage::ReadRepeatTick;break;
case Stage::ReadRepeatTick:tick_=uint16_t(read(Read::Tick));next();break;
case Stage::ReadRepeatStart:start_=uint16_t(read(Read::Start));next();break;
case Stage::ElapsedRepeat:delay_=uint16_t(tick_-start_);next();break;
case Stage::BiasRepeat:early_=delay_<10;delay_=uint16_t(delay_-10);next();break;
case Stage::SelectEarlyRepeat:stage_=early_?Stage::ReadFixedOffKey:Stage::IncrementRepeat;break;
case Stage::IncrementRepeat:++delay_;next();break;
case Stage::MarkRepeat:velocity_|=128;next();break;
case Stage::ReadRepeatKey:key_=uint8_t(read(Read::Key));message_=uint16_t(key_|(unsigned(velocity_)<<8));next();break;
case Stage::AppendRepeat:stage_=Stage::SaveAppendIndex;break;
case Stage::ReadReleaseFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectReleasePressed:stage_=flags_&2?Stage::ReadVelocityTick:Stage::ReturnReleaseVelocity;break;
case Stage::ReadVelocityTick:tick_=uint16_t(read(Read::Tick));next();break;
case Stage::ReadVelocityStart:start_=uint16_t(read(Read::Start));next();break;
case Stage::ElapsedVelocity:delay_=uint16_t(tick_-start_);next();break;
case Stage::ReadReleaseKey:key_=uint8_t(read(Read::Key));message_=uint16_t(key_|(unsigned(velocity_)<<8));next();break;
case Stage::AppendRelease:stage_=Stage::SaveAppendIndex;break;
case Stage::BindSlowPeriod:period_=120;next();break;
case Stage::BindSlowBias:bias_=3;next();break;
case Stage::ReadVelocityFlags:flags_=uint8_t(read(Read::Flags));next();break;
case Stage::SelectVelocityMode:stage_=flags_&1?Stage::BindFastPeriod:Stage::SubtractDelayBias;break;
case Stage::BindFastPeriod:period_=30;next();break;
case Stage::BindFastBias:bias_=2;next();break;
case Stage::SubtractDelayBias:delay_=uint16_t(delay_-bias_);next();break;
case Stage::SelectDelayFloor:stage_=delay_&0x8000?Stage::ClearDelayFloor:Stage::ClearNumeratorLow;break;
case Stage::ClearDelayFloor:delay_=0;next();break;
case Stage::ClearNumeratorLow:numerator_=uint32_t(period_)<<16;next();break;
case Stage::SquareDelay:denominator_=uint32_t(delay_)*delay_;next();break;
case Stage::AddDenominatorBias:denominator_+=period_;next();break;
case Stage::HalfNumerator:numerator_>>=1;next();break;
case Stage::SelectDenominatorShift:stage_=denominator_>>16?Stage::ShiftDenominator:Stage::DivideVelocity;break;
case Stage::ShiftDenominator:denominator_>>=1;next();break;
case Stage::ShiftNumerator:numerator_>>=1;next();break;
case Stage::RepeatDenominator:stage_=Stage::TestDenominatorHigh;break;
case Stage::DivideVelocity:quotient_=uint16_t(uint16_t(denominator_)?numerator_/uint16_t(denominator_):0);next();break;
case Stage::IncrementVelocity:negative_=int(int8_t(quotient_>>8))+1<0;velocity_=uint8_t((quotient_>>8)+1);next();break;
case Stage::SelectVelocityLimit:stage_=negative_?Stage::ClampVelocity:Stage::RestoreVelocityIndex;break;
case Stage::ClampVelocity:velocity_=127;next();break;
case Stage::ClearResetEvent:message_=0;next();break;
case Stage::WriteResetProducer:write(Write::Producer,0);next();break;
case Stage::WriteResetConsumer:write(Write::Consumer,0);next();break;
case Stage::ReadPopConsumer:consumer_=uint16_t(read(Read::Consumer));next();break;
case Stage::ReadPopProducer:producer_=uint16_t(read(Read::Producer));next();break;
case Stage::SelectPopEmpty:stage_=consumer_==producer_?Stage::ClearPopEvent:Stage::SavePopIndex;break;
case Stage::ClearPopEvent:message_=0;next();break;
case Stage::IncrementPopConsumer:next_=uint16_t(consumer_+2);next();break;
case Stage::WrapPopConsumer:next_&=0x1fe;next();break;
case Stage::WritePopConsumer:write(Write::Consumer,next_);next();break;
case Stage::ReadPopEvent:message_=uint16_t(read(Read::Event));next();break;
case Stage::ReadAppendProducer:producer_=uint16_t(read(Read::Producer));next();break;
case Stage::WriteAppendEvent:write(Write::Event,message_);next();break;
case Stage::IncrementAppendProducer:next_=uint16_t(producer_+2);next();break;
case Stage::WrapAppendProducer:next_&=0x1fe;next();break;
case Stage::WriteAppendProducer:write(Write::Producer,next_);next();break;
case Stage::Transition0:stage_=Stage::Complete;break;
case Stage::Transition2:stage_=Stage::Complete;break;
case Stage::Transition5:stage_=Stage::Complete;break;
case Stage::Transition6:stage_=Stage::Complete;break;
case Stage::Transition8:stage_=Stage::Complete;break;
case Stage::Transition9:stage_=Stage::Complete;break;
case Stage::TransitionA:stage_=Stage::Complete;break;
case Stage::TransitionB:stage_=Stage::Complete;break;
case Stage::TransitionE:stage_=Stage::Complete;break;
case Stage::TransitionF:stage_=Stage::Complete;break;
case Stage::ReturnPress:stage_=Stage::Complete;break;
case Stage::ReturnRelease:stage_=Stage::Complete;break;
case Stage::CalculateRepeat:await(Stage::MarkRepeat);break;
case Stage::ReturnRepeat:stage_=Stage::Complete;break;
case Stage::CalculateRelease:await(Stage::ReadReleaseKey);break;
case Stage::ReturnReleaseVelocity:stage_=Stage::Complete;break;
case Stage::ReturnVelocity:stage_=Stage::Complete;break;
case Stage::ReturnReset:stage_=Stage::Complete;break;
case Stage::ReturnEmptyPop:stage_=Stage::Complete;break;
case Stage::ReturnPop:stage_=Stage::Complete;break;
case Stage::ReturnAppend:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t record_=0,denominator_=0,numerator_=0;uint16_t tick_=0,start_=0,delay_=0,period_=0,quotient_=0,consumer_=0,producer_=0,next_=0,message_=0;uint8_t flags_=0,key_=0,velocity_=0,bias_=0;bool early_=false,negative_=false;
};
}
