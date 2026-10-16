#pragma once
#include <cstdint>
namespace ks {
class ArpPrimitiveTask final {
public:
 enum class Kind {NullResult,Tempo,Reset,Pattern};
 enum class Stage {ZeroResult,FinishNull,DispatchPart,AwaitPart,ReadTempo,CompareTempo,AdmitTempo,PublishTempo,ScaleIndex,NormalizeIndexWord,NormalizeIndex,ReadRate,PublishRate,DispatchReset,AwaitReset,FinishTempo,
 ZeroPhase,PublishAccumulator,PublishCursorZero,PublishPhaseZero,PublishGateZero,ReadMode,ObserveMono,AdmitMono,DispatchMono,AwaitMono,FinishReset,
 RetainInput,ClearPatternIndex,ReadCursor,CaptureCursor,DoubleCursor,ReadPattern,LocateStep,ReadStep,TickCursor,ReadLength,CompareLength,AdmitLength,WrapCursor,ReloadPattern,PublishCursor,ReleaseInput,FinishPattern,Complete};
 enum class Read {Tempo,DesiredTempo,Rate,Mode,Cursor,Pattern,Step,Length};
 enum class Write {Tempo,Rate,Accumulator,CursorZero,PhaseZero,GateZero,Cursor};
 explicit ArpPrimitiveTask(Kind k,uint8_t part=0):kind_(k),stage_(k==Kind::NullResult?Stage::ZeroResult:k==Kind::Tempo?Stage::DispatchPart:k==Kind::Reset?Stage::ZeroPhase:Stage::RetainInput),part_(part){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t tempo()const{return tempo_;}uint8_t desiredTempo()const{return desired_;}uint8_t rateIndex()const{return rateIndex_;}uint32_t rate()const{return rate_;}uint8_t mode()const{return mode_;}
 uint8_t cursor()const{return cursor_;}uint8_t nextCursor()const{return nextCursor_;}uint8_t wordIndex()const{return wordIndex_;}uint8_t length()const{return length_;}uint32_t pattern()const{return pattern_;}uint32_t nextReference()const{return nextReference_;}uint16_t step()const{return step_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=Stage::ReadTempo;return true;}
 bool awaitingChild()const{return stage_==Stage::AwaitReset||stage_==Stage::AwaitMono;}bool resumeChild(){if(stage_==Stage::AwaitReset)stage_=Stage::FinishTempo;else if(stage_==Stage::AwaitMono)stage_=Stage::FinishReset;else return false;return true;}
 bool publication()const{switch(stage_){case Stage::PublishTempo:case Stage::PublishCursorZero:case Stage::PublishPhaseZero:case Stage::PublishGateZero:case Stage::PublishCursor:return true;default:return false;}}
 Write publicationKind()const{return stage_==Stage::PublishTempo?Write::Tempo:stage_==Stage::PublishCursorZero?Write::CursorZero:stage_==Stage::PublishPhaseZero?Write::PhaseZero:stage_==Stage::PublishGateZero?Write::GateZero:Write::Cursor;}
 unsigned nextClocks(unsigned store=3,unsigned wordClocks=3)const{switch(stage_){
 case Stage::DispatchPart:case Stage::DispatchReset:case Stage::DispatchMono:return 20;
 case Stage::ReadTempo:case Stage::ReadMode:case Stage::ReadCursor:case Stage::ReadLength:return 15;
 case Stage::PublishTempo:case Stage::PublishCursorZero:case Stage::PublishPhaseZero:case Stage::PublishGateZero:case Stage::PublishCursor:return 12+store;
 case Stage::ReadRate:case Stage::PublishRate:case Stage::PublishAccumulator:case Stage::ReadPattern:case Stage::ReloadPattern:return 21;
 case Stage::RetainInput:case Stage::ReleaseInput:return 11;case Stage::ReadStep:return 5+wordClocks;
 case Stage::FinishNull:case Stage::FinishTempo:case Stage::FinishReset:case Stage::FinishPattern:return 17;
 case Stage::AwaitPart:case Stage::AwaitReset:case Stage::AwaitMono:case Stage::Complete:return 0;default:return 3;}}
 bool complete()const{return stage_==Stage::Complete;}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationKind()):3,stage_==Stage::ReadStep?timing(Write::Rate):3)){
  if(cost>available-used)break;switch(stage_){
  case Stage::ZeroResult:stage_=Stage::FinishNull;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::ReadTempo:tempo_=uint8_t(read(Read::Tempo));stage_=Stage::CompareTempo;break;
  case Stage::CompareTempo:desired_=uint8_t(read(Read::DesiredTempo));stage_=Stage::AdmitTempo;break;case Stage::AdmitTempo:stage_=tempo_==desired_?Stage::FinishTempo:Stage::PublishTempo;break;
  case Stage::PublishTempo:write(Write::Tempo,desired_);rateIndex_=desired_;stage_=Stage::ScaleIndex;break;case Stage::ScaleIndex:rateIndex_=uint8_t(rateIndex_*4);stage_=Stage::NormalizeIndexWord;break;
  case Stage::NormalizeIndexWord:stage_=Stage::NormalizeIndex;break;case Stage::NormalizeIndex:stage_=Stage::ReadRate;break;case Stage::ReadRate:rate_=uint32_t(read(Read::Rate));stage_=Stage::PublishRate;break;
  case Stage::PublishRate:write(Write::Rate,rate_);stage_=Stage::DispatchReset;break;case Stage::DispatchReset:stage_=Stage::AwaitReset;break;
  case Stage::ZeroPhase:stage_=Stage::PublishAccumulator;break;case Stage::PublishAccumulator:write(Write::Accumulator,0);stage_=Stage::PublishCursorZero;break;
  case Stage::PublishCursorZero:write(Write::CursorZero,0);stage_=Stage::PublishPhaseZero;break;case Stage::PublishPhaseZero:write(Write::PhaseZero,0);stage_=Stage::PublishGateZero;break;case Stage::PublishGateZero:write(Write::GateZero,0);stage_=Stage::ReadMode;break;
  case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));stage_=Stage::ObserveMono;break;case Stage::ObserveMono:stage_=Stage::AdmitMono;break;case Stage::AdmitMono:stage_=(mode_&64)?Stage::DispatchMono:Stage::FinishReset;break;case Stage::DispatchMono:stage_=Stage::AwaitMono;break;
  case Stage::RetainInput:stage_=Stage::ClearPatternIndex;break;case Stage::ClearPatternIndex:stage_=Stage::ReadCursor;break;case Stage::ReadCursor:cursor_=uint8_t(read(Read::Cursor));stage_=Stage::CaptureCursor;break;case Stage::CaptureCursor:nextCursor_=cursor_;stage_=Stage::DoubleCursor;break;
  case Stage::DoubleCursor:wordIndex_=uint8_t(cursor_*2);stage_=Stage::ReadPattern;break;case Stage::ReadPattern:pattern_=uint32_t(read(Read::Pattern));stage_=Stage::LocateStep;break;case Stage::LocateStep:nextReference_=pattern_+wordIndex_;stage_=Stage::ReadStep;break;
  case Stage::ReadStep:step_=uint16_t(read(Read::Step));nextReference_+=2;stage_=Stage::TickCursor;break;case Stage::TickCursor:++nextCursor_;stage_=Stage::ReadLength;break;case Stage::ReadLength:length_=uint8_t(read(Read::Length));stage_=Stage::CompareLength;break;
  case Stage::CompareLength:stage_=Stage::AdmitLength;break;case Stage::AdmitLength:stage_=nextCursor_<length_?Stage::PublishCursor:Stage::WrapCursor;break;case Stage::WrapCursor:nextCursor_=0;stage_=Stage::ReloadPattern;break;
  case Stage::ReloadPattern:nextReference_=uint32_t(read(Read::Pattern));stage_=Stage::PublishCursor;break;case Stage::PublishCursor:write(Write::Cursor,nextCursor_);stage_=Stage::ReleaseInput;break;case Stage::ReleaseInput:stage_=Stage::FinishPattern;break;
  case Stage::FinishNull:case Stage::FinishTempo:case Stage::FinishReset:case Stage::FinishPattern:stage_=Stage::Complete;break;default:break;}used+=cost;
 }return used;}
private:Kind kind_;Stage stage_;uint8_t part_=0,tempo_=0,desired_=0,rateIndex_=0,mode_=0,cursor_=0,nextCursor_=0,wordIndex_=0,length_=0;uint32_t rate_=0,pattern_=0,nextReference_=0;uint16_t step_=0;
};
}
